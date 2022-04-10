#include "ir.hh"

#include <cxxabi.h>

#include <filesystem>
#include <iostream>
#include <unordered_set>

#include "llvm/ADT/StringMap.h"
#include "llvm/Analysis/DebugInfo.h"
#include "llvm/Instructions.h"
#include "llvm/LLVMContext.h"
#include "llvm/Support/DebugLoc.h"
#include "llvm/Support/IRReader.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

llvm::LLVMContext *get_llvm_context() {
    static std::unique_ptr<llvm::LLVMContext> context;
    if (!context) {
        context = std::make_unique<llvm::LLVMContext>();
    }
    return context.get();
}

std::vector<const llvm::Instruction *> get_function_instructions(const llvm::Module &module,
                                                                 const std::string &func_name) {
    auto *function = module.getFunction(func_name);
    if (!function) return {};
    std::vector<const llvm::Instruction *> res;
    res.reserve(function->size());
    for (auto const &blk : *function) {
        for (auto const &inst : blk) {
            res.emplace_back(&inst);
        }
    }
    return res;
}

std::string get_filename(const llvm::Instruction *inst) {
    auto const &loc = inst->getDebugLoc();
    auto *scope = loc.getAsMDNode(*get_llvm_context());
    if (scope) {
        auto di_location = llvm::DILocation(scope);
        return di_location.getFilename().str();
    } else {
        return {};
    }
}

uint32_t get_line_num(const llvm::Instruction *inst) {
    auto const &loc = inst->getDebugLoc();
    return loc.getLine();
}

const llvm::Function *get_function(const llvm::Instruction *inst) {
    auto *bb = inst->getParent();
    if (bb) {
        return bb->getParent();
    } else {
        return nullptr;
    }
}

std::map<std::string, std::map<uint32_t, std::vector<const llvm::Instruction *>>> get_instr_loc(
    const llvm::Function *function) {
    std::map<std::string, std::map<uint32_t, std::vector<const llvm::Instruction *>>> result;
    if (!function) return {};

    for (auto const &blk : *function) {
        for (auto const &inst : blk) {
            auto filename = get_filename(&inst);
            if (!filename.empty()) {
                auto line = get_line_num(&inst);
                result[filename][line].emplace_back(&inst);
            }
        }
    }
    return result;
}

// NOLINTNEXTLINE
void get_contained_functions(const llvm::Function *function, std::set<std::string> &res) {
    if (!function) return;
    for (auto const &blk : *function) {
        for (auto const &inst : blk) {
            if (llvm::isa<llvm::CallInst>(inst)) {
                auto const &call = llvm::cast<llvm::CallInst>(inst);
                // recursive call
                auto *func = call.getCalledFunction();
                if (!func) continue;
                auto name = func->getName().str();
                if (res.find(name) == res.end()) {
                    res.emplace(name);
                    get_contained_functions(func, res);
                }
            }
        }
    }
}

std::set<std::string> get_contained_functions(const llvm::Function *function) {
    std::set<std::string> res;
    get_contained_functions(function, res);
    return res;
}

std::map<std::string, const llvm::Function *> get_optimized_functions(
    const llvm::Module *module, const std::set<std::string> &function_names) {
    // use the fact that all transformed basic blocks as original function name's label with .exit
    std::map<std::string, const llvm::Function *> res;
    std::unordered_map<std::string, std::string> processed_names;
    for (auto const &func : function_names) {
        processed_names.emplace(func, func + ".exit");
    }
    auto const &functions = module->getFunctionList();
    for (auto const &function : functions) {
        for (auto const &block : function) {
            auto name = block.getName().str();
            for (auto const &[func_name, label] : processed_names) {
                if (name.rfind(label, 0) == 0) {
                    // found it
                    res.emplace(func_name, &function);
                    break;
                }
            }
        }
    }

    return res;
}

inline std::string demangle(const char *name) {
    // code from https://stackoverflow.com/a/28048299
    int status = -1;
    std::unique_ptr<char, void (*)(void *)> res{
        abi::__cxa_demangle(name, nullptr, nullptr, &status), std::free};
    return (status == 0) ? res.get() : std::string(name);
}

std::string get_demangled_name(const llvm::Function *function) {
    if (!function) return {};
    auto const *name = function->getName().data();
    return demangle(name);
}

std::string get_name(const llvm::Function *function) {
    if (!function) return {};
    return function->getName().str();
}

const llvm::Instruction *get_pre_alloc(const llvm::Instruction *instruction) {
    auto *node = instruction->getPrevNode();
    while (node) {
        if (llvm::isa<llvm::AllocaInst>(node)) {
            return node;
        }
        node = node->getPrevNode();
    }
    return nullptr;
}

std::string guess_rtl_name(const llvm::Instruction *instruction) {
    // I have no idea how Vitis generates signal names and do weird optimizations.
    // this is just some ways I gathered from generated LLVM bitcode and RTL
    if (!llvm::isa<llvm::AllocaInst>(instruction)) {
        // for now, we only focus on allocation
        return {};
    }
    for (auto use = instruction->use_begin(); use != instruction->use_end(); use++) {
        // should only have one use?
        auto *user = use.getUse().getUser();
        if (!user) continue;
        std::string name = user->getName().str();
        if (!name.empty()) {
            // no idea where these prefix come from, maybe it's not even always correct, since cmp
            // sounds like a comparison to me
            auto res = "ap_sig_allocacmp_" + name;
            return res;
        }
    }
    return {};
}

llvm::Module *parse_llvm_bitcode(const std::string &path) {
    llvm::SMDiagnostic error;
    auto module = llvm::ParseIRFile(path, error, *get_llvm_context());
    if (!module) {
        std::cerr << error.getMessage() << std::endl;
    }
    return module;
}

std::string resolve_filename(const std::string &filename, const std::string &directory) {
    namespace fs = std::filesystem;
    fs::path path = directory;
    path /= filename;
    auto res = fs::weakly_canonical(fs::absolute(path));
    return res.string();
}

Scope *process_var_decl(const llvm::CallInst &call_inst, Context &context, Scope *root_scope) {
    auto *value = llvm::cast<llvm::MDNode>(call_inst.getOperand(0))->getOperand(0);
    auto *desc = llvm::cast<llvm::MDNode>(call_inst.getOperand(1));
    auto num_op = desc->getNumOperands();
    std::string var_name;
    std::string value_name;

    if (!value->hasName()) return nullptr;
    value_name = value->getName().str();

    for (auto i = 0u; i < num_op; i++) {
        auto *op = desc->getOperand(i);
        if (!op) continue;
        if (llvm::isa<llvm::MDString>(op)) {
            auto md = llvm::cast<llvm::MDString>(op);
            var_name = md->getString().str();
            break;
        }
    }
    // try to guess the actual RTL name
    // obtain the value and see the type
    // for now we just hack it
    if (var_name.find('[') != std::string::npos) {
        // this is an auto variable
        // very likely created from an array
        value_name.append("_d0");
    } else {
        // normal wire
        // need to follow the use
        for (auto use = value->use_begin(); use != value->use_end(); use++) {
            if (llvm::isa<llvm::LoadInst>(*use)) {
                auto load = llvm::cast<llvm::LoadInst>(*use);
                value_name = "ap_sig_allocacmp_" + load->getName().str();
                break;
            }
        }
    }

    Variable var(var_name, value_name);
    // compute the debugging scope
    // for now we put everything in the same scope. Need to refactor this to compute
    // actual debug scope
    auto debug_loc = call_inst.getDebugLoc();
    uint32_t line = debug_loc.getLine();
    auto *s = context.add_scope<DeclInstruction>(root_scope, var, line);
    return s;
}

Scope *get_debug_scope(const llvm::Function *function, Context &context) {
    if (!function) return nullptr;
    auto *root_scope = context.add_scope<Scope>(nullptr);
    std::unordered_map<const llvm::DIScope *, Scope *> scope_mapping;

    std::unordered_set<uint32_t> lines;

    for (auto const &blk : *function) {
        for (auto const &instr : blk) {
            // we are dealing with old LLVM code
            // see the debug information here:
            // https://releases.llvm.org/3.1/docs/SourceLevelDebugging.html
            auto debug_loc = instr.getDebugLoc();
            auto *node = debug_loc.getAsMDNode(*get_llvm_context());
            if (!node) continue;

            Scope *res = nullptr;
            if (llvm::isa<llvm::CallInst>(instr)) {
                auto const &call_inst = llvm::cast<llvm::CallInst>(instr);
                auto const *called_function = call_inst.getCalledFunction();
                if (called_function) {
                    auto name = called_function->getName();
                    if (name == "llvm.dbg.declare") {
                        res = process_var_decl(call_inst, context, root_scope);
                    }
                }
            }

            if (!res) {
                // normal block
                auto line = debug_loc.getLine();
                if (line > 0 && lines.find(line) == lines.end()) {
                    auto *s = context.add_scope<Instruction>(root_scope, line);
                    lines.emplace(line);
                    res = s;
                }
            }

            // invalid
            if (!res) continue;

            res->instruction = &instr;

            // get debug information
            auto loc = llvm::DILocation(node);
            auto resolved_filename =
                resolve_filename(loc.getFilename().str(), loc.getDirectory().str());
            auto raw_filename = loc.getFilename().str();

            if (root_scope->filename.empty()) {
                root_scope->filename = resolved_filename;
                root_scope->raw_filename = raw_filename;
            }

            if (res->get_filename() != resolved_filename) {
                res->filename = resolved_filename;
                res->raw_filename = raw_filename;
            }
        }
    }

    return root_scope;
}

std::string remap_filename(const std::string &filename, const SerializationOptions &options) {
    // we don't expect this kind of stuff to be done in parallel
    static std::unordered_map<std::string, std::string> mapped_filename;
    if (mapped_filename.find(filename) != mapped_filename.end()) {
        return mapped_filename.at(filename);
    }

    std::filesystem::path target_filename = filename;
    std::string result = filename;
    for (auto const &[before, after] : options.remap_filename) {
        std::filesystem::path path_before = before;
        auto target_it = target_filename.begin();
        bool match = true;
        for (auto const &p : path_before) {
            if (target_it == target_filename.end() || p != *target_it) {
                match = false;
                break;
            }
            target_it++;
        }
        if (!match) continue;

        std::filesystem::path path_after = after;
        while (target_it != target_filename.end()) {
            path_after = path_after / *target_it;
            target_it++;
        }
        result = path_after.string();
        mapped_filename.emplace(filename, result);
    }
    return result;
}

// NOLINTNEXTLINE
std::string Scope::serialize(const SerializationOptions &options) const {
    std::stringstream ss;
    ss << "{";
    ss << R"("type":")" << type() << R"(")";
    if (!scopes.empty()) {
        ss << R"(,"scope":[)";
        for (auto i = 0u; i < scopes.size(); i++) {
            auto s = scopes[i]->serialize(options);
            ss << s;
            if (i != (scopes.size() - 1)) {
                ss << ",";
            }
        }
        ss << "]";
    }

    if (!filename.empty()) {
        auto fn = remap_filename(filename, options);
        ss << R"(,"filename":")" << fn << '"';
    }
    auto member = serialize_member();
    if (!member.empty()) {
        ss << "," << member;
    }
    if (!state_ids.empty()) {
        ss << R"(,"condition":")";
        for (auto i = 0u; i < state_ids.size(); i++) {
            ss << "(ap_CS_fsm[" << (state_ids[i] - 1) << "])";
            if (i != (state_ids.size() - 1)) {
                ss << "||";
            }
        }
        ss << '"';
    }
    ss << "}";
    return ss.str();
}

// NOLINTNEXTLINE
Scope *Scope::find(const std::function<bool(Scope *)> &predicate) {
    if (predicate(this)) return this;
    for (auto *s : scopes) {
        auto *p = s->find(predicate);
        if (p) return p;
    }
    return nullptr;
}

// NOLINTNEXTLINE
void Scope::find_all(const std::function<bool(Scope *)> &predicate, std::vector<Scope *> &res) {
    if (predicate(this)) res.emplace_back(this);
    for (auto *s : scopes) {
        s->find_all(predicate, res);
    }
}

void Scope::bind_state(const std::map<uint32_t, StateInfo> &state_infos) {
    // we bind state to scope
    // if the state info has line number, we use that for matching
    // if not, we brute-force to match the instruction string
    // I miss C++20 features. structured bindings cannot be captured by lambda expressions until
    // C++20
    for (auto const &iter : state_infos) {
        auto state_id = iter.first;
        auto const &info = iter.second;
        std::vector<Scope *> child_scopes;
        find_all(
            [&info](Scope *scope) {
                return std::any_of(
                    info.instructions.begin(), info.instructions.end(), [scope](auto const &iter) {
                        auto const &loc = iter.second;
                        return loc.line > 0 && loc.filename == scope->get_raw_filename() &&
                               loc.line == scope->line;
                    });
            },
            child_scopes);
        if (!child_scopes.empty()) {
            // found it
            for (auto *scope : child_scopes) {
                scope->state_ids.emplace_back(state_id);
            }
        } else {
            // brute-force search instructions
            find_all(
                [&info](Scope *scope) {
                    return std::any_of(info.instructions.begin(), info.instructions.end(),
                                       [scope](auto const &iter) {
                                           auto const &instr = iter.first;
                                           auto const scope_instr = scope->get_instr_string();
                                           return instr == scope_instr;
                                       });
                },
                child_scopes);

            for (auto *scope : child_scopes) {
                scope->state_ids.emplace_back(state_id);
            }
        }
    }
}

// NOLINTNEXTLINE
std::string Scope::get_filename() const {
    if (filename.empty()) {
        return parent_scope ? parent_scope->get_filename() : "";
    } else {
        return filename;
    }
}

// NOLINTNEXTLINE
std::string Scope::get_raw_filename() const {
    if (raw_filename.empty()) {
        return parent_scope ? parent_scope->get_raw_filename() : "";
    } else {
        return raw_filename;
    }
}

[[nodiscard]] std::string Scope::get_instr_string() const {
    if (!instruction) return "";
    std::string buffer;
    llvm::raw_string_ostream stream(buffer);
    instruction->print(stream);
    return buffer;
}

std::string Instruction::serialize_member() const { return R"("line":)" + std::to_string(line); }

std::string DeclInstruction::serialize_member() const {
    auto base = Instruction::serialize_member();
    base.append(R"(,"variable":{"name":")").append(var.name).append(R"(",)");
    base.append(R"("value":")").append(var.rtl).append(R"(",)");
    base.append(R"("rtl":true})");
    return base;
}

void StateInfo::add_instruction(const std::string &instr, const std::string &filename,
                                uint32_t line) {
    LineInfo info{filename, line};
    instructions.emplace_back(std::make_pair(instr, info));
}

void StateInfo::add_instruction(const std::string &instr) { add_instruction(instr, "", 0); }

void SerializationOptions::add_mapping(const std::string &before, std::string &after) {
    remap_filename.emplace(before, after);
}
#include "ir.hh"

#include <cxxabi.h>

#include <filesystem>
#include <iostream>
#include <queue>
#include <stack>
#include <unordered_set>

#include "llvm/ADT/StringMap.h"
#include "llvm/Analysis/DebugInfo.h"
#include "llvm/Constants.h"
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
    if (!inst) return nullptr;
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

// NOLINTNEXTLINE
void find_array_range(const llvm::MDNode *node, std::vector<uint32_t> &res) {
    if (!node) return;
    auto di = llvm::DIDescriptor(node);
    auto tag = di.getTag();
    if (tag == llvm::dwarf::dwarf_constants::DW_TAG_subrange_type) {
        auto lo_op = llvm::cast<llvm::ConstantInt>(node->getOperand(1));
        auto hi_op = llvm::cast<llvm::ConstantInt>(node->getOperand(2));
        uint32_t lo = lo_op->getLimitedValue();
        uint32_t hi = hi_op->getLimitedValue();
        // inclusive
        res.emplace_back(hi - lo + 1);
    }
    auto num_op = node->getNumOperands();
    for (auto i = 0u; i < num_op; i++) {
        auto op = node->getOperand(i);
        if (!op) continue;
        if (auto *n = llvm::dyn_cast<llvm::MDNode>(op)) {
            find_array_range(n, res);
        }
    }
}

std::vector<Scope *> process_var_decl(const llvm::CallInst &call_inst, Context &context,
                                      Scope *root_scope, const RTLInfo &rtl_info) {
    auto *value = llvm::cast<llvm::MDNode>(call_inst.getOperand(0))->getOperand(0);
    auto *desc = llvm::cast<llvm::MDNode>(call_inst.getOperand(1));
    auto num_op = desc->getNumOperands();
    std::string var_name;
    std::string value_name;

    if (!value->hasName()) return {};
    value_name = value->getName().str();

    uint32_t line_num = 0;
    std::vector<uint32_t> array_range;
    for (auto i = 0u; i < num_op; i++) {
        auto *op = desc->getOperand(i);
        if (!op) continue;
        if (llvm::isa<llvm::MDString>(op) && var_name.empty()) {
            auto md = llvm::cast<llvm::MDString>(op);
            var_name = md->getString().str();
        } else if (i > 0 && llvm::isa<llvm::ConstantInt>(op) && line_num == 0) {
            auto c = llvm::cast<llvm::ConstantInt>(op);
            line_num = c->getLimitedValue();
        } else if (llvm::isa<llvm::MDNode>(op)) {
            auto *node = llvm::cast<llvm::MDNode>(op);
            auto di = llvm::DIDescriptor(node);
            auto tag = di.getTag();
            if (tag == llvm::dwarf::dwarf_constants::DW_TAG_array_type) {
                // it's an array
                // recursively loop through each operand and finding out if there is one marked
                // as subrange type
                find_array_range(node, array_range);
            }
        }
    }

    std::vector<Scope *> res;
    auto add_var = [&](const std::string &front_name, std::string rtl_name,
                       const std::string &instance_name = {}) {
        // need to check legality of the signal
        auto const module_name = root_scope->module->rtl_module_name();

        if (instance_name.empty()) {
            if (rtl_info.signals.find(module_name) == rtl_info.signals.end()) {
                return false;
            }
            auto const &signals = rtl_info.signals.at(module_name);
            if (signals.find(rtl_name) == signals.end()) {
                return false;
            }
        } else {
            auto const &instances = rtl_info.instances.at(module_name);
            if (instances.find(instance_name) == instances.end()) {
                return false;
            }
            auto const &target_module_name = instances.at(instance_name);
            if (rtl_info.signals.find(target_module_name) == rtl_info.signals.end()) {
                return false;
            }
            auto const &signals = rtl_info.signals.at(target_module_name);
            if (signals.find(rtl_name) == signals.end()) {
                return false;
            }
            rtl_name = instance_name + "." + rtl_name;
        }
        Variable var(front_name, rtl_name);
        // compute the debugging scope
        // for now we put everything in the same scope. Need to refactor this to compute
        // actual debug scope
        auto debug_loc = call_inst.getDebugLoc();
        uint32_t line = debug_loc.getLine();
        if (line == 0) line = line_num;
        auto *s = context.add_scope<DeclInstruction>(root_scope, var, line);
        res.emplace_back(s);
        return true;
    };
    // try to guess the actual RTL name
    // obtain the value and see the type
    // for now we just hack it
    bool already_flatten = var_name.find('[') != std::string::npos;
    if (!array_range.empty() && !already_flatten) {
        // we need to flatten the array
        if (array_range.size() != 2) {
            throw std::runtime_error("Only 2-dim array is implemented");
        }
        bool success = false;
        for (auto i = 0u; i < array_range[0]; i++) {
            std::string front_name = var_name + "." + std::to_string(i);
            // hgdb will query the ram type, which is an unpacked array
            std::string instance_name = var_name + '_' + std::to_string(i) + "_U";
            std::string rtl_name = "ram";
            success = add_var(front_name, rtl_name, instance_name);
        }
        if (!success) {
            // could be just a flattened array
            // hgdb will query the ram type, which is an unpacked array
            std::string instance_name = var_name + "_U";
            auto rtl_name = "ram";
            add_var(var_name, rtl_name, instance_name);
        }
    } else {
        if (already_flatten) {
            // this is an auto variable
            // very likely created from an array
            value_name.append("_q0");
        } else {
            // normal wire
            // need to follow the use
            // also need to read out the type in case it's an array, which has different
            for (auto use = value->use_begin(); use != value->use_end(); use++) {
                if (llvm::isa<llvm::LoadInst>(*use)) {
                    auto load = llvm::cast<llvm::LoadInst>(*use);
                    value_name = "ap_sig_allocacmp_" + load->getName().str();
                    break;
                }
            }
        }
        add_var(var_name, value_name);
    }

    return res;
}

Scope *get_debug_scope(const llvm::Function *function, Context &context, ModuleInfo *module) {
    if (!function) return nullptr;
    auto *root_scope = context.add_scope<Scope>(nullptr);
    root_scope->module = module;
    std::unordered_map<const llvm::DIScope *, Scope *> scope_mapping;

    std::unordered_set<uint32_t> lines;

    for (auto const &blk : *function) {
        for (auto const &instr : blk) {
            // we are dealing with old LLVM code
            // see the debug information here:
            // https://releases.llvm.org/3.1/docs/SourceLevelDebugging.html
            auto debug_loc = instr.getDebugLoc();
            auto *node = debug_loc.getAsMDNode(*get_llvm_context());

            std::vector<Scope *> res;
            if (llvm::isa<llvm::CallInst>(instr)) {
                auto const &call_inst = llvm::cast<llvm::CallInst>(instr);
                auto const *called_function = call_inst.getCalledFunction();
                if (called_function) {
                    auto name = called_function->getName();
                    if (name == "llvm.dbg.declare") {
                        res = process_var_decl(call_inst, context, root_scope, context.rtl_info());
                    }
                }
            }

            if (res.empty()) {
                // normal block
                auto line = debug_loc.getLine();
                if (line > 0 && lines.find(line) == lines.end()) {
                    auto *s = context.add_scope<Instruction>(root_scope, line);
                    lines.emplace(line);
                    res.emplace_back(s);
                }
            }

            // invalid
            if (res.empty()) continue;

            for (auto *scope : res) {
                scope->instruction = &instr;

                // for file name. some declare might not have
                if (!node) continue;
                auto loc = llvm::DILocation(node);
                auto resolved_filename =
                    resolve_filename(loc.getFilename().str(), loc.getDirectory().str());
                auto raw_filename = loc.getFilename().str();

                if (root_scope->filename.empty()) {
                    root_scope->filename = resolved_filename;
                    root_scope->raw_filename = raw_filename;
                }

                if (scope->get_filename() != resolved_filename) {
                    scope->filename = resolved_filename;
                    scope->raw_filename = raw_filename;
                }
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
    // TODO: use variable tracking for state IDs. this is to avoid issues where the symbols
    //  gets messed up after moving scopes around
    if (!state_ids.empty()) {
        ss << R"(,"condition":")";
        // we hardcode the idle here
        ss << "(!ap_idle)&&(";
        for (auto i = 0u; i < state_ids.size(); i++) {
            ss << "(ap_CS_fsm[" << (state_ids[i] - 1) << "])";
            if (i != (state_ids.size() - 1)) {
                ss << "||";
            }
        }
        ss << ')' << '"';
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

void Scope::bind_state(ModuleInfo &mod) {
    mod.root_scope = this;
    set_module(&mod);
    const std::map<uint32_t, StateInfo> &state_infos = mod.state_infos;
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

void Scope::add_scope(Scope *scope) {
    scopes.emplace_back(scope);
    scope->parent_scope = this;
    scope->module = module;
}

void Scope::remove_from_parent() {
    if (!parent_scope) return;
    auto it = std::find_if(parent_scope->scopes.begin(), parent_scope->scopes.end(),
                           [this](auto *p) { return p == this; });
    if (it != parent_scope->scopes.end()) {
        parent_scope->scopes.erase(it);
    }
    parent_scope = nullptr;
}

// NOLINTNEXTLINE
void Scope::clear_empty() {
    for (auto *s : scopes) {
        s->clear_empty();
    }

    auto ss = scopes;
    for (auto *s : ss) {
        if (s->scopes.empty() && s->type() == "block") {
            auto it = std::find(scopes.begin(), scopes.end(), s);
            scopes.erase(it);
        }
    }
}

bool Scope::contains(const Scope *scope) const {
    if (!module) return false;
    auto const *mod = scope->module;
    // breath-first search
    std::queue<const ModuleInfo *> modules;
    std::unordered_set<const ModuleInfo *> visited;
    modules.emplace(module);

    while (!modules.empty()) {
        auto const *m = modules.front();
        modules.pop();
        if (visited.find(m) != visited.end()) {
            continue;
        }
        if (m == mod) return true;
        visited.emplace(m);
        for (auto const &[name, inst] : m->instances) {
            modules.emplace(inst.get());
        }
    }
    return false;
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

Scope *Scope::copy() const {
    auto *new_scope = context->add_scope<Scope>(parent_scope);
    *new_scope = *this;
    return new_scope;
}

// NOLINTNEXTLINE
void Scope::set_module(ModuleInfo *mod) {
    module = mod;
    for (auto *s : scopes) {
        s->set_module(mod);
    }
}

std::string Instruction::serialize_member() const { return R"("line":)" + std::to_string(line); }

Scope *Instruction::copy() const {
    auto *new_scope = context->add_scope<Instruction>(parent_scope, line);
    *new_scope = *this;
    return new_scope;
}

std::string DeclInstruction::serialize_member() const {
    auto base = Instruction::serialize_member();
    base.append(R"(,"variable":{"name":")").append(var.name).append(R"(",)");
    base.append(R"("value":")").append(var.rtl).append(R"(",)");
    base.append(R"("rtl":true})");
    return base;
}

Scope *DeclInstruction::copy() const {
    auto *new_scope = context->add_scope<DeclInstruction>(parent_scope, var, line);
    *new_scope = *this;
    return new_scope;
}

std::shared_ptr<ModuleInfo> Context::get_module(const std::string &name) {
    if (module_infos_.find(name) == module_infos_.end())
        return nullptr;
    else
        return module_infos_.at(name);
}

void Context::add_module(const std::string &name, std::shared_ptr<ModuleInfo> mod) {
    mod->context = this;
    module_infos_.emplace(name, std::move(mod));
}

bool Context::has_module(const std::string &name) {
    return module_infos_.find(name) != module_infos_.end();
}

void Context::set_rtl_info(
    const std::unordered_map<std::string, std::unordered_set<std::string>> &signals,
    const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>
        &instances) {
    info_.signals = signals;
    info_.instances = instances;
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

std::map<uint32_t, StateInfo> merge_states(const std::map<uint32_t, StateInfo> &state_infos,
                                           const std::map<std::string, SignalInfo> &signals,
                                           const std::string &module_name) {
    // notice that if the statement variable is only 1-bit, it means all the states are merged
    // into one. Currently, we only support wither many to one mapping, or one-to-one mapping.
    constexpr auto fsm_signal_name = "ap_CS_fsm";
    if (signals.find(fsm_signal_name) == signals.end()) {
        // no states found
        std::cerr << "[Warning] no states found for " << module_name << std::endl;
        return {};
    }

    auto num_states = signals.at(fsm_signal_name).width;
    if (num_states != state_infos.size()) {
        if (num_states == 1) {
            // merge states
            StateInfo new_state(1);
            std::cout << "[Info] Merging states for " << module_name << " (" << state_infos.size()
                      << " -> 1)" << std::endl;
            for (auto const &[_, state] : state_infos) {
                new_state.instructions.insert(new_state.instructions.end(),
                                              state.instructions.begin(), state.instructions.end());
            }
            // starts from 1
            return {{1, new_state}};
        }
        throw std::runtime_error("Mismatch state information not implemented");
    }
    // no need to merge
    return state_infos;
}

void merge_scope(Scope *parent, Scope *child) {
    // compute the instance prefix
    auto *new_child = child->copy();
    auto *child_module = child->module;
    auto *parent_module = parent->module;
    if (!child_module || !parent_module)
        throw std::runtime_error("Top-level scope cannot have null module");

    // compute instance prefix
    // BFS with prefix in a map. probably the same memory size if implemented as a recursion
    std::queue<ModuleInfo *> mods;
    mods.push(parent_module);
    std::unordered_map<ModuleInfo *, std::pair<std::string, ModuleInfo *>> hierarchy;

    while (!mods.empty()) {
        auto *module = mods.front();
        mods.pop();
        for (auto [name, m] : module->instances) {
            hierarchy[m.get()] = std::make_pair(name, module);
            if (m.get() == child_module) {
                break;
            }
            mods.emplace(m.get());
        }
    }

    std::stack<std::string> prefixes;
    {
        auto *module = child_module;
        while (hierarchy.find(module) != hierarchy.end()) {
            auto [n, p] = hierarchy.at(module);
            prefixes.emplace(n);
            module = p;
        }
    }
    std::string prefix;
    while (!prefixes.empty()) {
        auto const &n = prefixes.top();
        prefix.append(n).append(".");
        prefixes.pop();
    }
    // fix all the variable declaration

    {
        std::queue<Scope *> scopes;
        scopes.push(new_child);

        while (!scopes.empty()) {
            auto *s = scopes.front();
            scopes.pop();
            if (s->type() == "decl") {
                // rename it
                auto *var_scope = reinterpret_cast<DeclInstruction *>(s);
                var_scope->var.rtl = prefix + var_scope->var.rtl;
            }
            for (auto *scope : s->scopes) {
                scopes.push(scope);
            }
        }
    }

    // merge the child into parent
    for (auto *s : new_child->scopes) {
        parent->add_scope(s);
    }
    new_child->scopes.clear();
    child->scopes.clear();
}

Scope *find_parent(const std::vector<Scope *> &scopes) {
    if (scopes.empty()) return nullptr;
    auto *scope = scopes[0];
    auto *mod = scope->module;
    ModuleInfo *parent_module = nullptr;
    auto const &module_infos = scope->context->module_infos();
    for (auto const &[name, module] : module_infos) {
        // just need to find one
        // assuming there is no duplicated basic block split out
        if (parent_module) break;
        for (auto const &[inst_name, inst] : module->instances) {
            if (inst.get() == mod) {
                parent_module = module.get();
                break;
            }
        }
    }
    if (!parent_module) throw std::runtime_error("Unable to find module for scope");
    auto *res = parent_module->root_scope;
    // make sure it contains
    for (auto *s : scopes) {
        if (!res->contains(s)) throw std::runtime_error("Scopes are not in the same function");
    }

    return res;
}

void merge_scopes(const std::map<std::string, std::vector<Scope *>> &scopes) {
    // we merge the scopes using the following rule
    // child merged into parent
    // during the merge, variable value will get fixed (name stays the same)
    for (auto const &[func, ss] : scopes) {
        if (ss.size() <= 1) continue;
        // first pass to determine the root. notice that root can possibly not exist
        // i.e. two nodes sharing the same scope
        Scope *parent = ss[0];
        bool contained = false;
        for (auto i = 1u; i < ss.size(); i++) {
            bool parent_ss = parent->contains(ss[i]);
            if (!parent_ss && ss[i]->contains(parent)) {
                parent = ss[i];
                contained = true;
            } else if (parent_ss) {
                contained = true;
            }
        }

        if (contained) {
            // now merge everything into parent
            // create a new scope for this function
            auto *target_parent = parent->context->add_scope<Scope>(parent);
            target_parent->module = parent->module;
            for (auto *s : ss) {
                if (s != parent) {
                    merge_scope(target_parent, s);
                }
            }
        } else {
            // leave the split function separate since they are doing different things
        }
    }
}

std::map<std::string, Scope *> reorganize_scopes(
    const llvm::Module *module,
    const std::map<std::string, std::map<std::string, std::pair<uint32_t, uint32_t>>>
        &original_functions,
    std::map<std::string, Scope *> scopes) {
    // we first sort through the scopes. i.e. put them into different buckets
    std::map<std::string, std::vector<Scope *>> function_scopes;

    for (auto const &[mod_name, scope] : scopes) {
        auto child_scopes = scope->scopes;
        scope->scopes.clear();

        std::map<std::string, Scope *> mod_functions;

        for (auto *child_scope : child_scopes) {
            auto filename = child_scope->get_filename();
            if (original_functions.find(filename) == original_functions.end()) {
                throw std::runtime_error("Unable to determine location for file " + filename);
            }
            auto const &function_range = original_functions.at(filename);
            auto line = child_scope->line;
            bool found = false;
            for (auto const &[func_name, line_range] : function_range) {
                auto const [min, max] = line_range;
                if (line >= min && line <= max) {
                    found = true;

                    if (mod_functions.find(func_name) == mod_functions.end()) {
                        auto *new_scope = scope->context->add_scope<Scope>(scope);
                        new_scope->module = scope->module;
                        mod_functions.emplace(func_name, new_scope);
                        function_scopes[func_name].emplace_back(new_scope);
                    }

                    auto *function_scope = mod_functions.at(func_name);
                    function_scope->add_scope(child_scope);
                }
            }

            if (!found) {
                throw std::runtime_error("Unable to determine scope location");
            }
        }
    }

    merge_scopes(function_scopes);

    // clean up the empty scope
    {
        std::unordered_set<std::string> remove;
        for (auto const &[n, s] : scopes) {
            s->clear_empty();
            if (s->scopes.empty()) {
                remove.emplace(n);
                auto *context = s->context;
                context->get_module(context->top_name)->remove_definition(n);
            }
        }

        for (auto const &n : remove) {
            scopes.erase(n);
        }
    }

    return scopes;
}

void ModuleInfo::add_instance(const std::string &m_name, const std::string &instance_name) {
    if (!context->has_module(m_name)) {
        auto ptr = std::make_shared<ModuleInfo>(m_name);
        context->add_module(m_name, ptr);
    }
    auto module = context->get_module(m_name);
    instances.emplace(instance_name, module);
}

// NOLINTNEXTLINE
void ModuleInfo::remove_definition(const std::string &target_module_name) {
    std::unordered_set<std::string> insts;
    for (auto const &[n, mod] : instances) {
        if (mod->module_name == target_module_name) {
            insts.emplace(n);
        }
    }
    for (auto const &n : insts) {
        instances.erase(n);
    }

    // recursively remove stuff
    for (auto const &[n, mod] : instances) {
        mod->remove_definition(target_module_name);
    }
}

std::string ModuleInfo::rtl_module_name() const {
    if (module_name == context->top_name) {
        return module_name;
    } else {
        return context->top_name + "_" + module_name;
    }
}

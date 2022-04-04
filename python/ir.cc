#include "ir.hh"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Instructions.h"
#include <unordered_set>
#include <cxxabi.h>

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
    for (auto const &blk: *function) {
        for (auto const &inst: blk) {
            res.emplace_back(&inst);
        }
    }
    return res;
}

std::string get_filename(const llvm::Instruction *inst) {
    auto const &loc = inst->getDebugLoc();
    auto *di_loc = loc.get();
    if (di_loc) {
        return di_loc->getFilename().str();
    } else {
        return {};
    }
}

uint32_t get_line_num(const llvm::Instruction *inst) {
    auto const &loc = inst->getDebugLoc();
    return loc.getLine();
}

// NOLINTNEXTLINE
void index_function(std::map<std::string, std::map<uint32_t, std::vector<const llvm::Instruction *>>> &res,
                    std::unordered_set<const llvm::Function *> &visited,
                    const llvm::Function *function) {
    if (!function) return;
    // prevent recursion
    if (visited.find(function) != visited.end()) return;
    visited.emplace(function);
    for (auto const &blk: *function) {
        for (auto const &inst: blk) {
            auto filename = get_filename(&inst);
            if (filename.empty()) continue;
            auto line = get_line_num(&inst);
            res[filename][line].emplace_back(&inst);

            // if it's a call instruction, we track through the called functions
            if (llvm::isa<llvm::CallInst>(inst)) {
                auto const &call = llvm::cast<llvm::CallInst>(inst);
                // recursive call
                auto *func = call.getCalledFunction();
                index_function(res, visited, func);
            }
        }
    }
}

std::map<std::string, std::map<uint32_t, std::vector<const llvm::Instruction *>>>
get_instr_loc(const llvm::Function *function) {
    std::map<std::string, std::map<uint32_t, std::vector<const llvm::Instruction *>>> result;
    std::unordered_set<const llvm::Function *> visited;
    index_function(result, visited, function);
    return result;
}

// NOLINTNEXTLINE
void get_contained_functions(const llvm::Function *function, std::set<std::string> &res) {
    if (!function) return;
    for (auto const &blk: *function) {
        for (auto const &inst: blk) {
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

std::map<std::string, const llvm::Function *>
get_optimized_functions(const llvm::Module *module, const std::set<std::string> &function_names) {
    // use the fact that all transformed basic blocks as original function name's label with .exit
    std::map<std::string, const llvm::Function *> res;
    std::unordered_map<std::string, std::string> processed_names;
    for (auto const &func: function_names) {
        processed_names.emplace(func, func + ".exit");
    }
    auto const &functions = module->getFunctionList();
    for (auto const &function: functions) {
        for (auto const &block: function) {
            auto name = block.getName().str();
            for (auto const &[func_name, label]: processed_names) {
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
    std::unique_ptr<char, void (*)(void *)> res{abi::__cxa_demangle(name, nullptr, nullptr, &status), std::free};
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

std::unique_ptr<llvm::Module> parse_llvm_bitcode(const std::string &path) {
    llvm::SMDiagnostic error;
    auto module = llvm::parseIRFile(path, error, *get_llvm_context());
    return module;
}
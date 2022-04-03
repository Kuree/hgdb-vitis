#include "ir.hh"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Instructions.h"
#include <unordered_set>

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

std::unique_ptr<llvm::Module> parse_llvm_bitcode(const std::string &path) {
    llvm::SMDiagnostic error;
    auto module = llvm::parseIRFile(path, error, *get_llvm_context());
    return module;
}
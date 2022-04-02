#include "ir.hh"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"

llvm::LLVMContext *get_llvm_context() {
    static std::unique_ptr<llvm::LLVMContext> context;
    if (!context) {
        context = std::make_unique<llvm::LLVMContext>();
    }
    return context.get();
}

std::unique_ptr<ModuleInfo> parse_llvm_bitcode(const std::string &path) {
    llvm::SMDiagnostic error;
    auto module = llvm::parseIRFile(path, error, *get_llvm_context());
    if (module) {
        return std::make_unique<ModuleInfo>(std::move(module));
    } else {
        return nullptr;
    }
}
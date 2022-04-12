#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

llvm::LLVMContext *get_llvm_context() {
    static std::unique_ptr<llvm::LLVMContext> context;
    if (!context) {
        context = std::make_unique<llvm::LLVMContext>();
    }
    return context.get();
}

PYBIND11_MODULE(vitis0, m) {
    m.def("get_function_names", [](const std::string &filename) {
        std::set<std::string> names;
        llvm::SMDiagnostic error;
        auto module = llvm::parseIRFile(filename, error, *get_llvm_context());
        if (!module) return names;
        for (auto const &func : *module) {
            names.emplace(func.getName().str());
        }
        return names;
    });
}
#ifndef HGDB_VITIS_IR_HH
#define HGDB_VITIS_IR_HH

#include <string>

#include "llvm/IR/Module.h"

struct ModuleInfo {
    // wrapper for LLVM IRs so we don't have to create binding for LLVM types
    explicit ModuleInfo(std::unique_ptr<llvm::Module> &&m): module(std::move(m)) {}
    std::unique_ptr<llvm::Module> module;
};

std::unique_ptr<ModuleInfo> parse_llvm_bitcode(const std::string &path);

#endif //HGDB_VITIS_IR_HH

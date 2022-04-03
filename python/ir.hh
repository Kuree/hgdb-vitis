#ifndef HGDB_VITIS_IR_HH
#define HGDB_VITIS_IR_HH

#include <string>

#include "llvm/IR/Module.h"

std::vector<const llvm::Instruction *> get_function_instructions(const llvm::Module &module,
                                                                 const std::string &func_name);

std::string get_filename(const llvm::Instruction *inst);

uint32_t get_line_num(const llvm::Instruction *inst);

std::map<std::string, std::map<uint32_t, std::vector<const llvm::Instruction *>>>
get_instr_loc(const llvm::Function *function);

std::unique_ptr<llvm::Module> parse_llvm_bitcode(const std::string &path);


#endif  // HGDB_VITIS_IR_HH

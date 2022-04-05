#ifndef HGDB_VITIS_IR_HH
#define HGDB_VITIS_IR_HH

#include <map>
#include <set>
#include <string>

#include "llvm/Module.h"

std::vector<const llvm::Instruction *> get_function_instructions(const llvm::Module &module,
                                                                 const std::string &func_name);

std::string get_filename(const llvm::Instruction *inst);

uint32_t get_line_num(const llvm::Instruction *inst);

const llvm::Function *get_function(const llvm::Instruction *inst);

std::map<std::string, std::map<uint32_t, std::vector<const llvm::Instruction *>>> get_instr_loc(
    const llvm::Function *function);

std::set<std::string> get_contained_functions(const llvm::Function *function);

std::map<std::string, const llvm::Function *> get_optimized_functions(
    const llvm::Module *module, const std::set<std::string> &function_names);

std::string get_demangled_name(const llvm::Function *function);

std::string get_name(const llvm::Function *function);

const llvm::Instruction *get_pre_alloc(const llvm::Instruction *instruction);

const llvm::Instruction *find_matching_instr(const llvm::Function *function,
                                             const llvm::Instruction *target);

std::string guess_rtl_name(const llvm::Instruction *instruction);

llvm::Module *parse_llvm_bitcode(const std::string &path);

#endif  // HGDB_VITIS_IR_HH

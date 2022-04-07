#ifndef HGDB_VITIS_IR_HH
#define HGDB_VITIS_IR_HH

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

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

// debugging scopes
struct Variable {
    std::string name;
    std::string rtl;

    Variable(std::string name, std::string rtl) : name(std::move(name)), rtl(std::move(rtl)) {}
};

class Scope {
public:
    std::vector<Scope *> scopes;
    std::string filename;

    const Scope *parent_scope;

    explicit Scope(const Scope *parent_scope) : parent_scope(parent_scope) {}
};

class Instruction : public Scope {
public:
    // basically a line
    Variable var;

    uint32_t line = 0;

    Instruction(const Scope *parent_scope, Variable var, uint32_t line)
        : Scope(parent_scope), var(std::move(var)), line(line) {}
};

class Context {
public:
    template <typename T, typename... Args>
    T *get_scope(Scope *parent_scope, Args... args) {
        auto entry = std::make_unique<T>(parent_scope, args...);
        if (parent_scope) parent_scope->scopes.emplace_back(entry.get());
        return reinterpret_cast<T *>(scopes_.emplace_back(std::move(entry)).get());
    }

private:
    std::vector<std::unique_ptr<Scope>> scopes_;
};

Scope *get_debug_scope(const llvm::Function *function, Context &context);

#endif  // HGDB_VITIS_IR_HH

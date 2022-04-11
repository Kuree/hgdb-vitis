#ifndef HGDB_VITIS_IR_HH
#define HGDB_VITIS_IR_HH

#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
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

struct StateInfo {
    uint32_t id;
    struct LineInfo {
        std::string filename;
        uint32_t line;
    };
    std::vector<std::pair<std::string, LineInfo>> instructions;

    explicit StateInfo(uint32_t id) : id(id) {}

    void add_instruction(const std::string &instr, const std::string &filename, uint32_t line);
    void add_instruction(const std::string &instr);
};

struct SignalInfo {
    std::string name;
    uint32_t width;

    SignalInfo(std::string name, uint32_t width) : name(std::move(name)), width(width) {}
};

struct SerializationOptions {
    std::map<std::string, std::string> remap_filename;

    void add_mapping(const std::string &before, std::string &after);
};

class Scope {
public:
    std::vector<Scope *> scopes;
    std::string filename;
    std::string raw_filename;
    uint32_t line = 0;

    // one single line can have multiple ids
    std::vector<uint32_t> state_ids;
    const llvm::Instruction *instruction = nullptr;

    const Scope *parent_scope;

    explicit Scope(const Scope *parent_scope) : parent_scope(parent_scope) {}

    [[nodiscard]] virtual std::string type() const { return "block"; }

    [[nodiscard]] std::string serialize(const SerializationOptions &options) const;

    Scope *find(const std::function<bool(Scope *)> &predicate);
    void find_all(const std::function<bool(Scope *)> &predicate, std::vector<Scope *> &res);
    void bind_state(const std::map<uint32_t, StateInfo> &state_infos);

    [[nodiscard]] std::string get_filename() const;
    [[nodiscard]] std::string get_raw_filename() const;
    [[nodiscard]] std::string get_instr_string() const;

private:
    [[nodiscard]] virtual std::string serialize_member() const { return {}; }
};

class Instruction : public Scope {
public:
    Instruction(const Scope *parent_scope, uint32_t line) : Scope(parent_scope) {
        this->line = line;
    }

    [[nodiscard]] std::string type() const override { return "none"; }

    [[nodiscard]] std::string serialize_member() const override;
};

class DeclInstruction : public Instruction {
public:
    // basically a line
    Variable var;

    DeclInstruction(const Scope *parent_scope, Variable var, uint32_t line)
        : Instruction(parent_scope, line), var(std::move(var)) {}

    [[nodiscard]] std::string type() const override { return "decl"; }

    [[nodiscard]] std::string serialize_member() const override;
};

class Context {
public:
    template <typename T, typename... Args>
    T *add_scope(Scope *parent_scope, Args... args) {
        auto entry = std::make_unique<T>(parent_scope, args...);
        if (parent_scope) parent_scope->scopes.emplace_back(entry.get());
        return reinterpret_cast<T *>(scopes_.emplace_back(std::move(entry)).get());
    }

private:
    std::vector<std::unique_ptr<Scope>> scopes_;
};

Scope *get_debug_scope(const llvm::Function *function, Context &context);

#endif  // HGDB_VITIS_IR_HH

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

std::string guess_rtl_name(const llvm::Instruction *instruction);

llvm::Module *parse_llvm_bitcode(const std::string &path);

// debugging scopes
struct Variable {
    std::string name;
    std::string rtl;

    Variable(std::string name, std::string rtl) : name(std::move(name)), rtl(std::move(rtl)) {}
};

struct StateInfo {
    std::string name;
    struct LineInfo {
        std::string filename;
        uint32_t line;
    };
    std::vector<LineInfo> instructions;

    explicit StateInfo(std::string name) : name(std::move(name)) {}

    void add_instruction(const std::string &filename, uint32_t line);
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

class Scope;
class Context;

struct ModuleInfo {
    std::string module_name;

    llvm::Function *function = nullptr;

    // we assume that the xrf files contain the signals that used for controlling the states
    std::map<std::string, StateInfo> state_infos;
    std::map<std::string, SignalInfo> signals;

    std::map<std::string, std::shared_ptr<ModuleInfo>> instances;

    Scope *root_scope = nullptr;
    Context *context = nullptr;

    explicit ModuleInfo(std::string module_name) : module_name(std::move(module_name)) {}

    void add_instance(const std::string &m_name, const std::string &instance_name);

    void remove_definition(const std::string &target_module_name);

    [[nodiscard]] std::string rtl_module_name() const;
};

class Scope {
public:
    std::vector<Scope *> scopes;
    std::string filename;
    std::string raw_filename;
    uint32_t line = 0;

    // one single line can have multiple ids
    std::vector<std::string> state_ids;
    const llvm::Instruction *instruction = nullptr;
    // used to indicating scoping changes (moved up)
    std::string instance_prefix;

    Scope *parent_scope;
    ModuleInfo *module = nullptr;
    Context *context = nullptr;

    explicit Scope(Scope *parent_scope) : parent_scope(parent_scope) {}

    [[nodiscard]] virtual std::string type() const { return "block"; }

    [[nodiscard]] std::string serialize(const SerializationOptions &options) const;

    Scope *find(const std::function<bool(Scope *)> &predicate);
    void find_all(const std::function<bool(Scope *)> &predicate, std::vector<Scope *> &res);
    void bind_state(ModuleInfo &module);
    void add_scope(Scope *scope);
    void remove_from_parent();
    void clear_empty();
    [[nodiscard]] bool contains(const Scope *scope) const;

    [[nodiscard]] std::string get_filename() const;
    [[nodiscard]] std::string get_raw_filename() const;

    [[nodiscard]] virtual Scope *copy() const;

    virtual ~Scope() = default;

private:
    [[nodiscard]] virtual std::string serialize_member() const { return {}; }

    void set_module(ModuleInfo *mod);
};

class Instruction : public Scope {
public:
    Instruction(Scope *parent_scope, uint32_t line) : Scope(parent_scope) { this->line = line; }

    [[nodiscard]] std::string type() const override { return "none"; }

    [[nodiscard]] std::string serialize_member() const override;

    [[nodiscard]] Scope *copy() const override;
};

class DeclInstruction : public Instruction {
public:
    // basically a line
    Variable var;

    DeclInstruction(Scope *parent_scope, Variable var, uint32_t line)
        : Instruction(parent_scope, line), var(std::move(var)) {}

    [[nodiscard]] std::string type() const override { return "decl"; }

    [[nodiscard]] std::string serialize_member() const override;

    [[nodiscard]] Scope *copy() const override;
};

struct RTLInfo {
    std::unordered_map<std::string, std::unordered_map<std::string, uint32_t>> signals;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> instances;
    std::unordered_map<std::string, std::set<std::pair<std::string, std::string>>> connections;
};

class Context {
public:
    template <typename T, typename... Args>
    T *add_scope(Scope *parent_scope, Args... args) {
        auto entry = std::make_unique<T>(parent_scope, args...);
        if (parent_scope) parent_scope->scopes.emplace_back(entry.get());
        entry->context = this;
        return reinterpret_cast<T *>(scopes_.emplace_back(std::move(entry)).get());
    }

    std::shared_ptr<ModuleInfo> get_module(const std::string &name);
    void add_module(const std::string &name, std::shared_ptr<ModuleInfo> mod);
    [[nodiscard]] bool has_module(const std::string &name);

    [[nodiscard]] inline const std::map<std::string, std::shared_ptr<ModuleInfo>> &module_infos()
        const {
        return module_infos_;
    }

    inline std::map<std::string, std::shared_ptr<ModuleInfo>> &module_infos() {
        return module_infos_;
    }

    // we do this because the RTL info is defined in a different module with different CXX
    // ABI string
    void set_rtl_info(
        const std::unordered_map<std::string, std::unordered_map<std::string, uint32_t>> &signals,
        const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>
            &instances,
        const std::unordered_map<std::string, std::set<std::pair<std::string, std::string>>>
            &connections);

    inline RTLInfo &rtl_info() { return info_; }

    std::string top_name;

private:
    std::vector<std::unique_ptr<Scope>> scopes_;
    std::map<std::string, std::shared_ptr<ModuleInfo>> module_infos_;
    RTLInfo info_;
};

Scope *get_debug_scope(const llvm::Function *function, Context &context, ModuleInfo *module);

std::map<std::string, Scope *> reorganize_scopes(
    const llvm::Module *module,
    const std::map<std::string, std::map<std::string, std::pair<uint32_t, uint32_t>>>
        &original_functions,
    std::map<std::string, Scope *> scopes);

#endif  // HGDB_VITIS_IR_HH

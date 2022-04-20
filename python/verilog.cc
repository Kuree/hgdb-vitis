

#include <iostream>

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"
#include "slang/compilation/Compilation.h"
#include "slang/diagnostics/DiagnosticEngine.h"
#include "slang/parsing/Parser.h"
#include "slang/parsing/Preprocessor.h"
#include "slang/symbols/ASTVisitor.h"
#include "slang/symbols/InstanceSymbols.h"
#include "slang/symbols/VariableSymbols.h"
#include "slang/syntax/SyntaxTree.h"
#include "slang/text/SourceManager.h"

namespace py = pybind11;

struct RTLInfo {
    std::unordered_map<std::string, std::unordered_map<std::string, uint32_t>> signals;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> instances;
    std::unordered_map<std::string,
                       std::vector<std::tuple<std::string, std::string, std::string, std::string>>>
        connections;
};

class VisitSignals : public slang::ASTVisitor<VisitSignals, true, true> {
public:
    VisitSignals(
        std::unordered_map<std::string, std::unordered_map<std::string, uint32_t>> &signals,
        std::unordered_map<std::string, std::unordered_map<std::string, std::string>> &instances,
        std::unordered_map<
            std::string,
            std::vector<std::tuple<std::string, std::string, std::string, std::string>>>
            &connections,
        const slang::InstanceSymbol *inst)
        : current_module_name(std::string(inst->getDefinition().name)),
          signals_(signals),
          instances_(instances),
          connections_(connections) {}

    [[maybe_unused]] void handle(const slang::InstanceSymbol &sym) {
        auto def_name = std::string(sym.getDefinition().name);
        if (instances_.find(def_name) != instances_.end()) return;
        auto inst_name = std::string(sym.name);
        instances_[current_module_name].emplace(inst_name, def_name);

        // store the port connections as well
        // compute_connection(sym);

        auto temp = current_module_name;
        current_module_name = def_name;
        visitDefault(sym);
        current_module_name = temp;
    }

    [[maybe_unused]] void handle(const slang::NetSymbol &sym) {
        auto name = std::string(sym.name);
        uint32_t width = sym.getType().getBitWidth();
        signals_[current_module_name].emplace(name, width);

        // check if the net is used for direct connection between two instances
    }

    [[maybe_unused]] void handle(const slang::VariableSymbol &sym) {
        auto name = std::string(sym.name);
        uint32_t width = sym.getType().getBitWidth();
        signals_[current_module_name].emplace(name, width);
    }

    std::string current_module_name;

private:
    std::unordered_map<std::string, std::unordered_map<std::string, uint32_t>> &signals_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> &instances_;
    std::unordered_map<std::string,
                       std::vector<std::tuple<std::string, std::string, std::string, std::string>>>
        &connections_;

    class SymbolCollector : public slang::ASTVisitor<SymbolCollector, true, true> {
    public:
        void handle(const slang::NamedValueExpression &expr) { symbols.emplace(&expr.symbol); }
        void handle(const slang::NetSymbol &net) { symbols.emplace(&net); }
        std::unordered_set<const slang::ValueSymbol *> symbols;
    };

    class PortConnectionCollector : public slang::ASTVisitor<PortConnectionCollector, true, false> {
    public:
        void handle(const slang::InstanceSymbol &instance) {
            if (!is_top_) {
                is_top_ = true;
                visitDefault(instance);
                return;
            }
            instance.forEachPortConnection([this, &instance](const slang::PortConnection &conn) {
                auto const *expr = conn.getExpression();
                if (!expr) return;
                SymbolCollector c;
                expr->visit(c);
                for (auto const &s : c.symbols) {
                    // we focus on multi-bit data path for now to reduce complexity
                    auto width = s->getType().getBitWidth();
                    if (width <= 1) continue;
                    connections[s].emplace(&instance);
                }
            });
        }

        std::unordered_map<const slang::ValueSymbol *,
                           std::unordered_set<const slang::InstanceSymbol *>>
            connections;

    private:
        bool is_top_ = false;
    };

    void compute_connection(const slang::InstanceSymbol &sym) {
        auto def_name = std::string(sym.getDefinition().name);
        if (connections_.find(def_name) != connections_.end()) return;
        PortConnectionCollector c;
        sym.visit(c);

        for (auto const &[_, instances] : c.connections) {
            auto ins = std::vector(instances.begin(), instances.end());
            for (auto i = 0; i < (ins.size() - 1); i++) {
                auto const *inst1 = ins[i];
                for (auto j = i + 1; j < ins.size(); j++) {
                    auto const *inst2 = ins[j];
                    auto entry = std::make_tuple(
                        std::string(inst1->name), std::string(inst1->getDefinition().name),
                        std::string(inst2->name), std::string(inst2->getDefinition().name));
                    connections_[def_name].emplace_back(entry);
                }
            }
        }
    }
};

std::unique_ptr<RTLInfo> parse_verilog(const std::vector<std::string> &files,
                                       const std::string &top_name) {
    slang::SourceManager source_manager;

    slang::PreprocessorOptions preprocessor_options;
    slang::LexerOptions lexer_options;
    slang::ParserOptions parser_options;
    slang::CompilationOptions compilation_options;
    compilation_options.suppressUnused = true;

    slang::Bag options;
    options.set(preprocessor_options);
    options.set(lexer_options);
    options.set(parser_options);
    options.set(compilation_options);

    std::vector<slang::SourceBuffer> buffers;
    for (const std::string &file : files) {
        slang::SourceBuffer buffer = source_manager.readSource(file);
        if (!buffer) {
            std::cerr << file << " does not exist" << std::endl;
        }

        buffers.push_back(buffer);
    }

    slang::Compilation compilation;
    for (const slang::SourceBuffer &buffer : buffers)
        compilation.addSyntaxTree(slang::SyntaxTree::fromBuffer(buffer, source_manager, options));

    auto const &top_instances = compilation.getRoot().topInstances;
    const slang::InstanceSymbol *top = nullptr;
    for (auto const *inst : top_instances) {
        if (inst->name == top_name) {
            top = inst;
            break;
        }
    }

    if (!top) {
        throw std::runtime_error("Unable to find top instance " + top_name);
    }

    auto res = std::make_unique<RTLInfo>();
    VisitSignals vis(res->signals, res->instances, res->connections, top);
    top->visit(vis);

    return res;
}

PYBIND11_MODULE(vitis_rtl, m) {
    py::class_<RTLInfo>(m, "RTLInfo")
        .def_readonly("signals", &RTLInfo::signals)
        .def_readonly("instances", &RTLInfo::instances)
        .def_readonly("connections", &RTLInfo::connections);
    m.def("parse_verilog", &parse_verilog);
}

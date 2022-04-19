

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
    std::unordered_map<std::string, std::set<std::pair<std::string, std::string>>> connections;
};

class VisitSignals : public slang::ASTVisitor<VisitSignals, true, true> {
public:
    VisitSignals(
        std::unordered_map<std::string, std::unordered_map<std::string, uint32_t>> &signals,
        std::unordered_map<std::string, std::unordered_map<std::string, std::string>> &instances,
        std::unordered_map<std::string, std::set<std::pair<std::string, std::string>>> &connections,
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
        compute_connection(sym);

        auto temp = current_module_name;
        current_module_name = def_name;
        visitDefault(sym);
        current_module_name = temp;
    }

    [[maybe_unused]] void handle(const slang::NetSymbol &sym) {
        auto name = std::string(sym.name);
        uint32_t width = sym.getType().getBitWidth();
        signals_[current_module_name].emplace(name, width);
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
    std::unordered_map<std::string, std::set<std::pair<std::string, std::string>>> &connections_;

    class SymbolCollector : public slang::ASTVisitor<SymbolCollector, true, true> {
    public:
        explicit SymbolCollector(std::string target_name) : target_name_(std::move(target_name)) {}

        void handle(const slang::NamedValueExpression &expr) {
            auto name = expr.symbol.name;
            if (name != target_name_ && port_name.empty()) {
                port_name = std::string(name);
            }
        }
        std::string port_name;

    private:
        std::string target_name_;
    };

    void compute_connection(const slang::InstanceSymbol &sym) {
        auto def_name = std::string(sym.getDefinition().name);
        auto *parent = sym.getParentScope();
        if (!parent) return;
        auto const &parent_symbol = parent->asSymbol();
        if (parent_symbol.kind != slang::SymbolKind::InstanceBody) return;
        auto parent_name =
            std::string(parent_symbol.as<slang::InstanceBodySymbol>().getDefinition().name);

        sym.forEachPortConnection(
            [this, &def_name, &parent_name](const slang::PortConnection &conn) {
                auto const port = std::string(conn.port.name);
                auto const *expr = conn.getExpression();
                if (!expr) return;
                // we only deal with named value expression
                auto entry = def_name + "." + port;
                SymbolCollector collector(port);
                expr->visit(collector);
                auto port_name = collector.port_name;
                if (!port_name.empty())
                    connections_[entry].emplace(std::make_pair(parent_name, port_name));
            });
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

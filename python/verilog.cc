#include "verilog.hh"

#include <iostream>

#include "slang/compilation/Compilation.h"
#include "slang/diagnostics/DiagnosticEngine.h"
#include "slang/parsing/Parser.h"
#include "slang/parsing/Preprocessor.h"
#include "slang/symbols/ASTVisitor.h"
#include "slang/symbols/InstanceSymbols.h"
#include "slang/symbols/VariableSymbols.h"
#include "slang/syntax/SyntaxTree.h"
#include "slang/text/SourceManager.h"

class VisitSignals : public slang::ASTVisitor<VisitSignals, true, true> {
public:
    explicit VisitSignals(std::unordered_map<std::string, std::unordered_set<std::string>> &signals)
        : signals_(signals) {}

    [[maybe_unused]] void handle(const slang::InstanceSymbol &sym) {
        auto def_name = std::string(sym.getDefinition().name);
        if (signals_.find(def_name) != signals_.end()) return;

        current_module_name = def_name;
        visitDefault(sym);
    }

    [[maybe_unused]] void handle(const slang::NetSymbol &sym) {
        auto name = std::string(sym.name);
        signals_[current_module_name].emplace(name);
    }

    [[maybe_unused]] void handle(const slang::VariableSymbol &sym) {
        auto name = std::string(sym.name);
        signals_[current_module_name].emplace(name);
    }

    std::string current_module_name;

private:
    std::unordered_map<std::string, std::unordered_set<std::string>> &signals_;
};

std::unordered_map<std::string, std::unordered_set<std::string>> parse_verilog(
    const std::vector<std::string> &files, const std::string &top_name) {
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

    std::unordered_map<std::string, std::unordered_set<std::string>> res;
    // visit them to build up the

    VisitSignals vis(res);
    top->visit(vis);

    return res;
}

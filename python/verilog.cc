#include "verilog.hh"

#include <iostream>

#include "slang/compilation/Compilation.h"
#include "slang/diagnostics/DiagnosticEngine.h"
#include "slang/parsing/Parser.h"
#include "slang/parsing/Preprocessor.h"
#include "slang/symbols/CompilationUnitSymbols.h"
#include "slang/symbols/VariableSymbols.h"
#include "slang/syntax/SyntaxTree.h"
#include "slang/text/SourceManager.h"
#include "slang/types/AllTypes.h"
#include "slang/types/Type.h"

std::unique_ptr<NamedScope> parse_verilog(const std::vector<std::string> &files) {
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

    return nullptr;
}

#include <filesystem>

#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

llvm::LLVMContext *get_llvm_context() {
    static std::unique_ptr<llvm::LLVMContext> context;
    if (!context) {
        context = std::make_unique<llvm::LLVMContext>();
    }
    return context.get();
}

std::string resolve_filename(const std::string &filename, const std::string &directory) {
    namespace fs = std::filesystem;
    fs::path path = directory;
    path /= filename;
    auto res = fs::weakly_canonical(fs::absolute(path));
    return res.string();
}

// because the optimized build has all the function scopes messed up, we need to extract out
// proper scopes from the debug build then use the source location to reconstruct the scopes

PYBIND11_MODULE(vitis0, m) {
    m.def("get_function_scopes", [](const std::string &filename) {
        // for now, we only use the following format
        // filename -> function name -> line range
        std::map<std::string, std::map<std::string, std::pair<uint32_t, uint32_t>>> res;
        llvm::SMDiagnostic error;
        auto module = llvm::parseIRFile(filename, error, *get_llvm_context());
        if (!module) return res;
        for (auto const &func : *module) {
            // need to resolve the filename
            std::string resolved_filename;
            uint32_t min = std::numeric_limits<uint32_t>::max();
            uint32_t max = 0;
            for (auto const &blk : func) {
                for (auto const &instr : blk) {
                    auto const &loc = instr.getDebugLoc();
                    auto *di_loc = loc.get();
                    if (!di_loc) continue;
                    auto line = loc.getLine();
                    if (line == 0) continue;
                    if (max < line) max = line;
                    if (min > line) min = line;

                    if (resolved_filename.empty()) {
                        // get filename
                        auto fn = di_loc->getFilename().str();
                        auto dir = di_loc->getDirectory().str();
                        resolved_filename = resolve_filename(fn, dir);
                    }
                }
            }
            auto function_name = func.getName().str();
            if (!resolved_filename.empty()) {
                res[resolved_filename][function_name] = std::make_pair(min, max);
            }
        }
        return res;
    });
}
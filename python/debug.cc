#include <filesystem>

#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/IR/IntrinsicInst.h"
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

std::unordered_map<const llvm::Value *, const llvm::CallInst *> index_debug_declare(
    const llvm::Function *function) {
    std::unordered_map<const llvm::Value *, const llvm::CallInst *> res;
    for (auto const &blk : *function) {
        for (auto const &instr : blk) {
            if (llvm::isa<llvm::CallInst>(instr)) {
                auto const &call = llvm::cast<llvm::CallInst>(instr);
                if (auto const *func = call.getCalledFunction()) {
                    if (func->getName() == "llvm.dbg.declare") {
                        auto *md = llvm::ValueAsMetadata::get(call.getOperand(0));
                        auto *v = md->getValue();
                        res.emplace(v, &call);
                    }
                }
            }
        }
    }
    return res;
}

// because the optimized build has all the function scopes messed up, we need to extract out
// proper scopes from the debug build then use the source location to reconstruct the scopes

PYBIND11_MODULE(vitis0, m) {
    m.def("get_function_scopes", [](const std::vector<std::string> &filenames) {
        // for now, we only use the following format
        // filename -> function name -> line range
        std::map<std::string, std::map<std::string, std::pair<uint32_t, uint32_t>>> res;
        llvm::SMDiagnostic error;
        for (auto const &filename : filenames) {
            auto module = llvm::parseIRFile(filename, error, *get_llvm_context());
            if (!module) continue;
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
        }

        return res;
    });

    m.def("get_function_args", [](const std::vector<std::string> &filenames) {
        llvm::SMDiagnostic error;
        std::map<std::string, std::vector<std::tuple<std::string, uint32_t, std::vector<uint32_t>>>>
            res;
        for (auto const &filename : filenames) {
            auto module = llvm::parseIRFile(filename, error, *get_llvm_context());
            if (!module) continue;

            for (auto const &func : *module) {
                auto func_name = func.getName().str();
                for (auto const &arg : func.args()) {
                    llvm::SmallVector<llvm::DbgVariableIntrinsic *, 1> debug_values;
                    for (auto use : arg.users()) {
                        if (auto store = llvm::dyn_cast<llvm::StoreInst>(use)) {
                            // find debug call
                            auto *store_dst = store->getPointerOperand();
                            if (!store_dst) continue;
                            llvm::findDbgUsers(debug_values, const_cast<llvm::Value *>(store_dst));
                        }
                    }
                    if (debug_values.empty()) continue;
                    auto local_var = debug_values[0]->getVariable();
                    if (!local_var) continue;
                    auto name = local_var->getName().str();
                    auto line = local_var->getLine();
                    auto *t = local_var->getType();
                    if (!t) continue;
                    std::vector<uint32_t> entry;
                    if (llvm::isa<llvm::DIBasicType>(t)) {
                        // for basic type we directly store them
                        res[func_name].emplace_back(std::make_tuple(name, line, entry));
                    } else if (auto derived_type = llvm::dyn_cast<llvm::DIDerivedType>(t)) {
                        auto base_type = derived_type->getBaseType();
                        if (auto composite = llvm::dyn_cast<llvm::DICompositeType>(base_type)) {
                            // we only deal with multi-dim array for now
                            auto elements = composite->getElements();
                            for (auto const &a : elements) {
                                auto sub = llvm::dyn_cast<llvm::DISubrange>(a);
                                if (!sub) continue;
                                auto count = sub->getCount().get<llvm::ConstantInt *>();
                                if (!count) continue;
                                entry.emplace_back(count->getLimitedValue());
                            }
                            // do we need to worry about the lower dim?
                            // or we assume vitis is going to use reg file/SRAM instead?
                            res[func_name].emplace_back(std::make_tuple(name, line, entry));
                        }
                    }
                }
            }
        }

        return res;
    });
}
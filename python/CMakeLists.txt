add_library(hgdb-vitis ir.cc)
target_include_directories(hgdb-vitis PUBLIC ${LLVM_INCLUDE_DIRS})
target_link_libraries(hgdb-vitis ${llvm_libs})
set_property(TARGET hgdb-vitis PROPERTY POSITION_INDEPENDENT_CODE ON)
target_compile_options(hgdb-vitis PRIVATE -Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter -Wno-deprecated-copy
        -Wno-unused-local-typedefs)

pybind11_add_module(vitis binding.cc)
target_link_libraries(vitis PUBLIC hgdb-vitis llvm::core llvm::support llvm::analysis)
# lots of werror since we're using ancient LLVM
## Xilinx you should upgrade your LLVM toolchain!
target_compile_options(vitis PRIVATE -Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter -Wno-deprecated-copy
        -Wno-unused-local-typedefs)

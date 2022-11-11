add_library(hgdb-vitis ir.cc)
target_include_directories(hgdb-vitis PUBLIC ${LLVM3_INCLUDE_DIRS} ../extern/slang/include)
target_link_libraries(hgdb-vitis PUBLIC llvm3::bitcode llvm3::core llvm3::support llvm3::analysis llvm3::bitcode)
set_property(TARGET hgdb-vitis PROPERTY POSITION_INDEPENDENT_CODE ON)
target_compile_options(hgdb-vitis PRIVATE -Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter -Wno-deprecated-copy
        -Wno-unused-local-typedefs)
target_compile_definitions(hgdb-vitis PUBLIC -D_GLIBCXX_USE_CXX11_ABI=0 span_CONFIG_SELECT_SPAN=1)

pybind11_add_module(vitis binding.cc)
target_link_libraries(vitis PUBLIC hgdb-vitis)
# lots of werror since we're using ancient LLVM
## Xilinx you should upgrade your LLVM toolchain!
target_compile_options(vitis PRIVATE -Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter -Wno-deprecated-copy
        -Wno-unused-local-typedefs)

pybind11_add_module(vitis_rtl verilog.cc)
target_link_libraries(vitis_rtl PUBLIC slangcompiler)

pybind11_add_module(vitis0 debug.cc)
set_property(TARGET vitis0 PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(vitis0 PUBLIC llvm10::core)
target_include_directories(vitis0 PRIVATE ${LLVM10_INCLUDE_DIRS})
target_compile_definitions(vitis0 PUBLIC -DLLVM_DISABLE_ABI_BREAKING_CHECKS_ENFORCING=1)

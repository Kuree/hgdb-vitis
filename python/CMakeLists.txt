add_library(hgdb-vitis ir.cc)
target_include_directories(hgdb-vitis PRIVATE ${LLVM_INCLUDE_DIRS})
set_target_properties(hgdb-vitis PROPERTIES COMPILE_FLAGS ${LLVM_DEFINITIONS})
set_property(TARGET hgdb-vitis PROPERTY POSITION_INDEPENDENT_CODE ON)

pybind11_add_module(vitis binding.cc)
target_link_libraries(vitis PUBLIC hgdb-vitis)

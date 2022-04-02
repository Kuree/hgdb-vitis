add_library(hgdb-vitis ir.cc)
target_include_directories(hgdb-vitis PUBLIC ${LLVM_INCLUDE_DIRS})
target_link_libraries(hgdb-vitis ${llvm_libs})
set_property(TARGET hgdb-vitis PROPERTY POSITION_INDEPENDENT_CODE ON)
llvm_map_components_to_libnames(llvm_libs support core irreader)

pybind11_add_module(vitis binding.cc)
target_link_libraries(vitis PUBLIC hgdb-vitis ${llvm_libs})

add_subdirectory(pybind11)
add_subdirectory(slang)

# set some extra flags for external libs
set_property(TARGET slangcompiler PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET slangcore PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET slangparser PROPERTY POSITION_INDEPENDENT_CODE ON)

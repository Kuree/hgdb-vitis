#include "pybind11/pybind11.h"
#include "ir.hh"

namespace py = pybind11;

void init_structs(py::module &m) {
    py::class_<ModuleInfo>(m, "ModuleInfo");
}

PYBIND11_MODULE(vitis, m) {
    init_structs(m);
    m.def("parse_llvm_bitcode", &parse_llvm_bitcode);
}
#include "ir.hh"
#include "pybind11/pybind11.h"
#include <pybind11/stl.h>

namespace py = pybind11;

void bind_llvm(py::module &m) {
    py::class_<llvm::Module>(m, "Module")
        .def("get_function_instructions", &get_function_instructions,
             py::return_value_policy::reference);

    py::class_<llvm::Instruction, std::unique_ptr<llvm::Instruction, py::nodelete>>(m,
                                                                                    "Instruction");
}

PYBIND11_MODULE(vitis, m) {
    bind_llvm(m);
    m.def("parse_llvm_bitcode", &parse_llvm_bitcode);
}
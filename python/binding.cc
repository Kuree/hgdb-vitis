#include "ir.hh"
#include "pybind11/pybind11.h"
#include <pybind11/stl.h>

namespace py = pybind11;

void bind_llvm(py::module &m) {
    py::class_<llvm::Module>(m, "Module")
            .def("get_function_instructions", &get_function_instructions,
                 py::return_value_policy::reference)
            .def("get_function", [](const llvm::Module &module, const std::string &function_name) {
                return module.getFunction(function_name);
            }, py::return_value_policy::reference);

    py::class_<llvm::Instruction, std::unique_ptr<llvm::Instruction, py::nodelete>>(m,
                                                                                    "Instruction")
            .def_property_readonly("filename", &get_filename)
            .def_property_readonly("line_num", &get_line_num);

    py::class_<llvm::Function, std::unique_ptr<llvm::Function, py::nodelete>>(m, "Function")
            .def("get_instr_loc", &get_instr_loc, py::return_value_policy::reference_internal);
}

PYBIND11_MODULE(vitis, m) {
    bind_llvm(m);
    m.def("parse_llvm_bitcode", &parse_llvm_bitcode);
}
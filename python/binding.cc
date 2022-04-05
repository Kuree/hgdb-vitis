#include <pybind11/stl.h>

#include "ir.hh"
#include "pybind11/pybind11.h"

namespace py = pybind11;

void bind_llvm(py::module &m) {
    py::class_<llvm::Module>(m, "Module")
        .def("get_function_instructions", &get_function_instructions,
             py::return_value_policy::reference)
        .def(
            "get_function",
            [](const llvm::Module &module, const std::string &function_name) {
                return module.getFunction(function_name);
            },
            py::return_value_policy::reference)
        .def("get_optimized_functions", get_optimized_functions);

    py::class_<llvm::Instruction, std::unique_ptr<llvm::Instruction, py::nodelete>>(m,
                                                                                    "Instruction")
        .def_property_readonly("filename", &get_filename)
        .def_property_readonly("line_num", &get_line_num)
        .def_property_readonly("function", &get_function, py::return_value_policy::reference)
        .def_property_readonly("prev", py::overload_cast<>(&llvm::Instruction::getPrevNode),
                               py::return_value_policy::reference)
        .def_property_readonly("prev_alloc", &get_pre_alloc, py::return_value_policy::reference)
        .def("identical", &llvm::Instruction::isIdenticalTo)
        .def_property_readonly("rtl_name", &guess_rtl_name);

    py::class_<llvm::Function, std::unique_ptr<llvm::Function, py::nodelete>>(m, "Function")
        .def("get_instr_loc", &get_instr_loc, py::return_value_policy::reference_internal)
        .def("get_contained_functions", &get_contained_functions)
        .def_property_readonly("demangled_name", &get_demangled_name)
        .def_property_readonly("name", py::overload_cast<const llvm::Function *>(&get_name))
        .def("find_matching_instr", &find_matching_instr);
}

PYBIND11_MODULE(vitis, m) {
    bind_llvm(m);
    m.def("parse_llvm_bitcode", &parse_llvm_bitcode, py::return_value_policy::reference);
}
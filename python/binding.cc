#include "ir.hh"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

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
        .def("get_debug_scope", &get_debug_scope, py::return_value_policy::reference);
}

void bind_scope(py::module &m) {
    py::class_<Scope>(m, "Scope")
        .def("serialize", &Scope::serialize)
        .def("bind_state", &Scope::bind_state)
        .def_readonly("instruction", &Scope::instruction);
    py::class_<Context>(m, "Context")
        .def(py::init<>())
        .def("__getitem__", &Context::get_module)
        .def("__setitem__", &Context::add_module)
        .def("__contains__", &Context::has_module)
        .def("modules", [](Context &context) { return context.module_infos(); })
        .def("set_rtl_info", &Context::set_rtl_info)
        .def_readwrite("top_name", &Context::top_name);

    py::class_<StateInfo>(m, "StateInfo")
        .def(py::init<std::string>())
        .def("add_instr", &StateInfo::add_instruction);

    py::class_<SerializationOptions>(m, "SerializationOptions")
        .def(py::init<>())
        .def("add_mapping", &SerializationOptions::add_mapping);

    py::class_<SignalInfo>(m, "SignalInfo")
        .def(py::init<std::string, uint32_t>())
        .def_readonly("name", &SignalInfo::name)
        .def_readonly("width", &SignalInfo::width);

    py::class_<ModuleInfo, std::shared_ptr<ModuleInfo>>(m, "ModuleInfo")
        .def(py::init<std::string>())
        .def_readonly("module_name", &ModuleInfo::module_name)
        .def_readwrite("state_infos", &ModuleInfo::state_infos)
        .def_readwrite("signals", &ModuleInfo::signals)
        .def_readwrite("function", &ModuleInfo::function)
        .def_readwrite("instances", &ModuleInfo::instances)
        .def("add_instance", &ModuleInfo::add_instance);

    m.def("reorganize_scopes", reorganize_scopes);
    m.def("infer_dangling_scope_state", infer_dangling_scope_state);
    m.def("infer_function_arg", infer_function_arg);
    m.def("inject_function_args", inject_function_args);
}

PYBIND11_MODULE(vitis, m) {
    bind_llvm(m);
    bind_scope(m);
    m.def("parse_llvm_bitcode", &parse_llvm_bitcode, py::return_value_policy::reference);
}
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "include/isa/Opcode.hpp"
#include "include/isa/Registers.hpp"
#include "components/SingleCycleCPU.hpp"
#include "components/MultiCycleCPU.hpp"
#include "components/PipelinedCPU.hpp"
#include "components/CPU.hpp"

namespace py = pybind11;

namespace {

std::uint32_t encode_r(cpu::Opcode op, cpu::Register rd, cpu::Register rs1, cpu::Register rs2) {
    return (static_cast<std::uint32_t>(op) << 26) |
           (static_cast<std::uint32_t>(rd) << 22) |
           (static_cast<std::uint32_t>(rs1) << 18) |
           (static_cast<std::uint32_t>(rs2) << 14);
}

std::uint32_t encode_i(cpu::Opcode op, cpu::Register rd, cpu::Register rs1, std::uint32_t imm) {
    return (static_cast<std::uint32_t>(op) << 26) |
           (static_cast<std::uint32_t>(rd) << 22) |
           (static_cast<std::uint32_t>(rs1) << 18) |
           (imm & 0x3FFFFU);
}

std::uint32_t encode_s(cpu::Opcode op, cpu::Register rs2, cpu::Register rs1, std::uint32_t imm) {
    return (static_cast<std::uint32_t>(op) << 26) |
           (static_cast<std::uint32_t>(rs2) << 22) |
           (static_cast<std::uint32_t>(rs1) << 18) |
           (imm & 0x3FFFFU);
}

std::uint32_t encode_b(cpu::Opcode op, cpu::Register rs1, cpu::Register rs2, std::uint32_t imm) {
    return (static_cast<std::uint32_t>(op) << 26) |
           (static_cast<std::uint32_t>(rs1) << 22) |
           (static_cast<std::uint32_t>(rs2) << 18) |
           (imm & 0x3FFFFU);
}

std::uint32_t encode_j(cpu::Opcode op, std::int32_t imm) {
    return (static_cast<std::uint32_t>(op) << 26) |
           (static_cast<std::uint32_t>(imm) & 0x03FFFFFFU);
}

template <typename CPUType, typename PyClass>
void bind_cpu_interface(PyClass& cls) {
    cls.def(py::init<>())
       .def("reset", &CPUType::reset)
       .def("step", &CPUType::step)
       .def("run", &CPUType::run, py::arg("max_cycles") = 100000)
       .def("halted", &CPUType::halted)
       .def("load_program", [](CPUType& cpu, const std::vector<std::size_t>& p) { cpu.load_program(p); })
       .def("read_register", &CPUType::read_register)
       .def("write_register", &CPUType::write_register)
       .def("pc", &CPUType::pc)
       .def("cycles", &CPUType::cycles);
}

} // namespace

PYBIND11_MODULE(_pycpu_core, m) {
    m.doc() = "C++ backend for pycpu architecture simulation";

    // Opcode Enum
    py::enum_<cpu::Opcode>(m, "Opcode")
        .value("NOP", cpu::Opcode::NOP)
        .value("ADD", cpu::Opcode::ADD)
        .value("SUB", cpu::Opcode::SUB)
        .value("AND", cpu::Opcode::AND)
        .value("OR", cpu::Opcode::OR)
        .value("XOR", cpu::Opcode::XOR)
        .value("NOT", cpu::Opcode::NOT)
        .value("LI", cpu::Opcode::LI)
        .value("ADDI", cpu::Opcode::ADDI)
        .value("LW", cpu::Opcode::LW)
        .value("SW", cpu::Opcode::SW)
        .value("BEQ", cpu::Opcode::BEQ)
        .value("BNE", cpu::Opcode::BNE)
        .value("J", cpu::Opcode::J)
        .value("HALT", cpu::Opcode::HALT)
        .export_values();

    // Register Enum
    py::enum_<cpu::Register>(m, "Register")
        .value("R0", cpu::Register::R0)
        .value("R1", cpu::Register::R1)
        .value("R2", cpu::Register::R2)
        .value("R3", cpu::Register::R3)
        .value("R4", cpu::Register::R4)
        .value("R5", cpu::Register::R5)
        .value("R6", cpu::Register::R6)
        .value("R7", cpu::Register::R7)
        .value("R8", cpu::Register::R8)
        .value("R9", cpu::Register::R9)
        .value("R10", cpu::Register::R10)
        .value("R11", cpu::Register::R11)
        .value("R12", cpu::Register::R12)
        .value("R13", cpu::Register::R13)
        .value("R14", cpu::Register::R14)
        .value("R15", cpu::Register::R15)
        .export_values();

    // Instruction Encoding Helpers
    m.def("encode_r_type", &encode_r, "Encode an R-type instruction word");
    m.def("encode_i_type", &encode_i, "Encode an I-type instruction word");
    m.def("encode_s_type", &encode_s, "Encode an S-type instruction word");
    m.def("encode_b_type", &encode_b, "Encode a B-type instruction word");
    m.def("encode_j_type", &encode_j, "Encode a J-type instruction word");

    // NativeSingleCycleCPU32 (and backward-compatible NativeCPU32)
    using SingleCycleCPU32 = cpu::SingleCycleCPU<32, 32, 32, 4, 8, 8>;
    py::class_<SingleCycleCPU32> single_cycle_cls(m, "NativeSingleCycleCPU32");
    bind_cpu_interface<SingleCycleCPU32>(single_cycle_cls);

    // Backward compatibility alias for NativeCPU32
    m.attr("NativeCPU32") = single_cycle_cls;

    // NativeMultiCycleCPU32
    using MultiCycleCPU32 = cpu::MultiCycleCPU<32, 32, 32, 4, 8>;
    py::class_<MultiCycleCPU32> multi_cycle_cls(m, "NativeMultiCycleCPU32");
    bind_cpu_interface<MultiCycleCPU32>(multi_cycle_cls);
    multi_cycle_cls.def("step_instruction", &MultiCycleCPU32::step_instruction);

    // NativePipelinedCPU32
    using PipelinedCPU32 = cpu::PipelinedCPU<32, 32, 32, 4, 8, 8>;
    py::class_<PipelinedCPU32> pipelined_cls(m, "NativePipelinedCPU32");
    bind_cpu_interface<PipelinedCPU32>(pipelined_cls);
}

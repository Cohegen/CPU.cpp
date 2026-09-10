#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "include/isa/Opcode.hpp"
#include "include/isa/Registers.hpp"
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

    // NativeCPU32
    using CPU32 = cpu::CPU<32, 32, 32, 4, 8, 8>;
    py::class_<CPU32>(m, "NativeCPU32")
        .def(py::init<>())
        .def("reset", &CPU32::reset)
        .def("step", &CPU32::step)
        .def("run", &CPU32::run, py::arg("max_cycles") = 100000)
        .def("halted", &CPU32::halted)
        .def("load_program", &CPU32::load_program)
        .def("read_register", &CPU32::read_register)
        .def("write_register", &CPU32::write_register)
        .def("pc", &CPU32::pc)
        .def("cycles", &CPU32::cycles);
}

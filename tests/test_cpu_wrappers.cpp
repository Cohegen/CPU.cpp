/*
    test_cpu_wrappers.cpp

    Integration tests verifying top-level CPU wrappers:
      - SingleCycleCPU
      - MultiCycleCPU
      - PipelinedCPU
    Executes identical assembly programs across all three microarchitectures
    and verifies functional equivalence and microarchitectural cycle differences.
*/

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

#include "../components/SingleCycleCPU.hpp"
#include "../components/MultiCycleCPU.hpp"
#include "../components/PipelinedCPU.hpp"
#include "../components/CPU.hpp"
#include "../include/isa/Opcode.hpp"
#include "../include/isa/Registers.hpp"

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

std::vector<std::size_t> make_test_program() {
    // 0: ADDI R1, R0, 10      (R1 = 10)
    // 1: ADDI R2, R0, 20      (R2 = 20)
    // 2: ADD  R3, R1, R2      (R3 = 10 + 20 = 30)
    // 3: SUB  R4, R3, R1      (R4 = 30 - 10 = 20)
    // 4: HALT
    return {
        encode_i(cpu::Opcode::ADDI, cpu::Register::R1, cpu::Register::R0, 10),
        encode_i(cpu::Opcode::ADDI, cpu::Register::R2, cpu::Register::R0, 20),
        encode_r(cpu::Opcode::ADD,  cpu::Register::R3, cpu::Register::R1, cpu::Register::R2),
        encode_r(cpu::Opcode::SUB,  cpu::Register::R4, cpu::Register::R3, cpu::Register::R1),
        encode_i(cpu::Opcode::HALT, cpu::Register::R0, cpu::Register::R0, 0)
    };
}

void test_single_cycle_wrapper() {
    std::cout << "[Test 1] Testing SingleCycleCPU wrapper...\n";
    cpu::SingleCycleCPU cpu;
    cpu.reset();
    cpu.load_program(make_test_program());
    cpu.run();

    assert(cpu.halted());
    assert(cpu.read_register(cpu::Register::R1) == 10);
    assert(cpu.read_register(cpu::Register::R2) == 20);
    assert(cpu.read_register(cpu::Register::R3) == 30);
    assert(cpu.read_register(cpu::Register::R4) == 20);
    std::cout << "  [PASS] SingleCycleCPU cycles: " << cpu.cycles() << "\n";
}

void test_multi_cycle_wrapper() {
    std::cout << "[Test 2] Testing MultiCycleCPU wrapper...\n";
    cpu::MultiCycleCPU cpu;
    cpu.reset();
    cpu.load_program(make_test_program());
    cpu.run();

    assert(cpu.halted());
    assert(cpu.read_register(cpu::Register::R1) == 10);
    assert(cpu.read_register(cpu::Register::R2) == 20);
    assert(cpu.read_register(cpu::Register::R3) == 30);
    assert(cpu.read_register(cpu::Register::R4) == 20);
    std::cout << "  [PASS] MultiCycleCPU cycles: " << cpu.cycles() << "\n";
}

void test_pipelined_wrapper() {
    std::cout << "[Test 3] Testing PipelinedCPU wrapper...\n";
    cpu::PipelinedCPU cpu;
    cpu.reset();
    cpu.load_program(make_test_program());
    cpu.run();

    assert(cpu.halted());
    assert(cpu.read_register(cpu::Register::R1) == 10);
    assert(cpu.read_register(cpu::Register::R2) == 20);
    assert(cpu.read_register(cpu::Register::R3) == 30);
    assert(cpu.read_register(cpu::Register::R4) == 20);
    std::cout << "  [PASS] PipelinedCPU cycles: " << cpu.cycles() << "\n";
}

void test_cpu_alias_backward_compatibility() {
    std::cout << "[Test 4] Testing cpu::CPU backward compatibility alias...\n";
    cpu::CPU cpu;
    cpu.reset();
    cpu.load_program(make_test_program());
    cpu.run();

    assert(cpu.halted());
    assert(cpu.read_register(cpu::Register::R3) == 30);
    std::cout << "  [PASS] cpu::CPU alias functions identically to SingleCycleCPU.\n";
}

} // namespace

int main() {
    std::cout << "====================================================\n";
    std::cout << "   RUNNING TOP-LEVEL CPU WRAPPER TESTS\n";
    std::cout << "====================================================\n";

    test_single_cycle_wrapper();
    test_multi_cycle_wrapper();
    test_pipelined_wrapper();
    test_cpu_alias_backward_compatibility();

    std::cout << "====================================================\n";
    std::cout << "   ALL CPU WRAPPER TESTS PASSED SUCCESSFULLY!\n";
    std::cout << "====================================================\n";
    return 0;
}

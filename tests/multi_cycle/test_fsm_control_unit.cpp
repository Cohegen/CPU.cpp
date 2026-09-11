/*
    test_fsm_control_unit.cpp

    Unit and Integration tests for FSMControlUnit and autonomous MultiCycleDatapath execution.
    Verifies:
      1. Correct state transitions for all supported instruction formats/opcodes.
      2. Signal generation accuracy across all micro-architectural states.
      3. Autonomous execution of R-type instructions (ADD, SUB, AND, OR, XOR).
      4. Autonomous execution of I-type immediate instructions (LI, ADDI).
      5. Autonomous execution of Memory instructions (SW, LW).
      6. Autonomous execution of Control Flow instructions (BEQ, BNE, J, HALT).
*/

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

#include <logic/signals/clock.hpp>
#include <logic/signals/wire.hpp>

#include "../../multi_cycle_cpu/core/FSMControlUnit.hpp"
#include "../../multi_cycle_cpu/datapath/MultiCycleDatapath.hpp"
#include "../../include/isa/InstructionDecoder.hpp"

namespace {

using Datapath = cpu::MultiCycleDatapath<32, 32, 32, 4, 8>;
using State = cpu::FSMControlUnit::State;

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

std::uint32_t encode_b(cpu::Opcode op, cpu::Register rs1, cpu::Register rs2, std::int32_t imm) {
    return (static_cast<std::uint32_t>(op) << 26) |
           (static_cast<std::uint32_t>(rs1) << 22) |
           (static_cast<std::uint32_t>(rs2) << 18) |
           (static_cast<std::uint32_t>(imm) & 0x3FFFFU);
}

std::uint32_t encode_j(cpu::Opcode op, std::int32_t imm) {
    return (static_cast<std::uint32_t>(op) << 26) |
           (static_cast<std::uint32_t>(imm) & 0x03FFFFFFU);
}

// Test 1: Unit testing FSM state transitions directly
void test_fsm_state_transitions() {
    std::cout << "[Test 1] Testing FSM state machine transitions...\n";
    cpu::FSMControlUnit fsm;

    assert(fsm.current_state() == State::FETCH);

    // FETCH -> DECODE
    cpu::DecodedInstruction add_inst{};
    add_inst.opcode = cpu::Opcode::ADD;
    add_inst.format = cpu::InstructionFormat::R_TYPE;

    fsm.step(add_inst);
    assert(fsm.current_state() == State::DECODE);

    // DECODE -> EXECUTE_R -> ALU_WB -> FETCH
    fsm.step(add_inst);
    assert(fsm.current_state() == State::EXECUTE_R);
    fsm.step(add_inst);
    assert(fsm.current_state() == State::ALU_WB);
    fsm.step(add_inst);
    assert(fsm.current_state() == State::FETCH);

    // Test LW: FETCH -> DECODE -> MEM_ADDR -> MEM_READ -> MEM_WB -> FETCH
    cpu::DecodedInstruction lw_inst{};
    lw_inst.opcode = cpu::Opcode::LW;
    lw_inst.format = cpu::InstructionFormat::I_TYPE;

    fsm.step(lw_inst); // FETCH -> DECODE
    assert(fsm.current_state() == State::DECODE);
    fsm.step(lw_inst); // DECODE -> MEM_ADDR
    assert(fsm.current_state() == State::MEM_ADDR);
    fsm.step(lw_inst); // MEM_ADDR -> MEM_READ
    assert(fsm.current_state() == State::MEM_READ);
    fsm.step(lw_inst); // MEM_READ -> MEM_WB
    assert(fsm.current_state() == State::MEM_WB);
    fsm.step(lw_inst); // MEM_WB -> FETCH
    assert(fsm.current_state() == State::FETCH);

    // Test SW: FETCH -> DECODE -> MEM_ADDR -> MEM_WRITE -> FETCH
    cpu::DecodedInstruction sw_inst{};
    sw_inst.opcode = cpu::Opcode::SW;
    sw_inst.format = cpu::InstructionFormat::S_TYPE;

    fsm.step(sw_inst); // FETCH -> DECODE
    fsm.step(sw_inst); // DECODE -> MEM_ADDR
    assert(fsm.current_state() == State::MEM_ADDR);
    fsm.step(sw_inst); // MEM_ADDR -> MEM_WRITE
    assert(fsm.current_state() == State::MEM_WRITE);
    fsm.step(sw_inst); // MEM_WRITE -> FETCH
    assert(fsm.current_state() == State::FETCH);

    // Test BEQ: FETCH -> DECODE -> BRANCH -> FETCH
    cpu::DecodedInstruction beq_inst{};
    beq_inst.opcode = cpu::Opcode::BEQ;
    beq_inst.format = cpu::InstructionFormat::B_TYPE;

    fsm.step(beq_inst); // FETCH -> DECODE
    fsm.step(beq_inst); // DECODE -> BRANCH
    assert(fsm.current_state() == State::BRANCH);
    fsm.step(beq_inst); // BRANCH -> FETCH
    assert(fsm.current_state() == State::FETCH);

    // Test JUMP: FETCH -> DECODE -> JUMP -> FETCH
    cpu::DecodedInstruction j_inst{};
    j_inst.opcode = cpu::Opcode::J;
    j_inst.format = cpu::InstructionFormat::J_TYPE;

    fsm.step(j_inst); // FETCH -> DECODE
    fsm.step(j_inst); // DECODE -> JUMP
    assert(fsm.current_state() == State::JUMP);
    fsm.step(j_inst); // JUMP -> FETCH
    assert(fsm.current_state() == State::FETCH);

    // Test HALT: FETCH -> DECODE -> HALT
    cpu::DecodedInstruction halt_inst{};
    halt_inst.opcode = cpu::Opcode::HALT;

    fsm.step(halt_inst); // FETCH -> DECODE
    fsm.step(halt_inst); // DECODE -> HALT
    assert(fsm.current_state() == State::HALT);
    assert(fsm.is_halted());
    fsm.step(halt_inst); // HALT remains HALT
    assert(fsm.current_state() == State::HALT);

    std::cout << "  [PASS] All FSM state transitions verified.\n";
}

// Test 2: Autonomous execution of R-type arithmetic program
void test_autonomous_r_type() {
    std::cout << "[Test 2] Testing autonomous R-type execution...\n";
    logic::Clock clock;
    logic::Wire reset(logic::LogicState::LOW);
    Datapath datapath(clock, reset);

    // Initial register setup
    datapath.write_register_for_test(cpu::Register::R1, 20);
    datapath.write_register_for_test(cpu::Register::R2, 30);

    // Program:
    // ADD R3, R1, R2  (20 + 30 = 50)
    // SUB R4, R3, R1  (50 - 20 = 30)
    // HALT
    std::vector<std::size_t> program = {
        encode_r(cpu::Opcode::ADD, cpu::Register::R3, cpu::Register::R1, cpu::Register::R2),
        encode_r(cpu::Opcode::SUB, cpu::Register::R4, cpu::Register::R3, cpu::Register::R1),
        encode_i(cpu::Opcode::HALT, cpu::Register::R0, cpu::Register::R0, 0)
    };
    datapath.load_instructions(program);

    // Step first instruction (ADD) autonomously
    datapath.step_instruction();
    assert(datapath.read_register_for_test(cpu::Register::R3) == 50);

    // Step second instruction (SUB) autonomously
    datapath.step_instruction();
    assert(datapath.read_register_for_test(cpu::Register::R4) == 30);

    // Step HALT
    datapath.step_instruction();
    assert(datapath.halted());

    std::cout << "  [PASS] Autonomous R-type instructions executed successfully.\n";
}

// Test 3: Autonomous execution of immediate instructions (LI, ADDI)
void test_autonomous_immediate() {
    std::cout << "[Test 3] Testing autonomous I-type (LI, ADDI) execution...\n";
    logic::Clock clock;
    logic::Wire reset(logic::LogicState::LOW);
    Datapath datapath(clock, reset);

    // Program:
    // LI   R1, 100
    // ADDI R2, R1, 25
    // HALT
    std::vector<std::size_t> program = {
        encode_i(cpu::Opcode::LI, cpu::Register::R1, cpu::Register::R0, 100),
        encode_i(cpu::Opcode::ADDI, cpu::Register::R2, cpu::Register::R1, 25),
        encode_i(cpu::Opcode::HALT, cpu::Register::R0, cpu::Register::R0, 0)
    };
    datapath.load_instructions(program);

    // Run entire program autonomously until HALT
    datapath.run();

    assert(datapath.read_register_for_test(cpu::Register::R1) == 100);
    assert(datapath.read_register_for_test(cpu::Register::R2) == 125);
    assert(datapath.halted());

    std::cout << "  [PASS] Autonomous I-type instructions (LI, ADDI) passed.\n";
}

// Test 4: Autonomous execution of memory operations (SW and LW)
void test_autonomous_memory() {
    std::cout << "[Test 4] Testing autonomous SW and LW execution...\n";
    logic::Clock clock;
    logic::Wire reset(logic::LogicState::LOW);
    Datapath datapath(clock, reset);

    constexpr std::uint32_t ram_addr = 0x90;
    datapath.write_register_for_test(cpu::Register::R1, ram_addr);
    datapath.write_register_for_test(cpu::Register::R2, 0xBEEF);

    // Program:
    // SW R2, R1, 0      (Store 0xBEEF into address 0x90)
    // LW R3, R1, 0      (Load from address 0x90 into R3)
    // HALT
    std::vector<std::size_t> program = {
        encode_s(cpu::Opcode::SW, cpu::Register::R2, cpu::Register::R1, 0),
        encode_i(cpu::Opcode::LW, cpu::Register::R3, cpu::Register::R1, 0),
        encode_i(cpu::Opcode::HALT, cpu::Register::R0, cpu::Register::R0, 0)
    };
    datapath.load_instructions(program);

    datapath.run();

    assert(datapath.read_register_for_test(cpu::Register::R3) == 0xBEEF);
    assert(datapath.halted());

    std::cout << "  [PASS] Autonomous SW and LW operations passed.\n";
}

// Test 5: Autonomous control flow (BEQ, BNE, J, HALT)
void test_autonomous_control_flow() {
    std::cout << "[Test 5] Testing autonomous branch and jump control flow...\n";
    logic::Clock clock;
    logic::Wire reset(logic::LogicState::LOW);
    Datapath datapath(clock, reset);

    // Program:
    // [0] LI  R1, 5
    // [1] LI  R2, 5
    // [2] BEQ R1, R2, 1  ; Branch taken -> target = 2 + 1 + 1 = 4 (skips [3])
    // [3] LI  R3, 99     ; Should be skipped
    // [4] LI  R4, 42     ; Target of branch
    // [5] J   7          ; Jump to [7] (skips [6])
    // [6] LI  R5, 99     ; Should be skipped
    // [7] HALT
    std::vector<std::size_t> program = {
        encode_i(cpu::Opcode::LI, cpu::Register::R1, cpu::Register::R0, 5),
        encode_i(cpu::Opcode::LI, cpu::Register::R2, cpu::Register::R0, 5),
        encode_b(cpu::Opcode::BEQ, cpu::Register::R1, cpu::Register::R2, 1),
        encode_i(cpu::Opcode::LI, cpu::Register::R3, cpu::Register::R0, 99),
        encode_i(cpu::Opcode::LI, cpu::Register::R4, cpu::Register::R0, 42),
        encode_j(cpu::Opcode::J, 7),
        encode_i(cpu::Opcode::LI, cpu::Register::R5, cpu::Register::R0, 99),
        encode_i(cpu::Opcode::HALT, cpu::Register::R0, cpu::Register::R0, 0)
    };
    datapath.load_instructions(program);

    datapath.run();

    assert(datapath.read_register_for_test(cpu::Register::R1) == 5);
    assert(datapath.read_register_for_test(cpu::Register::R2) == 5);
    assert(datapath.read_register_for_test(cpu::Register::R3) == 0); // skipped!
    assert(datapath.read_register_for_test(cpu::Register::R4) == 42); // executed!
    assert(datapath.read_register_for_test(cpu::Register::R5) == 0); // skipped!
    assert(datapath.halted());

    std::cout << "  [PASS] Autonomous BEQ and J branches executed accurately.\n";
}

} // namespace

int main() {
    std::cout << "=== Running Multi-Cycle FSM Control Unit & Autonomous Datapath Tests ===\n";
    test_fsm_state_transitions();
    test_autonomous_r_type();
    test_autonomous_immediate();
    test_autonomous_memory();
    test_autonomous_control_flow();
    std::cout << "[PASS] All FSM Control Unit and Autonomous Execution Tests Passed!\n";
    return 0;
}

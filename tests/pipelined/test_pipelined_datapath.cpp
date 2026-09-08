/*
    test_pipelined_datapath.cpp

    Unit and Integration tests for PipelinedCycleDatapath.
    Verifies:
      1. Datapath initialization and stage connectivity.
      2. R-type and I-type instruction execution through 5 stages.
      3. Data hazard resolution via MEM-to-EX forwarding (ForwardAE = 10).
      4. Data hazard resolution via WB-to-EX forwarding (ForwardAE = 01).
      5. ForwardBE forwarding for register operand B.
      6. Load-use hazard detection and 1-cycle stall insertion.
      7. Conditional branch resolution and pipeline flush (FlushD, FlushE).
*/

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

#include <logic/signals/clock.hpp>
#include <logic/signals/wire.hpp>

#include "../../pipelined_cpu/datapath/PipelinedDatapath.hpp"

namespace {

using PipelinedDatapath = cpu::PipelinedCycleDatapath<32, 32, 32, 4, 8, 8>;

std::uint32_t encode_r_type(
    cpu::Opcode opcode,
    cpu::Register rd,
    cpu::Register rs1,
    cpu::Register rs2
) {
    return (static_cast<std::uint32_t>(opcode) << 26) |
           (static_cast<std::uint32_t>(rd) << 22) |
           (static_cast<std::uint32_t>(rs1) << 18) |
           (static_cast<std::uint32_t>(rs2) << 14);
}

std::uint32_t encode_i_type(
    cpu::Opcode opcode,
    cpu::Register rd,
    cpu::Register rs1,
    std::uint32_t immediate
) {
    return (static_cast<std::uint32_t>(opcode) << 26) |
           (static_cast<std::uint32_t>(rd) << 22) |
           (static_cast<std::uint32_t>(rs1) << 18) |
           (immediate & 0x3FFFFU);
}

std::uint32_t encode_b_type(
    cpu::Opcode opcode,
    cpu::Register rs1,
    cpu::Register rs2,
    std::uint32_t immediate
) {
    return (static_cast<std::uint32_t>(opcode) << 26) |
           (static_cast<std::uint32_t>(rs1) << 22) |
           (static_cast<std::uint32_t>(rs2) << 18) |
           (immediate & 0x3FFFFU);
}

std::uint32_t encode_nop() {
    return static_cast<std::uint32_t>(cpu::Opcode::NOP) << 26;
}

// Test 1: Basic Datapath Construction & Reset State
void test_initialization() {
    std::cout << "[Test 1] Testing PipelinedDatapath initialization...\n";
    logic::Clock clock;
    logic::Wire reset(logic::LogicState::LOW);

    PipelinedDatapath datapath(clock, reset);
    datapath.evaluate();

    assert(datapath.pc().read_value() == 0);
    assert(datapath.stall_f().read() == logic::LogicState::LOW);
    assert(datapath.stall_d().read() == logic::LogicState::LOW);
    assert(datapath.flush_d().read() == logic::LogicState::LOW);
    assert(datapath.flush_e().read() == logic::LogicState::LOW);

    std::cout << "  [PASS] Datapath initializes correctly.\n";
}

// Test 2: R-type ADD instruction through all 5 pipeline stages
void test_r_type_add_pipeline() {
    std::cout << "[Test 2] Testing R-type ADD instruction execution...\n";
    logic::Clock clock;
    logic::Wire reset(logic::LogicState::LOW);

    PipelinedDatapath datapath(clock, reset);

    // Initial register setup: R2 = 15, R3 = 25
    datapath.write_register_for_test(cpu::Register::R2, 15);
    datapath.write_register_for_test(cpu::Register::R3, 25);

    // Instruction: ADD R1, R2, R3 (15 + 25 = 40)
    // Followed by NOPs to allow full progression through the pipeline
    std::vector<std::uint32_t> program = {
        encode_r_type(cpu::Opcode::ADD, cpu::Register::R1, cpu::Register::R2, cpu::Register::R3),
        encode_nop(),
        encode_nop(),
        encode_nop(),
        encode_nop()
    };
    datapath.load_instructions(program);

    // Cycle 1: Fetch ADD
    datapath.step();
    // Cycle 2: Decode ADD, Fetch NOP
    datapath.step();
    // Cycle 3: Execute ADD
    datapath.step();
    // Cycle 4: Memory stage ADD
    datapath.step();
    // Cycle 5: Writeback stage ADD
    datapath.step();

    // Verify result in Register File: R1 should now equal 40
    assert(datapath.read_register_for_test(cpu::Register::R1) == 40);
    std::cout << "  [PASS] R-type ADD executed and written back correctly (R1 = 40).\n";
}

// Test 3: Data Hazard Resolution with MEM-to-EX Forwarding (ForwardAE = 10)
void test_mem_to_ex_forwarding() {
    std::cout << "[Test 3] Testing MEM-to-EX forwarding (ForwardAE = 10)...\n";
    logic::Clock clock;
    logic::Wire reset(logic::LogicState::LOW);

    PipelinedDatapath datapath(clock, reset);

    datapath.write_register_for_test(cpu::Register::R2, 10);
    datapath.write_register_for_test(cpu::Register::R3, 20);
    datapath.write_register_for_test(cpu::Register::R5, 5);

    // Instruction 1: ADD R1, R2, R3 (R1 = 30)
    // Instruction 2: ADD R4, R1, R5 (R4 = R1 + 5 = 35) -> Back-to-back hazard on R1!
    std::vector<std::uint32_t> program = {
        encode_r_type(cpu::Opcode::ADD, cpu::Register::R1, cpu::Register::R2, cpu::Register::R3),
        encode_r_type(cpu::Opcode::ADD, cpu::Register::R4, cpu::Register::R1, cpu::Register::R5),
        encode_nop(),
        encode_nop(),
        encode_nop()
    };
    datapath.load_instructions(program);

    // Cycle 1: Fetch Instr 1
    datapath.step();
    // Cycle 2: Decode Instr 1, Fetch Instr 2
    datapath.step();
    // Cycle 3: Execute Instr 1, Decode Instr 2
    datapath.step();
    // Cycle 4: Memory Instr 1 (ALUOutM = 30), Execute Instr 2 (needs R1 from MEM stage!)
    // ForwardAE must be 10 (MEM stage forwarding)
    assert(datapath.forward_ae().read_value() == 0b10);
    datapath.step();

    // Cycle 5: Writeback Instr 1, Memory Instr 2
    datapath.step();
    // Cycle 6: Writeback Instr 2
    datapath.step();

    assert(datapath.read_register_for_test(cpu::Register::R1) == 30);
    assert(datapath.read_register_for_test(cpu::Register::R4) == 35);
    std::cout << "  [PASS] MEM-to-EX forwarding resolved hazard correctly (R4 = 35).\n";
}

// Test 4: Data Hazard Resolution with WB-to-EX Forwarding (ForwardAE = 01)
void test_wb_to_ex_forwarding() {
    std::cout << "[Test 4] Testing WB-to-EX forwarding (ForwardAE = 01)...\n";
    logic::Clock clock;
    logic::Wire reset(logic::LogicState::LOW);

    PipelinedDatapath datapath(clock, reset);

    datapath.write_register_for_test(cpu::Register::R2, 10);
    datapath.write_register_for_test(cpu::Register::R3, 20);
    datapath.write_register_for_test(cpu::Register::R5, 7);

    // Instruction 1: ADD R1, R2, R3 (R1 = 30)
    // Instruction 2: NOP
    // Instruction 3: ADD R4, R1, R5 (R4 = 30 + 7 = 37) -> Hazard with 1 NOP distance!
    std::vector<std::uint32_t> program = {
        encode_r_type(cpu::Opcode::ADD, cpu::Register::R1, cpu::Register::R2, cpu::Register::R3),
        encode_nop(),
        encode_r_type(cpu::Opcode::ADD, cpu::Register::R4, cpu::Register::R1, cpu::Register::R5),
        encode_nop(),
        encode_nop(),
        encode_nop()
    };
    datapath.load_instructions(program);

    // Advance to cycle where Instr 3 is in EX and Instr 1 is in WB
    datapath.step(); // Cycle 1
    datapath.step(); // Cycle 2
    datapath.step(); // Cycle 3
    datapath.step(); // Cycle 4
    // Cycle 5: Instr 1 is in WB (ResultW = 30), Instr 3 is in EX.
    // ForwardAE must be 01 (WB stage forwarding)
    assert(datapath.forward_ae().read_value() == 0b01);
    datapath.step(); // Cycle 5
    datapath.step(); // Cycle 6
    datapath.step(); // Cycle 7

    assert(datapath.read_register_for_test(cpu::Register::R1) == 30);
    assert(datapath.read_register_for_test(cpu::Register::R4) == 37);
    std::cout << "  [PASS] WB-to-EX forwarding resolved hazard correctly (R4 = 37).\n";
}

// Test 5: ForwardBE Forwarding for Register Operand B
void test_forward_be_forwarding() {
    std::cout << "[Test 5] Testing ForwardBE forwarding...\n";
    logic::Clock clock;
    logic::Wire reset(logic::LogicState::LOW);

    PipelinedDatapath datapath(clock, reset);

    datapath.write_register_for_test(cpu::Register::R2, 50);
    datapath.write_register_for_test(cpu::Register::R3, 10);
    datapath.write_register_for_test(cpu::Register::R5, 100);

    // Instruction 1: SUB R1, R2, R3 (R1 = 40)
    // Instruction 2: ADD R4, R5, R1 (R4 = 100 + R1 = 140) -> Hazard on operand B (rs2 = R1)
    std::vector<std::uint32_t> program = {
        encode_r_type(cpu::Opcode::SUB, cpu::Register::R1, cpu::Register::R2, cpu::Register::R3),
        encode_r_type(cpu::Opcode::ADD, cpu::Register::R4, cpu::Register::R5, cpu::Register::R1),
        encode_nop(),
        encode_nop(),
        encode_nop()
    };
    datapath.load_instructions(program);

    datapath.step(); // Cycle 1
    datapath.step(); // Cycle 2
    datapath.step(); // Cycle 3
    // Cycle 4: Instr 1 is in MEM, Instr 2 is in EX -> ForwardBE must be 10
    assert(datapath.forward_be().read_value() == 0b10);
    datapath.step(); // Cycle 4
    datapath.step(); // Cycle 5
    datapath.step(); // Cycle 6

    assert(datapath.read_register_for_test(cpu::Register::R4) == 140);
    std::cout << "  [PASS] ForwardBE forwarding resolved hazard correctly (R4 = 140).\n";
}

// Test 6: Load-Use Hazard Detection and Stall
void test_load_use_hazard_stall() {
    std::cout << "[Test 6] Testing Load-Use hazard detection and stall...\n";
    logic::Clock clock;
    logic::Wire reset(logic::LogicState::LOW);

    PipelinedDatapath datapath(clock, reset);

    // Set memory contents at address 4 = 42
    datapath.data_memory().load_rom({0, 0, 0, 0, 42});
    datapath.write_register_for_test(cpu::Register::R2, 4);
    datapath.write_register_for_test(cpu::Register::R5, 8);

    // Instruction 1: LW R1, R2, 0 (R1 = Mem[4] = 42)
    // Instruction 2: ADD R4, R1, R5 (R4 = 42 + 8 = 50) -> Load-use hazard!
    std::vector<std::uint32_t> program = {
        encode_i_type(cpu::Opcode::LW, cpu::Register::R1, cpu::Register::R2, 0),
        encode_r_type(cpu::Opcode::ADD, cpu::Register::R4, cpu::Register::R1, cpu::Register::R5),
        encode_nop(),
        encode_nop(),
        encode_nop(),
        encode_nop()
    };
    datapath.load_instructions(program);

    datapath.step(); // Cycle 1: Fetch LW
    datapath.step(); // Cycle 2: Decode LW, Fetch ADD
    // Cycle 3: Execute LW (MemtoReg = 1, WA3E = R1), Decode ADD (RA1D = R1)
    // Hazard unit should detect load-use hazard and assert StallF, StallD, FlushE!
    assert(datapath.stall_f().read() == logic::LogicState::HIGH);
    assert(datapath.stall_d().read() == logic::LogicState::HIGH);
    assert(datapath.flush_e().read() == logic::LogicState::HIGH);

    datapath.step(); // Cycle 3 -> Bubble inserted in EX, ADD re-decoded in ID
    datapath.step(); // Cycle 4 -> ADD executes with forwarded data from WB stage
    datapath.step(); // Cycle 5
    datapath.step(); // Cycle 6
    datapath.step(); // Cycle 7

    assert(datapath.read_register_for_test(cpu::Register::R1) == 42);
    assert(datapath.read_register_for_test(cpu::Register::R4) == 50);
    std::cout << "  [PASS] Load-use hazard correctly stalled and resolved (R4 = 50).\n";
}

// Test 7: Branch Instruction and Pipeline Flush
void test_branch_and_flush() {
    std::cout << "[Test 7] Testing branch resolution and pipeline flush...\n";
    logic::Clock clock;
    logic::Wire reset(logic::LogicState::LOW);

    PipelinedDatapath datapath(clock, reset);

    // Set R1 = 5, R2 = 5 (Equal)
    datapath.write_register_for_test(cpu::Register::R1, 5);
    datapath.write_register_for_test(cpu::Register::R2, 5);

    // Program:
    // 0: BEQ R1, R2, 2  (Branch taken! Target = 0 + 1 + 2 = 3)
    // 1: ADD R3, R1, R2 (In delay slot / fetched, must be FLUSHED!)
    // 2: ADD R4, R1, R2 (Must be FLUSHED!)
    // 3: ADD R5, R1, R2 (Branch target! R5 = 10)
    std::vector<std::uint32_t> program = {
        encode_b_type(cpu::Opcode::BEQ, cpu::Register::R1, cpu::Register::R2, 2),
        encode_r_type(cpu::Opcode::ADD, cpu::Register::R3, cpu::Register::R1, cpu::Register::R2),
        encode_r_type(cpu::Opcode::ADD, cpu::Register::R4, cpu::Register::R1, cpu::Register::R2),
        encode_r_type(cpu::Opcode::ADD, cpu::Register::R5, cpu::Register::R1, cpu::Register::R2),
        encode_nop(),
        encode_nop(),
        encode_nop()
    };
    datapath.load_instructions(program);

    datapath.step(); // Cycle 1: Fetch BEQ
    datapath.step(); // Cycle 2: Decode BEQ, Fetch ADD R3
    // Cycle 3: Execute BEQ (Zero = 1 -> Branch taken!)
    // Hazard unit should flush IF/ID and ID/EX!
    datapath.step(); // Cycle 3: Branch taken, flushes instructions 1 and 2
    datapath.step(); // Cycle 4: Fetch instruction at branch target (3)
    datapath.step(); // Cycle 5: Decode instruction 3
    datapath.step(); // Cycle 6: Execute instruction 3
    datapath.step(); // Cycle 7: Memory instruction 3
    datapath.step(); // Cycle 8: Writeback instruction 3

    // R3 and R4 should NOT have been updated (they were flushed)
    assert(datapath.read_register_for_test(cpu::Register::R3) == 0);
    assert(datapath.read_register_for_test(cpu::Register::R4) == 0);
    // R5 should be updated by target instruction (5 + 5 = 10)
    assert(datapath.read_register_for_test(cpu::Register::R5) == 10);
    std::cout << "  [PASS] Branch taken and pipeline flushed correctly (R3 = 0, R4 = 0, R5 = 10).\n";
}

} // namespace

int main() {
    std::cout << "====================================================\n";
    std::cout << "   RUNNING PIPELINED DATAPATH UNIT & INTEGRATION TESTS\n";
    std::cout << "====================================================\n";

    test_initialization();
    test_r_type_add_pipeline();
    test_mem_to_ex_forwarding();
    test_wb_to_ex_forwarding();
    test_forward_be_forwarding();
    test_load_use_hazard_stall();
    test_branch_and_flush();

    std::cout << "====================================================\n";
    std::cout << "   ALL PIPELINED DATAPATH TESTS PASSED SUCCESSFULLY!\n";
    std::cout << "====================================================\n";
    return 0;
}

#include "../../superscalar/core/decode/DecodeUnit.hpp"
#include "../../superscalar/core/decode/DecodeBundle.hpp"
#include "../../superscalar/core/decode/DecodeTypes.hpp"
#include "../../include/isa/Instruction.hpp"
#include "../../include/isa/Opcode.hpp"
#include "../../include/isa/InstructionFormat.hpp"
#include "../../include/isa/Registers.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>

namespace {

// ---------------------------------------------------------------------------
// Instruction Encoding Helpers
// ---------------------------------------------------------------------------

// R-type: [opcode:6][rd:4][rs1:4][rs2:4][unused:14]
constexpr uint32_t encode_r(cpu::Opcode op, cpu::Register rd, cpu::Register rs1, cpu::Register rs2)
{
    return (static_cast<uint32_t>(op) << 26) |
           ((static_cast<uint32_t>(rd) & 0x0F) << 22) |
           ((static_cast<uint32_t>(rs1) & 0x0F) << 18) |
           ((static_cast<uint32_t>(rs2) & 0x0F) << 14);
}

// I-type: [opcode:6][rd:4][rs1:4][immediate:18]
constexpr uint32_t encode_i(cpu::Opcode op, cpu::Register rd, cpu::Register rs1, int32_t imm)
{
    return (static_cast<uint32_t>(op) << 26) |
           ((static_cast<uint32_t>(rd) & 0x0F) << 22) |
           ((static_cast<uint32_t>(rs1) & 0x0F) << 18) |
           (static_cast<uint32_t>(imm) & 0x03FFFF);
}

// S-type: [opcode:6][rs2:4][rs1:4][immediate:18]
// Note: bits 25..22 store rs2, bits 21..18 store rs1
constexpr uint32_t encode_s(cpu::Opcode op, cpu::Register rs2, cpu::Register rs1, int32_t imm)
{
    return (static_cast<uint32_t>(op) << 26) |
           ((static_cast<uint32_t>(rs2) & 0x0F) << 22) |
           ((static_cast<uint32_t>(rs1) & 0x0F) << 18) |
           (static_cast<uint32_t>(imm) & 0x03FFFF);
}

// B-type: [opcode:6][rs1:4][rs2:4][immediate:18]
// Note: bits 25..22 store rs1, bits 21..18 store rs2
constexpr uint32_t encode_b(cpu::Opcode op, cpu::Register rs1, cpu::Register rs2, int32_t imm)
{
    return (static_cast<uint32_t>(op) << 26) |
           ((static_cast<uint32_t>(rs1) & 0x0F) << 22) |
           ((static_cast<uint32_t>(rs2) & 0x0F) << 18) |
           (static_cast<uint32_t>(imm) & 0x03FFFF);
}

// J-type: [opcode:6][immediate:26]
constexpr uint32_t encode_j(cpu::Opcode op, int32_t imm)
{
    return (static_cast<uint32_t>(op) << 26) |
           (static_cast<uint32_t>(imm) & 0x03FFFFFF);
}

const cpu::DecodeUnit decoder{};

// ---------------------------------------------------------------------------
// 1. Single-instruction decode tests: R-type
// ---------------------------------------------------------------------------

void test_add()
{
    // ADD R3, R1, R2
    const uint32_t raw = encode_r(cpu::Opcode::ADD, cpu::Register::R3, cpu::Register::R1, cpu::Register::R2);
    const cpu::DecodeBundle b = decoder.decode(0x100, raw, true);

    assert(b.valid == true);
    assert(b.pc == 0x100);
    assert(b.instruction == raw);
    assert(b.opcode == cpu::Opcode::ADD);
    assert(b.format == cpu::InstructionFormat::R_TYPE);
    assert(b.rd == cpu::Register::R3);
    assert(b.rs1 == cpu::Register::R1);
    assert(b.rs2 == cpu::Register::R2);
    assert(b.immediate == 0);
    assert(b.alu_operation == cpu::ALUOperation::ADD);
    assert(b.operand_a == cpu::OperandSource::REGISTER);
    assert(b.operand_b == cpu::OperandSource::REGISTER);
    assert(b.register_write == true);
    assert(b.memory_read == false);
    assert(b.memory_write == false);
    assert(b.control_flow == cpu::ControlFlow::NONE);
    assert(b.halt == false);

    std::cout << "  [PASS] test_add: ADD R3, R1, R2 decoded correctly.\n";
}

void test_sub()
{
    // SUB R6, R4, R5
    const uint32_t raw = encode_r(cpu::Opcode::SUB, cpu::Register::R6, cpu::Register::R4, cpu::Register::R5);
    const cpu::DecodeBundle b = decoder.decode(0x104, raw, true);

    assert(b.valid == true);
    assert(b.pc == 0x104);
    assert(b.instruction == raw);
    assert(b.opcode == cpu::Opcode::SUB);
    assert(b.format == cpu::InstructionFormat::R_TYPE);
    assert(b.rd == cpu::Register::R6);
    assert(b.rs1 == cpu::Register::R4);
    assert(b.rs2 == cpu::Register::R5);
    assert(b.immediate == 0);
    assert(b.alu_operation == cpu::ALUOperation::SUB);
    assert(b.operand_a == cpu::OperandSource::REGISTER);
    assert(b.operand_b == cpu::OperandSource::REGISTER);
    assert(b.register_write == true);
    assert(b.memory_read == false);
    assert(b.memory_write == false);
    assert(b.control_flow == cpu::ControlFlow::NONE);
    assert(b.halt == false);

    std::cout << "  [PASS] test_sub: SUB R6, R4, R5 decoded correctly.\n";
}

void test_and()
{
    // AND R7, R1, R2
    const uint32_t raw = encode_r(cpu::Opcode::AND, cpu::Register::R7, cpu::Register::R1, cpu::Register::R2);
    const cpu::DecodeBundle b = decoder.decode(0x108, raw, true);

    assert(b.valid == true);
    assert(b.pc == 0x108);
    assert(b.instruction == raw);
    assert(b.opcode == cpu::Opcode::AND);
    assert(b.format == cpu::InstructionFormat::R_TYPE);
    assert(b.rd == cpu::Register::R7);
    assert(b.rs1 == cpu::Register::R1);
    assert(b.rs2 == cpu::Register::R2);
    assert(b.immediate == 0);
    assert(b.alu_operation == cpu::ALUOperation::AND);
    assert(b.operand_a == cpu::OperandSource::REGISTER);
    assert(b.operand_b == cpu::OperandSource::REGISTER);
    assert(b.register_write == true);
    assert(b.memory_read == false);
    assert(b.memory_write == false);
    assert(b.control_flow == cpu::ControlFlow::NONE);
    assert(b.halt == false);

    std::cout << "  [PASS] test_and: AND R7, R1, R2 decoded correctly.\n";
}

void test_or()
{
    // OR R8, R3, R4
    const uint32_t raw = encode_r(cpu::Opcode::OR, cpu::Register::R8, cpu::Register::R3, cpu::Register::R4);
    const cpu::DecodeBundle b = decoder.decode(0x10C, raw, true);

    assert(b.valid == true);
    assert(b.pc == 0x10C);
    assert(b.instruction == raw);
    assert(b.opcode == cpu::Opcode::OR);
    assert(b.format == cpu::InstructionFormat::R_TYPE);
    assert(b.rd == cpu::Register::R8);
    assert(b.rs1 == cpu::Register::R3);
    assert(b.rs2 == cpu::Register::R4);
    assert(b.immediate == 0);
    assert(b.alu_operation == cpu::ALUOperation::OR);
    assert(b.operand_a == cpu::OperandSource::REGISTER);
    assert(b.operand_b == cpu::OperandSource::REGISTER);
    assert(b.register_write == true);
    assert(b.memory_read == false);
    assert(b.memory_write == false);
    assert(b.control_flow == cpu::ControlFlow::NONE);
    assert(b.halt == false);

    std::cout << "  [PASS] test_or: OR R8, R3, R4 decoded correctly.\n";
}

void test_xor()
{
    // XOR R9, R5, R6
    const uint32_t raw = encode_r(cpu::Opcode::XOR, cpu::Register::R9, cpu::Register::R5, cpu::Register::R6);
    const cpu::DecodeBundle b = decoder.decode(0x110, raw, true);

    assert(b.valid == true);
    assert(b.pc == 0x110);
    assert(b.instruction == raw);
    assert(b.opcode == cpu::Opcode::XOR);
    assert(b.format == cpu::InstructionFormat::R_TYPE);
    assert(b.rd == cpu::Register::R9);
    assert(b.rs1 == cpu::Register::R5);
    assert(b.rs2 == cpu::Register::R6);
    assert(b.immediate == 0);
    assert(b.alu_operation == cpu::ALUOperation::XOR);
    assert(b.operand_a == cpu::OperandSource::REGISTER);
    assert(b.operand_b == cpu::OperandSource::REGISTER);
    assert(b.register_write == true);
    assert(b.memory_read == false);
    assert(b.memory_write == false);
    assert(b.control_flow == cpu::ControlFlow::NONE);
    assert(b.halt == false);

    std::cout << "  [PASS] test_xor: XOR R9, R5, R6 decoded correctly.\n";
}

void test_not()
{
    // NOT R10, R7 (R-type unary logical: rd, rs1, rs2=R0)
    const uint32_t raw = encode_r(cpu::Opcode::NOT, cpu::Register::R10, cpu::Register::R7, cpu::Register::R0);
    const cpu::DecodeBundle b = decoder.decode(0x114, raw, true);

    assert(b.valid == true);
    assert(b.pc == 0x114);
    assert(b.instruction == raw);
    assert(b.opcode == cpu::Opcode::NOT);
    assert(b.format == cpu::InstructionFormat::R_TYPE);
    assert(b.rd == cpu::Register::R10);
    assert(b.rs1 == cpu::Register::R7);
    assert(b.rs2 == cpu::Register::R0);
    assert(b.immediate == 0);
    assert(b.alu_operation == cpu::ALUOperation::NOT);
    assert(b.operand_a == cpu::OperandSource::REGISTER);
    assert(b.operand_b == cpu::OperandSource::NONE);
    assert(b.register_write == true);
    assert(b.memory_read == false);
    assert(b.memory_write == false);
    assert(b.control_flow == cpu::ControlFlow::NONE);
    assert(b.halt == false);

    std::cout << "  [PASS] test_not: NOT R10, R7 decoded correctly.\n";
}

// ---------------------------------------------------------------------------
// 2. I-type tests: LI, ADDI, LW, NOP, HALT
// ---------------------------------------------------------------------------

void test_li()
{
    // LI R3, 42
    const uint32_t raw = encode_i(cpu::Opcode::LI, cpu::Register::R3, cpu::Register::R0, 42);
    const cpu::DecodeBundle b = decoder.decode(0x118, raw, true);

    assert(b.valid == true);
    assert(b.pc == 0x118);
    assert(b.instruction == raw);
    assert(b.opcode == cpu::Opcode::LI);
    assert(b.format == cpu::InstructionFormat::I_TYPE);
    assert(b.rd == cpu::Register::R3);
    assert(b.rs1 == cpu::Register::R0);
    assert(b.rs2 == cpu::Register::R0);
    assert(b.immediate == 42);
    assert(b.alu_operation == cpu::ALUOperation::ADD);
    assert(b.operand_a == cpu::OperandSource::NONE);
    assert(b.operand_b == cpu::OperandSource::IMMEDIATE);
    assert(b.register_write == true);
    assert(b.memory_read == false);
    assert(b.memory_write == false);
    assert(b.control_flow == cpu::ControlFlow::NONE);
    assert(b.halt == false);

    std::cout << "  [PASS] test_li: LI R3, 42 decoded correctly.\n";
}

void test_addi()
{
    // ADDI R3, R1, 42
    const uint32_t raw = encode_i(cpu::Opcode::ADDI, cpu::Register::R3, cpu::Register::R1, 42);
    const cpu::DecodeBundle b = decoder.decode(0x11C, raw, true);

    assert(b.valid == true);
    assert(b.pc == 0x11C);
    assert(b.instruction == raw);
    assert(b.opcode == cpu::Opcode::ADDI);
    assert(b.format == cpu::InstructionFormat::I_TYPE);
    assert(b.rd == cpu::Register::R3);
    assert(b.rs1 == cpu::Register::R1);
    assert(b.rs2 == cpu::Register::R0);
    assert(b.immediate == 42);
    assert(b.alu_operation == cpu::ALUOperation::ADD);
    assert(b.operand_a == cpu::OperandSource::REGISTER);
    assert(b.operand_b == cpu::OperandSource::IMMEDIATE);
    assert(b.register_write == true);
    assert(b.memory_read == false);
    assert(b.memory_write == false);
    assert(b.control_flow == cpu::ControlFlow::NONE);
    assert(b.halt == false);

    std::cout << "  [PASS] test_addi: ADDI R3, R1, 42 decoded correctly.\n";
}

void test_lw()
{
    // LW R3, R1, 16
    const uint32_t raw = encode_i(cpu::Opcode::LW, cpu::Register::R3, cpu::Register::R1, 16);
    const cpu::DecodeBundle b = decoder.decode(0x120, raw, true);

    assert(b.valid == true);
    assert(b.pc == 0x120);
    assert(b.instruction == raw);
    assert(b.opcode == cpu::Opcode::LW);
    assert(b.format == cpu::InstructionFormat::I_TYPE);
    assert(b.rd == cpu::Register::R3);
    assert(b.rs1 == cpu::Register::R1);
    assert(b.rs2 == cpu::Register::R0);
    assert(b.immediate == 16);
    assert(b.alu_operation == cpu::ALUOperation::ADD);
    assert(b.operand_a == cpu::OperandSource::REGISTER);
    assert(b.operand_b == cpu::OperandSource::IMMEDIATE);
    assert(b.register_write == true);
    assert(b.memory_read == true);
    assert(b.memory_write == false);
    assert(b.control_flow == cpu::ControlFlow::NONE);
    assert(b.halt == false);

    std::cout << "  [PASS] test_lw: LW R3, R1, 16 decoded correctly.\n";
}

void test_nop()
{
    // NOP (0x00000000)
    const uint32_t raw = 0x00000000;
    const cpu::DecodeBundle b = decoder.decode(0x124, raw, true);

    assert(b.valid == true);
    assert(b.pc == 0x124);
    assert(b.instruction == raw);
    assert(b.opcode == cpu::Opcode::NOP);
    assert(b.format == cpu::InstructionFormat::I_TYPE);
    assert(b.rd == cpu::Register::R0);
    assert(b.rs1 == cpu::Register::R0);
    assert(b.rs2 == cpu::Register::R0);
    assert(b.immediate == 0);
    assert(b.alu_operation == cpu::ALUOperation::NONE);
    assert(b.operand_a == cpu::OperandSource::NONE);
    assert(b.operand_b == cpu::OperandSource::NONE);
    assert(b.register_write == false);
    assert(b.memory_read == false);
    assert(b.memory_write == false);
    assert(b.control_flow == cpu::ControlFlow::NONE);
    assert(b.halt == false);

    std::cout << "  [PASS] test_nop: NOP decoded correctly.\n";
}

void test_halt()
{
    // HALT
    const uint32_t raw = encode_i(cpu::Opcode::HALT, cpu::Register::R0, cpu::Register::R0, 0);
    const cpu::DecodeBundle b = decoder.decode(0x128, raw, true);

    assert(b.valid == true);
    assert(b.pc == 0x128);
    assert(b.instruction == raw);
    assert(b.opcode == cpu::Opcode::HALT);
    assert(b.format == cpu::InstructionFormat::I_TYPE);
    assert(b.halt == true);
    assert(b.register_write == false);
    assert(b.memory_read == false);
    assert(b.memory_write == false);
    assert(b.control_flow == cpu::ControlFlow::NONE);

    std::cout << "  [PASS] test_halt: HALT decoded correctly.\n";
}

// ---------------------------------------------------------------------------
// 3. S-type test: SW rs2, rs1, immediate
// ---------------------------------------------------------------------------

void test_sw()
{
    // SW R5, R2, 100
    // bits 25..22 -> rs2 (R5)
    // bits 21..18 -> rs1 (R2)
    // bits 17..0  -> immediate (100)
    const uint32_t raw = encode_s(cpu::Opcode::SW, cpu::Register::R5, cpu::Register::R2, 100);
    const cpu::DecodeBundle b = decoder.decode(0x12C, raw, true);

    assert(b.valid == true);
    assert(b.pc == 0x12C);
    assert(b.instruction == raw);
    assert(b.opcode == cpu::Opcode::SW);
    assert(b.format == cpu::InstructionFormat::S_TYPE);
    assert(b.rd == cpu::Register::R0);
    assert(b.rs1 == cpu::Register::R2);
    assert(b.rs2 == cpu::Register::R5);
    assert(b.immediate == 100);
    assert(b.alu_operation == cpu::ALUOperation::ADD);
    assert(b.operand_a == cpu::OperandSource::REGISTER);
    assert(b.operand_b == cpu::OperandSource::IMMEDIATE);
    assert(b.register_write == false);
    assert(b.memory_read == false);
    assert(b.memory_write == true);
    assert(b.control_flow == cpu::ControlFlow::NONE);
    assert(b.halt == false);

    std::cout << "  [PASS] test_sw: SW R5, R2, 100 decoded correctly with proper field mapping.\n";
}

// ---------------------------------------------------------------------------
// 4. Branch tests: BEQ, BNE
// ---------------------------------------------------------------------------

void test_beq()
{
    // BEQ R1, R2, 20
    const uint32_t raw = encode_b(cpu::Opcode::BEQ, cpu::Register::R1, cpu::Register::R2, 20);
    const cpu::DecodeBundle b = decoder.decode(0x130, raw, true);

    assert(b.valid == true);
    assert(b.pc == 0x130);
    assert(b.instruction == raw);
    assert(b.opcode == cpu::Opcode::BEQ);
    assert(b.format == cpu::InstructionFormat::B_TYPE);
    assert(b.rd == cpu::Register::R0);
    assert(b.rs1 == cpu::Register::R1);
    assert(b.rs2 == cpu::Register::R2);
    assert(b.immediate == 20);
    assert(b.alu_operation == cpu::ALUOperation::SUB);
    assert(b.operand_a == cpu::OperandSource::REGISTER);
    assert(b.operand_b == cpu::OperandSource::REGISTER);
    assert(b.control_flow == cpu::ControlFlow::BRANCH);
    assert(b.register_write == false);
    assert(b.memory_read == false);
    assert(b.memory_write == false);
    assert(b.halt == false);

    std::cout << "  [PASS] test_beq: BEQ R1, R2, 20 decoded correctly.\n";
}

void test_bne()
{
    // BNE R1, R2, 20
    const uint32_t raw = encode_b(cpu::Opcode::BNE, cpu::Register::R1, cpu::Register::R2, 20);
    const cpu::DecodeBundle b = decoder.decode(0x134, raw, true);

    assert(b.valid == true);
    assert(b.pc == 0x134);
    assert(b.instruction == raw);
    assert(b.opcode == cpu::Opcode::BNE);
    assert(b.format == cpu::InstructionFormat::B_TYPE);
    assert(b.rd == cpu::Register::R0);
    assert(b.rs1 == cpu::Register::R1);
    assert(b.rs2 == cpu::Register::R2);
    assert(b.immediate == 20);
    assert(b.alu_operation == cpu::ALUOperation::SUB);
    assert(b.operand_a == cpu::OperandSource::REGISTER);
    assert(b.operand_b == cpu::OperandSource::REGISTER);
    assert(b.control_flow == cpu::ControlFlow::BRANCH);
    assert(b.register_write == false);
    assert(b.memory_read == false);
    assert(b.memory_write == false);
    assert(b.halt == false);

    std::cout << "  [PASS] test_bne: BNE R1, R2, 20 decoded correctly.\n";
}

// ---------------------------------------------------------------------------
// 5. Jump tests: J 100, J -100
// ---------------------------------------------------------------------------

void test_jump()
{
    // J 100
    {
        const uint32_t raw = encode_j(cpu::Opcode::J, 100);
        const cpu::DecodeBundle b = decoder.decode(0x138, raw, true);

        assert(b.valid == true);
        assert(b.pc == 0x138);
        assert(b.instruction == raw);
        assert(b.opcode == cpu::Opcode::J);
        assert(b.format == cpu::InstructionFormat::J_TYPE);
        assert(b.rd == cpu::Register::R0);
        assert(b.rs1 == cpu::Register::R0);
        assert(b.rs2 == cpu::Register::R0);
        assert(b.immediate == 100);
        assert(b.control_flow == cpu::ControlFlow::JUMP);
        assert(b.register_write == false);
        assert(b.memory_read == false);
        assert(b.memory_write == false);
        assert(b.halt == false);
    }

    // J -100 (26-bit sign extension)
    {
        const uint32_t raw = encode_j(cpu::Opcode::J, -100);
        const cpu::DecodeBundle b = decoder.decode(0x13C, raw, true);

        assert(b.valid == true);
        assert(b.pc == 0x13C);
        assert(b.instruction == raw);
        assert(b.opcode == cpu::Opcode::J);
        assert(b.format == cpu::InstructionFormat::J_TYPE);
        assert(b.rd == cpu::Register::R0);
        assert(b.rs1 == cpu::Register::R0);
        assert(b.rs2 == cpu::Register::R0);
        assert(b.immediate == -100);
        assert(b.control_flow == cpu::ControlFlow::JUMP);
        assert(b.register_write == false);
        assert(b.memory_read == false);
        assert(b.memory_write == false);
        assert(b.halt == false);
    }

    std::cout << "  [PASS] test_jump: J 100 and J -100 decoded correctly.\n";
}

// ---------------------------------------------------------------------------
// 6. Immediate boundary tests
// ---------------------------------------------------------------------------

void test_positive_immediate()
{
    // Positive immediate test with ADDI and branch
    const uint32_t raw_addi = encode_i(cpu::Opcode::ADDI, cpu::Register::R3, cpu::Register::R1, 4096);
    const cpu::DecodeBundle b_addi = decoder.decode(0x140, raw_addi, true);
    assert(b_addi.immediate == 4096);

    const uint32_t raw_beq = encode_b(cpu::Opcode::BEQ, cpu::Register::R1, cpu::Register::R2, 1024);
    const cpu::DecodeBundle b_beq = decoder.decode(0x144, raw_beq, true);
    assert(b_beq.immediate == 1024);

    std::cout << "  [PASS] test_positive_immediate: Positive immediates preserved correctly.\n";
}

void test_negative_immediate()
{
    // Negative branch offset: BEQ R1, R2, -16
    const uint32_t raw_beq = encode_b(cpu::Opcode::BEQ, cpu::Register::R1, cpu::Register::R2, -16);
    const cpu::DecodeBundle b_beq = decoder.decode(0x148, raw_beq, true);

    assert(b_beq.immediate == -16);
    assert(b_beq.control_flow == cpu::ControlFlow::BRANCH);

    // Negative branch offset: BNE R1, R2, -32
    const uint32_t raw_bne = encode_b(cpu::Opcode::BNE, cpu::Register::R1, cpu::Register::R2, -32);
    const cpu::DecodeBundle b_bne = decoder.decode(0x14C, raw_bne, true);

    assert(b_bne.immediate == -32);
    assert(b_bne.control_flow == cpu::ControlFlow::BRANCH);

    // Negative I-type immediate: ADDI R3, R1, -50
    const uint32_t raw_addi = encode_i(cpu::Opcode::ADDI, cpu::Register::R3, cpu::Register::R1, -50);
    const cpu::DecodeBundle b_addi = decoder.decode(0x150, raw_addi, true);

    assert(b_addi.immediate == -50);

    std::cout << "  [PASS] test_negative_immediate: Negative 18-bit immediates sign-extended correctly.\n";
}

void test_immediate_boundaries()
{
    // 18-bit signed immediate range: [-131072, 131071]
    const int32_t test_values[] = {
        131071,   // maximum (2^17 - 1)
        131070,   // maximum - 1
        -131072,  // minimum (-2^17)
        -131071,  // minimum + 1
        0,
        1,
        -1
    };

    for (const int32_t val : test_values)
    {
        const uint32_t raw = encode_i(cpu::Opcode::ADDI, cpu::Register::R1, cpu::Register::R2, val);
        const cpu::DecodeBundle b = decoder.decode(0x200, raw, true);

        assert(b.immediate == val);
        assert(b.rd == cpu::Register::R1);
        assert(b.rs1 == cpu::Register::R2);
    }

    std::cout << "  [PASS] test_immediate_boundaries: 18-bit boundary values (-131072 to 131071) verified.\n";
}

void test_jump_immediate()
{
    // 26-bit signed jump immediate range: [-33554432, 33554431]
    const int32_t j_values[] = {
        33554431,   // maximum (2^25 - 1)
        33554430,   // maximum - 1
        -33554432,  // minimum (-2^25)
        -33554431,  // minimum + 1
        0,
        1,
        -1
    };

    for (const int32_t val : j_values)
    {
        const uint32_t raw = encode_j(cpu::Opcode::J, val);
        const cpu::DecodeBundle b = decoder.decode(0x300, raw, true);

        assert(b.format == cpu::InstructionFormat::J_TYPE);
        assert(b.control_flow == cpu::ControlFlow::JUMP);
        assert(b.immediate == val);
    }

    std::cout << "  [PASS] test_jump_immediate: 26-bit jump boundary values (-33554432 to 33554431) verified.\n";
}

// ---------------------------------------------------------------------------
// 7. Register-field tests: R0, R1, R7, R8, R15
// ---------------------------------------------------------------------------

void test_register_boundaries()
{
    // Verify 4-bit register extraction at boundaries: R0, R1, R7, R8, R15
    {
        // ADD R15, R14, R13
        const uint32_t raw = encode_r(cpu::Opcode::ADD, cpu::Register::R15, cpu::Register::R14, cpu::Register::R13);
        const cpu::DecodeBundle b = decoder.decode(0x160, raw, true);

        assert(b.rd == cpu::Register::R15);
        assert(b.rs1 == cpu::Register::R14);
        assert(b.rs2 == cpu::Register::R13);
    }

    {
        // ADD R0, R1, R7
        const uint32_t raw = encode_r(cpu::Opcode::ADD, cpu::Register::R0, cpu::Register::R1, cpu::Register::R7);
        const cpu::DecodeBundle b = decoder.decode(0x164, raw, true);

        assert(b.rd == cpu::Register::R0);
        assert(b.rs1 == cpu::Register::R1);
        assert(b.rs2 == cpu::Register::R7);
    }

    {
        // SUB R8, R15, R0
        const uint32_t raw = encode_r(cpu::Opcode::SUB, cpu::Register::R8, cpu::Register::R15, cpu::Register::R0);
        const cpu::DecodeBundle b = decoder.decode(0x168, raw, true);

        assert(b.rd == cpu::Register::R8);
        assert(b.rs1 == cpu::Register::R15);
        assert(b.rs2 == cpu::Register::R0);
    }

    std::cout << "  [PASS] test_register_boundaries: 4-bit register boundaries (R0..R15) decoded accurately.\n";
}

// ---------------------------------------------------------------------------
// 8. Bubble tests: decode with valid = false
// ---------------------------------------------------------------------------

void test_invalid_lane()
{
    // Pass raw bytes of ADD instruction, but valid = false (bubble)
    const uint32_t raw = encode_r(cpu::Opcode::ADD, cpu::Register::R3, cpu::Register::R1, cpu::Register::R2);
    const cpu::DecodeBundle b = decoder.decode(0x100, raw, false);

    assert(b.valid == false);
    assert(b.pc == 0x100);
    assert(b.instruction == raw);

    // Verify safe defaults
    assert(b.opcode == cpu::Opcode::NOP);
    assert(b.format == cpu::InstructionFormat::I_TYPE);
    assert(b.rd == cpu::Register::R0);
    assert(b.rs1 == cpu::Register::R0);
    assert(b.rs2 == cpu::Register::R0);
    assert(b.immediate == 0);
    assert(b.alu_operation == cpu::ALUOperation::NONE);
    assert(b.operand_a == cpu::OperandSource::NONE);
    assert(b.operand_b == cpu::OperandSource::NONE);
    assert(b.register_write == false);
    assert(b.memory_read == false);
    assert(b.memory_write == false);
    assert(b.control_flow == cpu::ControlFlow::NONE);
    assert(b.halt == false);

    std::cout << "  [PASS] test_invalid_lane: Bubble produces valid == false and safe defaults.\n";
}

// ---------------------------------------------------------------------------
// 9. Two-wide tests
// ---------------------------------------------------------------------------

void test_two_wide_decode()
{
    // Lane 0: ADD R3, R1, R2 at PC = 0x100
    // Lane 1: SUB R6, R4, R5 at PC = 0x104
    const uint32_t raw0 = encode_r(cpu::Opcode::ADD, cpu::Register::R3, cpu::Register::R1, cpu::Register::R2);
    const uint32_t raw1 = encode_r(cpu::Opcode::SUB, cpu::Register::R6, cpu::Register::R4, cpu::Register::R5);

    cpu::DecodeBundle b0{};
    cpu::DecodeBundle b1{};

    decoder.decode(
        0x100, raw0, true,
        0x104, raw1, true,
        b0, b1
    );

    // Verify Lane 0
    assert(b0.valid == true);
    assert(b0.pc == 0x100);
    assert(b0.instruction == raw0);
    assert(b0.opcode == cpu::Opcode::ADD);
    assert(b0.rd == cpu::Register::R3);
    assert(b0.rs1 == cpu::Register::R1);
    assert(b0.rs2 == cpu::Register::R2);
    assert(b0.alu_operation == cpu::ALUOperation::ADD);
    assert(b0.register_write == true);

    // Verify Lane 1
    assert(b1.valid == true);
    assert(b1.pc == 0x104);
    assert(b1.instruction == raw1);
    assert(b1.opcode == cpu::Opcode::SUB);
    assert(b1.rd == cpu::Register::R6);
    assert(b1.rs1 == cpu::Register::R4);
    assert(b1.rs2 == cpu::Register::R5);
    assert(b1.alu_operation == cpu::ALUOperation::SUB);
    assert(b1.register_write == true);

    std::cout << "  [PASS] test_two_wide_decode: Both lanes decoded simultaneously and independently.\n";
}

void test_lane0_valid_lane1_invalid()
{
    // Lane 0: valid ADD instruction at PC = 0x100
    // Lane 1: invalid (bubble) at PC = 0x104
    const uint32_t raw0 = encode_r(cpu::Opcode::ADD, cpu::Register::R3, cpu::Register::R1, cpu::Register::R2);
    const uint32_t raw1 = encode_r(cpu::Opcode::SUB, cpu::Register::R6, cpu::Register::R4, cpu::Register::R5);

    cpu::DecodeBundle b0{};
    cpu::DecodeBundle b1{};

    decoder.decode(
        0x100, raw0, true,
        0x104, raw1, false,
        b0, b1
    );

    // Lane 0 is valid and decoded
    assert(b0.valid == true);
    assert(b0.pc == 0x100);
    assert(b0.instruction == raw0);
    assert(b0.opcode == cpu::Opcode::ADD);
    assert(b0.rd == cpu::Register::R3);
    assert(b0.register_write == true);

    // Lane 1 is a bubble with safe defaults
    assert(b1.valid == false);
    assert(b1.pc == 0x104);
    assert(b1.instruction == raw1);
    assert(b1.opcode == cpu::Opcode::NOP);
    assert(b1.rd == cpu::Register::R0);
    assert(b1.register_write == false);

    std::cout << "  [PASS] test_lane0_valid_lane1_invalid: Lane 0 decoded while Lane 1 is safe bubble.\n";
}

void test_lane0_invalid_lane1_valid()
{
    // Lane 0: invalid (bubble) at PC = 0x100
    // Lane 1: valid SUB instruction at PC = 0x104
    const uint32_t raw0 = encode_r(cpu::Opcode::ADD, cpu::Register::R3, cpu::Register::R1, cpu::Register::R2);
    const uint32_t raw1 = encode_r(cpu::Opcode::SUB, cpu::Register::R6, cpu::Register::R4, cpu::Register::R5);

    cpu::DecodeBundle b0{};
    cpu::DecodeBundle b1{};

    decoder.decode(
        0x100, raw0, false,
        0x104, raw1, true,
        b0, b1
    );

    // Lane 0 is a bubble with safe defaults
    assert(b0.valid == false);
    assert(b0.pc == 0x100);
    assert(b0.instruction == raw0);
    assert(b0.opcode == cpu::Opcode::NOP);
    assert(b0.rd == cpu::Register::R0);
    assert(b0.register_write == false);

    // Lane 1 is valid and decoded
    assert(b1.valid == true);
    assert(b1.pc == 0x104);
    assert(b1.instruction == raw1);
    assert(b1.opcode == cpu::Opcode::SUB);
    assert(b1.rd == cpu::Register::R6);
    assert(b1.rs1 == cpu::Register::R4);
    assert(b1.rs2 == cpu::Register::R5);
    assert(b1.alu_operation == cpu::ALUOperation::SUB);
    assert(b1.register_write == true);

    std::cout << "  [PASS] test_lane0_invalid_lane1_valid: Lane 0 bubble doesn't impede Lane 1 decode.\n";
}

} // namespace

int main()
{
    std::cout << "=========================================================\n";
    std::cout << "--- Testing Superscalar Decode Unit (2-wide & ISA) ---\n";
    std::cout << "=========================================================\n";

    test_add();
    test_sub();
    test_and();
    test_or();
    test_xor();
    test_not();

    test_li();
    test_addi();
    test_lw();
    test_nop();
    test_halt();

    test_sw();

    test_beq();
    test_bne();

    test_jump();

    test_positive_immediate();
    test_negative_immediate();
    test_immediate_boundaries();
    test_jump_immediate();

    test_register_boundaries();

    test_invalid_lane();

    test_two_wide_decode();
    test_lane0_valid_lane1_invalid();
    test_lane0_invalid_lane1_valid();

    std::cout << "\n[PASS] All Decode Unit tests passed successfully!\n";
    return 0;
}

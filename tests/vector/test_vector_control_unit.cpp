/*
    test_vector_control_unit.cpp

    Comprehensive unit test suite for VectorControlUnit:
    1. Instruction field extraction
    2. Supported operation decoding (funct3: 000..100)
    3. Unsupported operation decoding (funct3: 101..111)
    4. Invalid major opcodes
    5. Reset behavior
    6. Sequential instruction changes (stale signal checks)
    7. Exhaustive 1,024-case opcode and funct3 validation (128 opcodes x 8 funct3)
*/

#include "../vector_cpu/core/control/VectorControlUnit.hpp"

#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/signals/logicState.hpp>
#include <iostream>
#include <cassert>
#include <cstdint>
#include <string>

// Helper to construct a 32-bit instruction word:
// [31:27] rd (5)
// [26:22] rs1 (5)
// [21:17] rs2 (5)
// [16:14] funct3 (3)
// [13:7]  reserved (7)
// [6:0]   opcode (7)
static std::uint32_t encode_instruction(
    std::uint32_t rd,
    std::uint32_t rs1,
    std::uint32_t rs2,
    std::uint32_t funct3,
    std::uint32_t opcode,
    std::uint32_t reserved = 0)
{
    std::uint32_t inst = 0;
    inst |= (opcode & 0x7F);
    inst |= ((reserved & 0x7F) << 7);
    inst |= ((funct3 & 0x07) << 14);
    inst |= ((rs2 & 0x1F) << 17);
    inst |= ((rs1 & 0x1F) << 22);
    inst |= ((rd & 0x1F) << 27);
    return inst;
}

// -----------------------------------------------------------------------------
// Test 1: Instruction field extraction
// -----------------------------------------------------------------------------
static void test_field_extraction()
{
    std::cout << "--- Test 1: Instruction Field Extraction ---\n";

    logic::Bus<32> instruction;
    logic::Wire reset(logic::LogicState::LOW);
    logic::Bus<5> rs1, rs2, rd;
    logic::Bus<7> opcode;
    logic::Bus<3> funct3;
    logic::Bus<3> alu_opcode;
    logic::Wire write_enable;
    logic::Wire valid_instruction;

    cpu::VectorControlUnit<> unit(
        instruction, reset, rs1, rs2, rd, opcode, funct3, alu_opcode, write_enable, valid_instruction
    );

    // Target values: rd=10, rs1=5, rs2=7, funct3=0, opcode=0b1010111
    std::uint32_t raw = encode_instruction(10, 5, 7, 0, 0b1010111);
    instruction.write_value(raw);
    reset.write(logic::LogicState::LOW);

    unit.evaluate();

    std::cout << "  rd: " << rd.read_value() << " (expected 10)\n";
    std::cout << "  rs1: " << rs1.read_value() << " (expected 5)\n";
    std::cout << "  rs2: " << rs2.read_value() << " (expected 7)\n";
    std::cout << "  funct3: " << funct3.read_value() << " (expected 0)\n";
    std::cout << "  opcode: 0b" << std::to_string(opcode.read_value()) << " (expected 0b1010111 / 87)\n";

    assert(rd.read_value() == 10);
    assert(rs1.read_value() == 5);
    assert(rs2.read_value() == 7);
    assert(funct3.read_value() == 0);
    assert(opcode.read_value() == 0b1010111);

    std::cout << "  [PASS] Test 1 passed successfully.\n\n";
}

// -----------------------------------------------------------------------------
// Test 2: Supported operation decoding
// -----------------------------------------------------------------------------
static void test_supported_operations()
{
    std::cout << "--- Test 2: Supported Operation Decoding ---\n";

    logic::Bus<32> instruction;
    logic::Wire reset(logic::LogicState::LOW);
    logic::Bus<5> rs1, rs2, rd;
    logic::Bus<7> opcode;
    logic::Bus<3> funct3;
    logic::Bus<3> alu_opcode;
    logic::Wire write_enable;
    logic::Wire valid_instruction;

    cpu::VectorControlUnit<> unit(
        instruction, reset, rs1, rs2, rd, opcode, funct3, alu_opcode, write_enable, valid_instruction
    );

    struct OpCase {
        std::uint32_t funct3_val;
        const char* name;
    };

    const OpCase operations[] = {
        {0b000, "VADD"},
        {0b001, "VSUB"},
        {0b010, "VAND"},
        {0b011, "VOR"},
        {0b100, "VXOR"}
    };

    for (const auto& op : operations)
    {
        std::uint32_t raw = encode_instruction(1, 2, 3, op.funct3_val, 0b1010111);
        instruction.write_value(raw);
        reset.write(logic::LogicState::LOW);

        unit.evaluate();

        std::cout << "  Checking " << op.name << " (funct3=" << op.funct3_val << "): "
                  << "valid=" << (valid_instruction.read() == logic::LogicState::HIGH ? "HIGH" : "LOW")
                  << ", write_enable=" << (write_enable.read() == logic::LogicState::HIGH ? "HIGH" : "LOW")
                  << ", alu_opcode=" << alu_opcode.read_value() << "\n";

        assert(valid_instruction.read() == logic::LogicState::HIGH);
        assert(write_enable.read() == logic::LogicState::HIGH);
        assert(alu_opcode.read_value() == op.funct3_val);
    }

    std::cout << "  [PASS] Test 2 passed successfully.\n\n";
}

// -----------------------------------------------------------------------------
// Test 3: Unsupported operation decoding
// -----------------------------------------------------------------------------
static void test_unsupported_operations()
{
    std::cout << "--- Test 3: Unsupported Operation Decoding ---\n";

    logic::Bus<32> instruction;
    logic::Wire reset(logic::LogicState::LOW);
    logic::Bus<5> rs1, rs2, rd;
    logic::Bus<7> opcode;
    logic::Bus<3> funct3;
    logic::Bus<3> alu_opcode;
    logic::Wire write_enable;
    logic::Wire valid_instruction;

    cpu::VectorControlUnit<> unit(
        instruction, reset, rs1, rs2, rd, opcode, funct3, alu_opcode, write_enable, valid_instruction
    );

    const std::uint32_t unsupported_funct3[] = {0b101, 0b110, 0b111};

    for (std::uint32_t f3 : unsupported_funct3)
    {
        std::uint32_t raw = encode_instruction(1, 2, 3, f3, 0b1010111);
        instruction.write_value(raw);
        reset.write(logic::LogicState::LOW);

        unit.evaluate();

        std::cout << "  Checking unsupported funct3=" << f3 << ": "
                  << "valid=" << (valid_instruction.read() == logic::LogicState::HIGH ? "HIGH" : "LOW")
                  << ", write_enable=" << (write_enable.read() == logic::LogicState::HIGH ? "HIGH" : "LOW") << "\n";

        assert(valid_instruction.read() == logic::LogicState::LOW);
        assert(write_enable.read() == logic::LogicState::LOW);
    }

    std::cout << "  [PASS] Test 3 passed successfully.\n\n";
}

// -----------------------------------------------------------------------------
// Test 4: Invalid major opcode
// -----------------------------------------------------------------------------
static void test_invalid_opcodes()
{
    std::cout << "--- Test 4: Invalid Major Opcode ---\n";

    logic::Bus<32> instruction;
    logic::Wire reset(logic::LogicState::LOW);
    logic::Bus<5> rs1, rs2, rd;
    logic::Bus<7> opcode;
    logic::Bus<3> funct3;
    logic::Bus<3> alu_opcode;
    logic::Wire write_enable;
    logic::Wire valid_instruction;

    cpu::VectorControlUnit<> unit(
        instruction, reset, rs1, rs2, rd, opcode, funct3, alu_opcode, write_enable, valid_instruction
    );

    const std::uint32_t invalid_opcodes[] = {0b0000000, 0b0110011, 0b1111111};

    for (std::uint32_t op : invalid_opcodes)
    {
        std::uint32_t raw = encode_instruction(1, 2, 3, 0b000, op);
        instruction.write_value(raw);
        reset.write(logic::LogicState::LOW);

        unit.evaluate();

        std::cout << "  Checking invalid opcode 0x" << std::hex << op << std::dec << ": "
                  << "valid=" << (valid_instruction.read() == logic::LogicState::HIGH ? "HIGH" : "LOW")
                  << ", write_enable=" << (write_enable.read() == logic::LogicState::HIGH ? "HIGH" : "LOW") << "\n";

        assert(valid_instruction.read() == logic::LogicState::LOW);
        assert(write_enable.read() == logic::LogicState::LOW);
    }

    std::cout << "  [PASS] Test 4 passed successfully.\n\n";
}

// -----------------------------------------------------------------------------
// Test 5: Reset behavior
// -----------------------------------------------------------------------------
static void test_reset_behavior()
{
    std::cout << "--- Test 5: Reset Behavior ---\n";

    logic::Bus<32> instruction;
    logic::Wire reset(logic::LogicState::LOW);
    logic::Bus<5> rs1, rs2, rd;
    logic::Bus<7> opcode;
    logic::Bus<3> funct3;
    logic::Bus<3> alu_opcode;
    logic::Wire write_enable;
    logic::Wire valid_instruction;

    cpu::VectorControlUnit<> unit(
        instruction, reset, rs1, rs2, rd, opcode, funct3, alu_opcode, write_enable, valid_instruction
    );

    // Valid VADD instruction
    std::uint32_t raw = encode_instruction(3, 1, 2, 0b000, 0b1010111);
    instruction.write_value(raw);

    // 1. Reset LOW: valid=HIGH, write_enable=HIGH
    reset.write(logic::LogicState::LOW);
    unit.evaluate();

    std::cout << "  Reset LOW: valid="
              << (valid_instruction.read() == logic::LogicState::HIGH ? "HIGH" : "LOW")
              << ", write_enable="
              << (write_enable.read() == logic::LogicState::HIGH ? "HIGH" : "LOW") << "\n";

    assert(valid_instruction.read() == logic::LogicState::HIGH);
    assert(write_enable.read() == logic::LogicState::HIGH);

    // 2. Reset HIGH: valid=HIGH, write_enable=LOW
    reset.write(logic::LogicState::HIGH);
    unit.evaluate();

    std::cout << "  Reset HIGH: valid="
              << (valid_instruction.read() == logic::LogicState::HIGH ? "HIGH" : "LOW")
              << ", write_enable="
              << (write_enable.read() == logic::LogicState::HIGH ? "HIGH" : "LOW") << "\n";

    assert(valid_instruction.read() == logic::LogicState::HIGH);
    assert(write_enable.read() == logic::LogicState::LOW);

    // 3. Return to Reset LOW: write_enable restored to HIGH
    reset.write(logic::LogicState::LOW);
    unit.evaluate();

    assert(valid_instruction.read() == logic::LogicState::HIGH);
    assert(write_enable.read() == logic::LogicState::HIGH);

    std::cout << "  [PASS] Test 5 passed successfully.\n\n";
}

// -----------------------------------------------------------------------------
// Test 6: Sequential instruction changes
// -----------------------------------------------------------------------------
static void test_sequential_changes()
{
    std::cout << "--- Test 6: Sequential Instruction Changes ---\n";

    logic::Bus<32> instruction;
    logic::Wire reset(logic::LogicState::LOW);
    logic::Bus<5> rs1, rs2, rd;
    logic::Bus<7> opcode;
    logic::Bus<3> funct3;
    logic::Bus<3> alu_opcode;
    logic::Wire write_enable;
    logic::Wire valid_instruction;

    cpu::VectorControlUnit<> unit(
        instruction, reset, rs1, rs2, rd, opcode, funct3, alu_opcode, write_enable, valid_instruction
    );

    reset.write(logic::LogicState::LOW);

    // Cycle 1: VADD -> valid = HIGH, write = HIGH
    instruction.write_value(encode_instruction(3, 1, 2, 0b000, 0b1010111));
    unit.evaluate();
    assert(valid_instruction.read() == logic::LogicState::HIGH);
    assert(write_enable.read() == logic::LogicState::HIGH);
    assert(alu_opcode.read_value() == 0b000);
    std::cout << "  Cycle 1 (VADD): PASS\n";

    // Cycle 2: VXOR -> valid = HIGH, write = HIGH
    instruction.write_value(encode_instruction(4, 3, 2, 0b100, 0b1010111));
    unit.evaluate();
    assert(valid_instruction.read() == logic::LogicState::HIGH);
    assert(write_enable.read() == logic::LogicState::HIGH);
    assert(alu_opcode.read_value() == 0b100);
    std::cout << "  Cycle 2 (VXOR): PASS\n";

    // Cycle 3: 101 (unsupported) -> valid = LOW, write = LOW
    instruction.write_value(encode_instruction(5, 4, 3, 0b101, 0b1010111));
    unit.evaluate();
    assert(valid_instruction.read() == logic::LogicState::LOW);
    assert(write_enable.read() == logic::LogicState::LOW);
    std::cout << "  Cycle 3 (funct3=101): PASS\n";

    // Cycle 4: VAND -> valid = HIGH, write = HIGH
    instruction.write_value(encode_instruction(6, 5, 4, 0b010, 0b1010111));
    unit.evaluate();
    assert(valid_instruction.read() == logic::LogicState::HIGH);
    assert(write_enable.read() == logic::LogicState::HIGH);
    assert(alu_opcode.read_value() == 0b010);
    std::cout << "  Cycle 4 (VAND): PASS\n";

    // Cycle 5: Invalid major opcode -> valid = LOW, write = LOW
    instruction.write_value(encode_instruction(7, 6, 5, 0b000, 0b0000000));
    unit.evaluate();
    assert(valid_instruction.read() == logic::LogicState::LOW);
    assert(write_enable.read() == logic::LogicState::LOW);
    std::cout << "  Cycle 5 (invalid opcode 0b0000000): PASS\n";

    std::cout << "  [PASS] Test 6 passed successfully.\n\n";
}

// -----------------------------------------------------------------------------
// Test 7: Exhaustive opcode and funct3 validation (128 opcodes x 8 funct3 = 1024 cases)
// -----------------------------------------------------------------------------
static void test_exhaustive_validation()
{
    std::cout << "--- Test 7: Exhaustive 1,024-case Validation ---\n";

    logic::Bus<32> instruction;
    logic::Wire reset(logic::LogicState::LOW);
    logic::Bus<5> rs1, rs2, rd;
    logic::Bus<7> opcode;
    logic::Bus<3> funct3;
    logic::Bus<3> alu_opcode;
    logic::Wire write_enable;
    logic::Wire valid_instruction;

    cpu::VectorControlUnit<> unit(
        instruction, reset, rs1, rs2, rd, opcode, funct3, alu_opcode, write_enable, valid_instruction
    );

    reset.write(logic::LogicState::LOW);

    std::size_t passed = 0;
    std::size_t total = 128 * 8;

    for (std::uint32_t op = 0; op < 128; ++op)
    {
        for (std::uint32_t f3 = 0; f3 < 8; ++f3)
        {
            std::uint32_t raw = encode_instruction(1, 2, 3, f3, op);
            instruction.write_value(raw);

            unit.evaluate();

            bool expected_valid = (op == 0b1010111) && (f3 <= 4);
            logic::LogicState exp_state = expected_valid ? logic::LogicState::HIGH : logic::LogicState::LOW;

            assert(valid_instruction.read() == exp_state);
            assert(write_enable.read() == exp_state);
            assert(alu_opcode.read_value() == f3);

            ++passed;
        }
    }

    std::cout << "  Tested " << passed << "/" << total << " combinations.\n";
    std::cout << "  [PASS] Test 7 passed successfully.\n\n";
}

int main()
{
    std::cout << "====================================================\n";
    std::cout << "Running VectorControlUnit Test Suite\n";
    std::cout << "====================================================\n\n";

    test_field_extraction();
    test_supported_operations();
    test_unsupported_operations();
    test_invalid_opcodes();
    test_reset_behavior();
    test_sequential_changes();
    test_exhaustive_validation();

    std::cout << "====================================================\n";
    std::cout << "All VectorControlUnit tests PASSED successfully!\n";
    std::cout << "====================================================\n";

    return 0;
}

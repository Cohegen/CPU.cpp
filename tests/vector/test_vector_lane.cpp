/*
    test_vector_lane.cpp

    Tests for cpu::VectorLane in vector_cpu/core/execution/VectorLane.hpp.

    Covers:
      1. Arithmetic operation tests (ADD, SUB, overflow, underflow, zero)
      2. Logical operation tests (AND, OR, XOR)
      3. Zero flag tests (status output verification)
      4. Carry flag tests (unsigned addition, borrow convention, bitwise LOW)
      5. Repeated evaluation tests (lane reuse across changing operands and opcodes)
      6. UNKNOWN propagation tests (unknown operand, bitwise gate-level determinism,
                                   unknown opcode, recovery)
*/

#include "execution/VectorLane.hpp"

#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/signals/logicState.hpp>

#include <cassert>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>

/*
    ALU opcode mapping (3-bit):
        000 = ADD
        001 = SUB
        010 = AND
        011 = OR
        100 = XOR
        101 = NOT
        110 = PASS_A
        111 = PASS_B
*/

namespace
{

bool is_bus_all_state(const logic::Bus<32>& bus, logic::LogicState expected)
{
    for (std::size_t i = 0; i < 32; ++i)
    {
        if (bus[i].read() != expected)
        {
            return false;
        }
    }
    return true;
}

void print_header(const std::string& title)
{
    std::cout << "\n----------------------------------------\n";
    std::cout << " " << title << "\n";
    std::cout << "----------------------------------------\n";
}

} // namespace

int main()
{
    std::cout << "========================================\n";
    std::cout << "Running VectorLane Integration Tests\n";
    std::cout << "========================================\n";

    int failures = 0;

    // =========================================================================
    // 1. Arithmetic operation tests
    // =========================================================================
    print_header("1. Arithmetic Operation Tests");
    {
        logic::Bus<32> a;
        logic::Bus<32> b;
        logic::Bus<3> opcode;
        logic::Bus<32> result;
        logic::Wire zero;
        logic::Wire carry;

        cpu::VectorLane lane(a, b, opcode, result, zero, carry);

        struct ArithTestCase
        {
            const char* name;
            std::uint8_t op; // 0 = ADD, 1 = SUB
            std::uint32_t op_a;
            std::uint32_t op_b;
            std::uint32_t expected_res;
            logic::LogicState expected_zero;
            logic::LogicState expected_carry;
        };

        const ArithTestCase tests[] = {
            {"Basic addition", 0, 10, 20, 30, logic::LogicState::LOW, logic::LogicState::LOW},
            {"Addition with zero", 0, 25, 0, 25, logic::LogicState::LOW, logic::LogicState::LOW},
            {"Addition overflow", 0, 0xFFFFFFFFU, 1, 0, logic::LogicState::HIGH, logic::LogicState::HIGH},
            {"Basic subtraction", 1, 20, 10, 10, logic::LogicState::LOW, logic::LogicState::LOW},
            {"Subtraction producing zero", 1, 15, 15, 0, logic::LogicState::HIGH, logic::LogicState::LOW},
            {"Subtraction underflow", 1, 0, 1, 0xFFFFFFFFU, logic::LogicState::LOW, logic::LogicState::HIGH},
        };

        for (const auto& t : tests)
        {
            a.write_value(t.op_a);
            b.write_value(t.op_b);
            opcode.write_value(t.op);
            lane.evaluate();

            const std::uint32_t res = static_cast<std::uint32_t>(result.read_value());
            const logic::LogicState z = zero.read();
            const logic::LogicState c = carry.read();

            const bool ok = (res == t.expected_res) && (z == t.expected_zero) && (c == t.expected_carry);
            if (!ok)
            {
                ++failures;
            }

            std::cout << "  [" << (ok ? "PASS" : "FAIL") << "] " << t.name << "\n"
                      << "       A=0x" << std::hex << t.op_a << " B=0x" << t.op_b
                      << " OP=" << std::dec << static_cast<int>(t.op) << "\n"
                      << "       Result: expected=0x" << std::hex << t.expected_res << " got=0x" << res << "\n"
                      << "       Zero:   expected=" << t.expected_zero << " got=" << z << "\n"
                      << "       Carry:  expected=" << t.expected_carry << " got=" << c << std::dec << "\n";
        }
    }

    // =========================================================================
    // 2. Logical operation tests
    // =========================================================================
    print_header("2. Logical Operation Tests");
    {
        logic::Bus<32> a;
        logic::Bus<32> b;
        logic::Bus<3> opcode;
        logic::Bus<32> result;
        logic::Wire zero;
        logic::Wire carry;

        cpu::VectorLane lane(a, b, opcode, result, zero, carry);

        struct LogicTestCase
        {
            const char* op_name;
            std::uint8_t op;
            std::uint32_t op_a;
            std::uint32_t op_b;
            std::uint32_t expected;
        };

        const LogicTestCase tests[] = {
            {"AND", 2, 0xF0F0F0F0U, 0x0F0F0F0FU, 0x00000000U},
            {"OR",  3, 0xF0F0F0F0U, 0x0F0F0F0FU, 0xFFFFFFFFU},
            {"XOR", 4, 0xAAAAAAAAU, 0x55555555U, 0xFFFFFFFFU},
            {"AND", 2, 0xFFFFFFFFU, 0x12345678U, 0x12345678U},
            {"OR",  3, 0x00000000U, 0x12345678U, 0x12345678U},
            {"XOR", 4, 0x12345678U, 0x12345678U, 0x00000000U},
        };

        for (const auto& t : tests)
        {
            a.write_value(t.op_a);
            b.write_value(t.op_b);
            opcode.write_value(t.op);
            lane.evaluate();

            const std::uint32_t res = static_cast<std::uint32_t>(result.read_value());
            const bool ok = (res == t.expected);
            if (!ok)
            {
                ++failures;
            }

            std::cout << "  [" << (ok ? "PASS" : "FAIL") << "] " << t.op_name
                      << " A=0x" << std::hex << t.op_a << " B=0x" << t.op_b
                      << " -> got=0x" << res << " expected=0x" << t.expected << std::dec << "\n";
        }
    }

    // =========================================================================
    // 3. Zero flag tests
    // =========================================================================
    print_header("3. Zero Flag Tests");
    {
        logic::Bus<32> a;
        logic::Bus<32> b;
        logic::Bus<3> opcode;
        logic::Bus<32> result;
        logic::Wire zero;
        logic::Wire carry;

        cpu::VectorLane lane(a, b, opcode, result, zero, carry);

        struct ZeroTestCase
        {
            const char* op_name;
            std::uint8_t op;
            std::uint32_t op_a;
            std::uint32_t op_b;
            std::uint32_t expected_res;
            logic::LogicState expected_zero;
        };

        const ZeroTestCase tests[] = {
            {"ADD non-zero", 0, 10, 20, 30, logic::LogicState::LOW},
            {"ADD zero",     0, 0, 0, 0, logic::LogicState::HIGH},
            {"SUB zero",     1, 15, 15, 0, logic::LogicState::HIGH},
            {"AND zero",     2, 0xF0, 0x0F, 0, logic::LogicState::HIGH},
            {"XOR zero",     4, 0xAA, 0xAA, 0, logic::LogicState::HIGH},
        };

        for (const auto& t : tests)
        {
            a.write_value(t.op_a);
            b.write_value(t.op_b);
            opcode.write_value(t.op);
            lane.evaluate();

            const std::uint32_t res = static_cast<std::uint32_t>(result.read_value());
            const logic::LogicState z = zero.read();

            const bool ok = (res == t.expected_res) && (z == t.expected_zero);
            if (!ok)
            {
                ++failures;
            }

            std::cout << "  [" << (ok ? "PASS" : "FAIL") << "] " << t.op_name
                      << " A=" << std::dec << t.op_a << " B=" << t.op_b
                      << " Result=" << res << " Zero=" << z
                      << " (expected " << t.expected_zero << ")\n";
        }
    }

    // =========================================================================
    // 4. Carry flag tests
    // =========================================================================
    print_header("4. Carry Flag Tests");
    {
        logic::Bus<32> a;
        logic::Bus<32> b;
        logic::Bus<3> opcode;
        logic::Bus<32> result;
        logic::Wire zero;
        logic::Wire carry;

        cpu::VectorLane lane(a, b, opcode, result, zero, carry);

        // 4.1 Unsigned addition carry tests
        struct AddCarryTestCase
        {
            std::uint32_t op_a;
            std::uint32_t op_b;
            std::uint32_t expected_res;
            logic::LogicState expected_carry;
        };

        const AddCarryTestCase add_tests[] = {
            {10, 20, 30, logic::LogicState::LOW},
            {0xFFFFFFFFU, 1, 0, logic::LogicState::HIGH},
            {0x80000000U, 0x80000000U, 0, logic::LogicState::HIGH},
            {0xFFFFFFFFU, 0, 0xFFFFFFFFU, logic::LogicState::LOW},
        };

        for (const auto& t : add_tests)
        {
            a.write_value(t.op_a);
            b.write_value(t.op_b);
            opcode.write_value(0); // ADD
            lane.evaluate();

            const std::uint32_t res = static_cast<std::uint32_t>(result.read_value());
            const logic::LogicState c = carry.read();
            const bool ok = (res == t.expected_res) && (c == t.expected_carry);
            if (!ok)
            {
                ++failures;
            }

            std::cout << "  [" << (ok ? "PASS" : "FAIL") << "] ADD Carry: A=0x" << std::hex << t.op_a
                      << " B=0x" << t.op_b << " Res=0x" << res
                      << " Carry=" << c << " (expected " << t.expected_carry << ")" << std::dec << "\n";
        }

        // 4.2 Subtraction borrow convention tests (carry flag = borrow out)
        struct SubBorrowTestCase
        {
            const char* desc;
            std::uint32_t op_a;
            std::uint32_t op_b;
            std::uint32_t expected_res;
            logic::LogicState expected_borrow;
        };

        const SubBorrowTestCase sub_tests[] = {
            {"20 - 10 (no borrow)", 20, 10, 10, logic::LogicState::LOW},
            {"15 - 15 (no borrow)", 15, 15, 0, logic::LogicState::LOW},
            {"0 - 1 (underflow borrow)", 0, 1, 0xFFFFFFFFU, logic::LogicState::HIGH},
            {"0x10 - 0x20 (underflow borrow)", 0x10, 0x20, 0xFFFFFFF0U, logic::LogicState::HIGH},
        };

        for (const auto& t : sub_tests)
        {
            a.write_value(t.op_a);
            b.write_value(t.op_b);
            opcode.write_value(1); // SUB
            lane.evaluate();

            const std::uint32_t res = static_cast<std::uint32_t>(result.read_value());
            const logic::LogicState c = carry.read();
            const bool ok = (res == t.expected_res) && (c == t.expected_borrow);
            if (!ok)
            {
                ++failures;
            }

            std::cout << "  [" << (ok ? "PASS" : "FAIL") << "] SUB Borrow: " << t.desc
                      << " Res=0x" << std::hex << res
                      << " Borrow=" << c << " (expected " << t.expected_borrow << ")" << std::dec << "\n";
        }

        // 4.3 Bitwise operations carry convention (ALU forces carry LOW)
        a.write_value(0xFFFFFFFFU);
        b.write_value(1);
        opcode.write_value(0);
        lane.evaluate();
        assert(carry.read() == logic::LogicState::HIGH);

        // AND; verify carry is forced LOW
        a.write_value(0xAAAAAAAAU);
        b.write_value(0x55555555U);
        opcode.write_value(2); // AND
        lane.evaluate();
        const bool and_carry_low = (carry.read() == logic::LogicState::LOW);
        if (!and_carry_low) ++failures;
        std::cout << "  [" << (and_carry_low ? "PASS" : "FAIL")
                  << "] AND carry convention: forced LOW = " << carry.read() << "\n";

        // OR; verify carry is forced LOW
        opcode.write_value(3); // OR
        lane.evaluate();
        const bool or_carry_low = (carry.read() == logic::LogicState::LOW);
        if (!or_carry_low) ++failures;
        std::cout << "  [" << (or_carry_low ? "PASS" : "FAIL")
                  << "] OR carry convention: forced LOW = " << carry.read() << "\n";
    }

    // =========================================================================
    // 5. Repeated evaluation tests
    // =========================================================================
    print_header("5. Repeated Evaluation Tests");
    {
        logic::Bus<32> operand_a;
        logic::Bus<32> operand_b;
        logic::Bus<3> opcode;
        logic::Bus<32> result;
        logic::Wire zero;
        logic::Wire carry;

        // Create one single VectorLane instance and reuse across all operations
        cpu::VectorLane lane(operand_a, operand_b, opcode, result, zero, carry);

        // 5.1 Changing operands with constant opcode (ADD = 0)
        operand_a.write_value(10);
        operand_b.write_value(20);
        opcode.write_value(0);
        lane.evaluate();
        const bool r1_ok = (result.read_value() == 30);
        if (!r1_ok) ++failures;
        std::cout << "  [" << (r1_ok ? "PASS" : "FAIL")
                  << "] Repeated Eval Step 1: 10 + 20 = " << result.read_value() << " (expected 30)\n";

        operand_a.write_value(40);
        operand_b.write_value(50);
        lane.evaluate();
        const bool r2_ok = (result.read_value() == 90);
        if (!r2_ok) ++failures;
        std::cout << "  [" << (r2_ok ? "PASS" : "FAIL")
                  << "] Repeated Eval Step 2 (operands changed): 40 + 50 = " << result.read_value() << " (expected 90)\n";

        operand_a.write_value(1000);
        operand_b.write_value(2345);
        lane.evaluate();
        const bool r3_ok = (result.read_value() == 3345);
        if (!r3_ok) ++failures;
        std::cout << "  [" << (r3_ok ? "PASS" : "FAIL")
                  << "] Repeated Eval Step 3: 1000 + 2345 = " << result.read_value() << " (expected 3345)\n";

        // 5.2 Changing opcode with constant operands (A = 20, B = 10)
        operand_a.write_value(20);
        operand_b.write_value(10);

        // ADD -> 30
        opcode.write_value(0);
        lane.evaluate();
        const bool op_add_ok = (result.read_value() == 30);
        if (!op_add_ok) ++failures;
        std::cout << "  [" << (op_add_ok ? "PASS" : "FAIL")
                  << "] Opcode switch: A=20, B=10, ADD -> " << result.read_value() << " (expected 30)\n";

        // SUB -> 10
        opcode.write_value(1);
        lane.evaluate();
        const bool op_sub_ok = (result.read_value() == 10);
        if (!op_sub_ok) ++failures;
        std::cout << "  [" << (op_sub_ok ? "PASS" : "FAIL")
                  << "] Opcode switch: A=20, B=10, SUB -> " << result.read_value() << " (expected 10)\n";

        // AND -> 0 (20 & 10 = 0b10100 & 0b01010 = 0)
        opcode.write_value(2);
        lane.evaluate();
        const bool op_and_ok = (result.read_value() == 0);
        if (!op_and_ok) ++failures;
        std::cout << "  [" << (op_and_ok ? "PASS" : "FAIL")
                  << "] Opcode switch: A=20, B=10, AND -> " << result.read_value() << " (expected 0)\n";

        // OR -> 30 (20 | 10 = 0b10100 | 0b01010 = 0b11110 = 30)
        opcode.write_value(3);
        lane.evaluate();
        const bool op_or_ok = (result.read_value() == 30);
        if (!op_or_ok) ++failures;
        std::cout << "  [" << (op_or_ok ? "PASS" : "FAIL")
                  << "] Opcode switch: A=20, B=10, OR  -> " << result.read_value() << " (expected 30)\n";
    }

    // =========================================================================
    // 6. UNKNOWN propagation tests
    // =========================================================================
    print_header("6. UNKNOWN Propagation Tests");
    {
        logic::Bus<32> operand_a;
        logic::Bus<32> operand_b;
        logic::Bus<3> opcode;
        logic::Bus<32> result;
        logic::Wire zero;
        logic::Wire carry;

        cpu::VectorLane lane(operand_a, operand_b, opcode, result, zero, carry);

        // ---------------------------------------------------------------------
        // Test 6A: Unknown operand with ADD
        // ---------------------------------------------------------------------
        operand_a.write(logic::LogicState::UNKNOWN);
        operand_b.write_value(10);
        opcode.write_value(0); // ADD
        lane.evaluate();

        const bool res_a_unknown = is_bus_all_state(result, logic::LogicState::UNKNOWN);
        if (!res_a_unknown) ++failures;
        std::cout << "  [" << (res_a_unknown ? "PASS" : "FAIL")
                  << "] Test 6A: Operand A=UNKNOWN, B=10, ADD => all result bits UNKNOWN\n";

        // ---------------------------------------------------------------------
        // Test 6B: Gate-level bitwise determinism nuance
        // LOW AND UNKNOWN = LOW deterministically.
        // HIGH AND UNKNOWN = UNKNOWN.
        // ---------------------------------------------------------------------
        operand_a.write(logic::LogicState::UNKNOWN);
        operand_b.write_value(0xAAAAAAAAU); // odd bits 1, even bits 0
        opcode.write_value(2); // AND
        lane.evaluate();

        bool bitwise_nuance_ok = true;
        for (std::size_t i = 0; i < 32; ++i)
        {
            const logic::LogicState expected = (i % 2 == 0)
                ? logic::LogicState::LOW
                : logic::LogicState::UNKNOWN;

            if (result[i].read() != expected)
            {
                bitwise_nuance_ok = false;
                break;
            }
        }
        if (!bitwise_nuance_ok) ++failures;
        std::cout << "  [" << (bitwise_nuance_ok ? "PASS" : "FAIL")
                  << "] Test 6B: Bitwise nuance: LOW AND UNKNOWN produces LOW, HIGH AND UNKNOWN produces UNKNOWN\n";

        // ---------------------------------------------------------------------
        // Test 6C: Unknown opcode
        // ---------------------------------------------------------------------
        operand_a.write_value(10);
        operand_b.write_value(20);
        opcode.write(logic::LogicState::UNKNOWN);
        lane.evaluate();

        bool has_unknown = false;
        for (std::size_t i = 0; i < 32; ++i)
        {
            if (result[i].read() == logic::LogicState::UNKNOWN)
            {
                has_unknown = true;
                break;
            }
        }
        if (!has_unknown) ++failures;
        std::cout << "  [" << (has_unknown ? "PASS" : "FAIL")
                  << "] Test 6C: Opcode=UNKNOWN => multiplexer propagates UNKNOWN\n";

        // ---------------------------------------------------------------------
        // Test 6D: Recovery
        // ---------------------------------------------------------------------
        operand_a.write_value(10);
        operand_b.write_value(20);
        opcode.write_value(0); // ADD
        lane.evaluate();

        const bool recovered = (result.read_value() == 30) &&
                               (zero.read() == logic::LogicState::LOW) &&
                               (carry.read() == logic::LogicState::LOW);
        if (!recovered) ++failures;
        std::cout << "  [" << (recovered ? "PASS" : "FAIL")
                  << "] Test 6D: Recovery: Restored valid inputs -> Result="
                  << result.read_value() << " Zero=" << zero.read()
                  << " Carry=" << carry.read() << "\n";
    }

    // =========================================================================
    // Summary
    // =========================================================================
    std::cout << "\n========================================\n";
    if (failures == 0)
    {
        std::cout << "All VectorLane tests PASSED.\n";
    }
    else
    {
        std::cout << failures << " VectorLane test(s) FAILED.\n";
    }
    std::cout << "========================================\n";

    assert(failures == 0);
    return failures;
}

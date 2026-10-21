/*
    test_vlmax_unit.cpp

    Exhaustive tests for cpu::VLMAXUnit:
    1. Truth-table tests (exhaustive for all 2-bit valid inputs)
    2. Output-bit correctness (all 8 bits verified via bitmask)
    3. Dynamic transitions / changing SEW without reconstructing unit
    4. UNKNOWN state propagation (0X, X0, XX) and recovery
*/

#include "configuration/VLMAXUnit.hpp"

#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/signals/logicState.hpp>

#include <iostream>
#include <cassert>
#include <cstddef>

static void assert_bus_equals(logic::Bus<8>& bus, unsigned expected)
{
    for (std::size_t i = 0; i < 8; ++i)
    {
        const auto expected_state =
            ((expected >> i) & 1U)
                ? logic::LogicState::HIGH
                : logic::LogicState::LOW;

        assert(bus[i].read() == expected_state);
    }
}

// 1. Truth-table & 2. Output-bit correctness tests
static void test_truth_table_and_output_bits()
{
    std::cout << "[Test 1 & 2] Truth-table and 8-bit bus correctness...\n";

    struct TestCase
    {
        const char* name;
        unsigned sew;
        unsigned expected_vlmax;
        logic::LogicState expected_valid;
    };

    const TestCase cases[] = {
        {"SEW = 00 (8 bits)",   0, 16, logic::LogicState::HIGH},
        {"SEW = 01 (16 bits)",  1,  8, logic::LogicState::HIGH},
        {"SEW = 10 (32 bits)",  2,  4, logic::LogicState::HIGH},
        {"SEW = 11 (Reserved)", 3,  0, logic::LogicState::LOW}
    };

    for (const auto& tc : cases)
    {
        logic::Bus<2> sew;
        logic::Bus<8> vlmax;
        logic::Wire valid;

        cpu::VLMAXUnit unit(sew, vlmax, valid);

        for (std::size_t i = 0; i < 2; ++i)
        {
            sew[i].write(
                ((tc.sew >> i) & 1U)
                    ? logic::LogicState::HIGH
                    : logic::LogicState::LOW
            );
        }

        unit.evaluate();

        assert_bus_equals(vlmax, tc.expected_vlmax);
        assert(valid.read() == tc.expected_valid);

        std::cout << "  PASS: " << tc.name
                  << " -> VLMAX = " << tc.expected_vlmax
                  << ", valid = " << (tc.expected_valid == logic::LogicState::HIGH ? "HIGH" : "LOW")
                  << "\n";
    }
}

// 3. Test changing SEW without reconstructing the unit (stale output detection)
static void test_vlmax_transitions()
{
    std::cout << "[Test 3] Changing SEW on a single instance (transitions)...\n";

    logic::Bus<2> sew;
    logic::Bus<8> vlmax;
    logic::Wire valid;

    cpu::VLMAXUnit unit(sew, vlmax, valid);

    struct TestCase
    {
        unsigned sew;
        unsigned expected_vlmax;
        logic::LogicState expected_valid;
    };

    const TestCase cases[] = {
        {0, 16, logic::LogicState::HIGH},
        {1,  8, logic::LogicState::HIGH},
        {2,  4, logic::LogicState::HIGH},
        {3,  0, logic::LogicState::LOW},
        {0, 16, logic::LogicState::HIGH}
    };

    for (const auto& test : cases)
    {
        for (std::size_t i = 0; i < 2; ++i)
        {
            sew[i].write(
                ((test.sew >> i) & 1U)
                    ? logic::LogicState::HIGH
                    : logic::LogicState::LOW
            );
        }

        unit.evaluate();

        assert_bus_equals(vlmax, test.expected_vlmax);
        assert(valid.read() == test.expected_valid);

        std::cout << "  PASS: Transition to SEW=" << test.sew
                  << " -> VLMAX=" << test.expected_vlmax
                  << ", valid=" << (test.expected_valid == logic::LogicState::HIGH ? "HIGH" : "LOW")
                  << "\n";
    }
}

// 4. Test UNKNOWN propagation and recovery
static void test_unknown_propagation_and_recovery()
{
    std::cout << "[Test 4] UNKNOWN propagation and recovery...\n";

    logic::Bus<2> sew;
    logic::Bus<8> vlmax;
    logic::Wire valid;

    cpu::VLMAXUnit unit(sew, vlmax, valid);

    struct UnknownCase
    {
        const char* name;
        logic::LogicState bit0;
        logic::LogicState bit1;
    };

    const UnknownCase unknown_cases[] = {
        {"SEW = 0X", logic::LogicState::UNKNOWN, logic::LogicState::LOW},     // sew[0]=X, sew[1]=0
        {"SEW = X0", logic::LogicState::LOW,     logic::LogicState::UNKNOWN}, // sew[0]=0, sew[1]=X
        {"SEW = XX", logic::LogicState::UNKNOWN, logic::LogicState::UNKNOWN}  // sew[0]=X, sew[1]=X
    };

    for (const auto& uc : unknown_cases)
    {
        sew[0].write(uc.bit0);
        sew[1].write(uc.bit1);

        unit.evaluate();

        // Mapped decoder outputs must be UNKNOWN
        assert(vlmax[4].read() == logic::LogicState::UNKNOWN);
        assert(vlmax[3].read() == logic::LogicState::UNKNOWN);
        assert(vlmax[2].read() == logic::LogicState::UNKNOWN);

        // Constant zero outputs must remain LOW
        assert(vlmax[7].read() == logic::LogicState::LOW);
        assert(vlmax[6].read() == logic::LogicState::LOW);
        assert(vlmax[5].read() == logic::LogicState::LOW);
        assert(vlmax[1].read() == logic::LogicState::LOW);
        assert(vlmax[0].read() == logic::LogicState::LOW);

        // valid wire must propagate UNKNOWN from decoded_[3]
        assert(valid.read() == logic::LogicState::UNKNOWN);

        std::cout << "  PASS: " << uc.name << " correctly propagated UNKNOWN\n";
    }

    // Recovery test: transition from XX to valid SEW = 01 (16 bits)
    std::cout << "  Testing recovery: SEW = XX -> SEW = 01...\n";
    sew[0].write(logic::LogicState::HIGH);
    sew[1].write(logic::LogicState::LOW);
    unit.evaluate();

    assert_bus_equals(vlmax, 8);
    assert(valid.read() == logic::LogicState::HIGH);
    std::cout << "  PASS: Recovery to SEW=01 succeeded (VLMAX=8, valid=HIGH)\n";
}

int main()
{
    std::cout << "========================================\n";
    std::cout << "Running VLMAXUnit Tests\n";
    std::cout << "========================================\n\n";

    test_truth_table_and_output_bits();
    std::cout << "\n";

    test_vlmax_transitions();
    std::cout << "\n";

    test_unknown_propagation_and_recovery();
    std::cout << "\n";

    std::cout << "========================================\n";
    std::cout << "All VLMAXUnit tests PASSED successfully!\n";
    std::cout << "========================================\n";

    return 0;
}

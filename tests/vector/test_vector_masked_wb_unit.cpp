/*
    test_vector_masked_wb_unit.cpp

    Unit tests for cpu::VectorMaskedWriteBackUnit in:
    vector_cpu/core/configuration/VectorMaskedWBUnit.hpp

    Test Suite:
    1. Test all lanes active
    2. Test no lanes active
    3. Test partially active lanes
    4. Test a noncontiguous mask
    5. Test lane independence
    6. Test changing inputs between evaluations
    7. Test bit-level boundaries
*/

#include "configuration/VectorMaskedWBUnit.hpp"

#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/signals/logicState.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

// =============================================================================
// Helper Functions
// =============================================================================
namespace {

void write_lane(logic::Bus<128>& bus, std::size_t lane, std::uint32_t value)
{
    const std::size_t base = lane * 32;
    for (std::size_t bit = 0; bit < 32; ++bit)
    {
        bus[base + bit].write(
            ((value >> bit) & 1U) ? logic::LogicState::HIGH : logic::LogicState::LOW
        );
    }
}

std::uint32_t read_lane(const logic::Bus<128>& bus, std::size_t lane)
{
    const std::size_t base = lane * 32;
    std::uint32_t result = 0;
    for (std::size_t bit = 0; bit < 32; ++bit)
    {
        if (bus[base + bit].read() == logic::LogicState::HIGH)
        {
            result |= (static_cast<std::uint32_t>(1) << bit);
        }
    }
    return result;
}

void write_vector(logic::Bus<128>& bus, const std::array<std::uint32_t, 4>& lanes)
{
    for (std::size_t i = 0; i < 4; ++i)
    {
        write_lane(bus, i, lanes[i]);
    }
}

std::array<std::uint32_t, 4> read_vector(const logic::Bus<128>& bus)
{
    std::array<std::uint32_t, 4> lanes{};
    for (std::size_t i = 0; i < 4; ++i)
    {
        lanes[i] = read_lane(bus, i);
    }
    return lanes;
}

void set_mask(logic::Bus<4>& mask_bus, unsigned mask_val)
{
    for (std::size_t lane = 0; lane < 4; ++lane)
    {
        const bool active = (mask_val >> lane) & 1U;
        mask_bus[lane].write(active ? logic::LogicState::HIGH : logic::LogicState::LOW);
    }
}

std::string mask_to_string(const logic::Bus<4>& mask_bus)
{
    std::string s;
    for (std::size_t i = 4; i > 0; --i)
    {
        s += (mask_bus[i - 1].read() == logic::LogicState::HIGH) ? '1' : '0';
    }
    return s;
}

std::string format_vector_dec(const std::array<std::uint32_t, 4>& lanes)
{
    std::ostringstream oss;
    oss << "[" << lanes[0] << ", " << lanes[1] << ", " << lanes[2] << ", " << lanes[3] << "]";
    return oss.str();
}

std::string format_vector_hex(const std::array<std::uint32_t, 4>& lanes)
{
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    oss << "[0x" << std::setw(8) << lanes[0] << ", "
        << "0x" << std::setw(8) << lanes[1] << ", "
        << "0x" << std::setw(8) << lanes[2] << ", "
        << "0x" << std::setw(8) << lanes[3] << "]";
    return oss.str();
}

void print_header(const std::string& title)
{
    std::cout << "\n----------------------------------------\n";
    std::cout << " " << title << "\n";
    std::cout << "----------------------------------------\n";
}

} // anonymous namespace

// =============================================================================
// Test 1: Test all lanes active
// =============================================================================
static bool test_all_lanes_active()
{
    print_header("1. Test All Lanes Active");
    std::cout << "Purpose: Verify that every lane selects its execution result.\n";

    logic::Bus<128> exec_result;
    logic::Bus<128> old_dest;
    logic::Bus<4>   lane_mask;
    logic::Bus<128> wb_result;

    cpu::VectorMaskedWriteBackUnit unit(exec_result, old_dest, lane_mask, wb_result);

    const std::array<std::uint32_t, 4> exec_val = {11, 22, 33, 44};
    const std::array<std::uint32_t, 4> old_val  = {100, 200, 300, 400};
    const std::array<std::uint32_t, 4> expected = {11, 22, 33, 44};

    write_vector(exec_result, exec_val);
    write_vector(old_dest, old_val);
    set_mask(lane_mask, 0b1111);

    unit.evaluate();

    const auto actual = read_vector(wb_result);
    const bool passed = (actual == expected);

    std::cout << "  Inputs:\n"
              << "    Execution result: " << format_vector_dec(exec_val) << "\n"
              << "    Old destination:  " << format_vector_dec(old_val) << "\n"
              << "    Lane mask:        " << mask_to_string(lane_mask) << "\n"
              << "  Expected output:    " << format_vector_dec(expected) << "\n"
              << "  Actual output:      " << format_vector_dec(actual) << "\n";

    if (passed)
    {
        std::cout << "  [PASS] All four multiplexers correctly selected execution-result inputs.\n";
    }
    else
    {
        std::cout << "  [FAIL] Output did not match expected execution result.\n";
    }

    assert(passed);
    return passed;
}

// =============================================================================
// Test 2: Test no lanes active
// =============================================================================
static bool test_no_lanes_active()
{
    print_header("2. Test No Lanes Active");
    std::cout << "Purpose: Verify that every lane preserves its old value.\n";

    logic::Bus<128> exec_result;
    logic::Bus<128> old_dest;
    logic::Bus<4>   lane_mask;
    logic::Bus<128> wb_result;

    cpu::VectorMaskedWriteBackUnit unit(exec_result, old_dest, lane_mask, wb_result);

    const std::array<std::uint32_t, 4> exec_val = {11, 22, 33, 44};
    const std::array<std::uint32_t, 4> old_val  = {100, 200, 300, 400};
    const std::array<std::uint32_t, 4> expected = {100, 200, 300, 400};

    write_vector(exec_result, exec_val);
    write_vector(old_dest, old_val);
    set_mask(lane_mask, 0b0000);

    unit.evaluate();

    const auto actual = read_vector(wb_result);
    const bool passed = (actual == expected);

    std::cout << "  Inputs:\n"
              << "    Execution result: " << format_vector_dec(exec_val) << "\n"
              << "    Old destination:  " << format_vector_dec(old_val) << "\n"
              << "    Lane mask:        " << mask_to_string(lane_mask) << "\n"
              << "  Expected output:    " << format_vector_dec(expected) << "\n"
              << "  Actual output:      " << format_vector_dec(actual) << "\n";

    if (passed)
    {
        std::cout << "  [PASS] Masked-off lanes were not overwritten; old values preserved.\n";
    }
    else
    {
        std::cout << "  [FAIL] Old destination values were not preserved.\n";
    }

    assert(passed);
    return passed;
}

// =============================================================================
// Test 3: Test partially active lanes
// =============================================================================
static bool test_partially_active_lanes()
{
    print_header("3. Test Partially Active Lanes");
    std::cout << "Purpose: Verify that each mask bit independently controls its corresponding lane.\n";

    struct TestCase
    {
        unsigned mask;
        const char* mask_str;
        std::array<std::uint32_t, 4> expected;
    };

    const TestCase cases[] = {
        {0b0001, "0001", {11, 200, 300, 400}},
        {0b0011, "0011", {11, 22, 300, 400}},
        {0b0111, "0111", {11, 22, 33, 400}},
        {0b1111, "1111", {11, 22, 33, 44}}
    };

    const std::array<std::uint32_t, 4> exec_val = {11, 22, 33, 44};
    const std::array<std::uint32_t, 4> old_val  = {100, 200, 300, 400};

    bool all_passed = true;

    for (const auto& tc : cases)
    {
        logic::Bus<128> exec_result;
        logic::Bus<128> old_dest;
        logic::Bus<4>   lane_mask;
        logic::Bus<128> wb_result;

        cpu::VectorMaskedWriteBackUnit unit(exec_result, old_dest, lane_mask, wb_result);

        write_vector(exec_result, exec_val);
        write_vector(old_dest, old_val);
        set_mask(lane_mask, tc.mask);

        unit.evaluate();

        const auto actual = read_vector(wb_result);
        const bool passed = (actual == tc.expected);
        if (!passed) all_passed = false;

        std::cout << "  Mask: " << tc.mask_str
                  << " -> Expected: " << format_vector_dec(tc.expected)
                  << " | Actual: " << format_vector_dec(actual)
                  << " [" << (passed ? "PASS" : "FAIL") << "]\n";

        assert(passed);
    }

    if (all_passed)
    {
        std::cout << "  [PASS] Each mask bit independently controlled its corresponding lane.\n";
    }

    return all_passed;
}

// =============================================================================
// Test 4: Test a noncontiguous mask
// =============================================================================
static bool test_noncontiguous_mask()
{
    print_header("4. Test Noncontiguous Mask");
    std::cout << "Purpose: Verify that active lanes do not need to be consecutive (mask 0101).\n";

    logic::Bus<128> exec_result;
    logic::Bus<128> old_dest;
    logic::Bus<4>   lane_mask;
    logic::Bus<128> wb_result;

    cpu::VectorMaskedWriteBackUnit unit(exec_result, old_dest, lane_mask, wb_result);

    const std::array<std::uint32_t, 4> exec_val = {11, 22, 33, 44};
    const std::array<std::uint32_t, 4> old_val  = {100, 200, 300, 400};
    const std::array<std::uint32_t, 4> expected = {11, 200, 33, 400};

    write_vector(exec_result, exec_val);
    write_vector(old_dest, old_val);
    set_mask(lane_mask, 0b0101); // Lane 0=1, Lane 1=0, Lane 2=1, Lane 3=0

    unit.evaluate();

    const auto actual = read_vector(wb_result);
    const bool passed = (actual == expected);

    std::cout << "  Inputs:\n"
              << "    Execution result: " << format_vector_dec(exec_val) << "\n"
              << "    Old destination:  " << format_vector_dec(old_val) << "\n"
              << "    Lane mask:        " << mask_to_string(lane_mask) << " (0101)\n"
              << "  Expected output:    " << format_vector_dec(expected) << "\n"
              << "  Actual output:      " << format_vector_dec(actual) << "\n";

    if (passed)
    {
        std::cout << "  [PASS] Lanes 0 and 2 updated, while lanes 1 and 3 retained old values.\n";
    }
    else
    {
        std::cout << "  [FAIL] Noncontiguous mask produced incorrect output.\n";
    }

    assert(passed);
    return passed;
}

// =============================================================================
// Test 5: Test lane independence
// =============================================================================
static bool test_lane_independence()
{
    print_header("5. Test Lane Independence");
    std::cout << "Purpose: Give every lane a distinctive value to detect accidental lane swapping.\n";

    logic::Bus<128> exec_result;
    logic::Bus<128> old_dest;
    logic::Bus<4>   lane_mask;
    logic::Bus<128> wb_result;

    cpu::VectorMaskedWriteBackUnit unit(exec_result, old_dest, lane_mask, wb_result);

    const std::array<std::uint32_t, 4> exec_val = {
        0xAAAAAAAAU, 0xBBBBBBBBU, 0xCCCCCCCCU, 0xDDDDDDDDU
    };
    const std::array<std::uint32_t, 4> old_val = {
        0x11111111U, 0x22222222U, 0x33333333U, 0x44444444U
    };
    // Lane mask: 1010 -> Lane 0=0 (old), Lane 1=1 (exec), Lane 2=0 (old), Lane 3=1 (exec)
    const std::array<std::uint32_t, 4> expected = {
        0x11111111U, 0xBBBBBBBBU, 0x33333333U, 0xDDDDDDDDU
    };

    write_vector(exec_result, exec_val);
    write_vector(old_dest, old_val);
    set_mask(lane_mask, 0b1010);

    unit.evaluate();

    const auto actual = read_vector(wb_result);
    const bool passed = (actual == expected);

    std::cout << "  Inputs:\n"
              << "    Execution result: " << format_vector_hex(exec_val) << "\n"
              << "    Old destination:  " << format_vector_hex(old_val) << "\n"
              << "    Lane mask:        " << mask_to_string(lane_mask) << " (1010)\n"
              << "  Expected output:    " << format_vector_hex(expected) << "\n"
              << "  Actual output:      " << format_vector_hex(actual) << "\n";

    if (passed)
    {
        std::cout << "  [PASS] Verified correct lane ordering and independent lane selection.\n";
    }
    else
    {
        std::cout << "  [FAIL] Lane swapping or incorrect selection detected.\n";
    }

    assert(passed);
    return passed;
}

// =============================================================================
// Test 6: Test changing inputs between evaluations
// =============================================================================
static bool test_changing_inputs_between_evaluations()
{
    print_header("6. Test Changing Inputs Between Evaluations");
    std::cout << "Purpose: Reuse the same unit instance across multiple evaluate() calls\n"
              << "         to catch bugs with stale intermediate values.\n";

    logic::Bus<128> exec_result;
    logic::Bus<128> old_dest;
    logic::Bus<4>   lane_mask;
    logic::Bus<128> wb_result;

    cpu::VectorMaskedWriteBackUnit unit(exec_result, old_dest, lane_mask, wb_result);

    const std::array<std::uint32_t, 4> exec_val = {11, 22, 33, 44};
    const std::array<std::uint32_t, 4> old_val  = {100, 200, 300, 400};

    struct Step
    {
        int step_num;
        unsigned mask;
        const char* mask_str;
        std::array<std::uint32_t, 4> expected;
    };

    const Step steps[] = {
        {1, 0b1111, "1111", {11, 22, 33, 44}},
        {2, 0b0000, "0000", {100, 200, 300, 400}},
        {3, 0b0101, "0101", {11, 200, 33, 400}},
        {4, 0b0010, "0010", {100, 22, 300, 400}}
    };

    bool all_passed = true;

    for (const auto& s : steps)
    {
        // Update input buses before each evaluation
        write_vector(exec_result, exec_val);
        write_vector(old_dest, old_val);
        set_mask(lane_mask, s.mask);

        unit.evaluate();

        const auto actual = read_vector(wb_result);
        const bool passed = (actual == s.expected);
        if (!passed) all_passed = false;

        std::cout << "  Evaluation " << s.step_num
                  << " (Mask=" << s.mask_str << ")"
                  << " -> Expected: " << format_vector_dec(s.expected)
                  << " | Actual: " << format_vector_dec(actual)
                  << " [" << (passed ? "PASS" : "FAIL") << "]\n";

        assert(passed);
    }

    if (all_passed)
    {
        std::cout << "  [PASS] Single instance handled dynamic transitions without stale state.\n";
    }

    return all_passed;
}

// =============================================================================
// Test 7: Test bit-level boundaries
// =============================================================================
static bool test_bit_level_boundaries()
{
    print_header("7. Test Bit-Level Boundaries");
    std::cout << "Purpose: Catch incorrect indexing when splitting or recombining the 128-bit buses.\n";

    logic::Bus<128> exec_result;
    logic::Bus<128> old_dest;
    logic::Bus<4>   lane_mask;
    logic::Bus<128> wb_result;

    cpu::VectorMaskedWriteBackUnit unit(exec_result, old_dest, lane_mask, wb_result);

    const std::array<std::uint32_t, 4> boundary_exec = {
        0x00000001U, // Lane 0: bit 0
        0x00000100U, // Lane 1: bit 8
        0x00010000U, // Lane 2: bit 16
        0x80000000U  // Lane 3: bit 31
    };

    const std::array<std::uint32_t, 4> boundary_old = {
        0x12345678U,
        0x9ABCDEF0U,
        0x0FEDCBA9U,
        0x87654321U
    };

    bool all_passed = true;

    // Part A: Set mask to 1111 -> verify every bit pattern appears in correct output lane
    std::cout << "  Part A: Mask = 1111 (All execution bit patterns selected)\n";
    write_vector(exec_result, boundary_exec);
    write_vector(old_dest, boundary_old);
    set_mask(lane_mask, 0b1111);

    unit.evaluate();

    auto actual = read_vector(wb_result);
    bool passed_a = (actual == boundary_exec);
    if (!passed_a) all_passed = false;

    std::cout << "    Expected: " << format_vector_hex(boundary_exec) << "\n"
              << "    Actual:   " << format_vector_hex(actual) << "\n"
              << "    [" << (passed_a ? "PASS" : "FAIL") << "] Part A passed.\n";
    assert(passed_a);

    // Part B: Set mask to 0000 -> verify old destination patterns are preserved
    std::cout << "  Part B: Mask = 0000 (All old destination bit patterns preserved)\n";
    set_mask(lane_mask, 0b0000);

    unit.evaluate();

    actual = read_vector(wb_result);
    bool passed_b = (actual == boundary_old);
    if (!passed_b) all_passed = false;

    std::cout << "    Expected: " << format_vector_hex(boundary_old) << "\n"
              << "    Actual:   " << format_vector_hex(actual) << "\n"
              << "    [" << (passed_b ? "PASS" : "FAIL") << "] Part B passed.\n";
    assert(passed_b);

    // Part C: Test individual edge boundary bits (bits 0, 31, 32, 63, 64, 95, 96, 127)
    std::cout << "  Part C: Probing individual boundary bits between adjacent lanes\n";
    struct BoundaryProbe
    {
        const char* desc;
        std::size_t global_bit;
        std::size_t expected_lane;
        std::uint32_t expected_lane_val;
    };

    const BoundaryProbe probes[] = {
        {"Bit 0 (LSB of Lane 0)",    0,   0, 0x00000001U},
        {"Bit 31 (MSB of Lane 0)",   31,  0, 0x80000000U},
        {"Bit 32 (LSB of Lane 1)",   32,  1, 0x00000001U},
        {"Bit 63 (MSB of Lane 1)",   63,  1, 0x80000000U},
        {"Bit 64 (LSB of Lane 2)",   64,  2, 0x00000001U},
        {"Bit 95 (MSB of Lane 2)",   95,  2, 0x80000000U},
        {"Bit 96 (LSB of Lane 3)",   96,  3, 0x00000001U},
        {"Bit 127 (MSB of Lane 3)", 127,  3, 0x80000000U}
    };

    for (const auto& probe : probes)
    {
        // Zero out inputs
        exec_result.write(logic::LogicState::LOW);
        old_dest.write(logic::LogicState::LOW);
        set_mask(lane_mask, 0b1111);

        // Set single boundary bit
        exec_result[probe.global_bit].write(logic::LogicState::HIGH);

        unit.evaluate();

        const auto res = read_vector(wb_result);

        bool probe_ok = true;
        for (std::size_t lane = 0; lane < 4; ++lane)
        {
            const std::uint32_t expected_val =
                (lane == probe.expected_lane) ? probe.expected_lane_val : 0U;
            if (res[lane] != expected_val)
            {
                probe_ok = false;
            }
        }

        if (!probe_ok) all_passed = false;

        std::cout << "    [" << (probe_ok ? "PASS" : "FAIL") << "] " << probe.desc
                  << " -> Lane " << probe.expected_lane
                  << " = 0x" << std::hex << res[probe.expected_lane]
                  << " (expected 0x" << probe.expected_lane_val << ")"
                  << std::dec << "\n";

        assert(probe_ok);
    }

    if (all_passed)
    {
        std::cout << "  [PASS] All bit-level boundary and indexing checks passed.\n";
    }

    return all_passed;
}

// =============================================================================
// Main
// =============================================================================
int main()
{
    std::cout << "========================================\n";
    std::cout << "Running VectorMaskedWriteBackUnit Tests\n";
    std::cout << "========================================\n";

    int failures = 0;

    if (!test_all_lanes_active())                  ++failures;
    if (!test_no_lanes_active())                   ++failures;
    if (!test_partially_active_lanes())            ++failures;
    if (!test_noncontiguous_mask())                ++failures;
    if (!test_lane_independence())                 ++failures;
    if (!test_changing_inputs_between_evaluations()) ++failures;
    if (!test_bit_level_boundaries())              ++failures;

    std::cout << "\n========================================\n";
    if (failures == 0)
    {
        std::cout << "All VectorMaskedWriteBackUnit tests PASSED!\n";
    }
    else
    {
        std::cout << failures << " test suite(s) FAILED.\n";
    }
    std::cout << "========================================\n";

    return failures;
}

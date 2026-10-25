/*
    test_vector_execution_unit.cpp

    Comprehensive tests for cpu::VectorExecutionUnit in
    vector_cpu/core/execution/VectorExecutionUnit.hpp.

    Configuration: VLEN = 128, 4 lanes, 32 bits per lane.

    Covers:
      1. Splitting and combining (lane-ordering verification)
      2. Basic addition across all lanes
      3. Lane independence
      4. Carry isolation (overflow in one lane doesn't affect others)
      5. Zero operands
      6. Bit boundaries (addition at lane edges)
      7. Maximum values (all lanes overflow)
      8. Every supported opcode (SUB, AND, OR, XOR)
      9. Repeated evaluations (changing operands and opcode without
         reconstructing the VEU)
     10. Individual bit-boundary probes (bits 31/32, 63/64, 95/96)
*/

#include "execution/VectorExecutionUnit.hpp"

#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/signals/logicState.hpp>

#include <array>
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

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────
namespace
{

/// Write a 32-bit value into a specific lane (0–3) of a 128-bit bus.
/// Lane 0 = bits [0..31], Lane 1 = bits [32..63], etc.
void write_lane(logic::Bus<128>& bus, std::size_t lane, std::uint32_t value)
{
    const std::size_t base = lane * 32;
    for (std::size_t bit = 0; bit < 32; ++bit)
    {
        bus[base + bit].write(
            ((value >> bit) & 1) ? logic::LogicState::HIGH : logic::LogicState::LOW
        );
    }
}

/// Read a 32-bit value from a specific lane (0–3) of a 128-bit bus.
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

/// Write four 32-bit lane values into a 128-bit bus.
/// lanes[0] -> Lane 0 (bits 0–31) ... lanes[3] -> Lane 3 (bits 96–127)
void write_vector(logic::Bus<128>& bus,
                  const std::array<std::uint32_t, 4>& lanes)
{
    for (std::size_t i = 0; i < 4; ++i)
    {
        write_lane(bus, i, lanes[i]);
    }
}

/// Read four 32-bit lane values from a 128-bit bus.
std::array<std::uint32_t, 4> read_vector(const logic::Bus<128>& bus)
{
    std::array<std::uint32_t, 4> lanes{};
    for (std::size_t i = 0; i < 4; ++i)
    {
        lanes[i] = read_lane(bus, i);
    }
    return lanes;
}

/// Clear all 128 bits of a bus to LOW.
void clear_bus(logic::Bus<128>& bus)
{
    bus.write(logic::LogicState::LOW);
}

void print_header(const std::string& title)
{
    std::cout << "\n----------------------------------------\n";
    std::cout << " " << title << "\n";
    std::cout << "----------------------------------------\n";
}

/// Print a hex-formatted lane comparison.
void print_lane_result(const char* tag,
                       const std::array<std::uint32_t, 4>& expected,
                       const std::array<std::uint32_t, 4>& got,
                       bool ok)
{
    std::cout << "  [" << (ok ? "PASS" : "FAIL") << "] " << tag << "\n"
              << "       Expected: [" << std::hex
              << "0x" << expected[0] << ", 0x" << expected[1] << ", "
              << "0x" << expected[2] << ", 0x" << expected[3] << "]\n"
              << "       Got:      [0x" << got[0] << ", 0x" << got[1] << ", "
              << "0x" << got[2] << ", 0x" << got[3] << "]"
              << std::dec << "\n";
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────
int main()
{
    std::cout << "========================================\n";
    std::cout << "Running VectorExecutionUnit Tests\n";
    std::cout << "========================================\n";

    int failures = 0;

    // =========================================================================
    // 1. Splitting and combining – lane-ordering verification
    // =========================================================================
    print_header("1. Splitting & Combining (Lane Ordering)");
    {
        logic::Bus<128> vec_a;
        logic::Bus<128> vec_b;
        logic::Bus<3>   opcode;
        logic::Bus<128> result;

        cpu::VectorExecutionUnit<> veu(vec_a, vec_b, opcode, result);

        // A: Lane 0 = 0x11111111 ... Lane 3 = 0x44444444
        // B: all zeros
        // Opcode: ADD (000) → result should equal A
        write_vector(vec_a, {0x11111111U, 0x22222222U, 0x33333333U, 0x44444444U});
        clear_bus(vec_b);
        opcode.write_value(0); // ADD
        veu.evaluate();

        const auto res = read_vector(result);
        const std::array<std::uint32_t, 4> exp = {0x11111111U, 0x22222222U,
                                                    0x33333333U, 0x44444444U};
        const bool ok = (res == exp);
        if (!ok) ++failures;
        print_lane_result("Lane ordering (A + 0 = A)", exp, res, ok);
    }

    // =========================================================================
    // 2. Basic addition
    // =========================================================================
    print_header("2. Basic Addition");
    {
        logic::Bus<128> vec_a;
        logic::Bus<128> vec_b;
        logic::Bus<3>   opcode;
        logic::Bus<128> result;

        cpu::VectorExecutionUnit<> veu(vec_a, vec_b, opcode, result);

        write_vector(vec_a, {10, 20, 30, 40});
        write_vector(vec_b, {1, 2, 3, 4});
        opcode.write_value(0); // ADD
        veu.evaluate();

        const auto res = read_vector(result);
        const std::array<std::uint32_t, 4> exp = {11, 22, 33, 44};
        const bool ok = (res == exp);
        if (!ok) ++failures;
        print_lane_result("10+1, 20+2, 30+3, 40+4", exp, res, ok);
    }

    // =========================================================================
    // 3. Lane independence
    // =========================================================================
    print_header("3. Lane Independence");
    {
        logic::Bus<128> vec_a;
        logic::Bus<128> vec_b;
        logic::Bus<3>   opcode;
        logic::Bus<128> result;

        cpu::VectorExecutionUnit<> veu(vec_a, vec_b, opcode, result);

        write_vector(vec_a, {100, 0, 500, 7});
        write_vector(vec_b, {5, 20, 100, 3});
        opcode.write_value(0); // ADD
        veu.evaluate();

        const auto res = read_vector(result);
        const std::array<std::uint32_t, 4> exp = {105, 20, 600, 10};
        const bool ok = (res == exp);
        if (!ok) ++failures;
        print_lane_result("100+5, 0+20, 500+100, 7+3", exp, res, ok);
    }

    // =========================================================================
    // 4. Carry isolation
    // =========================================================================
    print_header("4. Carry Isolation");
    {
        logic::Bus<128> vec_a;
        logic::Bus<128> vec_b;
        logic::Bus<3>   opcode;
        logic::Bus<128> result;

        cpu::VectorExecutionUnit<> veu(vec_a, vec_b, opcode, result);

        // Lane 0 overflows: 0xFFFFFFFF + 1 = 0 (carry)
        // Lanes 1..3 must NOT be affected by Lane 0's carry.
        write_vector(vec_a, {0xFFFFFFFFU, 10, 20, 30});
        write_vector(vec_b, {1, 5, 5, 5});
        opcode.write_value(0); // ADD
        veu.evaluate();

        const auto res = read_vector(result);
        const std::array<std::uint32_t, 4> exp = {0, 15, 25, 35};
        const bool ok = (res == exp);
        if (!ok) ++failures;
        print_lane_result("0xFFFFFFFF+1, 10+5, 20+5, 30+5", exp, res, ok);
    }

    // =========================================================================
    // 5. Zero operands
    // =========================================================================
    print_header("5. Zero Operands");
    {
        logic::Bus<128> vec_a;
        logic::Bus<128> vec_b;
        logic::Bus<3>   opcode;
        logic::Bus<128> result;

        cpu::VectorExecutionUnit<> veu(vec_a, vec_b, opcode, result);

        write_vector(vec_a, {0, 0, 0, 0});
        write_vector(vec_b, {0, 0, 0, 0});
        opcode.write_value(0); // ADD
        veu.evaluate();

        const auto res = read_vector(result);
        const std::array<std::uint32_t, 4> exp = {0, 0, 0, 0};
        const bool ok = (res == exp);
        if (!ok) ++failures;
        print_lane_result("0+0 across all lanes", exp, res, ok);
    }

    // =========================================================================
    // 6. Bit boundaries
    // =========================================================================
    print_header("6. Bit Boundaries");
    {
        logic::Bus<128> vec_a;
        logic::Bus<128> vec_b;
        logic::Bus<3>   opcode;
        logic::Bus<128> result;

        cpu::VectorExecutionUnit<> veu(vec_a, vec_b, opcode, result);

        // 0x80000000 = bit 31 set, 1 = bit 0 set
        write_vector(vec_a, {0x80000000U, 1, 0, 0});
        write_vector(vec_b, {0, 0, 1, 0});
        opcode.write_value(0); // ADD
        veu.evaluate();

        const auto res = read_vector(result);
        const std::array<std::uint32_t, 4> exp = {0x80000000U, 1, 1, 0};
        const bool ok = (res == exp);
        if (!ok) ++failures;
        print_lane_result("MSB preservation & single-bit carry", exp, res, ok);
    }

    // =========================================================================
    // 7. Maximum values (all lanes overflow)
    // =========================================================================
    print_header("7. Maximum Values (All Lanes Overflow)");
    {
        logic::Bus<128> vec_a;
        logic::Bus<128> vec_b;
        logic::Bus<3>   opcode;
        logic::Bus<128> result;

        cpu::VectorExecutionUnit<> veu(vec_a, vec_b, opcode, result);

        write_vector(vec_a, {0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU});
        write_vector(vec_b, {1, 1, 1, 1});
        opcode.write_value(0); // ADD
        veu.evaluate();

        const auto res = read_vector(result);
        const std::array<std::uint32_t, 4> exp = {0, 0, 0, 0};
        const bool ok = (res == exp);
        if (!ok) ++failures;
        print_lane_result("0xFFFFFFFF+1 in all lanes", exp, res, ok);
    }

    // =========================================================================
    // 8. Every supported opcode
    // =========================================================================
    print_header("8. Every Supported Opcode");
    {
        logic::Bus<128> vec_a;
        logic::Bus<128> vec_b;
        logic::Bus<3>   opcode;
        logic::Bus<128> result;

        cpu::VectorExecutionUnit<> veu(vec_a, vec_b, opcode, result);

        // ─── 8a: Subtraction ─────────────────────────────────────────────
        write_vector(vec_a, {50, 100, 200, 400});
        write_vector(vec_b, {10, 20, 30, 40});
        opcode.write_value(1); // SUB
        veu.evaluate();
        {
            const auto res = read_vector(result);
            const std::array<std::uint32_t, 4> exp = {40, 80, 170, 360};
            const bool ok = (res == exp);
            if (!ok) ++failures;
            print_lane_result("SUB: 50-10, 100-20, 200-30, 400-40", exp, res, ok);
        }

        // ─── 8b: AND ────────────────────────────────────────────────────
        write_vector(vec_a, {0xFF00FF00U, 0x12345678U, 0xAAAAAAAAU, 0x0F0F0F0FU});
        write_vector(vec_b, {0x0F0F0F0FU, 0xFFFFFFFFU, 0x55555555U, 0xF0F0F0F0U});
        opcode.write_value(2); // AND
        veu.evaluate();
        {
            const auto res = read_vector(result);
            const std::array<std::uint32_t, 4> exp = {
                0xFF00FF00U & 0x0F0F0F0FU,   // 0x0F000F00
                0x12345678U & 0xFFFFFFFFU,   // 0x12345678
                0xAAAAAAAAU & 0x55555555U,   // 0x00000000
                0x0F0F0F0FU & 0xF0F0F0F0U    // 0x00000000
            };
            const bool ok = (res == exp);
            if (!ok) ++failures;
            print_lane_result("AND: various patterns", exp, res, ok);
        }

        // ─── 8c: OR ─────────────────────────────────────────────────────
        write_vector(vec_a, {0xF0F0F0F0U, 0x00000000U, 0xAAAAAAAAU, 0x12345678U});
        write_vector(vec_b, {0x0F0F0F0FU, 0x12345678U, 0x55555555U, 0x00000000U});
        opcode.write_value(3); // OR
        veu.evaluate();
        {
            const auto res = read_vector(result);
            const std::array<std::uint32_t, 4> exp = {
                0xFFFFFFFFU,   // F0|0F
                0x12345678U,   // 00|12345678
                0xFFFFFFFFU,   // AA|55
                0x12345678U    // 12345678|00
            };
            const bool ok = (res == exp);
            if (!ok) ++failures;
            print_lane_result("OR: various patterns", exp, res, ok);
        }

        // ─── 8d: XOR ────────────────────────────────────────────────────
        write_vector(vec_a, {0xAAAAAAAAU, 0x12345678U, 0xFFFFFFFFU, 0x00000000U});
        write_vector(vec_b, {0x55555555U, 0x12345678U, 0xFFFFFFFFU, 0x00000000U});
        opcode.write_value(4); // XOR
        veu.evaluate();
        {
            const auto res = read_vector(result);
            const std::array<std::uint32_t, 4> exp = {
                0xFFFFFFFFU,   // AA ^ 55
                0x00000000U,   // same ^ same
                0x00000000U,   // FF ^ FF
                0x00000000U    // 00 ^ 00
            };
            const bool ok = (res == exp);
            if (!ok) ++failures;
            print_lane_result("XOR: various patterns", exp, res, ok);
        }
    }

    // =========================================================================
    // 9. Repeated evaluations (no reconstruction)
    // =========================================================================
    print_header("9. Repeated Evaluations");
    {
        logic::Bus<128> vec_a;
        logic::Bus<128> vec_b;
        logic::Bus<3>   opcode;
        logic::Bus<128> result;

        cpu::VectorExecutionUnit<> veu(vec_a, vec_b, opcode, result);

        // ── Step 1: ADD ──────────────────────────────────────────────────
        write_vector(vec_a, {1, 2, 3, 4});
        write_vector(vec_b, {1, 1, 1, 1});
        opcode.write_value(0); // ADD
        veu.evaluate();
        {
            const auto res = read_vector(result);
            const std::array<std::uint32_t, 4> exp = {2, 3, 4, 5};
            const bool ok = (res == exp);
            if (!ok) ++failures;
            print_lane_result("Step 1 (ADD): 1+1, 2+1, 3+1, 4+1", exp, res, ok);
        }

        // ── Step 2: Change operands, same opcode ────────────────────────
        write_vector(vec_a, {10, 20, 30, 40});
        write_vector(vec_b, {5, 5, 5, 5});
        veu.evaluate();
        {
            const auto res = read_vector(result);
            const std::array<std::uint32_t, 4> exp = {15, 25, 35, 45};
            const bool ok = (res == exp);
            if (!ok) ++failures;
            print_lane_result("Step 2 (ADD): 10+5, 20+5, 30+5, 40+5", exp, res, ok);
        }

        // ── Step 3: Change opcode to SUB ────────────────────────────────
        opcode.write_value(1); // SUB
        veu.evaluate();
        {
            const auto res = read_vector(result);
            const std::array<std::uint32_t, 4> exp = {5, 15, 25, 35};
            const bool ok = (res == exp);
            if (!ok) ++failures;
            print_lane_result("Step 3 (SUB): 10-5, 20-5, 30-5, 40-5", exp, res, ok);
        }

        // ── Step 4: Change opcode to AND ────────────────────────────────
        write_vector(vec_a, {0xFFU, 0xF0F0U, 0xAAAAU, 0x1234U});
        write_vector(vec_b, {0x0FU, 0x0F0FU, 0x5555U, 0xFFFFU});
        opcode.write_value(2); // AND
        veu.evaluate();
        {
            const auto res = read_vector(result);
            const std::array<std::uint32_t, 4> exp = {
                0xFFU & 0x0FU,
                0xF0F0U & 0x0F0FU,
                0xAAAAU & 0x5555U,
                0x1234U & 0xFFFFU
            };
            const bool ok = (res == exp);
            if (!ok) ++failures;
            print_lane_result("Step 4 (AND): after opcode switch", exp, res, ok);
        }
    }

    // =========================================================================
    // 10. Individual bit-boundary probes
    // =========================================================================
    print_header("10. Individual Bit-Boundary Probes");
    {
        logic::Bus<128> vec_a;
        logic::Bus<128> vec_b;
        logic::Bus<3>   opcode;
        logic::Bus<128> result;

        cpu::VectorExecutionUnit<> veu(vec_a, vec_b, opcode, result);
        opcode.write_value(0); // ADD (A + 0 = A)

        // Boundary bits to probe:
        //   31  -> last bit of Lane 0   (value in lane 0 = 0x80000000)
        //   32  -> first bit of Lane 1  (value in lane 1 = 0x00000001)
        //   63  -> last bit of Lane 1   (value in lane 1 = 0x80000000)
        //   64  -> first bit of Lane 2  (value in lane 2 = 0x00000001)
        //   95  -> last bit of Lane 2   (value in lane 2 = 0x80000000)
        //   96  -> first bit of Lane 3  (value in lane 3 = 0x00000001)

        struct BitProbe
        {
            const char*  desc;
            std::size_t  global_bit;      // bit index in 128-bit bus
            std::size_t  expected_lane;   // which lane should have the bit
            std::uint32_t expected_value; // expected 32-bit value in that lane
        };

        const BitProbe probes[] = {
            {"Bit 31 (last of Lane 0)",   31, 0, 0x80000000U},
            {"Bit 32 (first of Lane 1)",  32, 1, 0x00000001U},
            {"Bit 63 (last of Lane 1)",   63, 1, 0x80000000U},
            {"Bit 64 (first of Lane 2)",  64, 2, 0x00000001U},
            {"Bit 95 (last of Lane 2)",   95, 2, 0x80000000U},
            {"Bit 96 (first of Lane 3)",  96, 3, 0x00000001U},
        };

        for (const auto& p : probes)
        {
            // Clear both operands
            clear_bus(vec_a);
            clear_bus(vec_b);

            // Set exactly one bit HIGH in vec_a
            vec_a[p.global_bit].write(logic::LogicState::HIGH);

            veu.evaluate();

            // Read all lanes
            const auto res = read_vector(result);

            // The expected lane should have the expected value;
            // all other lanes should be zero.
            bool ok = true;
            for (std::size_t lane = 0; lane < 4; ++lane)
            {
                const std::uint32_t expected =
                    (lane == p.expected_lane) ? p.expected_value : 0;
                if (res[lane] != expected)
                {
                    ok = false;
                }
            }

            if (!ok) ++failures;
            std::cout << "  [" << (ok ? "PASS" : "FAIL") << "] " << p.desc
                      << " -> Lane " << p.expected_lane
                      << " = 0x" << std::hex << res[p.expected_lane]
                      << " (expected 0x" << p.expected_value << ")"
                      << std::dec << "\n";
        }
    }

    // =========================================================================
    // Summary
    // =========================================================================
    std::cout << "\n========================================\n";
    if (failures == 0)
    {
        std::cout << "All VectorExecutionUnit tests PASSED.\n";
    }
    else
    {
        std::cout << failures << " VectorExecutionUnit test(s) FAILED.\n";
    }
    std::cout << "========================================\n";

    assert(failures == 0);
    return failures;
}

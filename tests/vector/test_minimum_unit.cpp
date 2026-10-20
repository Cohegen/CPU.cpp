/*
    test_minimum_unit.cpp

    Tests for the MinimumUnit hardware component used in vector_cpu
    to compute min(AVL, VLMAX).

    Test vectors:
        AVL (a)    VLMAX (b)    Expected minimum
        8          16           8
        20         16           16
        4          4            4
        0          16           0
        255        128          128
*/

#include "../vector_cpu/core/configuration/MinimumUnit.hpp"

#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <iostream>
#include <cassert>
#include <cstdint>

static bool run_minimum_test(
    const char* label,
    std::uint64_t avl_val,
    std::uint64_t vlmax_val,
    std::uint64_t expected_min)
{
    constexpr std::size_t WIDTH = 8;

    logic::Bus<WIDTH> a;       // AVL
    logic::Bus<WIDTH> b;       // VLMAX
    logic::Bus<WIDTH> minimum; // output

    cpu::MinimumUnit<WIDTH> unit(a, b, minimum);

    a.write_value(avl_val);
    b.write_value(vlmax_val);
    unit.evaluate();

    std::uint64_t result = minimum.read_value();

    std::cout << "[" << label << "] "
              << "AVL=" << avl_val
              << "  VLMAX=" << vlmax_val
              << "  expected=" << expected_min
              << "  got=" << result;

    if (result == expected_min)
    {
        std::cout << "  [PASS]\n";
        return true;
    }
    else
    {
        std::cout << "  [FAIL]\n";
        return false;
    }
}

int main()
{
    std::cout << "========================================\n";
    std::cout << "Running MinimumUnit Tests\n";
    std::cout << "========================================\n\n";

    int failures = 0;

    // Test 1: AVL < VLMAX  =>  min = AVL
    if (!run_minimum_test("Test 1: AVL < VLMAX",    8,  16,   8)) ++failures;

    // Test 2: AVL > VLMAX  =>  min = VLMAX
    if (!run_minimum_test("Test 2: AVL > VLMAX",   20,  16,  16)) ++failures;

    // Test 3: AVL == VLMAX =>  min = either (4)
    if (!run_minimum_test("Test 3: AVL == VLMAX",   4,   4,   4)) ++failures;

    // Test 4: AVL == 0     =>  min = 0
    if (!run_minimum_test("Test 4: AVL == 0",       0,  16,   0)) ++failures;

    // Test 5: Large values =>  min = VLMAX
    if (!run_minimum_test("Test 5: Large values", 255, 128, 128)) ++failures;

    std::cout << "\n========================================\n";
    if (failures == 0)
    {
        std::cout << "All MinimumUnit tests PASSED.\n";
    }
    else
    {
        std::cout << failures << " MinimumUnit test(s) FAILED.\n";
    }
    std::cout << "========================================\n";

    assert(failures == 0);
    return 0;
}

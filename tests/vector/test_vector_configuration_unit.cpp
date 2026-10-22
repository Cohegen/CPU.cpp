/*
    test_vector_configuration_unit.cpp

    Comprehensive test suite for cpu::VectorConfigurationUnit:
    1. Configuration calculations (AVL vs VLMAX branches and boundary 255)
    2. VTYPE output verification matching SEW
    3. Register updates on clock edges (combinational vs sequential separation)
    4. Invalid SEW (11) rejection (startup and after valid state)
    5. Write-enable (configure) behavior (asserted, deasserted, preserve)
    6. Successive configuration changes on a single instance
    7. UNKNOWN state propagation and conservative write prevention
    8. Reset behavior (independent reset, releasing reset)
*/

#include "configuration/VectorConfigurationUnit.hpp"

#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/signals/logicState.hpp>

#include <iostream>
#include <cassert>
#include <cstddef>
#include <cstdint>

static void pulse_clock(logic::Wire& clock, cpu::VectorConfigurationUnit& vcu)
{
    clock.write(logic::LogicState::LOW);
    vcu.evaluate();
    clock.write(logic::LogicState::HIGH);
    vcu.evaluate();
}

// 1. Test the configuration calculations
static void test_configuration_calculations()
{
    std::cout << "[Test 1] Configuration calculations (AVL, SEW -> VLMAX, VL)...\n";

    struct TestCase {
        const char* name;
        std::uint64_t avl;
        unsigned sew;
        unsigned expected_vlmax;
        unsigned expected_vl;
    };

    const TestCase cases[] = {
        {"Test 1: AVL < VLMAX (8 < 16, SEW=00)",    8, 0, 16,  8},
        {"Test 2: AVL > VLMAX (20 > 16, SEW=00)",  20, 0, 16, 16},
        {"Test 3: AVL > VLMAX (12 > 8,  SEW=01)",  12, 1,  8,  8},
        {"Test 4: AVL < VLMAX (4 < 8,   SEW=01)",   4, 1,  8,  4},
        {"Test 5: AVL > VLMAX (10 > 4,  SEW=10)",  10, 2,  4,  4},
        {"Test 6: AVL == 0 (0, SEW=10)",            0, 2,  4,  0},
        {"Test 7: AVL == VLMAX (4 == 4, SEW=10)",   4, 2,  4,  4},
        {"Test 8: AVL == 255 boundary (SEW=10)",  255, 2,  4,  4}
    };

    for (const auto& tc : cases) {
        logic::Bus<8> avl;
        logic::Bus<2> sew;
        logic::Wire clock(logic::LogicState::LOW);
        logic::Wire configure(logic::LogicState::HIGH);
        logic::Wire reset(logic::LogicState::LOW);

        cpu::VectorConfigurationUnit vcu(avl, sew, clock, configure, reset);

        avl.write_value(tc.avl);
        sew.write_value(tc.sew);

        // Evaluate combinational logic
        vcu.evaluate_combinational();
        assert(vcu.vlmax().read_value() == tc.expected_vlmax);
        assert(vcu.next_vl().read_value() == tc.expected_vl);
        assert(vcu.valid().read() == logic::LogicState::HIGH);

        // Commit to register via clock edge
        pulse_clock(clock, vcu);

        assert(vcu.vl().read_value() == tc.expected_vl);

        std::cout << "  PASS: " << tc.name
                  << " -> VLMAX=" << vcu.vlmax().read_value()
                  << ", VL=" << vcu.vl().read_value() << "\n";
    }
}

// 2. Test the VTYPE output
static void test_vtype_output()
{
    std::cout << "[Test 2] VTYPE output matches selected SEW...\n";

    struct TestCase {
        unsigned sew;
        unsigned expected_vtype;
        std::uint64_t avl;
        unsigned expected_vlmax;
        unsigned expected_vl;
    };

    const TestCase cases[] = {
        {0, 0, 16, 16, 16},
        {1, 1, 20,  8,  8}, // Prompt example: AVL=20, SEW=01 -> VLMAX=8, VL=8, VTYPE=01
        {2, 2, 10,  4,  4}
    };

    for (const auto& tc : cases) {
        logic::Bus<8> avl;
        logic::Bus<2> sew;
        logic::Wire clock(logic::LogicState::LOW);
        logic::Wire configure(logic::LogicState::HIGH);
        logic::Wire reset(logic::LogicState::LOW);

        cpu::VectorConfigurationUnit vcu(avl, sew, clock, configure, reset);

        avl.write_value(tc.avl);
        sew.write_value(tc.sew);

        pulse_clock(clock, vcu);

        assert(vcu.vlmax().read_value() == tc.expected_vlmax);
        assert(vcu.vl().read_value() == tc.expected_vl);
        assert(vcu.vtype().read_value() == tc.expected_vtype);

        std::cout << "  PASS: SEW=" << tc.sew
                  << " -> VLMAX=" << vcu.vlmax().read_value()
                  << ", VL=" << vcu.vl().read_value()
                  << ", VTYPE=" << vcu.vtype().read_value() << "\n";
    }
}

// 3. Test register updates on clock edges
static void test_register_updates_on_clock_edges()
{
    std::cout << "[Test 3] Register updates on active clock edges...\n";

    logic::Bus<8> avl;
    logic::Bus<2> sew;
    logic::Wire clock(logic::LogicState::LOW);
    logic::Wire configure(logic::LogicState::HIGH);
    logic::Wire reset(logic::LogicState::LOW);

    cpu::VectorConfigurationUnit vcu(avl, sew, clock, configure, reset);

    // Initial state setup: VL = 4, VTYPE = 10 (2)
    avl.write_value(10);
    sew.write_value(2);
    pulse_clock(clock, vcu);

    assert(vcu.vl().read_value() == 4);
    assert(vcu.vtype().read_value() == 2);
    std::cout << "  Initial state established: VL=" << vcu.vl().read_value()
              << ", VTYPE=" << vcu.vtype().read_value() << "\n";

    // Apply new configuration: AVL = 12, SEW = 01 (1)
    avl.write_value(12);
    sew.write_value(1);

    // Setup phase: clock is LOW
    clock.write(logic::LogicState::LOW);
    vcu.evaluate();

    // Verify combinational next VL is 8
    assert(vcu.next_vl().read_value() == 8);
    std::cout << "  Combinational next VL = " << vcu.next_vl().read_value() << "\n";

    // Verify stored register outputs haven't changed before active clock edge
    assert(vcu.vl().read_value() == 4);
    assert(vcu.vtype().read_value() == 2);
    std::cout << "  Pre-clock registers unchanged: VL=" << vcu.vl().read_value()
              << ", VTYPE=" << vcu.vtype().read_value() << "\n";

    // Apply active clock edge: LOW -> HIGH
    clock.write(logic::LogicState::HIGH);
    vcu.evaluate();

    // Verify registers updated on active clock edge
    assert(vcu.vl().read_value() == 8);
    assert(vcu.vtype().read_value() == 1);
    std::cout << "  PASS: Post-clock registers updated: VL=" << vcu.vl().read_value()
              << ", VTYPE=" << vcu.vtype().read_value() << "\n";
}

// 4. Test that an invalid SEW is rejected
static void test_invalid_sew_rejected()
{
    std::cout << "[Test 4] Invalid SEW (11) rejection...\n";

    logic::Bus<8> avl;
    logic::Bus<2> sew;
    logic::Wire clock(logic::LogicState::LOW);
    logic::Wire configure(logic::LogicState::HIGH);
    logic::Wire reset(logic::LogicState::LOW);

    cpu::VectorConfigurationUnit vcu(avl, sew, clock, configure, reset);

    // Stage A: Invalid configuration at startup
    std::cout << "  Stage A: Invalid configuration at startup (SEW=11)...\n";
    avl.write_value(16);
    sew.write_value(3); // SEW = 11

    vcu.evaluate_combinational();

    assert(vcu.vlmax().read_value() == 0);
    assert(vcu.valid().read() == logic::LogicState::LOW);
    assert(vcu.write_enable().read() == logic::LogicState::LOW);

    pulse_clock(clock, vcu);

    // Registers must not have accepted a valid write
    assert(vcu.vl().read_value() == 0);
    assert(vcu.vtype().read_value() == 0);
    assert(vcu.valid().read() == logic::LogicState::LOW);
    std::cout << "  PASS: Stage A rejected invalid config at startup.\n";

    // Stage B: Invalid configuration after a valid configuration
    std::cout << "  Stage B: Invalid configuration after valid state (VL=8, VTYPE=01)...\n";
    // Establish valid state: AVL=12, SEW=01
    avl.write_value(12);
    sew.write_value(1);
    pulse_clock(clock, vcu);

    assert(vcu.vl().read_value() == 8);
    assert(vcu.vtype().read_value() == 1);
    assert(vcu.valid().read() == logic::LogicState::HIGH);

    // Now request invalid configuration: AVL=12, SEW=11
    sew.write_value(3); // SEW = 11
    vcu.evaluate_combinational();
    assert(vcu.valid().read() == logic::LogicState::LOW);
    assert(vcu.write_enable().read() == logic::LogicState::LOW);

    pulse_clock(clock, vcu);

    // Verify existing configuration remains unchanged
    assert(vcu.vl().read_value() == 8);
    assert(vcu.vtype().read_value() == 1);
    std::cout << "  PASS: Stage B preserved state (VL=8, VTYPE=01) on invalid update.\n";
}

// 5. Test write-enable behavior
static void test_write_enable_behavior()
{
    std::cout << "[Test 5] Write-enable (configure) behavior...\n";

    logic::Bus<8> avl;
    logic::Bus<2> sew;
    logic::Wire clock(logic::LogicState::LOW);
    logic::Wire configure(logic::LogicState::HIGH);
    logic::Wire reset(logic::LogicState::LOW);

    cpu::VectorConfigurationUnit vcu(avl, sew, clock, configure, reset);

    // Initial valid setup: AVL=12, SEW=01 -> VL=8, VTYPE=01
    avl.write_value(12);
    sew.write_value(1);
    pulse_clock(clock, vcu);

    assert(vcu.vl().read_value() == 8);
    assert(vcu.vtype().read_value() == 1);

    // Test: Write-enable LOW
    std::cout << "  Testing Write-enable LOW...\n";
    avl.write_value(20);
    sew.write_value(0); // Valid config (SEW=00 -> VLMAX=16)
    configure.write(logic::LogicState::LOW);

    pulse_clock(clock, vcu);

    assert(vcu.vl().read_value() == 8);
    assert(vcu.vtype().read_value() == 1);
    std::cout << "  PASS: Write-enable LOW preserved previous VL and VTYPE.\n";

    // Test: Write-enable HIGH
    std::cout << "  Testing Write-enable HIGH...\n";
    configure.write(logic::LogicState::HIGH);

    pulse_clock(clock, vcu);

    assert(vcu.vl().read_value() == 16);
    assert(vcu.vtype().read_value() == 0);
    std::cout << "  PASS: Write-enable HIGH committed new configuration (VL=16, VTYPE=00).\n";

    // Test: Deasserting enable preserves last configuration
    std::cout << "  Testing deasserting Write-enable...\n";
    configure.write(logic::LogicState::LOW);
    avl.write_value(4);
    sew.write_value(2);

    pulse_clock(clock, vcu);

    assert(vcu.vl().read_value() == 16);
    assert(vcu.vtype().read_value() == 0);
    std::cout << "  PASS: Deasserting enable preserved configuration.\n";
}

// 6. Test configuration changes (successive updates on single instance)
static void test_configuration_changes()
{
    std::cout << "[Test 6] Successive configuration changes on single instance...\n";

    logic::Bus<8> avl;
    logic::Bus<2> sew;
    logic::Wire clock(logic::LogicState::LOW);
    logic::Wire configure(logic::LogicState::HIGH);
    logic::Wire reset(logic::LogicState::LOW);

    cpu::VectorConfigurationUnit vcu(avl, sew, clock, configure, reset);

    struct Step {
        int update_num;
        std::uint64_t avl;
        unsigned sew;
        unsigned expected_vl;
        unsigned expected_vtype;
    };

    const Step steps[] = {
        {1,  20, 0, 16, 0},
        {2,  12, 1,  8, 1},
        {3,   3, 2,  3, 2},
        {4, 255, 2,  4, 2},
        {5,   7, 0,  7, 0}
    };

    for (const auto& step : steps) {
        avl.write_value(step.avl);
        sew.write_value(step.sew);

        pulse_clock(clock, vcu);

        assert(vcu.vl().read_value() == step.expected_vl);
        assert(vcu.vtype().read_value() == step.expected_vtype);

        std::cout << "  PASS: Update " << step.update_num
                  << " (AVL=" << step.avl << ", SEW=" << step.sew
                  << ") -> VL=" << vcu.vl().read_value()
                  << ", VTYPE=" << vcu.vtype().read_value() << "\n";
    }
}

// 7. Test UNKNOWN propagation
static void test_unknown_propagation()
{
    std::cout << "[Test 7] UNKNOWN propagation and write inhibition...\n";

    logic::Bus<8> avl;
    logic::Bus<2> sew;
    logic::Wire clock(logic::LogicState::LOW);
    logic::Wire configure(logic::LogicState::HIGH);
    logic::Wire reset(logic::LogicState::LOW);

    cpu::VectorConfigurationUnit vcu(avl, sew, clock, configure, reset);

    // Initial known state: VL=7, VTYPE=00
    avl.write_value(7);
    sew.write_value(0);
    pulse_clock(clock, vcu);
    assert(vcu.vl().read_value() == 7);
    assert(vcu.vtype().read_value() == 0);

    // Case 7.1: SEW = 0X (sew[0]=UNKNOWN, sew[1]=LOW)
    std::cout << "  Testing SEW = 0X...\n";
    sew[0].write(logic::LogicState::UNKNOWN);
    sew[1].write(logic::LogicState::LOW);
    avl.write_value(20);

    vcu.evaluate_combinational();
    assert(vcu.valid().read() == logic::LogicState::UNKNOWN);
    assert(vcu.write_enable().read() == logic::LogicState::LOW);

    pulse_clock(clock, vcu);
    // State must NOT be corrupted
    assert(vcu.vl().read_value() == 7);
    assert(vcu.vtype().read_value() == 0);
    std::cout << "  PASS: SEW = 0X did not commit write; state preserved.\n";

    // Case 7.2: SEW = X0 (sew[0]=LOW, sew[1]=UNKNOWN)
    std::cout << "  Testing SEW = X0...\n";
    sew[0].write(logic::LogicState::LOW);
    sew[1].write(logic::LogicState::UNKNOWN);

    vcu.evaluate_combinational();
    assert(vcu.valid().read() == logic::LogicState::UNKNOWN);
    assert(vcu.write_enable().read() == logic::LogicState::LOW);

    pulse_clock(clock, vcu);
    assert(vcu.vl().read_value() == 7);
    assert(vcu.vtype().read_value() == 0);
    std::cout << "  PASS: SEW = X0 did not commit write; state preserved.\n";

    // Case 7.3: Unknown bit in AVL
    std::cout << "  Testing unknown bit in AVL...\n";
    sew.write_value(1); // valid SEW = 01 (16 bits)
    avl.write_value(10);
    avl[0].write(logic::LogicState::UNKNOWN); // make bit 0 UNKNOWN

    vcu.evaluate_combinational();
    assert(vcu.valid().read() == logic::LogicState::UNKNOWN);
    assert(vcu.write_enable().read() == logic::LogicState::LOW);

    pulse_clock(clock, vcu);
    assert(vcu.vl().read_value() == 7);
    assert(vcu.vtype().read_value() == 0);
    std::cout << "  PASS: Unknown AVL bit inhibited write; state preserved.\n";

    // Case 7.4: Recovery from UNKNOWN
    std::cout << "  Testing recovery from UNKNOWN to valid configuration...\n";
    avl.write_value(10);
    sew.write_value(2); // SEW = 10 -> VLMAX = 4
    vcu.evaluate_combinational();
    assert(vcu.valid().read() == logic::LogicState::HIGH);
    assert(vcu.write_enable().read() == logic::LogicState::HIGH);

    pulse_clock(clock, vcu);
    assert(vcu.vl().read_value() == 4);
    assert(vcu.vtype().read_value() == 2);
    std::cout << "  PASS: Successfully recovered and committed valid config.\n";
}

// 8. Test reset behavior
static void test_reset_behavior()
{
    std::cout << "[Test 8] Reset behavior independently of configuration writes...\n";

    logic::Bus<8> avl;
    logic::Bus<2> sew;
    logic::Wire clock(logic::LogicState::LOW);
    logic::Wire configure(logic::LogicState::HIGH);
    logic::Wire reset(logic::LogicState::LOW);

    cpu::VectorConfigurationUnit vcu(avl, sew, clock, configure, reset);

    // Establish known non-zero state: VL=8, VTYPE=01
    avl.write_value(12);
    sew.write_value(1);
    pulse_clock(clock, vcu);
    assert(vcu.vl().read_value() == 8);
    assert(vcu.vtype().read_value() == 1);
    std::cout << "  Initial non-zero state: VL=" << vcu.vl().read_value()
              << ", VTYPE=" << vcu.vtype().read_value() << "\n";

    // Assert reset HIGH
    std::cout << "  Asserting reset...\n";
    reset.write(logic::LogicState::HIGH);
    pulse_clock(clock, vcu);

    assert(vcu.vl().read_value() == 0);
    assert(vcu.vtype().read_value() == 0);
    std::cout << "  PASS: Reset placed registers in state VL=0, VTYPE=00.\n";

    // Release reset with configure=LOW
    std::cout << "  Releasing reset with configure=LOW...\n";
    reset.write(logic::LogicState::LOW);
    configure.write(logic::LogicState::LOW);
    avl.write_value(20);
    sew.write_value(0);

    pulse_clock(clock, vcu);

    // Verify releasing reset didn't cause unintended update
    assert(vcu.vl().read_value() == 0);
    assert(vcu.vtype().read_value() == 0);
    std::cout << "  PASS: Releasing reset did not cause unintended configuration update.\n";
}

int main()
{
    std::cout << "==================================================\n";
    std::cout << "Running VectorConfigurationUnit Comprehensive Tests\n";
    std::cout << "==================================================\n\n";

    test_configuration_calculations();
    std::cout << "\n";

    test_vtype_output();
    std::cout << "\n";

    test_register_updates_on_clock_edges();
    std::cout << "\n";

    test_invalid_sew_rejected();
    std::cout << "\n";

    test_write_enable_behavior();
    std::cout << "\n";

    test_configuration_changes();
    std::cout << "\n";

    test_unknown_propagation();
    std::cout << "\n";

    test_reset_behavior();
    std::cout << "\n";

    std::cout << "==================================================\n";
    std::cout << "All VectorConfigurationUnit tests PASSED!\n";
    std::cout << "==================================================\n";

    return 0;
}

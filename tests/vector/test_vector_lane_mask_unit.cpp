/*
    test_vector_lane_mask_unit.cpp

    Comprehensive test suite for cpu::VectorLaneMaskUnit:
    1. Expected activation masks (VL = 0 to 4, plus saturation values VL = 5 and VL = 255)
    2. Changing VL repeatedly on a single instance without reconstruction
    3. Lane ordering (inspecting individual wires to verify comparator-to-lane mapping)
    4. Comparator boundary condition (active[i] = (i < VL); lane is inactive when index == VL)
    5. Constant lane indices (verifying indices_[0..3] contain 0, 1, 2, 3 in 8-bit buses)
    6. Invalid or unknown inputs (handling LogicState::UNKNOWN and recovery)
*/

#include "configuration/VectorLaneMaskUnit.hpp"

#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/signals/logicState.hpp>

#include <iostream>
#include <iomanip>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>

static std::string mask_to_string(const logic::Bus<4>& bus)
{
    std::string s;
    for (std::size_t i = 4; i > 0; --i)
    {
        s += logic::to_char(bus[i - 1].read());
    }
    return s;
}

// -----------------------------------------------------------------------------
// 1. Test the expected activation masks
// -----------------------------------------------------------------------------
static void test_expected_activation_masks()
{
    std::cout << "----------------------------------------\n";
    std::cout << " 1. Expected Activation Masks\n";
    std::cout << "----------------------------------------\n";

    struct TestCase
    {
        const char* label;
        std::uint64_t vl;
        unsigned expected_mask; // binary mask: bit 0 = lane 0, bit 3 = lane 3
    };

    const TestCase cases[] = {
        {"Test 1: VL = 0",   0,   0b0000},
        {"Test 2: VL = 1",   1,   0b0001},
        {"Test 3: VL = 2",   2,   0b0011},
        {"Test 4: VL = 3",   3,   0b0111},
        {"Test 5: VL = 4",   4,   0b1111},
        {"Test 6: VL = 5",   5,   0b1111},
        {"Test 7: VL = 255", 255, 0b1111}
    };

    for (const auto& tc : cases)
    {
        logic::Bus<8> vl_bus;
        logic::Bus<4> mask_bus;

        cpu::VectorLaneMaskUnit unit(vl_bus, mask_bus);

        vl_bus.write_value(tc.vl);
        unit.evaluate();

        const std::uint64_t actual_mask = mask_bus.read_value();
        const std::string mask_str = mask_to_string(mask_bus);

        std::cout << "  " << tc.label
                  << " (VL=" << std::setw(3) << tc.vl << ")"
                  << " -> Mask (Lane 3->0): " << mask_str
                  << " (numeric: 0x" << std::hex << actual_mask << std::dec << ")\n";

        // Verify full bus numeric value
        assert(actual_mask == tc.expected_mask);

        // Verify each individual wire
        for (std::size_t lane = 0; lane < 4; ++lane)
        {
            const bool bit_expected = (tc.expected_mask >> lane) & 1U;
            const logic::LogicState expected_state =
                bit_expected ? logic::LogicState::HIGH : logic::LogicState::LOW;

            assert(mask_bus[lane].read() == expected_state);
        }

        std::cout << "    [PASS] " << tc.label << "\n";
    }
}

// -----------------------------------------------------------------------------
// 2. Test changing VL repeatedly on a single instance
// -----------------------------------------------------------------------------
static void test_changing_vl_repeatedly()
{
    std::cout << "\n----------------------------------------\n";
    std::cout << " 2. Changing VL Repeatedly (Single Instance)\n";
    std::cout << "----------------------------------------\n";

    logic::Bus<8> vl_bus;
    logic::Bus<4> mask_bus;

    // Single instance constructed once
    cpu::VectorLaneMaskUnit unit(vl_bus, mask_bus);

    struct Step
    {
        std::uint64_t vl;
        unsigned expected_mask;
    };

    // Sequence specified: 4 -> 2 -> 0 -> 3 -> 1, followed by saturation and re-check
    const Step sequence[] = {
        {4,   0b1111},
        {2,   0b0011},
        {0,   0b0000},
        {3,   0b0111},
        {1,   0b0001},
        {5,   0b1111}, // test transition to saturation
        {2,   0b0011}, // test transition down from saturation
        {255, 0b1111}, // test max 8-bit VL
        {0,   0b0000}  // test full reset to zero
    };

    for (std::size_t step = 0; step < sizeof(sequence)/sizeof(sequence[0]); ++step)
    {
        const auto& s = sequence[step];
        vl_bus.write_value(s.vl);
        unit.evaluate();

        const std::uint64_t actual_mask = mask_bus.read_value();
        const std::string mask_str = mask_to_string(mask_bus);

        std::cout << "  Step " << (step + 1)
                  << ": VL=" << std::setw(3) << s.vl
                  << " -> Got Mask: " << mask_str
                  << " (Expected: 0x" << std::hex << s.expected_mask << std::dec << ")\n";

        assert(actual_mask == s.expected_mask);

        for (std::size_t lane = 0; lane < 4; ++lane)
        {
            const bool bit_expected = (s.expected_mask >> lane) & 1U;
            assert(mask_bus[lane].read() ==
                   (bit_expected ? logic::LogicState::HIGH : logic::LogicState::LOW));
        }
    }

    std::cout << "  [PASS] All successive VL transitions evaluated correctly on single instance.\n";
}

// -----------------------------------------------------------------------------
// 3. Test lane ordering (wire-by-wire inspection)
// -----------------------------------------------------------------------------
static void test_lane_ordering()
{
    std::cout << "\n----------------------------------------\n";
    std::cout << " 3. Lane Ordering (Wire-by-Wire Inspection)\n";
    std::cout << "----------------------------------------\n";

    logic::Bus<8> vl_bus;
    logic::Bus<4> mask_bus;
    cpu::VectorLaneMaskUnit unit(vl_bus, mask_bus);

    // VL = 1: only lane_activation_[0] must be HIGH
    vl_bus.write_value(1);
    unit.evaluate();
    std::cout << "  VL = 1: lane_activation_[0]=" << mask_bus[0].read()
              << ", [1]=" << mask_bus[1].read()
              << ", [2]=" << mask_bus[2].read()
              << ", [3]=" << mask_bus[3].read() << "\n";
    assert(mask_bus[0].read() == logic::LogicState::HIGH);
    assert(mask_bus[1].read() == logic::LogicState::LOW);
    assert(mask_bus[2].read() == logic::LogicState::LOW);
    assert(mask_bus[3].read() == logic::LogicState::LOW);
    std::cout << "    [PASS] VL = 1 correctly drives only Lane 0.\n";

    // VL = 2: lane_activation_[0] and [1] must be HIGH
    vl_bus.write_value(2);
    unit.evaluate();
    std::cout << "  VL = 2: lane_activation_[0]=" << mask_bus[0].read()
              << ", [1]=" << mask_bus[1].read()
              << ", [2]=" << mask_bus[2].read()
              << ", [3]=" << mask_bus[3].read() << "\n";
    assert(mask_bus[0].read() == logic::LogicState::HIGH);
    assert(mask_bus[1].read() == logic::LogicState::HIGH);
    assert(mask_bus[2].read() == logic::LogicState::LOW);
    assert(mask_bus[3].read() == logic::LogicState::LOW);
    std::cout << "    [PASS] VL = 2 correctly drives Lanes 0 and 1.\n";

    // VL = 3: lane_activation_[0], [1], and [2] must be HIGH, [3] remains LOW
    vl_bus.write_value(3);
    unit.evaluate();
    std::cout << "  VL = 3: lane_activation_[0]=" << mask_bus[0].read()
              << ", [1]=" << mask_bus[1].read()
              << ", [2]=" << mask_bus[2].read()
              << ", [3]=" << mask_bus[3].read() << "\n";
    assert(mask_bus[0].read() == logic::LogicState::HIGH);
    assert(mask_bus[1].read() == logic::LogicState::HIGH);
    assert(mask_bus[2].read() == logic::LogicState::HIGH);
    assert(mask_bus[3].read() == logic::LogicState::LOW);
    std::cout << "    [PASS] VL = 3 correctly drives Lanes 0, 1, and 2, while Lane 3 is LOW.\n";
}

// -----------------------------------------------------------------------------
// 4. Test the comparator boundary condition (active[i] = (i < VL))
// -----------------------------------------------------------------------------
static void test_comparator_boundary_condition()
{
    std::cout << "\n----------------------------------------\n";
    std::cout << " 4. Comparator Boundary Condition (i == VL Inactive)\n";
    std::cout << "----------------------------------------\n";

    logic::Bus<8> vl_bus;
    logic::Bus<4> mask_bus;
    cpu::VectorLaneMaskUnit unit(vl_bus, mask_bus);

    // Test boundary for each possible equality condition: VL = 0, 1, 2, 3
    for (std::size_t boundary_vl = 0; boundary_vl < 4; ++boundary_vl)
    {
        vl_bus.write_value(boundary_vl);
        unit.evaluate();

        std::cout << "  Testing boundary VL = " << boundary_vl << ":\n";

        for (std::size_t lane = 0; lane < 4; ++lane)
        {
            const logic::LogicState activation = mask_bus[lane].read();
            const logic::LogicState eq_flag = unit.get_equal_flag(lane);
            const logic::LogicState gt_flag = unit.get_greater_flag(lane);

            std::cout << "    Lane " << lane
                      << " (" << lane << " < " << boundary_vl << "): activation=" << activation
                      << ", equal=" << eq_flag
                      << ", greater=" << gt_flag << "\n";

            if (lane < boundary_vl)
            {
                // Strict inequality satisfied: active
                assert(activation == logic::LogicState::HIGH);
                assert(eq_flag == logic::LogicState::LOW);
                assert(gt_flag == logic::LogicState::LOW);
            }
            else if (lane == boundary_vl)
            {
                // Boundary condition: lane_index == VL must be INACTIVE
                assert(activation == logic::LogicState::LOW);
                // Equal flag must be asserted
                assert(eq_flag == logic::LogicState::HIGH);
                assert(gt_flag == logic::LogicState::LOW);
            }
            else
            {
                // lane_index > VL: inactive
                assert(activation == logic::LogicState::LOW);
                assert(eq_flag == logic::LogicState::LOW);
                assert(gt_flag == logic::LogicState::HIGH);
            }
        }
        std::cout << "    [PASS] Boundary VL = " << boundary_vl << " strictly adheres to (i < VL).\n";
    }
}

// -----------------------------------------------------------------------------
// 5. Test constant lane indices initialization
// -----------------------------------------------------------------------------
static void test_constant_lane_indices()
{
    std::cout << "\n----------------------------------------\n";
    std::cout << " 5. Constant Lane Indices Initialization\n";
    std::cout << "----------------------------------------\n";

    logic::Bus<8> vl_bus;
    logic::Bus<4> mask_bus;
    cpu::VectorLaneMaskUnit unit(vl_bus, mask_bus);

    for (std::size_t lane = 0; lane < 4; ++lane)
    {
        const auto& bus = unit.get_lane_index(lane);
        const std::uint64_t bus_value = bus.read_value();

        std::cout << "  indices_[" << lane << "]: value = " << bus_value << " (wires: ";
        for (std::size_t bit = 8; bit > 0; --bit)
        {
            std::cout << logic::to_char(bus[bit - 1].read());
        }
        std::cout << ")\n";

        // Verify numeric value
        assert(bus_value == lane);

        // Verify every individual wire matches exact binary representation
        for (std::size_t bit = 0; bit < 8; ++bit)
        {
            const bool expected_bit = (lane >> bit) & 1U;
            const logic::LogicState expected_state =
                expected_bit ? logic::LogicState::HIGH : logic::LogicState::LOW;

            assert(bus[bit].read() == expected_state);
        }

        std::cout << "    [PASS] indices_[" << lane << "] initialized correctly to " << lane << ".\n";
    }

    // Also verify accessibility through indices() array accessor
    assert(unit.indices().size() == 4);
    for (std::size_t lane = 0; lane < 4; ++lane)
    {
        assert(unit.indices()[lane].read_value() == lane);
    }
}

// -----------------------------------------------------------------------------
// 6. Test invalid or unknown inputs (LogicState::UNKNOWN)
// -----------------------------------------------------------------------------
static void test_invalid_or_unknown_inputs()
{
    std::cout << "\n----------------------------------------\n";
    std::cout << " 6. Invalid / Unknown Inputs & Recovery\n";
    std::cout << "----------------------------------------\n";

    logic::Bus<8> vl_bus;
    logic::Bus<4> mask_bus;
    cpu::VectorLaneMaskUnit unit(vl_bus, mask_bus);

    // Scenario 6.1: All bits of VL are UNKNOWN
    std::cout << "  Scenario 6.1: Entire VL bus = UNKNOWN (XXXXXXXX)...\n";
    vl_bus.write(logic::LogicState::UNKNOWN);
    unit.evaluate();

    for (std::size_t lane = 0; lane < 4; ++lane)
    {
        std::cout << "    Lane " << lane << " activation: " << mask_bus[lane].read() << "\n";
        assert(mask_bus[lane].read() == logic::LogicState::UNKNOWN);
    }
    std::cout << "    [PASS] All lanes evaluate to UNKNOWN when VL is entirely UNKNOWN.\n";

    // Scenario 6.2: VL = 0000000X (Bit 0 is UNKNOWN, Bits 1..7 are LOW)
    // Lane 0 (00000000): 0 < X is uncertain -> UNKNOWN
    // Lane 1 (00000001): 1 < X is impossible for 1-bit X in {0,1} -> LOW
    // Lane 2 (00000010): 2 < X is impossible -> LOW
    // Lane 3 (00000011): 3 < X is impossible -> LOW
    std::cout << "  Scenario 6.2: VL = 0000000X (Bit 0 = UNKNOWN, Bits 1..7 = LOW)...\n";
    vl_bus.write_value(0);
    vl_bus[0].write(logic::LogicState::UNKNOWN);
    unit.evaluate();

    std::cout << "    Lane 0 activation: " << mask_bus[0].read() << " (expected: X)\n";
    std::cout << "    Lane 1 activation: " << mask_bus[1].read() << " (expected: 0)\n";
    std::cout << "    Lane 2 activation: " << mask_bus[2].read() << " (expected: 0)\n";
    std::cout << "    Lane 3 activation: " << mask_bus[3].read() << " (expected: 0)\n";

    assert(mask_bus[0].read() == logic::LogicState::UNKNOWN);
    assert(mask_bus[1].read() == logic::LogicState::LOW);
    assert(mask_bus[2].read() == logic::LogicState::LOW);
    assert(mask_bus[3].read() == logic::LogicState::LOW);
    std::cout << "    [PASS] 0000000X produces UNKNOWN for Lane 0 and definite LOW for Lanes 1..3.\n";

    // Scenario 6.3: VL has MSB HIGH with lower bits UNKNOWN (e.g. 1XXXXXXX)
    // Since Lane indices are in {0, 1, 2, 3}, their MSBs (bit 7) are 0.
    // Comparator MSB comparison (0 < 1) resolves to definitely HIGH regardless of lower UNKNOWN bits!
    std::cout << "  Scenario 6.3: VL = 1XXXXXXX (Bit 7 = HIGH, Bits 0..6 = UNKNOWN)...\n";
    vl_bus.write(logic::LogicState::UNKNOWN);
    vl_bus[7].write(logic::LogicState::HIGH);
    unit.evaluate();

    for (std::size_t lane = 0; lane < 4; ++lane)
    {
        std::cout << "    Lane " << lane << " activation: " << mask_bus[lane].read() << " (expected: 1)\n";
        assert(mask_bus[lane].read() == logic::LogicState::HIGH);
    }
    std::cout << "    [PASS] High MSB resolves comparison to definitely HIGH despite lower unknown bits.\n";

    // Scenario 6.4: Recovery from UNKNOWN to valid vector length (VL = 2)
    std::cout << "  Scenario 6.4: Recovery from UNKNOWN to valid VL = 2...\n";
    vl_bus.write_value(2);
    unit.evaluate();

    std::cout << "    Mask (Lane 3->0): " << mask_to_string(mask_bus) << " (expected: 0011)\n";
    assert(mask_bus.read_value() == 0b0011);
    assert(mask_bus[0].read() == logic::LogicState::HIGH);
    assert(mask_bus[1].read() == logic::LogicState::HIGH);
    assert(mask_bus[2].read() == logic::LogicState::LOW);
    assert(mask_bus[3].read() == logic::LogicState::LOW);
    std::cout << "    [PASS] Successfully recovered from UNKNOWN to correct mask 0011.\n";
}

int main()
{
    std::cout << "========================================\n";
    std::cout << "Running VectorLaneMaskUnit Tests\n";
    std::cout << "========================================\n\n";

    test_expected_activation_masks();
    test_changing_vl_repeatedly();
    test_lane_ordering();
    test_comparator_boundary_condition();
    test_constant_lane_indices();
    test_invalid_or_unknown_inputs();

    std::cout << "\n========================================\n";
    std::cout << "All VectorLaneMaskUnit tests PASSED!\n";
    std::cout << "========================================\n";

    return 0;
}

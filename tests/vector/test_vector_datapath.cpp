#include "../../vector_cpu/datapath/vector_datapath.hpp"

#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/signals/logicState.hpp>

#include <array>
#include <cassert>
#include <cstdint>
#include <iomanip>
#include <iostream>

namespace {

void print_vec(const char* label, const std::array<std::uint32_t, 4>& v)
{
    std::cout << "    " << std::left << std::setw(22) << label << " = ["
              << v[0] << ", " << v[1] << ", " << v[2] << ", " << v[3] << "]\n";
}

void print_mask(const char* label, const std::array<std::uint8_t, 4>& m)
{
    std::cout << "    " << std::left << std::setw(22) << label << " = ["
              << static_cast<int>(m[0]) << ", "
              << static_cast<int>(m[1]) << ", "
              << static_cast<int>(m[2]) << ", "
              << static_cast<int>(m[3]) << "]\n";
}

// -----------------------------------------------------------------------------
// Test 1: Basic Vector Addition (VL = 4, writes disabled)
// -----------------------------------------------------------------------------
void test_1_basic_vector_addition()
{
    std::cout << "====================================================\n";
    std::cout << "Test 1: Basic Vector Addition (VL = 4)\n";
    std::cout << "====================================================\n";

    logic::Bus<5> rs1, rs2, rd, write_addr;
    logic::Bus<128> write_data, result;
    logic::Bus<3> opcode;
    logic::Bus<8> vl;
    logic::Wire clock(logic::LogicState::LOW);
    logic::Wire enable(logic::LogicState::LOW);
    logic::Wire reset(logic::LogicState::LOW);

    cpu::VectorDatapath dp(rs1, rs2, rd, write_data, write_addr, opcode, vl, clock, enable, reset, result);

    // Initial register values:
    // V1 = [10, 20, 30, 40]
    // V2 = [ 1,  2,  3,  4]
    // V3 = [ 0,  0,  0,  0]
    dp.initialize_register(1, {10, 20, 30, 40});
    dp.initialize_register(2, { 1,  2,  3,  4});
    dp.initialize_register(3, { 0,  0,  0,  0});

    // Setup Instruction: VADD V3, V1, V2 with VL = 4
    rs1.write_value(1);
    rs2.write_value(2);
    rd.write_value(3);
    opcode.write_value(0); // ALU ADD = 000
    vl.write_value(4);
    enable.write(logic::LogicState::LOW); // Writes disabled (combinational path only)

    dp.evaluate();

    // Verify intermediate signals
    auto rd1 = cpu::VectorDatapath::read_vector(dp.read_data1());
    auto rd2 = cpu::VectorDatapath::read_vector(dp.read_data2());
    auto rd3 = cpu::VectorDatapath::read_vector(dp.read_data3());
    auto exec_res = cpu::VectorDatapath::read_vector(dp.execution_result());
    auto mask = cpu::VectorDatapath::read_mask(dp.lane_mask());
    auto wb_res = cpu::VectorDatapath::read_vector(dp.writeback_result());
    auto out_res = cpu::VectorDatapath::read_vector(dp.result());

    std::cout << "Intermediate Signals:\n";
    print_vec("read_data1 (V1)", rd1);
    print_vec("read_data2 (V2)", rd2);
    print_vec("read_data3 (V3)", rd3);
    print_vec("execution_result", exec_res);
    print_mask("lane_mask", mask);
    print_vec("writeback_result", wb_res);
    print_vec("result", out_res);

    assert(rd1 == (std::array<std::uint32_t, 4>{10, 20, 30, 40}));
    assert(rd2 == (std::array<std::uint32_t, 4>{ 1,  2,  3,  4}));
    assert(rd3 == (std::array<std::uint32_t, 4>{ 0,  0,  0,  0}));
    assert(exec_res == (std::array<std::uint32_t, 4>{11, 22, 33, 44}));
    assert(mask == (std::array<std::uint8_t, 4>{1, 1, 1, 1}));
    assert(wb_res == (std::array<std::uint32_t, 4>{11, 22, 33, 44}));
    assert(out_res == (std::array<std::uint32_t, 4>{11, 22, 33, 44}));

    std::cout << "  [PASS] Test 1 passed successfully!\n\n";
}

// -----------------------------------------------------------------------------
// Test 2: Partial Vector Length (VL = 2)
// -----------------------------------------------------------------------------
void test_2_partial_vector_length()
{
    std::cout << "====================================================\n";
    std::cout << "Test 2: Partial Vector Length (VL = 2)\n";
    std::cout << "====================================================\n";

    logic::Bus<5> rs1, rs2, rd, write_addr;
    logic::Bus<128> write_data, result;
    logic::Bus<3> opcode;
    logic::Bus<8> vl;
    logic::Wire clock(logic::LogicState::LOW);
    logic::Wire enable(logic::LogicState::LOW);
    logic::Wire reset(logic::LogicState::LOW);

    cpu::VectorDatapath dp(rs1, rs2, rd, write_data, write_addr, opcode, vl, clock, enable, reset, result);

    // Initial register values:
    // V1 = [ 10,  20,  30,  40]
    // V2 = [  1,   2,   3,   4]
    // V3 = [100, 200, 300, 400]
    dp.initialize_register(1, { 10,  20,  30,  40});
    dp.initialize_register(2, {  1,   2,   3,   4});
    dp.initialize_register(3, {100, 200, 300, 400});

    // Instruction: VADD V3, V1, V2 with VL = 2
    rs1.write_value(1);
    rs2.write_value(2);
    rd.write_value(3);
    opcode.write_value(0); // ADD
    vl.write_value(2);
    enable.write(logic::LogicState::LOW);

    dp.evaluate();

    auto rd1 = cpu::VectorDatapath::read_vector(dp.read_data1());
    auto rd2 = cpu::VectorDatapath::read_vector(dp.read_data2());
    auto rd3 = cpu::VectorDatapath::read_vector(dp.read_data3());
    auto exec_res = cpu::VectorDatapath::read_vector(dp.execution_result());
    auto mask = cpu::VectorDatapath::read_mask(dp.lane_mask());
    auto wb_res = cpu::VectorDatapath::read_vector(dp.writeback_result());
    auto out_res = cpu::VectorDatapath::read_vector(dp.result());

    std::cout << "Intermediate Signals:\n";
    print_vec("read_data1 (V1)", rd1);
    print_vec("read_data2 (V2)", rd2);
    print_vec("read_data3 (V3)", rd3);
    print_vec("execution_result", exec_res);
    print_mask("lane_mask", mask);
    print_vec("writeback_result", wb_res);
    print_vec("result", out_res);

    // Notice that the execution unit still computes all four lanes [11, 22, 33, 44]
    assert(exec_res == (std::array<std::uint32_t, 4>{11, 22, 33, 44}));
    // Lane mask should only activate lanes 0 and 1
    assert(mask == (std::array<std::uint8_t, 4>{1, 1, 0, 0}));
    // Inactive lanes retain old destination values (300, 400)
    assert(wb_res == (std::array<std::uint32_t, 4>{11, 22, 300, 400}));
    assert(out_res == (std::array<std::uint32_t, 4>{11, 22, 300, 400}));

    std::cout << "  [PASS] Test 2 passed successfully!\n\n";
}

// -----------------------------------------------------------------------------
// Test 3: Boundary Values of VL (0, 1, 2, 3, 4, 5)
// -----------------------------------------------------------------------------
void test_3_boundary_values_of_vl()
{
    std::cout << "====================================================\n";
    std::cout << "Test 3: Boundary Values of VL (0, 1, 2, 3, 4, 5)\n";
    std::cout << "====================================================\n";

    logic::Bus<5> rs1, rs2, rd, write_addr;
    logic::Bus<128> write_data, result;
    logic::Bus<3> opcode;
    logic::Bus<8> vl;
    logic::Wire clock(logic::LogicState::LOW);
    logic::Wire enable(logic::LogicState::LOW);
    logic::Wire reset(logic::LogicState::LOW);

    cpu::VectorDatapath dp(rs1, rs2, rd, write_data, write_addr, opcode, vl, clock, enable, reset, result);

    // Initial register values:
    dp.initialize_register(1, { 10,  20,  30,  40});
    dp.initialize_register(2, {  1,   2,   3,   4});
    dp.initialize_register(3, {100, 200, 300, 400});

    rs1.write_value(1);
    rs2.write_value(2);
    rd.write_value(3);
    opcode.write_value(0); // ADD
    enable.write(logic::LogicState::LOW);

    struct VLCase {
        std::uint32_t vl_val;
        std::array<std::uint8_t, 4> expected_mask;
        std::array<std::uint32_t, 4> expected_wb;
        const char* description;
    };

    const std::array<VLCase, 6> test_cases{{
        {0, {0, 0, 0, 0}, {100, 200, 300, 400}, "VL = 0 (No lanes active)"},
        {1, {1, 0, 0, 0}, { 11, 200, 300, 400}, "VL = 1 (Only lane 0 active)"},
        {2, {1, 1, 0, 0}, { 11,  22, 300, 400}, "VL = 2 (Lanes 0, 1 active)"},
        {3, {1, 1, 1, 0}, { 11,  22,  33, 400}, "VL = 3 (Lanes 0, 1, 2 active)"},
        {4, {1, 1, 1, 1}, { 11,  22,  33,  44}, "VL = 4 (All lanes active)"},
        {5, {1, 1, 1, 1}, { 11,  22,  33,  44}, "VL = 5 (VL exceeds number of lanes)"}
    }};

    for (const auto& tc : test_cases)
    {
        std::cout << "  Testing " << tc.description << ":\n";
        vl.write_value(tc.vl_val);
        dp.evaluate();

        auto mask = cpu::VectorDatapath::read_mask(dp.lane_mask());
        auto exec = cpu::VectorDatapath::read_vector(dp.execution_result());
        auto wb = cpu::VectorDatapath::read_vector(dp.writeback_result());
        auto out = cpu::VectorDatapath::read_vector(dp.result());

        print_mask("lane_mask", mask);
        print_vec("writeback_result", wb);

        // Execution unit always computes all 4 lanes
        assert(exec == (std::array<std::uint32_t, 4>{11, 22, 33, 44}));
        assert(mask == tc.expected_mask);
        assert(wb == tc.expected_wb);
        assert(out == tc.expected_wb);
    }

    std::cout << "  [PASS] Test 3 passed successfully!\n\n";
}

// -----------------------------------------------------------------------------
// Test 4: Source Registers Selected Correctly
// -----------------------------------------------------------------------------
void test_4_source_register_selection()
{
    std::cout << "====================================================\n";
    std::cout << "Test 4: Correct Source-Register Selection\n";
    std::cout << "====================================================\n";

    logic::Bus<5> rs1, rs2, rd, write_addr;
    logic::Bus<128> write_data, result;
    logic::Bus<3> opcode;
    logic::Bus<8> vl;
    logic::Wire clock(logic::LogicState::LOW);
    logic::Wire enable(logic::LogicState::LOW);
    logic::Wire reset(logic::LogicState::LOW);

    cpu::VectorDatapath dp(rs1, rs2, rd, write_data, write_addr, opcode, vl, clock, enable, reset, result);

    // Initial distinct vectors:
    // V1 = [  1,   2,   3,   4]
    // V2 = [ 10,  20,  30,  40]
    // V3 = [100, 200, 300, 400]
    dp.initialize_register(1, {  1,   2,   3,   4});
    dp.initialize_register(2, { 10,  20,  30,  40});
    dp.initialize_register(3, {100, 200, 300, 400});

    vl.write_value(4);
    enable.write(logic::LogicState::LOW);

    // ---------------------------------------------------------
    // Subtest 4a: VADD V3, V1, V2
    // ---------------------------------------------------------
    std::cout << "  [4a] Commutative VADD V3, V1, V2 (rs1=1, rs2=2):\n";
    rs1.write_value(1);
    rs2.write_value(2);
    rd.write_value(3);
    opcode.write_value(0); // ADD
    dp.evaluate();

    auto rd1_a = cpu::VectorDatapath::read_vector(dp.read_data1());
    auto rd2_a = cpu::VectorDatapath::read_vector(dp.read_data2());
    auto wb_a = cpu::VectorDatapath::read_vector(dp.writeback_result());
    print_vec("read_data1", rd1_a);
    print_vec("read_data2", rd2_a);
    print_vec("writeback_result", wb_a);
    assert(rd1_a == (std::array<std::uint32_t, 4>{ 1,  2,  3,  4}));
    assert(rd2_a == (std::array<std::uint32_t, 4>{10, 20, 30, 40}));
    assert(wb_a == (std::array<std::uint32_t, 4>{11, 22, 33, 44}));

    // ---------------------------------------------------------
    // Subtest 4b: Swap source register addresses: VADD V3, V2, V1
    // ---------------------------------------------------------
    std::cout << "  [4b] Swapped source addresses VADD V3, V2, V1 (rs1=2, rs2=1):\n";
    rs1.write_value(2);
    rs2.write_value(1);
    rd.write_value(3);
    opcode.write_value(0); // ADD
    dp.evaluate();

    auto rd1_b = cpu::VectorDatapath::read_vector(dp.read_data1());
    auto rd2_b = cpu::VectorDatapath::read_vector(dp.read_data2());
    auto wb_b = cpu::VectorDatapath::read_vector(dp.writeback_result());
    print_vec("read_data1", rd1_b);
    print_vec("read_data2", rd2_b);
    print_vec("writeback_result", wb_b);
    assert(rd1_b == (std::array<std::uint32_t, 4>{10, 20, 30, 40}));
    assert(rd2_b == (std::array<std::uint32_t, 4>{ 1,  2,  3,  4}));
    assert(wb_b == (std::array<std::uint32_t, 4>{11, 22, 33, 44}));

    // ---------------------------------------------------------
    // Subtest 4c: Noncommutative Subtraction: VSUB V3, V1, V2
    // ---------------------------------------------------------
    std::cout << "  [4c] Noncommutative VSUB V3, V1, V2 (V1 - V2):\n";
    dp.initialize_register(1, {10, 20, 30, 40});
    dp.initialize_register(2, { 1,  2,  3,  4});
    dp.initialize_register(3, {100, 200, 300, 400});

    rs1.write_value(1);
    rs2.write_value(2);
    rd.write_value(3);
    opcode.write_value(1); // ALU SUB = 001
    dp.evaluate();

    auto rd1_c = cpu::VectorDatapath::read_vector(dp.read_data1());
    auto rd2_c = cpu::VectorDatapath::read_vector(dp.read_data2());
    auto exec_c = cpu::VectorDatapath::read_vector(dp.execution_result());
    auto wb_c = cpu::VectorDatapath::read_vector(dp.writeback_result());
    print_vec("read_data1", rd1_c);
    print_vec("read_data2", rd2_c);
    print_vec("execution_result", exec_c);
    print_vec("writeback_result", wb_c);
    assert(rd1_c == (std::array<std::uint32_t, 4>{10, 20, 30, 40}));
    assert(rd2_c == (std::array<std::uint32_t, 4>{ 1,  2,  3,  4}));
    assert(exec_c == (std::array<std::uint32_t, 4>{ 9, 18, 27, 36}));
    assert(wb_c == (std::array<std::uint32_t, 4>{ 9, 18, 27, 36}));

    // ---------------------------------------------------------
    // Subtest 4d: Reversed Operands VSUB V3, V2, V1
    // ---------------------------------------------------------
    std::cout << "  [4d] Reversed Noncommutative VSUB V3, V2, V1 (V2 - V1):\n";
    rs1.write_value(2);
    rs2.write_value(1);
    rd.write_value(3);
    opcode.write_value(1); // ALU SUB = 001
    dp.evaluate();

    auto rd1_d = cpu::VectorDatapath::read_vector(dp.read_data1());
    auto rd2_d = cpu::VectorDatapath::read_vector(dp.read_data2());
    auto exec_d = cpu::VectorDatapath::read_vector(dp.execution_result());
    auto wb_d = cpu::VectorDatapath::read_vector(dp.writeback_result());
    print_vec("read_data1", rd1_d);
    print_vec("read_data2", rd2_d);
    print_vec("execution_result", exec_d);
    print_vec("writeback_result", wb_d);

    // 1 - 10 = -9 (0xFFFFFFF7)
    // 2 - 20 = -18 (0xFFFFFFEE)
    // 3 - 30 = -27 (0xFFFFFFE5)
    // 4 - 40 = -36 (0xFFFFFFDC)
    const std::array<std::uint32_t, 4> expected_sub_rev{
        static_cast<std::uint32_t>(1 - 10),
        static_cast<std::uint32_t>(2 - 20),
        static_cast<std::uint32_t>(3 - 30),
        static_cast<std::uint32_t>(4 - 40)
    };
    assert(rd1_d == (std::array<std::uint32_t, 4>{ 1,  2,  3,  4}));
    assert(rd2_d == (std::array<std::uint32_t, 4>{10, 20, 30, 40}));
    assert(exec_d == expected_sub_rev);
    assert(wb_d == expected_sub_rev);
    assert(exec_d != exec_c);

    std::cout << "  [PASS] Test 4 passed successfully!\n\n";
}

// -----------------------------------------------------------------------------
// Test 5: Destination and Other-Register Preservation
// -----------------------------------------------------------------------------
void test_5_destination_and_other_register_preservation()
{
    std::cout << "====================================================\n";
    std::cout << "Test 5: Destination & Other-Register Preservation\n";
    std::cout << "====================================================\n";

    logic::Bus<5> rs1, rs2, rd, write_addr;
    logic::Bus<128> write_data, result;
    logic::Bus<3> opcode;
    logic::Bus<8> vl;
    logic::Wire clock(logic::LogicState::LOW);
    logic::Wire enable(logic::LogicState::LOW);
    logic::Wire reset(logic::LogicState::LOW);

    cpu::VectorDatapath dp(rs1, rs2, rd, write_data, write_addr, opcode, vl, clock, enable, reset, result);

    // Initialize:
    // V1 = [ 10,  20,  30,  40]
    // V2 = [  1,   2,   3,   4]
    // V3 = [100, 200, 300, 400]
    // V4 = [  7,   8,   9,  10]
    dp.initialize_register(1, { 10,  20,  30,  40});
    dp.initialize_register(2, {  1,   2,   3,   4});
    dp.initialize_register(3, {100, 200, 300, 400});
    dp.initialize_register(4, {  7,   8,   9,  10});

    // Execute Instruction: VADD V3, V1, V2 with VL = 4
    rs1.write_value(1);
    rs2.write_value(2);
    rd.write_value(3);
    opcode.write_value(0); // ADD
    vl.write_value(4);

    // Enable sequential writeback
    enable.write(logic::LogicState::HIGH);

    // Step through clock cycle: LOW -> HIGH -> LOW
    clock.write(logic::LogicState::LOW);
    dp.evaluate();
    clock.write(logic::LogicState::HIGH);
    dp.evaluate();
    clock.write(logic::LogicState::LOW);
    dp.evaluate();

    // Disable write enable after writeback cycle
    enable.write(logic::LogicState::LOW);
    dp.evaluate();

    // Inspect registers directly
    auto v1 = cpu::VectorDatapath::read_vector(dp.register_file().get_register(1).output());
    auto v2 = cpu::VectorDatapath::read_vector(dp.register_file().get_register(2).output());
    auto v3 = cpu::VectorDatapath::read_vector(dp.register_file().get_register(3).output());
    auto v4 = cpu::VectorDatapath::read_vector(dp.register_file().get_register(4).output());

    std::cout << "Register values after sequential writeback:\n";
    print_vec("V1 (expected unchanged)", v1);
    print_vec("V2 (expected unchanged)", v2);
    print_vec("V3 (expected updated)",   v3);
    print_vec("V4 (expected unchanged)", v4);

    assert(v3 == (std::array<std::uint32_t, 4>{11, 22, 33, 44}));
    assert(v1 == (std::array<std::uint32_t, 4>{10, 20, 30, 40}));
    assert(v2 == (std::array<std::uint32_t, 4>{ 1,  2,  3,  4}));
    assert(v4 == (std::array<std::uint32_t, 4>{ 7,  8,  9, 10}));

    std::cout << "  [PASS] Test 5 passed successfully!\n\n";
}

// -----------------------------------------------------------------------------
// Test 6: Clock-Edge Behavior
// -----------------------------------------------------------------------------
void test_6_clock_edge_behavior()
{
    std::cout << "====================================================\n";
    std::cout << "Test 6: Writes Happen at the Correct Clock Edge\n";
    std::cout << "====================================================\n";

    logic::Bus<5> rs1, rs2, rd, write_addr;
    logic::Bus<128> write_data, result;
    logic::Bus<3> opcode;
    logic::Bus<8> vl;
    logic::Wire clock(logic::LogicState::LOW);
    logic::Wire enable(logic::LogicState::LOW);
    logic::Wire reset(logic::LogicState::LOW);

    cpu::VectorDatapath dp(rs1, rs2, rd, write_data, write_addr, opcode, vl, clock, enable, reset, result);

    // 1. Initialize V1, V2, V3
    dp.initialize_register(1, { 10,  20,  30,  40});
    dp.initialize_register(2, {  1,   2,   3,   4});
    dp.initialize_register(3, {100, 200, 300, 400});

    // 2. Set the instruction inputs and calculate the writeback result
    rs1.write_value(1);
    rs2.write_value(2);
    rd.write_value(3);
    opcode.write_value(0); // ADD
    vl.write_value(4);
    clock.write(logic::LogicState::LOW);
    enable.write(logic::LogicState::LOW);
    dp.evaluate();

    auto initial_wb = cpu::VectorDatapath::read_vector(dp.writeback_result());
    std::cout << "  Calculated writeback result:\n";
    print_vec("writeback_result", initial_wb);
    assert(initial_wb == (std::array<std::uint32_t, 4>{11, 22, 33, 44}));

    // 3. Keep the clock LOW and enable writes
    std::cout << "  Phase 1: Clock LOW, Enable HIGH -> Evaluate datapath\n";
    clock.write(logic::LogicState::LOW);
    enable.write(logic::LogicState::HIGH);
    dp.evaluate();

    // 4. Verify that V3 has not changed while clock is LOW
    auto v3_low = cpu::VectorDatapath::read_vector(dp.register_file().get_register(3).output());
    print_vec("V3 during clock LOW", v3_low);
    assert(v3_low == (std::array<std::uint32_t, 4>{100, 200, 300, 400}));
    std::cout << "    [CONFIRMED] V3 remained unchanged during clock LOW.\n";

    // 5. Drive the clock HIGH (rising edge) and evaluate the sequential logic
    std::cout << "  Phase 2: Clock HIGH (active edge) -> Evaluate datapath\n";
    clock.write(logic::LogicState::HIGH);
    dp.evaluate();

    // 6. Verify that V3 now contains the expected result
    auto v3_high = cpu::VectorDatapath::read_vector(dp.register_file().get_register(3).output());
    print_vec("V3 after clock HIGH", v3_high);
    assert(v3_high == (std::array<std::uint32_t, 4>{11, 22, 33, 44}));
    std::cout << "    [CONFIRMED] V3 updated to expected result on active edge.\n";

    // 7. Complete cycle and disable write enable
    clock.write(logic::LogicState::LOW);
    enable.write(logic::LogicState::LOW);
    dp.evaluate();

    auto v3_final = cpu::VectorDatapath::read_vector(dp.register_file().get_register(3).output());
    assert(v3_final == (std::array<std::uint32_t, 4>{11, 22, 33, 44}));

    std::cout << "  [PASS] Test 6 passed successfully!\n\n";
}

// -----------------------------------------------------------------------------
// Test 7: Reset Behavior
// -----------------------------------------------------------------------------
void test_7_reset_behavior()
{
    std::cout << "====================================================\n";
    std::cout << "Test 7: Reset Behavior\n";
    std::cout << "====================================================\n";

    logic::Bus<5> rs1, rs2, rd, write_addr;
    logic::Bus<128> write_data, result;
    logic::Bus<3> opcode;
    logic::Bus<8> vl;
    logic::Wire clock(logic::LogicState::LOW);
    logic::Wire enable(logic::LogicState::LOW);
    logic::Wire reset(logic::LogicState::LOW);

    cpu::VectorDatapath dp(rs1, rs2, rd, write_data, write_addr, opcode, vl, clock, enable, reset, result);

    // ---------------------------------------------------------
    // 7a: Synchronous Reset zeroes register
    // ---------------------------------------------------------
    std::cout << "  [7a] Normal Synchronous Reset:\n";
    // Write nonzero vector into V3
    dp.initialize_register(3, {0xDEAD, 0xBEEF, 0xCAFE, 0xBABE});
    auto v3_init = cpu::VectorDatapath::read_vector(dp.register_file().get_register(3).output());
    print_vec("V3 before reset", v3_init);
    assert(v3_init == (std::array<std::uint32_t, 4>{0xDEAD, 0xBEEF, 0xCAFE, 0xBABE}));

    // Assert reset
    reset.write(logic::LogicState::HIGH);
    enable.write(logic::LogicState::LOW);

    // Evaluate clock cycle for synchronous reset
    clock.write(logic::LogicState::LOW);
    dp.evaluate();
    clock.write(logic::LogicState::HIGH);
    dp.evaluate();
    clock.write(logic::LogicState::LOW);
    dp.evaluate();

    auto v3_reset = cpu::VectorDatapath::read_vector(dp.register_file().get_register(3).output());
    print_vec("V3 after reset", v3_reset);
    assert(v3_reset == (std::array<std::uint32_t, 4>{0, 0, 0, 0}));
    std::cout << "    [CONFIRMED] V3 reset to all zeros.\n";

    // Deassert reset
    reset.write(logic::LogicState::LOW);
    dp.evaluate();

    // ---------------------------------------------------------
    // 7b: Reset Precedence over Write Enable
    // ---------------------------------------------------------
    std::cout << "  [7b] Reset Precedence over Write Enable:\n";
    // Prepare instruction to compute [11, 22, 33, 44] for V3
    dp.initialize_register(1, {10, 20, 30, 40});
    dp.initialize_register(2, { 1,  2,  3,  4});
    rs1.write_value(1);
    rs2.write_value(2);
    rd.write_value(3);
    opcode.write_value(0); // ADD
    vl.write_value(4);

    // Assert BOTH write enable AND reset
    enable.write(logic::LogicState::HIGH);
    reset.write(logic::LogicState::HIGH);

    // Step clock: reset should override write enable
    clock.write(logic::LogicState::LOW);
    dp.evaluate();
    clock.write(logic::LogicState::HIGH);
    dp.evaluate();
    clock.write(logic::LogicState::LOW);
    dp.evaluate();

    auto v3_overridden = cpu::VectorDatapath::read_vector(dp.register_file().get_register(3).output());
    print_vec("V3 with WE=1 & RESET=1", v3_overridden);
    assert(v3_overridden == (std::array<std::uint32_t, 4>{0, 0, 0, 0}));
    std::cout << "    [CONFIRMED] Reset takes precedence over write enable.\n";

    // Clean up signals
    reset.write(logic::LogicState::LOW);
    enable.write(logic::LogicState::LOW);
    dp.evaluate();

    std::cout << "  [PASS] Test 7 passed successfully!\n\n";
}

} // namespace

int main()
{
    std::cout << "****************************************************\n";
    std::cout << "Running VectorDatapath Tests (Tests 1 to 7)\n";
    std::cout << "****************************************************\n\n";

    test_1_basic_vector_addition();
    test_2_partial_vector_length();
    test_3_boundary_values_of_vl();
    test_4_source_register_selection();
    test_5_destination_and_other_register_preservation();
    test_6_clock_edge_behavior();
    test_7_reset_behavior();

    std::cout << "****************************************************\n";
    std::cout << "ALL TESTS 1–7 PASSED SUCCESSFULLY!\n";
    std::cout << "****************************************************\n";

    return 0;
}

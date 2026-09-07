#include <cassert>
#include <iostream>
#include <cstdint>

#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>

#include "../../pipelined_cpu/multiplexers/ForwardAE_mux.hpp"
#include "../../pipelined_cpu/multiplexers/ForwardBE_mux.hpp"

// ============================================================================
// Test 1: ForwardAE_mux with separate Wire control signals
// ============================================================================
void test_forward_ae_mux_wires()
{
    std::cout << "[Test 1] Running ForwardAE_mux with Wire controls...\n";

    logic::Bus<32> rd1;
    logic::Bus<32> result_w;
    logic::Bus<32> alu_out_m;
    logic::Wire select0(logic::LogicState::LOW);
    logic::Wire select1(logic::LogicState::LOW);
    logic::Bus<32> src_ae;

    cpu::ForwardAE_mux<32, 32> mux(rd1, result_w, alu_out_m, select0, select1, src_ae);

    rd1.write_value(0xAAAA1111);
    result_w.write_value(0xBBBB2222);
    alu_out_m.write_value(0xCCCC3333);

    // 00 -> RD1
    select1.write(logic::LogicState::LOW);
    select0.write(logic::LogicState::LOW);
    mux.evaluate();
    assert(src_ae.read_value() == 0xAAAA1111);
    assert(mux.output().read_value() == 0xAAAA1111);
    assert(mux.src_a_e().read_value() == 0xAAAA1111);

    // 01 -> ResultW
    select1.write(logic::LogicState::LOW);
    select0.write(logic::LogicState::HIGH);
    mux.evaluate();
    assert(src_ae.read_value() == 0xBBBB2222);

    // 10 -> ALUOutM
    select1.write(logic::LogicState::HIGH);
    select0.write(logic::LogicState::LOW);
    mux.evaluate();
    assert(src_ae.read_value() == 0xCCCC3333);

    std::cout << "  [PASS] ForwardAE_mux with Wire controls passed.\n";
}

// ============================================================================
// Test 2: ForwardAE_mux with Bus<2> control signal
// ============================================================================
void test_forward_ae_mux_bus()
{
    std::cout << "[Test 2] Running ForwardAE_mux with Bus<2> control...\n";

    logic::Bus<32> rd1;
    logic::Bus<32> result_w;
    logic::Bus<32> alu_out_m;
    logic::Bus<2> forward_ae;
    logic::Bus<32> src_ae;

    cpu::ForwardAE_mux<32, 32> mux(rd1, result_w, alu_out_m, forward_ae, src_ae);

    rd1.write_value(0x11110000);
    result_w.write_value(0x22220000);
    alu_out_m.write_value(0x33330000);

    // 00 -> RD1
    forward_ae.write_value(0);
    mux.evaluate();
    assert(src_ae.read_value() == 0x11110000);

    // 01 -> ResultW
    forward_ae.write_value(1);
    mux.evaluate();
    assert(src_ae.read_value() == 0x22220000);

    // 10 -> ALUOutM
    forward_ae.write_value(2);
    mux.evaluate();
    assert(src_ae.read_value() == 0x33330000);

    std::cout << "  [PASS] ForwardAE_mux with Bus<2> control passed.\n";
}

// ============================================================================
// Test 3: ForwardBE_mux with separate Wire control signals
// ============================================================================
void test_forward_be_mux_wires()
{
    std::cout << "[Test 3] Running ForwardBE_mux with Wire controls...\n";

    logic::Bus<32> rd2;
    logic::Bus<32> result_w;
    logic::Bus<32> alu_out_m;
    logic::Wire select0(logic::LogicState::LOW);
    logic::Wire select1(logic::LogicState::LOW);
    logic::Bus<32> write_data_e;

    cpu::ForwardBE_mux<32, 32> mux(rd2, result_w, alu_out_m, select0, select1, write_data_e);

    rd2.write_value(0xDEADBEEF);
    result_w.write_value(0xCAFEBABE);
    alu_out_m.write_value(0x12345678);

    // 00 -> RD2
    select1.write(logic::LogicState::LOW);
    select0.write(logic::LogicState::LOW);
    mux.evaluate();
    assert(write_data_e.read_value() == 0xDEADBEEF);
    assert(mux.output().read_value() == 0xDEADBEEF);
    assert(mux.write_data_e().read_value() == 0xDEADBEEF);

    // 01 -> ResultW
    select1.write(logic::LogicState::LOW);
    select0.write(logic::LogicState::HIGH);
    mux.evaluate();
    assert(write_data_e.read_value() == 0xCAFEBABE);

    // 10 -> ALUOutM
    select1.write(logic::LogicState::HIGH);
    select0.write(logic::LogicState::LOW);
    mux.evaluate();
    assert(write_data_e.read_value() == 0x12345678);

    std::cout << "  [PASS] ForwardBE_mux with Wire controls passed.\n";
}

// ============================================================================
// Test 4: ForwardBE_mux with Bus<2> control signal
// ============================================================================
void test_forward_be_mux_bus()
{
    std::cout << "[Test 4] Running ForwardBE_mux with Bus<2> control...\n";

    logic::Bus<32> rd2;
    logic::Bus<32> result_w;
    logic::Bus<32> alu_out_m;
    logic::Bus<2> forward_be;
    logic::Bus<32> write_data_e;

    cpu::ForwardBE_mux<32, 32> mux(rd2, result_w, alu_out_m, forward_be, write_data_e);

    rd2.write_value(0x44440000);
    result_w.write_value(0x55550000);
    alu_out_m.write_value(0x66660000);

    // 00 -> RD2
    forward_be.write_value(0);
    mux.evaluate();
    assert(write_data_e.read_value() == 0x44440000);

    // 01 -> ResultW
    forward_be.write_value(1);
    mux.evaluate();
    assert(write_data_e.read_value() == 0x55550000);

    // 10 -> ALUOutM
    forward_be.write_value(2);
    mux.evaluate();
    assert(write_data_e.read_value() == 0x66660000);

    std::cout << "  [PASS] ForwardBE_mux with Bus<2> control passed.\n";
}

// ============================================================================
// Test 5: Dynamic input updates
// ============================================================================
void test_dynamic_input_update()
{
    std::cout << "[Test 5] Running Dynamic Input Update Test...\n";

    logic::Bus<32> rd1;
    logic::Bus<32> result_w;
    logic::Bus<32> alu_out_m;
    logic::Bus<2> forward_ae;
    logic::Bus<32> src_ae;

    cpu::ForwardAE_mux<32, 32> mux(rd1, result_w, alu_out_m, forward_ae, src_ae);

    // Select ALUOutM (10)
    forward_ae.write_value(2);
    alu_out_m.write_value(100);
    mux.evaluate();
    assert(src_ae.read_value() == 100);

    // Change value on ALUOutM without changing select
    alu_out_m.write_value(999);
    mux.evaluate();
    assert(src_ae.read_value() == 999);

    std::cout << "  [PASS] Dynamic input update passed.\n";
}

// ============================================================================
// Test 6: Custom Widths (16-bit) and Aliases
// ============================================================================
void test_custom_widths_and_aliases()
{
    std::cout << "[Test 6] Running 16-bit and Alias Test...\n";

    logic::Bus<16> rd1;
    logic::Bus<16> result_w;
    logic::Bus<16> alu_out_m;
    logic::Bus<2> forward_a;
    logic::Bus<16> out_a;

    // Using alias ForwardA_mux
    cpu::ForwardA_mux<16, 16> mux_a(rd1, result_w, alu_out_m, forward_a, out_a);

    rd1.write_value(0x1234);
    result_w.write_value(0x5678);
    alu_out_m.write_value(0x9ABC);

    forward_a.write_value(1);
    mux_a.evaluate();
    assert(out_a.read_value() == 0x5678);

    // Using alias ForwardB_mux
    logic::Bus<16> rd2;
    logic::Bus<2> forward_b;
    logic::Bus<16> out_b;
    cpu::ForwardB_mux<16, 16> mux_b(rd2, result_w, alu_out_m, forward_b, out_b);

    rd2.write_value(0xABCD);
    forward_b.write_value(0);
    mux_b.evaluate();
    assert(out_b.read_value() == 0xABCD);

    std::cout << "  [PASS] 16-bit and Alias test passed.\n";
}

int main()
{
    std::cout << "========================================================\n";
    std::cout << "=== Running Pipelined 3:1 Multiplexers Unit Tests ======\n";
    std::cout << "========================================================\n";

    test_forward_ae_mux_wires();
    test_forward_ae_mux_bus();
    test_forward_be_mux_wires();
    test_forward_be_mux_bus();
    test_dynamic_input_update();
    test_custom_widths_and_aliases();

    std::cout << "\n[SUCCESS] All 6 Pipelined 3:1 Multiplexers Tests Passed!\n";
    return 0;
}

#include "../../multi_cycle_cpu/core/muxes/ALUSrcA_mux.hpp"
#include "../../multi_cycle_cpu/core/muxes/ALUSrcB_mux.hpp"
#include "../../multi_cycle_cpu/core/muxes/PCSource_mux.hpp"

#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <iostream>
#include <cassert>

void test_alu_src_a_mux()
{
    std::cout << "Testing ALUSrcA_mux...\n";

    logic::Bus<32> pc_value;
    logic::Bus<32> read_data_1;
    logic::Wire alu_src_a(logic::LogicState::LOW);
    logic::Bus<32> src_a;

    cpu::ALUSrcA_mux<32, 32, 32> mux(pc_value, read_data_1, alu_src_a, src_a);

    pc_value.write_value(0x00400000);
    read_data_1.write_value(0x12345678);

    // Test select = LOW (should select pc_value)
    alu_src_a.write(logic::LogicState::LOW);
    mux.evaluate();
    assert(src_a.read_value() == 0x00400000);
    std::cout << "  [PASS] select LOW -> outputs pc_value\n";

    // Test select = HIGH (should select read_data_1)
    alu_src_a.write(logic::LogicState::HIGH);
    mux.evaluate();
    assert(src_a.read_value() == 0x12345678);
    std::cout << "  [PASS] select HIGH -> outputs read_data_1\n";
}

void test_alu_src_b_mux()
{
    std::cout << "Testing ALUSrcB_mux...\n";

    logic::Bus<32> read_data_2;
    logic::Bus<32> constant_val;
    logic::Bus<32> sign_extended;
    logic::Bus<32> shifted_val;
    logic::Wire select0(logic::LogicState::LOW);
    logic::Wire select1(logic::LogicState::LOW);
    logic::Bus<32> src_b;

    cpu::ALUSrcB_mux<32, 32> mux(
        read_data_2,
        constant_val,
        sign_extended,
        shifted_val,
        select0,
        select1,
        src_b
    );

    read_data_2.write_value(0x11111111);
    constant_val.write_value(0x00000004);
    sign_extended.write_value(0x00000020);
    shifted_val.write_value(0x00000080);

    // Test select1 select0 = 00 -> read_data_2
    select1.write(logic::LogicState::LOW);
    select0.write(logic::LogicState::LOW);
    mux.evaluate();
    assert(src_b.read_value() == 0x11111111);
    std::cout << "  [PASS] select 00 -> outputs read_data_2\n";

    // Test select1 select0 = 01 -> constant_val (4)
    select1.write(logic::LogicState::LOW);
    select0.write(logic::LogicState::HIGH);
    mux.evaluate();
    assert(src_b.read_value() == 0x00000004);
    std::cout << "  [PASS] select 01 -> outputs constant_val\n";

    // Test select1 select0 = 10 -> sign_extended
    select1.write(logic::LogicState::HIGH);
    select0.write(logic::LogicState::LOW);
    mux.evaluate();
    assert(src_b.read_value() == 0x00000020);
    std::cout << "  [PASS] select 10 -> outputs sign_extended\n";

    // Test select1 select0 = 11 -> shifted_val
    select1.write(logic::LogicState::HIGH);
    select0.write(logic::LogicState::HIGH);
    mux.evaluate();
    assert(src_b.read_value() == 0x00000080);
    std::cout << "  [PASS] select 11 -> outputs shifted_val\n";
}

void test_pc_source_mux()
{
    std::cout << "Testing PCSource_mux (IorD address selector)...\n";

    logic::Bus<32> pc_val;
    logic::Bus<32> alu_result;
    logic::Wire IorD(logic::LogicState::LOW);
    logic::Bus<32> address;

    cpu::PCSource_mux<32, 32, 32> mux(pc_val, alu_result, IorD, address);

    pc_val.write_value(0x00100000);
    alu_result.write_value(0x7ffffff0);

    // Test IorD = LOW (selects pc_val)
    IorD.write(logic::LogicState::LOW);
    mux.evaluate();
    assert(address.read_value() == 0x00100000);
    std::cout << "  [PASS] select LOW (IorD = LOW) -> outputs pc_val\n";

    // Test IorD = HIGH (selects alu_result)
    IorD.write(logic::LogicState::HIGH);
    mux.evaluate();
    assert(address.read_value() == 0x7ffffff0);
    std::cout << "  [PASS] select HIGH (IorD = HIGH) -> outputs alu_result\n";
}

int main()
{
    std::cout << "=== Running Multi-Cycle Muxes Unit Tests ===\n";
    test_alu_src_a_mux();
    test_alu_src_b_mux();
    test_pc_source_mux();
    std::cout << "[PASS] All Multi-Cycle Muxes Unit Tests Passed!\n";
    return 0;
}

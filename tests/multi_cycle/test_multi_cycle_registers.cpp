#include "../../multi_cycle_cpu/core/registers/InstructionRegister.hpp"
#include "../../multi_cycle_cpu/core/registers/ALU_out_reg.hpp"
#include "../../multi_cycle_cpu/core/registers/MemoryDataRegister.hpp"
#include "../../multi_cycle_cpu/core/registers/OperandA_reg.hpp"
#include "../../multi_cycle_cpu/core/registers/OperandB_reg.hpp"

#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <iostream>
#include <cassert>

void test_instruction_register()
{
    std::cout << "Testing InstructionRegister...\n";
    logic::Bus<32> input_bus;
    logic::Wire clock(logic::LogicState::LOW);
    logic::Wire ir_write(logic::LogicState::LOW);
    logic::Wire reset(logic::LogicState::LOW);

    cpu::InstructionRegister<32> ir(input_bus, clock, ir_write, reset);

    // Initial evaluation
    ir.evaluate();
    assert(ir.read().read_value() == 0);

    // Write enable HIGH, write value 0x12345678
    input_bus.write_value(0x12345678);
    ir_write.write(logic::LogicState::HIGH);
    clock.write(logic::LogicState::LOW);
    ir.evaluate();

    clock.write(logic::LogicState::HIGH);
    ir.evaluate();
    assert(ir.read().read_value() == 0x12345678);

    // Write enable LOW, write value 0xabcdef00 (should hold previous value)
    input_bus.write_value(0xabcdef00);
    ir_write.write(logic::LogicState::LOW);
    clock.write(logic::LogicState::LOW);
    ir.evaluate();

    clock.write(logic::LogicState::HIGH);
    ir.evaluate();
    assert(ir.read().read_value() == 0x12345678);

    // Reset HIGH
    reset.write(logic::LogicState::HIGH);
    clock.write(logic::LogicState::LOW);
    ir.evaluate();

    clock.write(logic::LogicState::HIGH);
    ir.evaluate();
    assert(ir.read().read_value() == 0);
}

void test_alu_out_reg()
{
    std::cout << "Testing ALU_out_reg...\n";
    logic::Bus<32> alu_data;
    logic::Wire clock(logic::LogicState::LOW);
    logic::Wire out_write(logic::LogicState::LOW);
    logic::Wire reset(logic::LogicState::LOW);

    cpu::ALU_out_reg<32> alu_out(alu_data, clock, out_write, reset);

    // Initial evaluation
    alu_out.evaluate();
    assert(alu_out.read().read_value() == 0);

    // Write enable HIGH, write value 0x11223344
    alu_data.write_value(0x11223344);
    out_write.write(logic::LogicState::HIGH);
    clock.write(logic::LogicState::LOW);
    alu_out.evaluate();

    clock.write(logic::LogicState::HIGH);
    alu_out.evaluate();
    assert(alu_out.read().read_value() == 0x11223344);

    // Write enable LOW, write value 0x55667788 (should hold)
    alu_data.write_value(0x55667788);
    out_write.write(logic::LogicState::LOW);
    clock.write(logic::LogicState::LOW);
    alu_out.evaluate();

    clock.write(logic::LogicState::HIGH);
    alu_out.evaluate();
    assert(alu_out.read().read_value() == 0x11223344);

    // Reset HIGH
    reset.write(logic::LogicState::HIGH);
    clock.write(logic::LogicState::LOW);
    alu_out.evaluate();

    clock.write(logic::LogicState::HIGH);
    alu_out.evaluate();
    assert(alu_out.read().read_value() == 0);
}

void test_memory_data_register()
{
    std::cout << "Testing MemoryDataRegister...\n";
    logic::Bus<32> data_bus;
    logic::Wire clock(logic::LogicState::LOW);
    logic::Wire mdr_write(logic::LogicState::LOW);
    logic::Wire reset(logic::LogicState::LOW);

    cpu::MemoryDataRegister<32> mdr(data_bus, clock, mdr_write, reset);

    // Initial evaluation
    mdr.evaluate();
    assert(mdr.read().read_value() == 0);

    // Write enable HIGH, write value 0xaabbccdd
    data_bus.write_value(0xaabbccdd);
    mdr_write.write(logic::LogicState::HIGH);
    clock.write(logic::LogicState::LOW);
    mdr.evaluate();

    clock.write(logic::LogicState::HIGH);
    mdr.evaluate();
    assert(mdr.read().read_value() == 0xaabbccdd);

    // Write enable LOW, write value 0x11223344 (should hold)
    data_bus.write_value(0x11223344);
    mdr_write.write(logic::LogicState::LOW);
    clock.write(logic::LogicState::LOW);
    mdr.evaluate();

    clock.write(logic::LogicState::HIGH);
    mdr.evaluate();
    assert(mdr.read().read_value() == 0xaabbccdd);

    // Reset HIGH
    reset.write(logic::LogicState::HIGH);
    clock.write(logic::LogicState::LOW);
    mdr.evaluate();

    clock.write(logic::LogicState::HIGH);
    mdr.evaluate();
    assert(mdr.read().read_value() == 0);
}

void test_operand_a_reg()
{
    std::cout << "Testing OperandA_reg...\n";
    logic::Bus<32> data_bus;
    logic::Wire clock(logic::LogicState::LOW);
    logic::Wire reset(logic::LogicState::LOW);
    logic::Wire a_write(logic::LogicState::LOW);

    cpu::OperandA_reg<32> op_a(data_bus, clock, reset, a_write);

    // Initial evaluation
    op_a.evaluate();
    assert(op_a.read().read_value() == 0);

    // Write enable HIGH, write value 0x55555555
    data_bus.write_value(0x55555555);
    a_write.write(logic::LogicState::HIGH);
    clock.write(logic::LogicState::LOW);
    op_a.evaluate();

    clock.write(logic::LogicState::HIGH);
    op_a.evaluate();
    assert(op_a.read().read_value() == 0x55555555);

    // Write enable LOW, write value 0xaaaaaaaa (should hold)
    data_bus.write_value(0xaaaaaaaa);
    a_write.write(logic::LogicState::LOW);
    clock.write(logic::LogicState::LOW);
    op_a.evaluate();

    clock.write(logic::LogicState::HIGH);
    op_a.evaluate();
    assert(op_a.read().read_value() == 0x55555555);

    // Reset HIGH
    reset.write(logic::LogicState::HIGH);
    clock.write(logic::LogicState::LOW);
    op_a.evaluate();

    clock.write(logic::LogicState::HIGH);
    op_a.evaluate();
    assert(op_a.read().read_value() == 0);
}

void test_operand_b_reg()
{
    std::cout << "Testing OperandB_reg...\n";
    logic::Bus<32> data_bus;
    logic::Wire clock(logic::LogicState::LOW);
    logic::Wire b_write(logic::LogicState::LOW);
    logic::Wire reset(logic::LogicState::LOW);

    cpu::OperandB_reg<32> op_b(data_bus, clock, b_write, reset);

    // Initial evaluation
    op_b.evaluate();
    assert(op_b.read().read_value() == 0);

    // Write enable HIGH, write value 0x66666666
    data_bus.write_value(0x66666666);
    b_write.write(logic::LogicState::HIGH);
    clock.write(logic::LogicState::LOW);
    op_b.evaluate();

    clock.write(logic::LogicState::HIGH);
    op_b.evaluate();
    assert(op_b.read().read_value() == 0x66666666);

    // Write enable LOW, write value 0xbbbbbbbb (should hold)
    data_bus.write_value(0xbbbbbbbb);
    b_write.write(logic::LogicState::LOW);
    clock.write(logic::LogicState::LOW);
    op_b.evaluate();

    clock.write(logic::LogicState::HIGH);
    op_b.evaluate();
    assert(op_b.read().read_value() == 0x66666666);

    // Reset HIGH
    reset.write(logic::LogicState::HIGH);
    clock.write(logic::LogicState::LOW);
    op_b.evaluate();

    clock.write(logic::LogicState::HIGH);
    op_b.evaluate();
    assert(op_b.read().read_value() == 0);
}

int main()
{
    std::cout << "=== Running Multi-Cycle Registers Unit Tests ===\n";
    test_instruction_register();
    test_alu_out_reg();
    test_memory_data_register();
    test_operand_a_reg();
    test_operand_b_reg();
    std::cout << "[PASS] All Multi-Cycle Registers Unit Tests Passed!\n";
    return 0;
}

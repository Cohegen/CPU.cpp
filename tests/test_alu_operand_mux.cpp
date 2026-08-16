#include "../single_cycle_cpu/datapath/ALUOperandMux.hpp"
#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <cassert>
#include <iostream>

void test_alu_operand_mux()
{
    std::cout << "--- Testing ALUOperandMux<32> ---\n";

    logic::Bus<32> register_operand;
    logic::Bus<32> immediate_operand;
    logic::Wire select(logic::LogicState::LOW);
    logic::Bus<32> output;

    cpu::ALUOperandMux<32> mux(
        register_operand,
        immediate_operand,
        select,
        output
    );

    // 1. Register operand (select = LOW)
    register_operand.write_value(25);
    immediate_operand.write_value(100);
    select.write(logic::LogicState::LOW);

    mux.evaluate();

    assert(output.read_value() == 25);

    // 2. Immediate operand (select = HIGH)
    select.write(logic::LogicState::HIGH);

    mux.evaluate();

    assert(output.read_value() == 100);

    // 3. Changing both inputs
    register_operand.write_value(42);
    immediate_operand.write_value(123);

    select.write(logic::LogicState::LOW);
    mux.evaluate();

    assert(output.read_value() == 42);

    select.write(logic::LogicState::HIGH);
    mux.evaluate();

    assert(output.read_value() == 123);

    std::cout << "[PASS] ALUOperandMux Unit Test Successful!\n";
}

int main()
{
    test_alu_operand_mux();
    return 0;
}

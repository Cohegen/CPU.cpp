#include "../../superscalar/core/execution/ALUExecutionUnit.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>

#include <logic/signals/bus.hpp>
#include <logic/signals/logicState.hpp>
#include <logic/signals/wire.hpp>

int main()
{
    logic::Wire valid{logic::LogicState::HIGH};
    logic::Wire completed{};
    logic::Bus<32> operand_a{};
    logic::Bus<32> operand_b{};
    logic::Bus<32> result{};

    cpu::ALUExecutionUnit<32> lane{
        valid, operand_a, operand_b, result, completed
    };

    operand_a.write_value(40);
    operand_b.write_value(2);
    lane.set_operation(cpu::ALUOperation::ADD);
    lane.set_destination(7, 21);
    lane.evaluate();

    const auto add = lane.result_bundle();
    assert(add.valid);
    assert(add.result == 42);
    assert(add.rob_index == 7);
    assert(add.physical_rd == 21);

    operand_a.write_value(0xF0U);
    operand_b.write_value(0x0FU);
    lane.set_operation(cpu::ALUOperation::OR);
    lane.evaluate();
    assert(lane.result_bundle().result == 0xFFU);

    valid.write(logic::LogicState::LOW);
    lane.evaluate();
    assert(!lane.result_bundle().valid);

    std::cout << "[PASS] ALU execution lane reuses ALUInterface.\n";
    return 0;
}

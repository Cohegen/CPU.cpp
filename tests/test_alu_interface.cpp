#include "../components/ALUInterface.hpp"
#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <cassert>
#include <iostream>

void test_alu_interface()
{
    std::cout << "--- Testing ALUInterface<32> ---\n";

    logic::Bus<32> a;
    logic::Bus<32> b;
    logic::Bus<32> result;

    logic::Wire zero;
    logic::Wire carry;

    a.write_value(10);
    b.write_value(5);

    cpu::ALUInterface<32> alu(
        a,
        b,
        cpu::ALUOperation::ADD,
        result,
        zero,
        carry
    );

    // 1. ADD: 10 + 5 = 15
    alu.evaluate();
    assert(result.read_value() == 15);
    assert(zero.read() == logic::LogicState::LOW);

    // 2. SUB: 10 - 5 = 5
    alu.set_operation(cpu::ALUOperation::SUB);
    alu.evaluate();
    assert(result.read_value() == 5);
    assert(zero.read() == logic::LogicState::LOW);

    // 3. AND: 10 & 5 = 0 (10 is 0b1010, 5 is 0b0101)
    alu.set_operation(cpu::ALUOperation::AND);
    alu.evaluate();
    assert(result.read_value() == (10 & 5));

    // 4. OR: 10 | 5 = 15
    alu.set_operation(cpu::ALUOperation::OR);
    alu.evaluate();
    assert(result.read_value() == (10 | 5));

    // 5. XOR: 10 ^ 5 = 15
    alu.set_operation(cpu::ALUOperation::XOR);
    alu.evaluate();
    assert(result.read_value() == (10 ^ 5));

    // 6. NOT: ~10
    alu.set_operation(cpu::ALUOperation::NOT);
    alu.evaluate();
    assert(result.read_value() == static_cast<std::uint32_t>(~10));

    // 7. PASS_A: 10
    alu.set_operation(cpu::ALUOperation::PASS_A);
    alu.evaluate();
    assert(result.read_value() == 10);

    // 8. PASS_B: 5
    alu.set_operation(cpu::ALUOperation::PASS_B);
    alu.evaluate();
    assert(result.read_value() == 5);

    // --- Zero Flag Tests ---
    // 10 - 10 -> result = 0 -> zero = HIGH
    a.write_value(10);
    b.write_value(10);
    alu.set_operation(cpu::ALUOperation::SUB);
    alu.evaluate();
    assert(result.read_value() == 0);
    assert(zero.read() == logic::LogicState::HIGH);

    // 10 - 5 -> result != 0 -> zero = LOW
    b.write_value(5);
    alu.evaluate();
    assert(result.read_value() == 5);
    assert(zero.read() == logic::LogicState::LOW);

    std::cout << "[PASS] ALUInterface Unit Test Successful!\n";
}

int main()
{
    test_alu_interface();
    return 0;
}

#include "../single_cycle_cpu/datapath/ProgramCounter.hpp"
#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <iostream>
#include <cassert>

int main()
{
    std::cout << "--- Testing ProgramCounter<32> ---\n";

    logic::Wire clock(logic::LogicState::LOW);
    logic::Wire reset(logic::LogicState::LOW);
    logic::Wire enable(logic::LogicState::HIGH);

    logic::Bus<32> next_pc;
    logic::Bus<32> current_pc;

    cpu::ProgramCounter<32> pc(clock, reset, enable, next_pc, current_pc);

    // Initial evaluation
    pc.evaluate();
    assert(current_pc.read_value() == 0);

    // Load next PC value = 0x00000004
    next_pc.write_value(4);
    clock.write(logic::LogicState::LOW);
    pc.evaluate();

    // Clock rising edge: LOW -> HIGH
    clock.write(logic::LogicState::HIGH);
    pc.evaluate();
    assert(current_pc.read_value() == 4);

    // Next PC = 0x00000008
    next_pc.write_value(8);
    clock.write(logic::LogicState::LOW);
    pc.evaluate();

    // Clock rising edge: LOW -> HIGH
    clock.write(logic::LogicState::HIGH);
    pc.evaluate();
    assert(current_pc.read_value() == 8);

    // Test Enable = LOW (Stall)
    enable.write(logic::LogicState::LOW);
    next_pc.write_value(0x100);

    clock.write(logic::LogicState::LOW);
    pc.evaluate();
    clock.write(logic::LogicState::HIGH);
    pc.evaluate();

    // PC should hold previous value 8
    assert(current_pc.read_value() == 8);

    // Test Reset = HIGH
    reset.write(logic::LogicState::HIGH);
    enable.write(logic::LogicState::HIGH);

    clock.write(logic::LogicState::LOW);
    pc.evaluate();
    clock.write(logic::LogicState::HIGH);
    pc.evaluate();

    // PC should reset to 0
    assert(current_pc.read_value() == 0);

    std::cout << "[PASS] ProgramCounter Unit Test Successful!\n";
    return 0;
}

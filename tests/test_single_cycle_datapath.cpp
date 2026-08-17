#include "../single_cycle_cpu/datapath/SingleCycleDatapath.hpp"
#include <logic/signals/wire.hpp>
#include <iostream>
#include <cassert>
#include <vector>

int main()
{
    std::cout << "--- Testing SingleCycleDatapath (Instruction Fetch Path) ---\n";

    logic::Wire clock(logic::LogicState::LOW);
    logic::Wire reset(logic::LogicState::LOW);

    cpu::SingleCycleDatapath<32, 32, 32, 4, 8> datapath(clock, reset);

    std::vector<std::size_t> instructions = {
        0xA1A1A1A1, // Address 0
        0xB2B2B2B2, // Address 1
        0xC3C3C3C3, // Address 2
        0xD4D4D4D4  // Address 3
    };

    datapath.load_instructions(instructions);

    // Initial PC = 0
    datapath.evaluate();
    assert(datapath.pc().read_value() == 0);
    std::cout << "Initial PC: " << datapath.pc().read_value() << "\n";

    // evaluate() -> instruction fetched from address 0
    assert(datapath.instruction().read_value() == 0xA1A1A1A1);
    std::cout << "evaluate() -> instruction fetched from address 0: 0x"
              << std::hex << datapath.instruction().read_value() << std::dec << "\n";

    // clock rising edge -> PC becomes 1
    clock.write(logic::LogicState::LOW);
    datapath.evaluate();
    clock.write(logic::LogicState::HIGH);
    datapath.evaluate();

    assert(datapath.pc().read_value() == 1);
    std::cout << "clock rising edge -> PC becomes: " << datapath.pc().read_value() << "\n";

    // evaluate() -> instruction fetched from address 1
    assert(datapath.instruction().read_value() == 0xB2B2B2B2);
    std::cout << "evaluate() -> instruction fetched from address 1: 0x"
              << std::hex << datapath.instruction().read_value() << std::dec << "\n";

    // clock rising edge -> PC becomes 2
    clock.write(logic::LogicState::LOW);
    datapath.evaluate();
    clock.write(logic::LogicState::HIGH);
    datapath.evaluate();

    assert(datapath.pc().read_value() == 2);
    std::cout << "clock rising edge -> PC becomes: " << datapath.pc().read_value() << "\n";

    // evaluate() -> instruction fetched from address 2
    assert(datapath.instruction().read_value() == 0xC3C3C3C3);
    std::cout << "evaluate() -> instruction fetched from address 2: 0x"
              << std::hex << datapath.instruction().read_value() << std::dec << "\n";

    std::cout << "[PASS] SingleCycleDatapath Instruction Fetch Path Test Successful!\n";
    return 0;
}

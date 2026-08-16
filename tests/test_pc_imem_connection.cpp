#include "../single_cycle_cpu/datapath/ProgramCounter.hpp"
#include "../single_cycle_cpu/datapath/InstructionMemory.hpp"
#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <iostream>
#include <cassert>
#include <vector>

int main()
{
    std::cout << "--- Testing Connection: ProgramCounter -> InstructionMemory ---\n";

    logic::Wire clock(logic::LogicState::LOW);
    logic::Wire pc_reset(logic::LogicState::LOW);
    logic::Wire pc_enable(logic::LogicState::HIGH);

    logic::Bus<32> next_pc;
    logic::Bus<32> pc_out_bus; // Shared bus connecting PC output to InstructionMemory address
    logic::Bus<32> instruction_bus;

    // Instantiate PC
    cpu::ProgramCounter<32> pc(clock, pc_reset, pc_enable, next_pc, pc_out_bus);

    // Initial instruction memory contents
    std::vector<std::size_t> instructions = {
        0x11111111, // Address 0
        0x22222222, // Address 1
        0x33333333, // Address 2
        0x44444444  // Address 3
    };

    // Instantiate InstructionMemory connected to pc_out_bus
    cpu::InstructionMemory<32, 8, 32> imem(pc_out_bus, instruction_bus, instructions);

    // Step 1: Initial state (PC = 0)
    pc.evaluate();
    imem.evaluate();
    assert(pc_out_bus.read_value() == 0);
    assert(instruction_bus.read_value() == 0x11111111);
    std::cout << "PC: " << pc_out_bus.read_value() << " => Instruction: 0x" << std::hex << instruction_bus.read_value() << std::dec << "\n";

    // Step 2: Next PC = 1, trigger rising edge
    next_pc.write_value(1);
    clock.write(logic::LogicState::LOW);
    pc.evaluate();
    clock.write(logic::LogicState::HIGH);
    pc.evaluate();
    imem.evaluate();
    assert(pc_out_bus.read_value() == 1);
    assert(instruction_bus.read_value() == 0x22222222);
    std::cout << "PC: " << pc_out_bus.read_value() << " => Instruction: 0x" << std::hex << instruction_bus.read_value() << std::dec << "\n";

    // Step 3: Next PC = 2, trigger rising edge
    next_pc.write_value(2);
    clock.write(logic::LogicState::LOW);
    pc.evaluate();
    clock.write(logic::LogicState::HIGH);
    pc.evaluate();
    imem.evaluate();
    assert(pc_out_bus.read_value() == 2);
    assert(instruction_bus.read_value() == 0x33333333);
    std::cout << "PC: " << pc_out_bus.read_value() << " => Instruction: 0x" << std::hex << instruction_bus.read_value() << std::dec << "\n";

    std::cout << "[PASS] ProgramCounter and InstructionMemory Connection Test Successful!\n";
    return 0;
}

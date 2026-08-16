#include "../single_cycle_cpu/datapath/InstructionMemory.hpp"
#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <iostream>
#include <cassert>
#include <vector>

int main()
{
    std::cout << "--- Testing InstructionMemory<32, 8, 32> ---\n";

    logic::Bus<32> address;
    logic::Bus<32> instruction;
    logic::Wire enable(logic::LogicState::HIGH);

    std::vector<std::size_t> init_code = {
        0x00200093, // addi x1, x0, 2
        0x00300113, // addi x2, x0, 3
        0x002081b3, // add x3, x1, x2
        0x0000006f  // jal x0, 0
    };

    cpu::InstructionMemory<32, 8, 32> imem(enable, address, instruction, init_code);

    // 1. Reading instruction at address 0
    address.write_value(0);
    imem.evaluate();
    std::cout << "Address 0 instruction: 0x" << std::hex << instruction.read_value() << std::dec << "\n";
    assert(instruction.read_value() == 0x00200093);

    // 2. Reading several different addresses
    address.write_value(1);
    imem.evaluate();
    assert(instruction.read_value() == 0x00300113);

    address.write_value(2);
    imem.evaluate();
    assert(instruction.read_value() == 0x002081b3);

    address.write_value(3);
    imem.evaluate();
    assert(instruction.read_value() == 0x0000006f);

    // 3. Loading new instructions dynamically
    std::vector<std::size_t> new_code = {
        0x12345678,
        0x9ABCDEF0
    };
    imem.load(new_code);

    address.write_value(0);
    imem.evaluate();
    assert(instruction.read_value() == 0x12345678);

    address.write_value(1);
    imem.evaluate();
    assert(instruction.read_value() == 0x9ABCDEF0);

    // 4. Disabled/Enabled behavior
    enable.write(logic::LogicState::LOW);
    imem.evaluate();
    assert(instruction.read_value() == 0); // Output disabled -> zero

    enable.write(logic::LogicState::HIGH);
    imem.evaluate();
    assert(instruction.read_value() == 0x9ABCDEF0);

    // 5. Address bits mapping correctly
    // High address bits set (e.g. 0x00000101 -> low 8 bits are 0x01)
    address.write_value(0x00000101);
    imem.evaluate();
    assert(instruction.read_value() == 0x9ABCDEF0);

    std::cout << "[PASS] InstructionMemory Unit Test Successful!\n";
    return 0;
}

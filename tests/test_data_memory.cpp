#include "../single_cycle_cpu/datapath/DataMemory.hpp"

#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/signals/clock.hpp>
#include <iostream>
#include <cassert>
#include <cstdint>

int main() {
    std::cout << "--- Testing DataMemory<8, 32> Four Cases Independently ---\n";

    constexpr std::size_t RAM_ADDRESS = 0x84; // MSB (bit 7) = 1 -> RAM space (address 0x84)


    // Case 1: No operation -> memory unchanged
    {
        logic::Clock clock;
        logic::Wire reset(logic::LogicState::LOW);
        logic::Wire read_enable(logic::LogicState::LOW);
        logic::Wire write_enable(logic::LogicState::LOW);
        logic::Bus<8> address;
        logic::Bus<32> write_data;
        logic::Bus<32> read_data;

        cpu::DataMemory<8, 32> dmem(clock, reset, read_enable, write_enable, address, write_data, read_data);


        address.write_value(RAM_ADDRESS);
        write_data.write_value(0xDEADBEEF);
        
        dmem.evaluate();
        clock.tick();
        dmem.evaluate();

        assert(dmem.ready().read() == logic::LogicState::LOW);
        assert(read_data.read_value() == 0);
        std::cout << "[PASS] Case 1: No operation -> memory unchanged\n";
    }

    // Case 2: Read -> returns the word at the supplied address
    {
        logic::Clock clock;
        logic::Wire reset(logic::LogicState::LOW);
        logic::Wire read_enable(logic::LogicState::HIGH);
        logic::Wire write_enable(logic::LogicState::LOW);
        logic::Bus<8> address;
        logic::Bus<32> write_data;
        logic::Bus<32> read_data;

        cpu::DataMemory<8, 32> dmem(clock, reset, read_enable, write_enable, address, write_data, read_data);

        dmem.load_rom({0x12345678, 0xABCDEF01});

        constexpr std::size_t ROM_ADDRESS_1 = 0x00000001; // address 1 in ROM space
        address.write_value(ROM_ADDRESS_1);

        dmem.evaluate();

        assert(dmem.ready().read() == logic::LogicState::HIGH);
        assert(read_data.read_value() == 0xABCDEF01);
        std::cout << "[PASS] Case 2: Read -> returns the word at the supplied address\n";
    }

    // Case 3: Write -> writes write_data to the RAM address
    {
        logic::Clock clock;
        logic::Wire reset(logic::LogicState::LOW);
        logic::Wire read_enable(logic::LogicState::LOW);
        logic::Wire write_enable(logic::LogicState::HIGH);
        logic::Bus<8> address;
        logic::Bus<32> write_data;
        logic::Bus<32> read_data;

        cpu::DataMemory<8, 32> dmem(clock, reset, read_enable, write_enable, address, write_data, read_data);


        address.write_value(RAM_ADDRESS);
        write_data.write_value(0xCAFEBABE);

        dmem.evaluate();
        clock.tick();
        dmem.evaluate();

        assert(dmem.ready().read() == logic::LogicState::HIGH);

        // Verification via read operation
        write_enable.write(logic::LogicState::LOW);
        read_enable.write(logic::LogicState::HIGH);
        dmem.evaluate();

        assert(read_data.read_value() == 0xCAFEBABE);
        std::cout << "[PASS] Case 3: Write -> writes write_data to the RAM address\n";
    }

    // Case 4: Write followed by read -> confirms the written value comes back
    {
        logic::Clock clock;
        logic::Wire reset(logic::LogicState::LOW);
        logic::Wire read_enable(logic::LogicState::LOW);
        logic::Wire write_enable(logic::LogicState::LOW);
        logic::Bus<8> address;
        logic::Bus<32> write_data;
        logic::Bus<32> read_data;

        cpu::DataMemory<8, 32> dmem(clock, reset, read_enable, write_enable, address, write_data, read_data);


        // 1. Perform Write to RAM address
        address.write_value(RAM_ADDRESS);
        write_data.write_value(0x55AA55AA);
        write_enable.write(logic::LogicState::HIGH);
        read_enable.write(logic::LogicState::LOW);

        dmem.evaluate();
        clock.tick();
        dmem.evaluate();

        // 2. Perform Read from RAM address
        write_enable.write(logic::LogicState::LOW);
        read_enable.write(logic::LogicState::HIGH);

        dmem.evaluate();

        assert(read_data.read_value() == 0x55AA55AA);
        std::cout << "[PASS] Case 4: Write followed by read -> confirms the written value comes back\n";
    }

    std::cout << "\nAll 4 DataMemory test cases passed successfully!\n";
    return 0;
}

#include "../components/WriteBackMux.hpp"

#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <iostream>
#include <cassert>
#include <cstdint>

int main() {
    std::cout << "--- Testing WriteBackMux<32> ---\n";

    logic::Bus<32> alu_result;
    logic::Bus<32> memory_data;
    logic::Wire memory_to_register(logic::LogicState::LOW);
    logic::Bus<32> output;

    cpu::WriteBackMux<32> wb_mux(alu_result, memory_data, memory_to_register, output);

    alu_result.write_value(0x12345678);
    memory_data.write_value(0x87654321);

    // Select 0: ALU result
    memory_to_register.write(logic::LogicState::LOW);
    wb_mux.evaluate();
    assert(output.read_value() == 0x12345678);
    std::cout << "[PASS] Select LOW -> outputs ALU result\n";

    // Select 1: Memory data
    memory_to_register.write(logic::LogicState::HIGH);
    wb_mux.evaluate();
    assert(output.read_value() == 0x87654321);
    std::cout << "[PASS] Select HIGH -> outputs memory data\n";

    std::cout << "All WriteBackMux tests passed successfully!\n";
    return 0;
}

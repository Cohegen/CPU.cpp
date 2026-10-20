#include "../vector_cpu/core/VectorRegister.hpp"

#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <iostream>
#include <cassert>

void run_vector_register_tests() {
    std::cout << "========================================\n";
    std::cout << "Running VectorRegister Tests\n";
    std::cout << "========================================\n\n";

    constexpr std::size_t WIDTH = 128;
    logic::Bus<WIDTH> data;
    logic::Wire clock(logic::LogicState::LOW);
    logic::Wire we(logic::LogicState::LOW);
    logic::Wire reset(logic::LogicState::LOW);

    cpu::VectorRegister<WIDTH> vec_reg(data, clock, we, reset);

    // Initial settle
    clock.write(logic::LogicState::LOW);
    vec_reg.evaluate();

    // ---------------------------------------------------------
    // Test 1: WE=1, RESET=0, clock ↑ => Q = DATA
    // ---------------------------------------------------------
    std::cout << "[Test 1] WE=1, RESET=0, clock ^ => Q = DATA\n";
    data.write_value(0xDEADBEEF);
    we.write(logic::LogicState::HIGH);
    reset.write(logic::LogicState::LOW);

    // Clock low phase (setup)
    clock.write(logic::LogicState::LOW);
    vec_reg.evaluate();

    // Clock rising edge
    clock.write(logic::LogicState::HIGH);
    vec_reg.evaluate();

    std::cout << "  DATA = 0xDEADBEEF\n";
    std::cout << "  Q    = 0x" << std::hex << vec_reg.output().read_value() << std::dec << "\n";
    assert(vec_reg.output().read_value() == 0xDEADBEEF);
    std::cout << "  [PASS] Q == DATA\n\n";

    // ---------------------------------------------------------
    // Test 2: WE=0, RESET=0, clock ↑ => Q remains unchanged
    // ---------------------------------------------------------
    std::cout << "[Test 2] WE=0, RESET=0, clock ^ => Q remains unchanged\n";
    data.write_value(0xCAFEBABE);
    we.write(logic::LogicState::LOW);
    reset.write(logic::LogicState::LOW);

    // Clock low phase
    clock.write(logic::LogicState::LOW);
    vec_reg.evaluate();

    // Clock rising edge
    clock.write(logic::LogicState::HIGH);
    vec_reg.evaluate();

    std::cout << "  DATA input changed to 0xCAFEBABE, but WE=0\n";
    std::cout << "  Q = 0x" << std::hex << vec_reg.output().read_value() << std::dec << "\n";
    assert(vec_reg.output().read_value() == 0xDEADBEEF);
    std::cout << "  [PASS] Q remains unchanged (0xDEADBEEF)\n\n";

    // ---------------------------------------------------------
    // Test 3: WE=1, RESET=1, clock ↑ => Q = 0
    // ---------------------------------------------------------
    std::cout << "[Test 3] WE=1, RESET=1, clock ^ => Q = 0\n";
    data.write_value(0x12345678);
    we.write(logic::LogicState::HIGH);
    reset.write(logic::LogicState::HIGH);

    // Clock low phase
    clock.write(logic::LogicState::LOW);
    vec_reg.evaluate();

    // Clock rising edge
    clock.write(logic::LogicState::HIGH);
    vec_reg.evaluate();

    std::cout << "  DATA = 0x12345678, WE=1, RESET=1\n";
    std::cout << "  Q    = 0x" << std::hex << vec_reg.output().read_value() << std::dec << "\n";
    assert(vec_reg.output().read_value() == 0);
    std::cout << "  [PASS] Q is 0\n\n";

    // ---------------------------------------------------------
    // Test 4: Change DATA without clock ↑ => Q remains unchanged
    // ---------------------------------------------------------
    std::cout << "[Test 4] Change DATA without clock ^ => Q remains unchanged\n";
    // First latch a known non-zero value into Q
    data.write_value(0x55AA55AA);
    we.write(logic::LogicState::HIGH);
    reset.write(logic::LogicState::LOW);
    clock.write(logic::LogicState::LOW);
    vec_reg.evaluate();
    clock.write(logic::LogicState::HIGH);
    vec_reg.evaluate();
    assert(vec_reg.output().read_value() == 0x55AA55AA);

    // Now while clock remains HIGH (no clock rising edge), change DATA
    data.write_value(0x99999999);
    vec_reg.evaluate();
    std::cout << "  Clock held HIGH: DATA changed to 0x99999999\n";
    std::cout << "  Q = 0x" << std::hex << vec_reg.output().read_value() << std::dec << "\n";
    assert(vec_reg.output().read_value() == 0x55AA55AA);

    // And while clock transitions to LOW (falling edge, not rising edge), change DATA
    data.write_value(0x88888888);
    clock.write(logic::LogicState::LOW);
    vec_reg.evaluate();
    std::cout << "  Clock fell to LOW: DATA changed to 0x88888888\n";
    std::cout << "  Q = 0x" << std::hex << vec_reg.output().read_value() << std::dec << "\n";
    assert(vec_reg.output().read_value() == 0x55AA55AA);

    // Change DATA again while clock stays LOW
    data.write_value(0x77777777);
    vec_reg.evaluate();
    std::cout << "  Clock held LOW: DATA changed to 0x77777777\n";
    std::cout << "  Q = 0x" << std::hex << vec_reg.output().read_value() << std::dec << "\n";
    assert(vec_reg.output().read_value() == 0x55AA55AA);

    std::cout << "  [PASS] Q remains unchanged without clock ^\n\n";

    std::cout << "========================================\n";
    std::cout << "All 4 VectorRegister tests PASSED!\n";
    std::cout << "========================================\n";
}

int main() {
    run_vector_register_tests();
    return 0;
}

#include "../vector_cpu/core/memory/VectorRegisterFile.hpp"

#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <iostream>
#include <cassert>

void run_vector_register_file_tests()
{
    std::cout << "========================================\n";
    std::cout << "Running VectorRegisterFile Tests\n";
    std::cout << "========================================\n\n";

    constexpr std::size_t NUM_REGS = 32;
    constexpr std::size_t WIDTH = 128;
    constexpr std::size_t ADDR_WIDTH = 5;

    logic::Bus<ADDR_WIDTH> read_addr1;
    logic::Bus<ADDR_WIDTH> read_addr2;
    logic::Bus<ADDR_WIDTH> write_addr;
    logic::Bus<WIDTH> write_data;
    logic::Wire write_enable(logic::LogicState::LOW);
    logic::Wire reset(logic::LogicState::LOW);
    logic::Wire clock(logic::LogicState::LOW);

    cpu::VectorRegisterFile<NUM_REGS, WIDTH, ADDR_WIDTH> vrf(
        read_addr1,
        read_addr2,
        write_addr,
        write_data,
        write_enable,
        reset,
        clock
    );

    auto tick_clock = [&]() {
        clock.write(logic::LogicState::LOW);
        vrf.evaluate();
        clock.write(logic::LogicState::HIGH);
        vrf.evaluate();
        clock.write(logic::LogicState::LOW);
        vrf.evaluate();
    };

    // Initial settle
    clock.write(logic::LogicState::LOW);
    vrf.evaluate();

    // ---------------------------------------------------------
    // Test 1: Initial state is 0 for all registers
    // ---------------------------------------------------------
    std::cout << "[Test 1] Verify initial read outputs are 0\n";
    read_addr1.write_value(0);
    read_addr2.write_value(31);
    vrf.evaluate();
    assert(vrf.read_data1().read_value() == 0);
    assert(vrf.read_data2().read_value() == 0);
    std::cout << "  [PASS] Initial outputs are 0\n\n";

    // ---------------------------------------------------------
    // Test 2: Write with Global WE = 0 does NOT modify register
    // ---------------------------------------------------------
    std::cout << "[Test 2] Write with WE=0 does not update register\n";
    write_addr.write_value(5);
    write_data.write_value(0xDEADBEEF);
    write_enable.write(logic::LogicState::LOW);
    tick_clock();

    read_addr1.write_value(5);
    vrf.evaluate();
    assert(vrf.read_data1().read_value() == 0);
    std::cout << "  [PASS] Register 5 remains 0 when WE=0\n\n";

    // ---------------------------------------------------------
    // Test 3: Write with Global WE = 1 enables exactly one register
    // ---------------------------------------------------------
    std::cout << "[Test 3] Write with WE=1 updates exactly the targeted register\n";
    write_enable.write(logic::LogicState::HIGH);
    write_addr.write_value(5);
    write_data.write_value(0xDEADBEEF);
    tick_clock();

    read_addr1.write_value(5);
    vrf.evaluate();
    assert(vrf.read_data1().read_value() == 0xDEADBEEF);

    // Verify neighbors (V4 and V6) were not modified
    read_addr1.write_value(4);
    read_addr2.write_value(6);
    vrf.evaluate();
    assert(vrf.read_data1().read_value() == 0);
    assert(vrf.read_data2().read_value() == 0);
    std::cout << "  [PASS] V5 updated to 0xDEADBEEF, V4 and V6 untouched\n\n";

    // ---------------------------------------------------------
    // Test 4: Write to multiple registers and verify dual read ports
    // ---------------------------------------------------------
    std::cout << "[Test 4] Independent dual-port read multiplexing\n";
    // Write V1 = 0x1111, V2 = 0x2222, V31 = 0x9999
    write_enable.write(logic::LogicState::HIGH);

    write_addr.write_value(1);
    write_data.write_value(0x1111);
    tick_clock();

    write_addr.write_value(2);
    write_data.write_value(0x2222);
    tick_clock();

    write_addr.write_value(31);
    write_data.write_value(0x9999);
    tick_clock();

    // Read Port 1 = V1, Port 2 = V2
    read_addr1.write_value(1);
    read_addr2.write_value(2);
    vrf.evaluate();
    assert(vrf.read_data1().read_value() == 0x1111);
    assert(vrf.read_data2().read_value() == 0x2222);

    // Read Port 1 = V31, Port 2 = V5
    read_addr1.write_value(31);
    read_addr2.write_value(5);
    vrf.evaluate();
    assert(vrf.read_data1().read_value() == 0x9999);
    assert(vrf.read_data2().read_value() == 0xDEADBEEF);

    std::cout << "  [PASS] Dual read ports successfully read independent registers\n\n";

    // ---------------------------------------------------------
    // Test 5: Global Synchronous Reset
    // ---------------------------------------------------------
    std::cout << "[Test 5] Synchronous Reset zeroes all registers\n";
    reset.write(logic::LogicState::HIGH);
    write_enable.write(logic::LogicState::LOW);
    tick_clock();

    reset.write(logic::LogicState::LOW);

    for (std::uint64_t i = 0; i < NUM_REGS; ++i)
    {
        read_addr1.write_value(i);
        vrf.evaluate();
        assert(vrf.read_data1().read_value() == 0);
    }
    std::cout << "  [PASS] All 32 registers reset to 0\n\n";

    std::cout << "========================================\n";
    std::cout << "All VectorRegisterFile tests PASSED!\n";
    std::cout << "========================================\n";
}

int main()
{
    run_vector_register_file_tests();
    return 0;
}

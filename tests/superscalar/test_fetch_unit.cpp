#include "../../superscalar/core/fetch/FetchUnit.hpp"
#include "../../superscalar/core/fetch/FetchBundle.hpp"

#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/signals/logicState.hpp>

#include <cassert>
#include <iostream>

namespace {

using FetchUnit32 = cpu::FetchUnit<32, 32>;

struct FetchUnitHarness {
    logic::Wire clock{logic::LogicState::LOW};
    logic::Wire reset{logic::LogicState::LOW};

    logic::Bus<32> pc0;
    logic::Bus<32> pc1;

    logic::Bus<32> instruction0;
    logic::Bus<32> instruction1;

    logic::Wire hit0{logic::LogicState::LOW};
    logic::Wire hit1{logic::LogicState::LOW};

    logic::Wire valid0{logic::LogicState::LOW};
    logic::Wire valid1{logic::LogicState::LOW};

    logic::Bus<32> next_pc;

    FetchUnit32 fetch_unit{
        clock,
        reset,
        pc0,
        pc1,
        instruction0,
        instruction1,
        hit0,
        hit1,
        valid0,
        valid1,
        next_pc
    };

    void clock_edge()
    {
        // Falling edge: master latches capture
        clock.write(logic::LogicState::LOW);
        fetch_unit.evaluate();

        // Rising edge: slave latches output new state
        clock.write(logic::LogicState::HIGH);
        fetch_unit.evaluate();
    }

    void reset_system()
    {
        reset.write(logic::LogicState::HIGH);
        clock_edge();
        reset.write(logic::LogicState::LOW);
        clock.write(logic::LogicState::LOW);
        fetch_unit.evaluate();
    }
};

struct StallingFetchUnitHarness {
    logic::Wire clock{logic::LogicState::LOW};
    logic::Wire reset{logic::LogicState::LOW};

    logic::Bus<32> pc0;
    logic::Bus<32> pc1;

    logic::Bus<32> instruction0;
    logic::Bus<32> instruction1;

    logic::Wire hit0{logic::LogicState::LOW};
    logic::Wire hit1{logic::LogicState::LOW};

    logic::Wire valid0{logic::LogicState::LOW};
    logic::Wire valid1{logic::LogicState::LOW};

    logic::Bus<32> next_pc;
    logic::Wire stall{logic::LogicState::LOW};

    FetchUnit32 fetch_unit{
        clock,
        reset,
        pc0,
        pc1,
        instruction0,
        instruction1,
        hit0,
        hit1,
        valid0,
        valid1,
        next_pc,
        stall
    };

    void clock_edge()
    {
        clock.write(logic::LogicState::LOW);
        fetch_unit.evaluate();

        clock.write(logic::LogicState::HIGH);
        fetch_unit.evaluate();
    }

    void reset_system()
    {
        reset.write(logic::LogicState::HIGH);
        clock_edge();
        reset.write(logic::LogicState::LOW);
        clock.write(logic::LogicState::LOW);
        fetch_unit.evaluate();
    }
};

void test_initial_reset_and_addresses() {
    std::cout << "[Test 1] Initial reset and address outputs...\n";

    FetchUnitHarness h;

    // During active reset, valids must remain LOW even if hits are asserted
    h.hit0.write(logic::LogicState::HIGH);
    h.hit1.write(logic::LogicState::HIGH);
    h.reset.write(logic::LogicState::HIGH);

    h.clock_edge();

    assert(h.pc0.read_value() == 0x00);
    assert(h.pc1.read_value() == 0x04);
    assert(h.next_pc.read_value() == 0x08);
    assert(h.valid0.read() == logic::LogicState::LOW);
    assert(h.valid1.read() == logic::LogicState::LOW);

    // Deassert reset
    h.reset.write(logic::LogicState::LOW);
    h.fetch_unit.evaluate();

    assert(h.pc0.read_value() == 0x00);
    assert(h.pc1.read_value() == 0x04);
    assert(h.next_pc.read_value() == 0x08);
    assert(h.valid0.read() == logic::LogicState::HIGH);
    assert(h.valid1.read() == logic::LogicState::HIGH);

    std::cout << "  [PASS] PC0=0x00, PC1=0x04, next_pc=0x08, reset suppress verified\n";
}

void test_sequential_pc_advancement() {
    std::cout << "[Test 2] Sequential dual-issue PC advancement (+8 bytes/cycle)...\n";

    FetchUnitHarness h;
    h.reset_system();

    const std::size_t expected_pc0[]    = {0x00, 0x08, 0x10, 0x18, 0x20, 0x28};
    const std::size_t expected_pc1[]    = {0x04, 0x0C, 0x14, 0x1C, 0x24, 0x2C};
    const std::size_t expected_next[]   = {0x08, 0x10, 0x18, 0x20, 0x28, 0x30};

    for (std::size_t cycle = 0; cycle < 6; ++cycle)
    {
        assert(h.pc0.read_value() == expected_pc0[cycle]);
        assert(h.pc1.read_value() == expected_pc1[cycle]);
        assert(h.next_pc.read_value() == expected_next[cycle]);

        std::cout << "  Cycle " << cycle
                  << ": PC0 = 0x" << std::hex << h.pc0.read_value()
                  << ", PC1 = 0x" << h.pc1.read_value()
                  << ", next_pc = 0x" << h.next_pc.read_value()
                  << std::dec << "\n";

        h.clock_edge();
    }

    std::cout << "  [PASS] Sequential PC increments by 8 confirmed\n";
}

void test_hit_valid_propagation() {
    std::cout << "[Test 3] Cache hit to instruction valid propagation...\n";

    FetchUnitHarness h;
    h.reset_system();

    // Permutation 1: Both hit
    h.hit0.write(logic::LogicState::HIGH);
    h.hit1.write(logic::LogicState::HIGH);
    h.fetch_unit.evaluate();
    assert(h.valid0.read() == logic::LogicState::HIGH);
    assert(h.valid1.read() == logic::LogicState::HIGH);

    // Permutation 2: Slot 0 hit, Slot 1 miss
    h.hit0.write(logic::LogicState::HIGH);
    h.hit1.write(logic::LogicState::LOW);
    h.fetch_unit.evaluate();
    assert(h.valid0.read() == logic::LogicState::HIGH);
    assert(h.valid1.read() == logic::LogicState::LOW);

    // Permutation 3: Slot 0 miss, Slot 1 hit
    h.hit0.write(logic::LogicState::LOW);
    h.hit1.write(logic::LogicState::HIGH);
    h.fetch_unit.evaluate();
    assert(h.valid0.read() == logic::LogicState::LOW);
    assert(h.valid1.read() == logic::LogicState::HIGH);

    // Permutation 4: Both miss
    h.hit0.write(logic::LogicState::LOW);
    h.hit1.write(logic::LogicState::LOW);
    h.fetch_unit.evaluate();
    assert(h.valid0.read() == logic::LogicState::LOW);
    assert(h.valid1.read() == logic::LogicState::LOW);

    std::cout << "  [PASS] All hit/valid combinations verified\n";
}

void test_fetch_bundle_generation() {
    std::cout << "[Test 4] FetchBundle generation from FetchUnit...\n";

    FetchUnitHarness h;
    h.reset_system();

    h.hit0.write(logic::LogicState::HIGH);
    h.hit1.write(logic::LogicState::HIGH);
    h.instruction0.write_value(0x13000000); // NOP / ADDI x0, x0, 0
    h.instruction1.write_value(0x00500113); // ADDI x2, x0, 5

    h.fetch_unit.evaluate();

    cpu::FetchBundle bundle = h.fetch_unit.get_bundle();

    assert(bundle.getPC0() == 0x00);
    assert(bundle.getPC1() == 0x04);
    assert(bundle.getInstruction0() == 0x13000000);
    assert(bundle.getInstruction1() == 0x00500113);
    assert(bundle.isValid0() == true);
    assert(bundle.isValid1() == true);

    std::cout << "  [PASS] FetchBundle correctly bundles dual-issue state\n";
}

void test_stall_freezes_pc() {
    std::cout << "[Test 5] Stall freezes ProgramCounter progression...\n";

    StallingFetchUnitHarness h;
    h.reset_system();

    // Step to PC = 0x10
    h.clock_edge(); // PC -> 0x08
    h.clock_edge(); // PC -> 0x10
    assert(h.pc0.read_value() == 0x10);
    assert(h.pc1.read_value() == 0x14);
    assert(h.next_pc.read_value() == 0x18);

    // Assert stall
    h.stall.write(logic::LogicState::HIGH);

    // Clock edge 1 during stall
    h.clock_edge();
    assert(h.pc0.read_value() == 0x10);
    assert(h.pc1.read_value() == 0x14);
    assert(h.next_pc.read_value() == 0x18);

    // Clock edge 2 during stall
    h.clock_edge();
    assert(h.pc0.read_value() == 0x10);
    assert(h.pc1.read_value() == 0x14);
    assert(h.next_pc.read_value() == 0x18);

    // Deassert stall
    h.stall.write(logic::LogicState::LOW);
    h.clock_edge();

    // PC resumes advancing
    assert(h.pc0.read_value() == 0x18);
    assert(h.pc1.read_value() == 0x1C);
    assert(h.next_pc.read_value() == 0x20);

    std::cout << "  [PASS] Stall freezes and unfreezes PC accurately\n";
}

void test_midstream_reset() {
    std::cout << "[Test 6] Mid-stream reset returns PC to zero...\n";

    FetchUnitHarness h;
    h.reset_system();

    // Advance several cycles
    for (int i = 0; i < 5; ++i) {
        h.clock_edge();
    }
    assert(h.pc0.read_value() == 0x28);

    // Assert reset
    h.reset.write(logic::LogicState::HIGH);
    h.clock_edge();

    assert(h.pc0.read_value() == 0x00);
    assert(h.pc1.read_value() == 0x04);
    assert(h.next_pc.read_value() == 0x08);

    h.reset.write(logic::LogicState::LOW);
    h.clock_edge();

    assert(h.pc0.read_value() == 0x08);

    std::cout << "  [PASS] Reset mid-stream restores PC to 0x00\n";
}

} // namespace

int main() {
    std::cout << "=======================================\n";
    std::cout << "--- Testing Superscalar FetchUnit ---\n";
    std::cout << "=======================================\n";

    test_initial_reset_and_addresses();
    test_sequential_pc_advancement();
    test_hit_valid_propagation();
    test_fetch_bundle_generation();
    test_stall_freezes_pc();
    test_midstream_reset();

    std::cout << "\n[PASS] All FetchUnit unit tests passed successfully!\n";
    return 0;
}

#include "../../superscalar/core/fetch/FetchUnit.hpp"
#include "../../superscalar/core/fetch/FetchBundle.hpp"
#include "../../superscalar/core/fetch/FetchDecodeReg.hpp"

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

struct RedirectingFetchUnitHarness {
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

    logic::Bus<32> redirect_pc;
    logic::Wire redirect{logic::LogicState::LOW};

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
        redirect_pc,
        redirect
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
        redirect.write(logic::LogicState::LOW);
        redirect_pc.clear();
        clock_edge();
        reset.write(logic::LogicState::LOW);
        clock.write(logic::LogicState::LOW);
        fetch_unit.evaluate();
    }
};

// 7. Branch target redirect wins over sequential next (+8)
void test_branch_target_redirect_wins() {
    std::cout << "[Test 7] Branch target redirect wins over sequential PC (+8)...\n";

    RedirectingFetchUnitHarness h;
    h.reset_system();

    // Advance 1 cycle sequentially: 0x00 -> 0x08
    h.clock_edge();
    assert(h.pc0.read_value() == 0x08);
    assert(h.pc1.read_value() == 0x0C);
    assert(h.next_pc.read_value() == 0x10);

    // Assert branch target redirect to 0x200
    h.redirect_pc.write_value(0x200);
    h.redirect.write(logic::LogicState::HIGH);

    // Clock edge: redirect MUST win over sequential PC (0x10)
    h.clock_edge();

    assert(h.pc0.read_value() == 0x200);
    assert(h.pc1.read_value() == 0x204);
    assert(h.next_pc.read_value() == 0x208);

    // Deassert redirect: sequential execution resumes from target + 8
    h.redirect.write(logic::LogicState::LOW);
    h.clock_edge();

    assert(h.pc0.read_value() == 0x208);
    assert(h.pc1.read_value() == 0x20C);
    assert(h.next_pc.read_value() == 0x210);

    // Another cycle of sequential execution
    h.clock_edge();
    assert(h.pc0.read_value() == 0x210);
    assert(h.pc1.read_value() == 0x214);
    assert(h.next_pc.read_value() == 0x218);

    std::cout << "  [PASS] Branch target 0x200 overrode sequential PC and execution resumed cleanly.\n";
}

// 8. Back-to-back branch redirects
void test_back_to_back_redirects() {
    std::cout << "[Test 8] Back-to-back branch redirects...\n";

    RedirectingFetchUnitHarness h;
    h.reset_system();

    // Target 1: 0x1000
    h.redirect_pc.write_value(0x1000);
    h.redirect.write(logic::LogicState::HIGH);
    h.clock_edge();

    assert(h.pc0.read_value() == 0x1000);
    assert(h.pc1.read_value() == 0x1004);
    assert(h.next_pc.read_value() == 0x1008);

    // Immediate Target 2: 0x3000
    h.redirect_pc.write_value(0x3000);
    h.redirect.write(logic::LogicState::HIGH);
    h.clock_edge();

    assert(h.pc0.read_value() == 0x3000);
    assert(h.pc1.read_value() == 0x3004);
    assert(h.next_pc.read_value() == 0x3008);

    // Deassert redirect
    h.redirect.write(logic::LogicState::LOW);
    h.clock_edge();

    assert(h.pc0.read_value() == 0x3008);
    assert(h.pc1.read_value() == 0x300C);
    assert(h.next_pc.read_value() == 0x3010);

    std::cout << "  [PASS] Consecutive redirects handled with correct PC updates.\n";
}

// 9. End-to-end integration: FetchUnit redirect + FetchDecodeReg flush
void test_combined_redirect_with_fetch_decode_flush() {
    std::cout << "[Test 9] End-to-end: FetchUnit redirect + FetchDecodeReg flush...\n";

    // Setup shared clock and reset
    logic::Wire clock{logic::LogicState::LOW};
    logic::Wire reset{logic::LogicState::LOW};

    // FetchUnit signals
    logic::Bus<32> fetch_pc0;
    logic::Bus<32> fetch_pc1;
    logic::Bus<32> fetch_instr0;
    logic::Bus<32> fetch_instr1;
    logic::Wire hit0{logic::LogicState::HIGH};
    logic::Wire hit1{logic::LogicState::HIGH};
    logic::Wire fetch_valid0{logic::LogicState::LOW};
    logic::Wire fetch_valid1{logic::LogicState::LOW};
    logic::Bus<32> next_pc;

    logic::Bus<32> redirect_pc;
    logic::Wire redirect{logic::LogicState::LOW};

    FetchUnit32 fetch_unit{
        clock, reset,
        fetch_pc0, fetch_pc1,
        fetch_instr0, fetch_instr1,
        hit0, hit1,
        fetch_valid0, fetch_valid1,
        next_pc,
        redirect_pc, redirect
    };

    // FetchDecodeReg signals
    logic::Wire reg_enable{logic::LogicState::HIGH};
    logic::Wire reg_flush{logic::LogicState::LOW};

    logic::Bus<32> decode_pc0;
    logic::Bus<32> decode_instr0;
    logic::Wire decode_valid0{logic::LogicState::LOW};

    logic::Bus<32> decode_pc1;
    logic::Bus<32> decode_instr1;
    logic::Wire decode_valid1{logic::LogicState::LOW};

    cpu::FetchDecodeReg<32, 32> fetch_decode_reg{
        clock, reset,
        reg_enable, reg_flush,
        fetch_pc0, fetch_instr0, fetch_valid0,
        fetch_pc1, fetch_instr1, fetch_valid1,
        decode_pc0, decode_instr0, decode_valid0,
        decode_pc1, decode_instr1, decode_valid1
    };

    auto evaluate_pipeline = [&]() {
        fetch_unit.evaluate();
        fetch_decode_reg.evaluate();
    };

    auto clock_pipeline = [&]() {
        clock.write(logic::LogicState::LOW);
        evaluate_pipeline();
        clock.write(logic::LogicState::HIGH);
        evaluate_pipeline();
    };

    // Reset pipeline
    reset.write(logic::LogicState::HIGH);
    clock_pipeline();
    reset.write(logic::LogicState::LOW);
    clock.write(logic::LogicState::LOW);
    evaluate_pipeline();

    // Mock instruction data
    constexpr uint32_t INST_BEQ = 0x11000000;
    constexpr uint32_t INST_ADD = 0x01000000;
    constexpr uint32_t INST_SUB = 0x02000000;
    constexpr uint32_t INST_AND = 0x03000000;
    constexpr uint32_t INST_TARGET_0 = 0x05000000;
    constexpr uint32_t INST_TARGET_1 = 0x06000000;

    // Cycle 1: Fetch branch at 0x00 (BEQ) and sequential inst at 0x04 (ADD)
    fetch_instr0.write_value(INST_BEQ);
    fetch_instr1.write_value(INST_ADD);
    clock_pipeline();

    // Decode now holds branch and sequential inst from wrong/speculative path
    assert(decode_pc0.read_value() == 0x00);
    assert(decode_instr0.read_value() == INST_BEQ);
    assert(decode_valid0.read() == logic::LogicState::HIGH);
    assert(decode_pc1.read_value() == 0x04);
    assert(decode_instr1.read_value() == INST_ADD);
    assert(decode_valid1.read() == logic::LogicState::HIGH);

    // FetchUnit is currently fetching 0x08 (SUB) and 0x0C (AND)
    assert(fetch_pc0.read_value() == 0x08);
    assert(fetch_pc1.read_value() == 0x0C);

    // Cycle 2: Clock in wrong-path instructions (SUB, AND) into FetchDecodeReg
    fetch_instr0.write_value(INST_SUB);
    fetch_instr1.write_value(INST_AND);
    clock_pipeline();

    assert(decode_pc0.read_value() == 0x08);
    assert(decode_instr0.read_value() == INST_SUB);
    assert(decode_valid0.read() == logic::LogicState::HIGH);

    // --- RECOVERY EVENT: Branch BEQ misprediction detected! ---
    // Recovery actions:
    // 1. Flush FetchDecodeReg (clears wrong-path instructions)
    // 2. Redirect FetchUnit to branch target 0x200
    reg_flush.write(logic::LogicState::HIGH);
    redirect_pc.write_value(0x200);
    redirect.write(logic::LogicState::HIGH);

    // Clock edge applying recovery
    clock_pipeline();

    // Verify: FetchDecodeReg is completely flushed (valid=LOW, zeroed)
    assert(decode_valid0.read() == logic::LogicState::LOW);
    assert(decode_valid1.read() == logic::LogicState::LOW);
    assert(decode_pc0.read_value() == 0);
    assert(decode_pc1.read_value() == 0);

    // Verify: FetchUnit redirected to target 0x200
    assert(fetch_pc0.read_value() == 0x200);
    assert(fetch_pc1.read_value() == 0x204);
    assert(next_pc.read_value() == 0x208);

    // Cycle 3: Deassert recovery controls, present correct-path instructions
    reg_flush.write(logic::LogicState::LOW);
    redirect.write(logic::LogicState::LOW);

    fetch_instr0.write_value(INST_TARGET_0);
    fetch_instr1.write_value(INST_TARGET_1);

    clock_pipeline();

    // Decode now latches correct-path instructions from redirected PC 0x200
    assert(decode_valid0.read() == logic::LogicState::HIGH);
    assert(decode_valid1.read() == logic::LogicState::HIGH);
    assert(decode_pc0.read_value() == 0x200);
    assert(decode_instr0.read_value() == INST_TARGET_0);
    assert(decode_pc1.read_value() == 0x204);
    assert(decode_instr1.read_value() == INST_TARGET_1);

    // FetchUnit continues sequentially from 0x208
    assert(fetch_pc0.read_value() == 0x208);
    assert(fetch_pc1.read_value() == 0x20C);
    assert(next_pc.read_value() == 0x210);

    std::cout << "  [PASS] Full recovery: wrong-path flushed, FetchUnit redirected, execution resumed at 0x200!\n";
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
    test_branch_target_redirect_wins();
    test_back_to_back_redirects();
    test_combined_redirect_with_fetch_decode_flush();

    std::cout << "\n[PASS] All FetchUnit unit tests passed successfully!\n";
    return 0;
}

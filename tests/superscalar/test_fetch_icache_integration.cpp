#include "../../superscalar/core/FetchUnit.hpp"
#include "../../superscalar/core/FetchBundle.hpp"
#include "../../superscalar/core/CacheLine.hpp"
#include "../../superscalar/core/InstructionCacheController.hpp"
#include "../../superscalar/core/SuperScalarInstructionMemory.hpp"

#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/signals/logicState.hpp>

#include <cassert>
#include <iostream>
#include <vector>

namespace {

using Cache = cpu::InstructionCacheLine<32, 32, 16, 4>;
using Controller = cpu::InstructionCacheController<32, 32, 16, 4>;
using Memory = cpu::SuperscalarInstructionMemory<32, 8, 32>;
using FetchUnit32 = cpu::FetchUnit<32, 32>;

struct IntegratedFetchCacheHarness {
    // Control wires
    logic::Wire clock{logic::LogicState::LOW};
    logic::Wire reset{logic::LogicState::LOW};
    logic::Wire stall{logic::LogicState::LOW};

    // FetchUnit <-> Cache buses
    logic::Bus<32> pc0;
    logic::Bus<32> pc1;
    logic::Bus<32> instruction0;
    logic::Bus<32> instruction1;
    logic::Wire hit0{logic::LogicState::LOW};
    logic::Wire hit1{logic::LogicState::LOW};
    logic::Wire valid0{logic::LogicState::LOW};
    logic::Wire valid1{logic::LogicState::LOW};
    logic::Bus<32> next_pc;

    // Cache miss interface to Controller
    logic::Wire cache_miss{logic::LogicState::LOW};
    logic::Bus<32> miss_address;
    logic::Wire refill_done{logic::LogicState::LOW};

    // Controller <-> Memory Port 0 interface
    logic::Wire memory_request0{logic::LogicState::LOW};
    logic::Bus<32> memory_address0;
    logic::Wire memory_response0{logic::LogicState::LOW};
    logic::Bus<32> memory_instruction0;

    // Memory Port 1 (idle / tied low)
    logic::Wire memory_request1{logic::LogicState::LOW};
    logic::Bus<32> memory_address1;
    logic::Wire memory_response1{logic::LogicState::LOW};
    logic::Bus<32> memory_instruction1;

    // Cache enable
    logic::Wire cache_enable{logic::LogicState::HIGH};

    // Integrated components
    Cache cache{
        cache_enable,
        pc0,
        pc1,
        instruction0,
        instruction1,
        hit0,
        hit1
    };

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

    Controller controller{
        clock,
        reset,
        cache_miss,
        miss_address,
        memory_request0,
        memory_address0,
        memory_response0,
        memory_instruction0,
        refill_done,
        cache
    };

    Memory memory;

    IntegratedFetchCacheHarness(const std::vector<std::size_t>& rom_contents)
        : memory(
            memory_request0,
            memory_request1,
            memory_address0,
            memory_address1,
            memory_instruction0,
            memory_instruction1,
            memory_response0,
            memory_response1,
            rom_contents
        )
    {
    }

    void update_miss_detection()
    {
        // When not already servicing a refill, check if current fetch misses
        if (controller.state() == Controller::State::IDLE)
        {
            if (hit0.read() == logic::LogicState::LOW)
            {
                cache_miss.write(logic::LogicState::HIGH);
                miss_address.write_value(pc0.read_value());
            }
            else if (hit1.read() == logic::LogicState::LOW)
            {
                cache_miss.write(logic::LogicState::HIGH);
                miss_address.write_value(pc1.read_value());
            }
            else
            {
                cache_miss.write(logic::LogicState::LOW);
            }
        }
        else
        {
            // Once controller leaves IDLE, miss request has been accepted
            cache_miss.write(logic::LogicState::LOW);
        }
    }

    void evaluate_system()
    {
        // 1. Evaluate controller to update registered state on clock transition
        controller.evaluate();

        // 2. FetchUnit drives PC0 and PC1 onto cache address ports
        fetch_unit.evaluate();

        // 3. Cache performs lookup for PC0 and PC1
        cache.evaluate();

        // 4. Update miss detector
        update_miss_detection();

        // 5. Stall FetchUnit when cache misses or controller is refilling
        const bool should_stall = (controller.state() != Controller::State::IDLE) ||
                                  (cache_miss.read() == logic::LogicState::HIGH);
        stall.write(should_stall ? logic::LogicState::HIGH : logic::LogicState::LOW);

        // 6. Re-evaluate FetchUnit with updated stall status and cache hits
        fetch_unit.evaluate();

        // 7. Evaluate controller combinational logic (memory request / next state)
        controller.evaluate();

        // 8. Evaluate ROM memory
        memory.evaluate();

        // 9. Re-evaluate controller to receive memory response
        controller.evaluate();
    }

    void clock_edge()
    {
        clock.write(logic::LogicState::LOW);
        evaluate_system();

        clock.write(logic::LogicState::HIGH);
        evaluate_system();
    }

    void reset_system()
    {
        reset.write(logic::LogicState::HIGH);
        clock_edge();
        reset.write(logic::LogicState::LOW);
        clock.write(logic::LogicState::LOW);
        evaluate_system();
    }

    void step_refill_to_completion()
    {
        std::size_t safety_timeout = 50;
        while (controller.state() != Controller::State::IDLE && safety_timeout-- > 0)
        {
            clock_edge();
        }
        evaluate_system();
    }
};

void test_cold_start_refill_and_dual_fetch() {
    std::cout << "[Test 1] Cold start: I-cache miss triggers refill, then dual-issue fetch succeeds...\n";

    const std::vector<std::size_t> rom = {
        0x00100093, // 0x00: addi x1, x0, 1
        0x00200113, // 0x04: addi x2, x0, 2
        0x00300193, // 0x08: addi x3, x0, 3
        0x00400213, // 0x0C: addi x4, x0, 4
        0x00500293, // 0x10: addi x5, x0, 5
        0x00600313  // 0x14: addi x6, x0, 6
    };

    IntegratedFetchCacheHarness h(rom);
    h.reset_system();

    // 1. Initial PC is 0x00. Cache is cold, so both hits must be LOW
    assert(h.pc0.read_value() == 0x00);
    assert(h.pc1.read_value() == 0x04);
    assert(h.hit0.read() == logic::LogicState::LOW);
    assert(h.hit1.read() == logic::LogicState::LOW);
    assert(h.valid0.read() == logic::LogicState::LOW);
    assert(h.valid1.read() == logic::LogicState::LOW);
    assert(h.cache_miss.read() == logic::LogicState::HIGH);
    assert(h.miss_address.read_value() == 0x00);
    assert(h.stall.read() == logic::LogicState::HIGH);

    std::cout << "  [Step 1] Cold miss detected at PC0=0x00. Fetch stalled.\n";

    // 2. Clock edge starts refill: controller enters REFILL_REQUEST
    h.clock_edge();
    assert(h.controller.state() == Controller::State::REFILL_REQUEST);
    assert(h.pc0.read_value() == 0x00); // Fetch remains frozen during refill

    // 3. Step through line refill to completion
    h.step_refill_to_completion();
    assert(h.controller.state() == Controller::State::IDLE);
    std::cout << "  [Step 2] Line 0 (0x00..0x0C) refilled into I-cache.\n";

    // 4. Now line 0 is resident. Both 0x00 and 0x04 hit immediately!
    assert(h.hit0.read() == logic::LogicState::HIGH);
    assert(h.hit1.read() == logic::LogicState::HIGH);
    assert(h.valid0.read() == logic::LogicState::HIGH);
    assert(h.valid1.read() == logic::LogicState::HIGH);
    assert(h.instruction0.read_value() == 0x00100093);
    assert(h.instruction1.read_value() == 0x00200113);
    assert(h.stall.read() == logic::LogicState::LOW);

    cpu::FetchBundle bundle = h.fetch_unit.get_bundle();
    assert(bundle.getPC0() == 0x00);
    assert(bundle.getPC1() == 0x04);
    assert(bundle.getInstruction0() == 0x00100093);
    assert(bundle.getInstruction1() == 0x00200113);
    assert(bundle.isValid0() && bundle.isValid1());

    std::cout << "  [Step 3] FetchBundle captured instructions: [0x00]=0x"
              << std::hex << bundle.getInstruction0() << ", [0x04]=0x"
              << bundle.getInstruction1() << std::dec << " (both valid)\n";
    std::cout << "  [PASS] Cold start refill & dual-issue fetch verified\n";
}

void test_inline_consecutive_hit_zero_stall() {
    std::cout << "\n[Test 2] In-line dual hit: PC advances to 0x08, hits immediately with 0 stalls (IPC = 2.0)...\n";

    const std::vector<std::size_t> rom = {
        0x00100093, // 0x00
        0x00200113, // 0x04
        0x00300193, // 0x08
        0x00400213, // 0x0C
        0x00500293, // 0x10
        0x00600313  // 0x14
    };

    IntegratedFetchCacheHarness h(rom);
    h.reset_system();

    // Cold miss at 0x00 & refill line 0
    h.clock_edge();
    h.step_refill_to_completion();

    // Verify 0x00 / 0x04 are valid and stall is deasserted
    assert(h.stall.read() == logic::LogicState::LOW);
    assert(h.valid0.read() == logic::LogicState::HIGH);
    assert(h.valid1.read() == logic::LogicState::HIGH);

    // Next clock edge: FetchUnit advances PC by 8 -> PC0 = 0x08, PC1 = 0x0C
    h.clock_edge();

    assert(h.pc0.read_value() == 0x08);
    assert(h.pc1.read_value() == 0x0C);
    assert(h.next_pc.read_value() == 0x10);

    // Both words are already resident in line 0: immediate hit!
    assert(h.hit0.read() == logic::LogicState::HIGH);
    assert(h.hit1.read() == logic::LogicState::HIGH);
    assert(h.valid0.read() == logic::LogicState::HIGH);
    assert(h.valid1.read() == logic::LogicState::HIGH);
    assert(h.stall.read() == logic::LogicState::LOW);
    assert(h.instruction0.read_value() == 0x00300193);
    assert(h.instruction1.read_value() == 0x00400213);

    cpu::FetchBundle bundle = h.fetch_unit.get_bundle();
    assert(bundle.getPC0() == 0x08);
    assert(bundle.getPC1() == 0x0C);
    assert(bundle.getInstruction0() == 0x00300193);
    assert(bundle.getInstruction1() == 0x00400213);

    std::cout << "  [Step 1] In-line hit confirmed: PC0=0x08, PC1=0x0C with zero wait cycles!\n";
    std::cout << "  [PASS] Zero-stall dual-issue cache hit verified\n";
}

void test_multiline_transition_and_refill() {
    std::cout << "\n[Test 3] Multi-line transition: PC advances to line 1 (0x10), misses, refills, and fetches...\n";

    const std::vector<std::size_t> rom = {
        0x00100093, // 0x00 (Line 0, word 0)
        0x00200113, // 0x04 (Line 0, word 1)
        0x00300193, // 0x08 (Line 0, word 2)
        0x00400213, // 0x0C (Line 0, word 3)
        0x00500293, // 0x10 (Line 1, word 0)
        0x00600313, // 0x14 (Line 1, word 1)
        0x00700393, // 0x18 (Line 1, word 2)
        0x00800413  // 0x1C (Line 1, word 3)
    };

    IntegratedFetchCacheHarness h(rom);
    h.reset_system();

    // Refill Line 0
    h.clock_edge();
    h.step_refill_to_completion();

    // Step to 0x08 (hits in Line 0)
    h.clock_edge();
    assert(h.pc0.read_value() == 0x08);

    // Step to 0x10 (Line 1)
    h.clock_edge();
    assert(h.pc0.read_value() == 0x10);
    assert(h.pc1.read_value() == 0x14);

    // Line 1 is not in cache yet: miss!
    assert(h.hit0.read() == logic::LogicState::LOW);
    assert(h.hit1.read() == logic::LogicState::LOW);
    assert(h.cache_miss.read() == logic::LogicState::HIGH);
    assert(h.miss_address.read_value() == 0x10);
    assert(h.stall.read() == logic::LogicState::HIGH);

    std::cout << "  [Step 1] Miss detected at line boundary PC = 0x10\n";

    // Refill Line 1
    h.clock_edge();
    h.step_refill_to_completion();

    // Line 1 is now in cache: both 0x10 and 0x14 hit!
    assert(h.hit0.read() == logic::LogicState::HIGH);
    assert(h.hit1.read() == logic::LogicState::HIGH);
    assert(h.valid0.read() == logic::LogicState::HIGH);
    assert(h.valid1.read() == logic::LogicState::HIGH);
    assert(h.instruction0.read_value() == 0x00500293);
    assert(h.instruction1.read_value() == 0x00600313);
    assert(h.stall.read() == logic::LogicState::LOW);

    std::cout << "  [Step 2] Line 1 refilled. Delivered instructions at 0x10 and 0x14.\n";
    std::cout << "  [PASS] Multi-line cache transition and refill verified\n";
}

void test_dual_port_cross_line_fetch() {
    std::cout << "\n[Test 4] Dual-port cross-line fetch: Lane 0 in Line 0, Lane 1 in Line 1...\n";

    // Line 0: 0x00 .. 0x0C
    // Line 1: 0x10 .. 0x1C
    const std::vector<std::size_t> rom = {
        0x1000, 0x1004, 0x1008, 0x100C, // Line 0
        0x2000, 0x2004, 0x2008, 0x200C  // Line 1
    };

    IntegratedFetchCacheHarness h(rom);
    h.reset_system();

    // Pre-install both line 0 and line 1 into cache
    h.cache.install_line(0x00, {0x1000, 0x1004, 0x1008, 0x100C});
    h.cache.install_line(0x10, {0x2000, 0x2004, 0x2008, 0x200C});

    // Directly present address 0x0C to port 0 and 0x10 to port 1
    // (Tests dual-port simultaneous lookup crossing the 16-byte cache line boundary)
    h.pc0.write_value(0x0C);
    h.pc1.write_value(0x10);
    h.cache.evaluate();

    // Dual-port cache must hit both ports simultaneously across lines
    assert(h.hit0.read() == logic::LogicState::HIGH);
    assert(h.hit1.read() == logic::LogicState::HIGH);
    assert(h.instruction0.read_value() == 0x100C);
    assert(h.instruction1.read_value() == 0x2000);

    // Bundle generation from dual-port cross-line hit
    cpu::FetchBundle bundle(
        0x0C,
        static_cast<uint32_t>(h.instruction0.read_value()),
        h.hit0.read() == logic::LogicState::HIGH,
        0x10,
        static_cast<uint32_t>(h.instruction1.read_value()),
        h.hit1.read() == logic::LogicState::HIGH
    );
    assert(bundle.getPC0() == 0x0C);
    assert(bundle.getPC1() == 0x10);
    assert(bundle.getInstruction0() == 0x100C);
    assert(bundle.getInstruction1() == 0x2000);
    assert(bundle.isValid0() && bundle.isValid1());

    std::cout << "  [Step 1] Dual-port cross-line lookup succeeded: [0x0C]=0x"
              << std::hex << bundle.getInstruction0() << ", [0x10]=0x"
              << bundle.getInstruction1() << std::dec << "\n";
    std::cout << "  [PASS] Cross-line dual fetch verified\n";
}

void test_continuous_program_stream_execution() {
    std::cout << "\n[Test 5] Continuous program stream: fetch 16 instructions sequentially...\n";

    // 16 instructions across 4 cache lines
    std::vector<std::size_t> rom;
    for (std::size_t i = 0; i < 16; ++i) {
        rom.push_back(0x10000000 + i * 4);
    }

    IntegratedFetchCacheHarness h(rom);
    h.reset_system();

    std::size_t total_cycles = 0;
    std::size_t valid_instructions_fetched = 0;
    std::vector<std::size_t> fetched_stream;

    // Run until 16 instructions are fetched (8 dual bundles)
    while (fetched_stream.size() < 16 && total_cycles < 200)
    {
        total_cycles++;

        if (h.stall.read() == logic::LogicState::HIGH)
        {
            // Controller is refilling
            h.clock_edge();
            continue;
        }

        // Cache hit: record valid bundle instructions
        if (h.valid0.read() == logic::LogicState::HIGH)
        {
            fetched_stream.push_back(h.instruction0.read_value());
            valid_instructions_fetched++;
        }
        if (h.valid1.read() == logic::LogicState::HIGH)
        {
            fetched_stream.push_back(h.instruction1.read_value());
            valid_instructions_fetched++;
        }

        h.clock_edge();
    }

    assert(fetched_stream.size() == 16);
    for (std::size_t i = 0; i < 16; ++i)
    {
        assert(fetched_stream[i] == rom[i]);
    }

    std::cout << "  [Step 1] Successfully fetched all " << valid_instructions_fetched
              << " instructions across 4 cache lines in " << total_cycles << " cycles.\n";
    std::cout << "  [Step 2] Verified every instruction in stream matches ROM exactly.\n";
    std::cout << "  [PASS] Continuous program fetch stream verified!\n";
}

} // namespace

int main() {
    std::cout << "===============================================================\n";
    std::cout << "--- Superscalar Fetch + I-Cache Full Integration Tests ---\n";
    std::cout << "===============================================================\n";

    test_cold_start_refill_and_dual_fetch();
    test_inline_consecutive_hit_zero_stall();
    test_multiline_transition_and_refill();
    test_dual_port_cross_line_fetch();
    test_continuous_program_stream_execution();

    std::cout << "\n[PASS] All Fetch + I-Cache integration tests passed successfully!\n";
    return 0;
}

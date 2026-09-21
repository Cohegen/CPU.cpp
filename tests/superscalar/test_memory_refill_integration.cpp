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

struct Testbench {
    // Control wires
    logic::Wire clock{logic::LogicState::LOW};
    logic::Wire reset{logic::LogicState::LOW};

    // Miss interface to Controller
    logic::Wire miss{logic::LogicState::LOW};
    logic::Bus<32> miss_address;

    // Memory request / response bus between Controller and Memory Port 0
    logic::Wire memory_request{logic::LogicState::LOW};
    logic::Bus<32> memory_address;
    logic::Wire memory_response{logic::LogicState::LOW};
    logic::Bus<32> memory_instruction;

    // Refill completion pulse from Controller
    logic::Wire refill_done{logic::LogicState::LOW};

    // Memory Port 1 (idle / tied low)
    logic::Wire memory_request1{logic::LogicState::LOW};
    logic::Bus<32> memory_address1;
    logic::Wire memory_response1{logic::LogicState::LOW};
    logic::Bus<32> memory_instruction1;

    // Cache fetch ports
    logic::Wire cache_enable{logic::LogicState::HIGH};
    logic::Bus<32> cache_address0;
    logic::Bus<32> cache_address1;
    logic::Bus<32> cache_instruction0;
    logic::Bus<32> cache_instruction1;
    logic::Wire cache_hit0{logic::LogicState::LOW};
    logic::Wire cache_hit1{logic::LogicState::LOW};

    // Core components
    Cache cache{
        cache_enable,
        cache_address0,
        cache_address1,
        cache_instruction0,
        cache_instruction1,
        cache_hit0,
        cache_hit1
    };

    Controller controller{
        clock,
        reset,
        miss,
        miss_address,
        memory_request,
        memory_address,
        memory_response,
        memory_instruction,
        refill_done,
        cache
    };

    Memory memory;

    Testbench(const std::vector<std::size_t>& rom_contents)
        : memory(
              memory_request,
              memory_request1,
              memory_address,
              memory_address1,
              memory_instruction,
              memory_instruction1,
              memory_response,
              memory_response1,
              rom_contents
          )
    {
    }

    void clock_edge()
    {
        // Falling edge / setup: master latches capture next state
        clock.write(logic::LogicState::LOW);
        controller.evaluate();

        // Rising edge / state capture: slave latches output new state
        clock.write(logic::LogicState::HIGH);
        controller.evaluate();
    }

    void reset_system()
    {
        reset.write(logic::LogicState::HIGH);
        clock_edge();
        reset.write(logic::LogicState::LOW);
        clock_edge();
    }
};

void test_clock_edge_refill_sequence() {
    std::cout << "[Test 1] Testing exact clock-edge refill sequence:\n";
    std::cout << "         IDLE -> REFILL_REQUEST -> REFILL_WAIT -> REFILL_REQUEST ... -> INSTALL -> COMPLETE -> IDLE\n";

    const std::vector<std::size_t> rom_contents = {
        0x10101010, // 0x00: instruction 0
        0x20202020, // 0x04: instruction 1
        0x30303030, // 0x08: instruction 2
        0x40404040, // 0x0C: instruction 3
        0x50505050, // 0x10: instruction 4
        0x60606060  // 0x14: instruction 5
    };

    Testbench tb(rom_contents);

    // 1. Reset -> IDLE
    tb.reset_system();
    assert(tb.controller.state() == Controller::State::IDLE);
    std::cout << "  [Step 1] Reset -> IDLE confirmed\n";

    // 2. miss = HIGH -> clock edge -> REFILL_REQUEST
    tb.miss.write(logic::LogicState::HIGH);
    tb.miss_address.write_value(0x08);

    // Before clock edge, state remains IDLE
    tb.clock.write(logic::LogicState::LOW);
    tb.controller.evaluate();
    assert(tb.controller.state() == Controller::State::IDLE);

    // Rising clock edge triggers transition to REFILL_REQUEST
    tb.clock.write(logic::LogicState::HIGH);
    tb.controller.evaluate();
    assert(tb.controller.state() == Controller::State::REFILL_REQUEST);
    assert(tb.controller.line_base_address() == 0x00);
    assert(tb.controller.refill_word() == 0);
    assert(tb.memory_request.read() == logic::LogicState::HIGH);
    assert(tb.memory_address.read_value() == 0x00);
    tb.miss.write(logic::LogicState::LOW);
    std::cout << "  [Step 2] miss = HIGH -> clock edge -> REFILL_REQUEST (word 0, addr 0x00)\n";

    // 3. clock edge -> REFILL_WAIT
    tb.clock_edge();
    assert(tb.controller.state() == Controller::State::REFILL_WAIT);
    assert(tb.memory_request.read() == logic::LogicState::HIGH);
    assert(tb.memory_address.read_value() == 0x00);
    std::cout << "  [Step 3] clock edge -> REFILL_WAIT (waiting for memory response)\n";

    // Memory responds with word 0
    tb.memory.evaluate();
    assert(tb.memory_response.read() == logic::LogicState::HIGH);
    assert(tb.memory_instruction.read_value() == 0x10101010);

    // 4. memory_response = HIGH -> clock edge -> REFILL_REQUEST (word 1)
    tb.clock_edge();
    assert(tb.controller.state() == Controller::State::REFILL_REQUEST);
    assert(tb.controller.refill_word() == 1);
    assert(tb.controller.refill_buffer()[0] == 0x10101010);
    assert(tb.memory_request.read() == logic::LogicState::HIGH);
    assert(tb.memory_address.read_value() == 0x04);
    std::cout << "  [Step 4] memory_response = HIGH -> clock edge -> REFILL_REQUEST (word 1, addr 0x04)\n";

    // 5. clock edge -> REFILL_WAIT (word 1)
    tb.clock_edge();
    assert(tb.controller.state() == Controller::State::REFILL_WAIT);
    assert(tb.memory_request.read() == logic::LogicState::HIGH);
    assert(tb.memory_address.read_value() == 0x04);
    std::cout << "  [Step 5] clock edge -> REFILL_WAIT (word 1)\n";

    // Memory responds with word 1
    tb.memory.evaluate();
    assert(tb.memory_response.read() == logic::LogicState::HIGH);
    assert(tb.memory_instruction.read_value() == 0x20202020);

    // 6. memory_response = HIGH -> clock edge -> REFILL_REQUEST (word 2)
    tb.clock_edge();
    assert(tb.controller.state() == Controller::State::REFILL_REQUEST);
    assert(tb.controller.refill_word() == 2);
    assert(tb.controller.refill_buffer()[1] == 0x20202020);
    assert(tb.memory_request.read() == logic::LogicState::HIGH);
    assert(tb.memory_address.read_value() == 0x08);
    std::cout << "  [Step 6] memory_response = HIGH -> clock edge -> REFILL_REQUEST (word 2, addr 0x08)\n";

    // 7. clock edge -> REFILL_WAIT (word 2)
    tb.clock_edge();
    assert(tb.controller.state() == Controller::State::REFILL_WAIT);
    assert(tb.memory_request.read() == logic::LogicState::HIGH);
    assert(tb.memory_address.read_value() == 0x08);
    std::cout << "  [Step 7] clock edge -> REFILL_WAIT (word 2)\n";

    // Memory responds with word 2
    tb.memory.evaluate();
    assert(tb.memory_response.read() == logic::LogicState::HIGH);
    assert(tb.memory_instruction.read_value() == 0x30303030);

    // 8. memory_response = HIGH -> clock edge -> REFILL_REQUEST (word 3)
    tb.clock_edge();
    assert(tb.controller.state() == Controller::State::REFILL_REQUEST);
    assert(tb.controller.refill_word() == 3);
    assert(tb.controller.refill_buffer()[2] == 0x30303030);
    assert(tb.memory_request.read() == logic::LogicState::HIGH);
    assert(tb.memory_address.read_value() == 0x0C);
    std::cout << "  [Step 8] memory_response = HIGH -> clock edge -> REFILL_REQUEST (word 3, addr 0x0C)\n";

    // 9. clock edge -> REFILL_WAIT (word 3)
    tb.clock_edge();
    assert(tb.controller.state() == Controller::State::REFILL_WAIT);
    assert(tb.memory_request.read() == logic::LogicState::HIGH);
    assert(tb.memory_address.read_value() == 0x0C);
    std::cout << "  [Step 9] clock edge -> REFILL_WAIT (word 3)\n";

    // Memory responds with word 3
    tb.memory.evaluate();
    assert(tb.memory_response.read() == logic::LogicState::HIGH);
    assert(tb.memory_instruction.read_value() == 0x40404040);

    // 10. memory_response = HIGH -> clock edge -> INSTALL
    tb.clock_edge();
    assert(tb.controller.state() == Controller::State::INSTALL);
    assert(tb.controller.refill_buffer()[3] == 0x40404040);
    std::cout << "  [Step 10] memory_response = HIGH -> clock edge -> INSTALL\n";

    // 11. clock edge -> COMPLETE (refill_done = HIGH)
    tb.clock_edge();
    assert(tb.controller.state() == Controller::State::COMPLETE);
    assert(tb.refill_done.read() == logic::LogicState::HIGH);
    std::cout << "  [Step 11] clock edge -> COMPLETE (refill_done = HIGH)\n";

    // 12. clock edge -> IDLE (refill_done = LOW)
    tb.clock_edge();
    assert(tb.controller.state() == Controller::State::IDLE);
    assert(tb.refill_done.read() == logic::LogicState::LOW);
    std::cout << "  [Step 12] clock edge -> IDLE (refill_done = LOW)\n";
    std::cout << "  -> Exact clock-edge state sequence verified successfully!\n";
}

void test_complete_cache_line_refill() {
    std::cout << "\n[Test 2] Complete cache-line refill integration test...\n";

    const std::vector<std::size_t> rom_contents = {
        0x10101010, // 0x00: instruction 0
        0x20202020, // 0x04: instruction 1
        0x30303030, // 0x08: instruction 2
        0x40404040, // 0x0C: instruction 3
        0x50505050, // 0x10: instruction 4
        0x60606060  // 0x14: instruction 5
    };

    Testbench tb(rom_contents);
    tb.reset_system();

    // 1. Initial State: cold cache, lookup at 0x08 must MISS
    std::cout << "  [Step 1] Cold cache lookup at 0x08 misses...\n";
    tb.cache_address0.write_value(0x08);
    tb.cache.evaluate();
    assert(tb.cache_hit0.read() == logic::LogicState::LOW);
    assert(tb.cache_instruction0.read_value() == 0);
    std::cout << "    -> Cache miss confirmed at 0x08\n";

    // 2. Trigger cache miss at PC = 0x08
    std::cout << "  [Step 2] Trigger miss at 0x08 and step clock to transition to REFILL_REQUEST...\n";
    tb.miss.write(logic::LogicState::HIGH);
    tb.miss_address.write_value(0x08);
    tb.clock_edge();

    // Verify alignment: 0x08 & ~15 = 0x00
    assert(tb.controller.line_base_address() == 0x00);
    assert(tb.controller.state() == Controller::State::REFILL_REQUEST);
    assert(tb.refill_done.read() == logic::LogicState::LOW);
    tb.miss.write(logic::LogicState::LOW);
    std::cout << "    -> Miss at 0x08 correctly aligned to line base 0x00\n";

    // 3. Verify 4-word refill sequence with clock edges: 0x00 -> 0x04 -> 0x08 -> 0x0C
    const std::size_t expected_addresses[4] = {0x00, 0x04, 0x08, 0x0C};
    const std::size_t expected_instructions[4] = {
        0x10101010,
        0x20202020,
        0x30303030,
        0x40404040
    };

    for (std::size_t word = 0; word < 4; ++word) {
        std::cout << "  [Step 3." << word << "] Refill word " << word
                  << ": expecting request at 0x" << std::hex << expected_addresses[word]
                  << std::dec << "...\n";

        // In REFILL_REQUEST: memory request is asserted
        assert(tb.controller.state() == Controller::State::REFILL_REQUEST);
        assert(tb.memory_request.read() == logic::LogicState::HIGH);
        assert(tb.memory_address.read_value() == expected_addresses[word]);

        // Clock edge transitions to REFILL_WAIT
        tb.clock_edge();
        assert(tb.controller.state() == Controller::State::REFILL_WAIT);
        assert(tb.memory_request.read() == logic::LogicState::HIGH);
        assert(tb.memory_address.read_value() == expected_addresses[word]);

        // Memory responds to active request
        tb.memory.evaluate();
        assert(tb.memory_response.read() == logic::LogicState::HIGH);
        assert(tb.memory_instruction.read_value() == expected_instructions[word]);
        std::cout << "    -> Memory responded with instruction 0x"
                  << std::hex << tb.memory_instruction.read_value() << std::dec << "\n";

        // Clock edge captures instruction and transitions to next state
        tb.clock_edge();
        assert(tb.controller.refill_buffer()[word] == expected_instructions[word]);
        std::cout << "    -> refill_buffer[" << word << "] captured: 0x"
                  << std::hex << tb.controller.refill_buffer()[word] << std::dec << "\n";
    }

    // 4. INSTALL Phase: cache is refilled
    std::cout << "  [Step 4] Verify INSTALL state and transition to COMPLETE...\n";
    assert(tb.controller.state() == Controller::State::INSTALL);
    tb.clock_edge();

    // 5. COMPLETE Phase: refill_done pulse
    std::cout << "  [Step 5] Verify COMPLETE asserts refill_done = HIGH...\n";
    assert(tb.controller.state() == Controller::State::COMPLETE);
    assert(tb.refill_done.read() == logic::LogicState::HIGH);
    std::cout << "    -> refill_done is HIGH (active pulse)\n";

    // Next cycle: transition back to IDLE, refill_done drops to LOW
    tb.clock_edge();
    assert(tb.controller.state() == Controller::State::IDLE);
    assert(tb.refill_done.read() == logic::LogicState::LOW);
    std::cout << "    -> Returned to IDLE, refill_done dropped back to LOW\n";

    // 6. Verify subsequent lookup at 0x08 hits
    std::cout << "  [Step 6] Subsequent lookup at 0x08 hits...\n";
    tb.cache_address0.write_value(0x08);
    tb.cache.evaluate();
    assert(tb.cache_hit0.read() == logic::LogicState::HIGH);
    assert(tb.cache_instruction0.read_value() == 0x30303030);
    std::cout << "    -> Cache HIT at 0x08: returned instruction 0x"
              << std::hex << tb.cache_instruction0.read_value() << std::dec << "\n";

    // 7. Verify lookup at 0x0C hits without going back to memory
    std::cout << "  [Step 7] Lookup at 0x0C hits without memory request...\n";
    tb.cache_address0.write_value(0x0C);
    tb.cache.evaluate();
    assert(tb.cache_hit0.read() == logic::LogicState::HIGH);
    assert(tb.cache_instruction0.read_value() == 0x40404040);
    assert(tb.memory_request.read() == logic::LogicState::LOW);
    std::cout << "    -> Cache HIT at 0x0C: returned instruction 0x"
              << std::hex << tb.cache_instruction0.read_value()
              << ", memory_request is LOW\n" << std::dec;

    // 8. Verify all other words in line hit and dual-port capability
    std::cout << "  [Step 8] Dual-port lookup at 0x00 and 0x04 simultaneously...\n";
    tb.cache_address0.write_value(0x00);
    tb.cache_address1.write_value(0x04);
    tb.cache.evaluate();
    assert(tb.cache_hit0.read() == logic::LogicState::HIGH);
    assert(tb.cache_instruction0.read_value() == 0x10101010);
    assert(tb.cache_hit1.read() == logic::LogicState::HIGH);
    assert(tb.cache_instruction1.read_value() == 0x20202020);
    std::cout << "    -> Port 0 (0x00) returned 0x" << std::hex << tb.cache_instruction0.read_value()
              << ", Port 1 (0x04) returned 0x" << tb.cache_instruction1.read_value() << std::dec << "\n";

    // Dual-port simultaneous lookup at 0x08 and 0x0C
    std::cout << "  [Step 9] Dual-port lookup at 0x08 and 0x0C simultaneously...\n";
    tb.cache_address0.write_value(0x08);
    tb.cache_address1.write_value(0x0C);
    tb.cache.evaluate();
    assert(tb.cache_hit0.read() == logic::LogicState::HIGH);
    assert(tb.cache_instruction0.read_value() == 0x30303030);
    assert(tb.cache_hit1.read() == logic::LogicState::HIGH);
    assert(tb.cache_instruction1.read_value() == 0x40404040);
    std::cout << "    -> Port 0 (0x08) returned 0x" << std::hex << tb.cache_instruction0.read_value()
              << ", Port 1 (0x0C) returned 0x" << tb.cache_instruction1.read_value() << std::dec << "\n";

    // 9. Lookup for an unrefilled line (e.g. 0x10) must miss
    std::cout << "  [Step 10] Lookup outside refilled line at 0x10 still misses...\n";
    tb.cache_address0.write_value(0x10);
    tb.cache.evaluate();
    assert(tb.cache_hit0.read() == logic::LogicState::LOW);
    assert(tb.cache_instruction0.read_value() == 0);
    std::cout << "    -> Cache MISS confirmed at 0x10\n";
}

} // namespace

int main() {
    std::cout << "========================================================\n";
    std::cout << "--- Superscalar Memory Interface Integration Tests ---\n";
    std::cout << "========================================================\n";

    test_clock_edge_refill_sequence();
    test_complete_cache_line_refill();

    std::cout << "\n[PASS] All superscalar memory interface tests successful!\n";
    return 0;
}

#include "../../superscalar/core/FetchDecodeReg.hpp"
#include "../../superscalar/core/FetchBundle.hpp"
#include "../../include/isa/Instruction.hpp"
#include "../../include/isa/Opcode.hpp"

#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/signals/logicState.hpp>

#include <cassert>
#include <cstdint>
#include <iostream>

namespace {

using FetchDecodeReg32 = cpu::FetchDecodeReg<32, 32>;

// Helper to encode realistic instructions according to the CPU's ISA:
// Opcode (bits 31..26), Rd (bits 25..22), Rs1 (bits 21..18), Rs2 (bits 17..14)
constexpr uint32_t encode_r_type(cpu::Opcode op, uint32_t rd, uint32_t rs1, uint32_t rs2)
{
    return (static_cast<uint32_t>(op) << 26) |
           ((rd & 0x0F) << 22) |
           ((rs1 & 0x0F) << 18) |
           ((rs2 & 0x0F) << 14);
}

// Realistic instruction definitions for tests
const uint32_t INST_ADD = encode_r_type(cpu::Opcode::ADD, 1, 2, 3); // ADD r1, r2, r3
const uint32_t INST_SUB = encode_r_type(cpu::Opcode::SUB, 4, 5, 6); // SUB r4, r5, r6
const uint32_t INST_AND = encode_r_type(cpu::Opcode::AND, 7, 1, 2); // AND r7, r1, r2
const uint32_t INST_OR  = encode_r_type(cpu::Opcode::OR,  8, 3, 4); // OR  r8, r3, r4
const uint32_t INST_XOR = encode_r_type(cpu::Opcode::XOR, 9, 5, 6); // XOR r9, r5, r6
const uint32_t INST_LW  = encode_r_type(cpu::Opcode::LW,  10, 1, 0);// LW  r10, 0(r1)
const uint32_t INST_SW  = encode_r_type(cpu::Opcode::SW,  11, 2, 0);// SW  r11, 0(r2)

struct TestHarness {
    logic::Wire clock{logic::LogicState::LOW};
    logic::Wire reset{logic::LogicState::LOW};
    logic::Wire enable{logic::LogicState::HIGH};

    // Lane 0 inputs
    logic::Bus<32> fetch_pc0;
    logic::Bus<32> fetch_instruction0;
    logic::Wire fetch_valid0{logic::LogicState::LOW};

    // Lane 1 inputs
    logic::Bus<32> fetch_pc1;
    logic::Bus<32> fetch_instruction1;
    logic::Wire fetch_valid1{logic::LogicState::LOW};

    // Lane 0 outputs
    logic::Bus<32> decode_pc0;
    logic::Bus<32> decode_instruction0;
    logic::Wire decode_valid0{logic::LogicState::LOW};

    // Lane 1 outputs
    logic::Bus<32> decode_pc1;
    logic::Bus<32> decode_instruction1;
    logic::Wire decode_valid1{logic::LogicState::LOW};

    FetchDecodeReg32 reg{
        clock,
        reset,
        enable,
        fetch_pc0,
        fetch_instruction0,
        fetch_valid0,
        fetch_pc1,
        fetch_instruction1,
        fetch_valid1,
        decode_pc0,
        decode_instruction0,
        decode_valid0,
        decode_pc1,
        decode_instruction1,
        decode_valid1
    };

    void clock_edge()
    {
        // Falling edge: master latches capture inputs through MUXes
        clock.write(logic::LogicState::LOW);
        reg.evaluate();

        // Rising edge: slave latches transfer state to outputs
        clock.write(logic::LogicState::HIGH);
        reg.evaluate();
    }

    void reset_pipeline()
    {
        reset.write(logic::LogicState::HIGH);
        clock_edge();
        reset.write(logic::LogicState::LOW);
        clock.write(logic::LogicState::LOW);
        reg.evaluate();
    }
};

// 1. Basic capture — both lanes
void captures_both_lanes()
{
    std::cout << "[Test 1] captures_both_lanes...\n";

    TestHarness h;
    h.reset_pipeline();

    // Lane 0: PC = 0x100, Instruction = ADD, Valid = 1
    h.fetch_pc0.write_value(0x100);
    h.fetch_instruction0.write_value(INST_ADD);
    h.fetch_valid0.write(logic::LogicState::HIGH);

    // Lane 1: PC = 0x104, Instruction = SUB, Valid = 1
    h.fetch_pc1.write_value(0x104);
    h.fetch_instruction1.write_value(INST_SUB);
    h.fetch_valid1.write(logic::LogicState::HIGH);

    // Clock
    h.clock_edge();

    // Verify all 6 pieces of pipeline state
    assert(h.decode_pc0.read_value() == 0x100);
    assert(h.decode_instruction0.read_value() == INST_ADD);
    assert(h.decode_valid0.read() == logic::LogicState::HIGH);

    assert(h.decode_pc1.read_value() == 0x104);
    assert(h.decode_instruction1.read_value() == INST_SUB);
    assert(h.decode_valid1.read() == logic::LogicState::HIGH);

    // Also verify bundle accessor
    cpu::FetchBundle bundle = h.reg.get_bundle();
    assert(bundle.getPC0() == 0x100);
    assert(bundle.getInstruction0() == INST_ADD);
    assert(bundle.isValid0() == true);
    assert(bundle.getPC1() == 0x104);
    assert(bundle.getInstruction1() == INST_SUB);
    assert(bundle.isValid1() == true);

    std::cout << "  [PASS] All 6 pipeline state pieces correctly captured.\n";
}

// 2. Verify clocked behavior
void captures_only_on_clock()
{
    std::cout << "[Test 2] captures_only_on_clock...\n";

    TestHarness h;
    h.reset_pipeline();

    // Initial state is 0 after reset
    assert(h.decode_pc0.read_value() == 0);
    assert(h.decode_instruction0.read_value() == 0);
    assert(h.decode_valid0.read() == logic::LogicState::LOW);

    // Set new inputs while clock is LOW
    h.clock.write(logic::LogicState::LOW);
    h.fetch_pc0.write_value(0x100);
    h.fetch_instruction0.write_value(INST_ADD);
    h.fetch_valid0.write(logic::LogicState::HIGH);

    // Evaluate without a clock edge
    h.reg.evaluate();

    // Before clock: Decode MUST remain old value (0)
    assert(h.decode_pc0.read_value() == 0);
    assert(h.decode_instruction0.read_value() == 0);
    assert(h.decode_valid0.read() == logic::LogicState::LOW);

    // Apply clock edge (Clock ^)
    h.clock.write(logic::LogicState::HIGH);
    h.reg.evaluate();

    // After clock: Decode becomes new value
    assert(h.decode_pc0.read_value() == 0x100);
    assert(h.decode_instruction0.read_value() == INST_ADD);
    assert(h.decode_valid0.read() == logic::LogicState::HIGH);

    // Change input again while clock is HIGH
    h.fetch_pc0.write_value(0x200);
    h.fetch_instruction0.write_value(INST_SUB);
    h.reg.evaluate();

    // Decode output must NOT change without a new rising clock edge
    assert(h.decode_pc0.read_value() == 0x100);
    assert(h.decode_instruction0.read_value() == INST_ADD);

    // Complete clock transition to LOW (no rising edge yet)
    h.clock.write(logic::LogicState::LOW);
    h.reg.evaluate();
    assert(h.decode_pc0.read_value() == 0x100);
    assert(h.decode_instruction0.read_value() == INST_ADD);

    // Now rising edge occurs
    h.clock.write(logic::LogicState::HIGH);
    h.reg.evaluate();
    assert(h.decode_pc0.read_value() == 0x200);
    assert(h.decode_instruction0.read_value() == INST_SUB);

    std::cout << "  [PASS] Component behaves strictly as an edge-triggered pipeline register.\n";
}

// 3. Verify both lanes independently
void lanes_are_independent()
{
    std::cout << "[Test 3] lanes_are_independent...\n";

    TestHarness h;
    h.reset_pipeline();

    // Initial setup: Lane 0 = (0x100, ADD), Lane 1 = (0x104, SUB)
    h.fetch_pc0.write_value(0x100);
    h.fetch_instruction0.write_value(INST_ADD);
    h.fetch_valid0.write(logic::LogicState::HIGH);

    h.fetch_pc1.write_value(0x104);
    h.fetch_instruction1.write_value(INST_SUB);
    h.fetch_valid1.write(logic::LogicState::HIGH);

    h.clock_edge();

    assert(h.decode_pc0.read_value() == 0x100);
    assert(h.decode_instruction0.read_value() == INST_ADD);
    assert(h.decode_pc1.read_value() == 0x104);
    assert(h.decode_instruction1.read_value() == INST_SUB);

    // Change ONLY Lane 0:
    // Lane 0: PC = 0x200, INST = AND
    // Lane 1: PC = 0x104, INST = SUB (unchanged)
    h.fetch_pc0.write_value(0x200);
    h.fetch_instruction0.write_value(INST_AND);

    h.clock_edge();

    // Expected: Lane 0 -> changed, Lane 1 -> unchanged
    assert(h.decode_pc0.read_value() == 0x200);
    assert(h.decode_instruction0.read_value() == INST_AND);
    assert(h.decode_pc1.read_value() == 0x104);
    assert(h.decode_instruction1.read_value() == INST_SUB);

    // Now change ONLY Lane 1:
    // Lane 0: PC = 0x200, INST = AND (unchanged)
    // Lane 1: PC = 0x304, INST = OR (changed)
    h.fetch_pc1.write_value(0x304);
    h.fetch_instruction1.write_value(INST_OR);

    h.clock_edge();

    // Expected: Lane 0 -> unchanged, Lane 1 -> changed
    assert(h.decode_pc0.read_value() == 0x200);
    assert(h.decode_instruction0.read_value() == INST_AND);
    assert(h.decode_pc1.read_value() == 0x304);
    assert(h.decode_instruction1.read_value() == INST_OR);

    std::cout << "  [PASS] Independent lane operation confirmed without coupling.\n";
}

// 4. Test valid: valid_lane0_only
void valid_lane0_only()
{
    std::cout << "[Test 4a] valid_lane0_only (valid0=1, valid1=0)...\n";

    TestHarness h;
    h.reset_pipeline();

    // Lane 0: valid = 1
    h.fetch_pc0.write_value(0x100);
    h.fetch_instruction0.write_value(INST_ADD);
    h.fetch_valid0.write(logic::LogicState::HIGH);

    // Lane 1: valid = 0 (bubble/invalid)
    h.fetch_pc1.write_value(0x104);
    h.fetch_instruction1.write_value(INST_SUB);
    h.fetch_valid1.write(logic::LogicState::LOW);

    h.clock_edge();

    assert(h.decode_valid0.read() == logic::LogicState::HIGH);
    assert(h.decode_valid1.read() == logic::LogicState::LOW);
    assert(h.decode_pc0.read_value() == 0x100);
    assert(h.decode_instruction0.read_value() == INST_ADD);

    std::cout << "  [PASS] Lane 0 valid, Lane 1 invalid verified.\n";
}

// 4. Test valid: valid_lane1_only
void valid_lane1_only()
{
    std::cout << "[Test 4b] valid_lane1_only (valid0=0, valid1=1)...\n";

    TestHarness h;
    h.reset_pipeline();

    // Lane 0: valid = 0
    h.fetch_pc0.write_value(0x100);
    h.fetch_instruction0.write_value(INST_ADD);
    h.fetch_valid0.write(logic::LogicState::LOW);

    // Lane 1: valid = 1
    h.fetch_pc1.write_value(0x104);
    h.fetch_instruction1.write_value(INST_SUB);
    h.fetch_valid1.write(logic::LogicState::HIGH);

    h.clock_edge();

    assert(h.decode_valid0.read() == logic::LogicState::LOW);
    assert(h.decode_valid1.read() == logic::LogicState::HIGH);
    assert(h.decode_pc1.read_value() == 0x104);
    assert(h.decode_instruction1.read_value() == INST_SUB);

    std::cout << "  [PASS] Lane 0 invalid, Lane 1 valid verified.\n";
}

// 4. All four valid combinations: (0,0), (0,1), (1,0), (1,1)
void valid_combinations()
{
    std::cout << "[Test 4c] valid_combinations (all 4 combinations)...\n";

    TestHarness h;
    h.reset_pipeline();

    const bool combos[4][2] = {
        {false, false},
        {false, true},
        {true,  false},
        {true,  true}
    };

    for (int i = 0; i < 4; ++i)
    {
        bool v0 = combos[i][0];
        bool v1 = combos[i][1];

        h.fetch_valid0.write(v0 ? logic::LogicState::HIGH : logic::LogicState::LOW);
        h.fetch_valid1.write(v1 ? logic::LogicState::HIGH : logic::LogicState::LOW);
        h.fetch_pc0.write_value(0x100 + i * 8);
        h.fetch_pc1.write_value(0x104 + i * 8);

        h.clock_edge();

        assert(h.decode_valid0.read() == (v0 ? logic::LogicState::HIGH : logic::LogicState::LOW));
        assert(h.decode_valid1.read() == (v1 ? logic::LogicState::HIGH : logic::LogicState::LOW));
    }

    std::cout << "  [PASS] All 4 valid combinations successfully verified.\n";
}

// 5. Bubble propagation
void propagates_bubble()
{
    std::cout << "[Test 5] propagates_bubble...\n";

    TestHarness h;
    h.reset_pipeline();

    // Lane 0: PC = 0x100, INST = ADD, valid = 1
    h.fetch_pc0.write_value(0x100);
    h.fetch_instruction0.write_value(INST_ADD);
    h.fetch_valid0.write(logic::LogicState::HIGH);

    // Lane 1: PC = 0x104, INST = 0, valid = 0 (bubble)
    h.fetch_pc1.write_value(0x104);
    h.fetch_instruction1.write_value(0);
    h.fetch_valid1.write(logic::LogicState::LOW);

    h.clock_edge();

    // After clock:
    // Decode lane 0: valid = 1
    // Decode lane 1: valid = 0
    assert(h.decode_valid0.read() == logic::LogicState::HIGH);
    assert(h.decode_valid1.read() == logic::LogicState::LOW);
    assert(h.decode_pc0.read_value() == 0x100);
    assert(h.decode_instruction0.read_value() == INST_ADD);

    std::cout << "  [PASS] Bubble correctly propagated (valid=0).\n";
}

// 6. Reset test
void reset_clears_pipeline()
{
    std::cout << "[Test 6] reset_clears_pipeline...\n";

    TestHarness h;

    // Load non-zero state first:
    // Lane 0: PC = 0x100, INST = ADD, valid = 1
    // Lane 1: PC = 0x104, INST = SUB, valid = 1
    h.fetch_pc0.write_value(0x100);
    h.fetch_instruction0.write_value(INST_ADD);
    h.fetch_valid0.write(logic::LogicState::HIGH);

    h.fetch_pc1.write_value(0x104);
    h.fetch_instruction1.write_value(INST_SUB);
    h.fetch_valid1.write(logic::LogicState::HIGH);

    h.clock_edge();

    // Verify non-zero state is present
    assert(h.decode_valid0.read() == logic::LogicState::HIGH);
    assert(h.decode_valid1.read() == logic::LogicState::HIGH);
    assert(h.decode_pc0.read_value() == 0x100);
    assert(h.decode_pc1.read_value() == 0x104);
    assert(h.decode_instruction0.read_value() == INST_ADD);
    assert(h.decode_instruction1.read_value() == INST_SUB);

    // Assert reset and clock
    h.reset.write(logic::LogicState::HIGH);
    h.clock_edge();

    // Expected:
    // decode_valid0 = 0, decode_valid1 = 0
    // decode_pc0 = 0, decode_instruction0 = 0
    // decode_pc1 = 0, decode_instruction1 = 0
    assert(h.decode_valid0.read() == logic::LogicState::LOW);
    assert(h.decode_valid1.read() == logic::LogicState::LOW);
    assert(h.decode_pc0.read_value() == 0);
    assert(h.decode_instruction0.read_value() == 0);
    assert(h.decode_pc1.read_value() == 0);
    assert(h.decode_instruction1.read_value() == 0);

    // Deassert reset
    h.reset.write(logic::LogicState::LOW);

    // Clock in new instruction to verify pipeline resumes cleanly
    h.fetch_pc0.write_value(0x500);
    h.fetch_instruction0.write_value(INST_XOR);
    h.fetch_valid0.write(logic::LogicState::HIGH);
    h.fetch_valid1.write(logic::LogicState::LOW);

    h.clock_edge();

    assert(h.decode_valid0.read() == logic::LogicState::HIGH);
    assert(h.decode_valid1.read() == logic::LogicState::LOW);
    assert(h.decode_pc0.read_value() == 0x500);
    assert(h.decode_instruction0.read_value() == INST_XOR);

    std::cout << "  [PASS] Reset cleared pipeline state to zero and resumed cleanly.\n";
}

// 7. Back-to-back pipeline test
void supports_back_to_back_bundles()
{
    std::cout << "[Test 7] supports_back_to_back_bundles...\n";

    TestHarness h;
    h.reset_pipeline();

    // Define stream of bundles:
    // Cycle 0: Fetch ADD / SUB -> Decode initially 0
    // Cycle 1: Fetch AND / OR  -> Decode ADD / SUB
    // Cycle 2: Fetch XOR / ADD -> Decode AND / OR
    // Cycle 3: Fetch LW  / SW  -> Decode XOR / ADD
    struct BundleSpec {
        uint32_t pc0;
        uint32_t inst0;
        uint32_t pc1;
        uint32_t inst1;
    };

    const BundleSpec stream[] = {
        {0x00, INST_ADD, 0x04, INST_SUB},
        {0x08, INST_AND, 0x0C, INST_OR },
        {0x10, INST_XOR, 0x14, INST_ADD},
        {0x18, INST_LW,  0x1C, INST_SW }
    };

    for (std::size_t cycle = 0; cycle < 4; ++cycle)
    {
        // Present new fetch inputs for current cycle
        h.fetch_pc0.write_value(stream[cycle].pc0);
        h.fetch_instruction0.write_value(stream[cycle].inst0);
        h.fetch_valid0.write(logic::LogicState::HIGH);

        h.fetch_pc1.write_value(stream[cycle].pc1);
        h.fetch_instruction1.write_value(stream[cycle].inst1);
        h.fetch_valid1.write(logic::LogicState::HIGH);

        // Before clock edge: verify Decode holds previous cycle's bundle
        if (cycle == 0)
        {
            assert(h.decode_valid0.read() == logic::LogicState::LOW);
            assert(h.decode_valid1.read() == logic::LogicState::LOW);
        }
        else
        {
            assert(h.decode_pc0.read_value() == stream[cycle - 1].pc0);
            assert(h.decode_instruction0.read_value() == stream[cycle - 1].inst0);
            assert(h.decode_pc1.read_value() == stream[cycle - 1].pc1);
            assert(h.decode_instruction1.read_value() == stream[cycle - 1].inst1);
            assert(h.decode_valid0.read() == logic::LogicState::HIGH);
            assert(h.decode_valid1.read() == logic::LogicState::HIGH);
        }

        // Clock edge advances pipeline
        h.clock_edge();

        // After clock edge: verify Decode holds this cycle's fetched bundle
        assert(h.decode_pc0.read_value() == stream[cycle].pc0);
        assert(h.decode_instruction0.read_value() == stream[cycle].inst0);
        assert(h.decode_pc1.read_value() == stream[cycle].pc1);
        assert(h.decode_instruction1.read_value() == stream[cycle].inst1);
        assert(h.decode_valid0.read() == logic::LogicState::HIGH);
        assert(h.decode_valid1.read() == logic::LogicState::HIGH);

        std::cout << "  Cycle " << cycle << ": Latched PC0=0x" << std::hex
                  << stream[cycle].pc0 << ", PC1=0x" << stream[cycle].pc1 << std::dec << "\n";
    }

    std::cout << "  [PASS] 1-cycle pipeline delay across 4 back-to-back bundles verified.\n";
}

// 8. Test the two lanes with realistic instructions
void test_realistic_instructions()
{
    std::cout << "[Test 8] test_realistic_instructions...\n";

    TestHarness h;
    h.reset_pipeline();

    // Cycle 0:
    // Lane 0: PC = 0x00, instruction = ADD
    // Lane 1: PC = 0x04, instruction = SUB
    h.fetch_pc0.write_value(0x00);
    h.fetch_instruction0.write_value(INST_ADD);
    h.fetch_valid0.write(logic::LogicState::HIGH);

    h.fetch_pc1.write_value(0x04);
    h.fetch_instruction1.write_value(INST_SUB);
    h.fetch_valid1.write(logic::LogicState::HIGH);

    h.clock_edge();

    // Verify architectural instruction decoding on Decode side
    cpu::Instruction dec_inst0{static_cast<uint32_t>(h.decode_instruction0.read_value())};
    cpu::Instruction dec_inst1{static_cast<uint32_t>(h.decode_instruction1.read_value())};

    assert(dec_inst0.opcode() == cpu::Opcode::ADD);
    assert(dec_inst0.rd() == cpu::Register::R1);
    assert(dec_inst0.rs1() == cpu::Register::R2);
    assert(dec_inst0.rs2() == cpu::Register::R3);

    assert(dec_inst1.opcode() == cpu::Opcode::SUB);
    assert(dec_inst1.rd() == cpu::Register::R4);
    assert(dec_inst1.rs1() == cpu::Register::R5);
    assert(dec_inst1.rs2() == cpu::Register::R6);

    // Cycle 1:
    // Lane 0: PC = 0x08, instruction = LW
    // Lane 1: PC = 0x0C, instruction = SW
    h.fetch_pc0.write_value(0x08);
    h.fetch_instruction0.write_value(INST_LW);
    h.fetch_valid0.write(logic::LogicState::HIGH);

    h.fetch_pc1.write_value(0x0C);
    h.fetch_instruction1.write_value(INST_SW);
    h.fetch_valid1.write(logic::LogicState::HIGH);

    h.clock_edge();

    cpu::Instruction dec_inst2{static_cast<uint32_t>(h.decode_instruction0.read_value())};
    cpu::Instruction dec_inst3{static_cast<uint32_t>(h.decode_instruction1.read_value())};

    assert(dec_inst2.opcode() == cpu::Opcode::LW);
    assert(dec_inst2.rd() == cpu::Register::R10);
    assert(dec_inst2.rs1() == cpu::Register::R1);

    assert(dec_inst3.opcode() == cpu::Opcode::SW);
    assert(dec_inst3.rd() == cpu::Register::R11);
    assert(dec_inst3.rs1() == cpu::Register::R2);

    std::cout << "  [PASS] Realistic instruction decoding from pipeline register verified.\n";
}

// 9. Pipeline Stall Mechanism via Enable MUX Feedback
void test_stall_enable_mux_feedback()
{
    std::cout << "[Test 9] test_stall_enable_mux_feedback...\n";

    TestHarness h;
    h.reset_pipeline();

    // Cycle 0: Capture bundle 1 with enable = 1
    h.enable.write(logic::LogicState::HIGH);
    h.fetch_pc0.write_value(0x100);
    h.fetch_instruction0.write_value(INST_ADD);
    h.fetch_valid0.write(logic::LogicState::HIGH);

    h.fetch_pc1.write_value(0x104);
    h.fetch_instruction1.write_value(INST_SUB);
    h.fetch_valid1.write(logic::LogicState::HIGH);

    h.clock_edge();

    assert(h.decode_pc0.read_value() == 0x100);
    assert(h.decode_pc1.read_value() == 0x104);

    // Present bundle 2, but ASSERT STALL (enable = LOW)
    h.enable.write(logic::LogicState::LOW);
    h.fetch_pc0.write_value(0x200);
    h.fetch_instruction0.write_value(INST_AND);
    h.fetch_pc1.write_value(0x204);
    h.fetch_instruction1.write_value(INST_OR);

    // Clock edge 1 during stall: output MUST retain bundle 1 via MUX feedback
    h.clock_edge();
    assert(h.decode_pc0.read_value() == 0x100);
    assert(h.decode_instruction0.read_value() == INST_ADD);
    assert(h.decode_pc1.read_value() == 0x104);
    assert(h.decode_instruction1.read_value() == INST_SUB);

    // Clock edge 2 during stall: still retains bundle 1
    h.clock_edge();
    assert(h.decode_pc0.read_value() == 0x100);
    assert(h.decode_pc1.read_value() == 0x104);

    // Deassert stall (enable = HIGH) -> now captures bundle 2
    h.enable.write(logic::LogicState::HIGH);
    h.clock_edge();
    assert(h.decode_pc0.read_value() == 0x200);
    assert(h.decode_instruction0.read_value() == INST_AND);
    assert(h.decode_pc1.read_value() == 0x204);
    assert(h.decode_instruction1.read_value() == INST_OR);

    std::cout << "  [PASS] Enable MUX feedback successfully stalled and resumed pipeline.\n";
}

} // namespace

int main()
{
    std::cout << "=========================================================\n";
    std::cout << "--- Testing Superscalar Fetch/Decode Pipeline Register ---\n";
    std::cout << "=========================================================\n";

    captures_both_lanes();
    captures_only_on_clock();
    lanes_are_independent();
    valid_lane0_only();
    valid_lane1_only();
    valid_combinations();
    propagates_bubble();
    reset_clears_pipeline();
    supports_back_to_back_bundles();
    test_realistic_instructions();
    test_stall_enable_mux_feedback();

    std::cout << "\n[PASS] All Fetch/Decode Register tests passed successfully!\n";
    return 0;
}

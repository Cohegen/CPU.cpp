#include "../../superscalar/core/rename/RenameUnit.hpp"
#include "../../superscalar/core/rename/RegisterAliasTable.hpp"
#include "../../superscalar/core/rename/PhysicalRegisterFreeList.hpp"
#include "../../superscalar/core/rename/RenameBundle.hpp"
#include "../../superscalar/core/decode/DecodeBundle.hpp"
#include "../../superscalar/core/decode/DecodeTypes.hpp"
#include "../../include/isa/Opcode.hpp"
#include "../../include/isa/Registers.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>

namespace {

// Helper to construct a synthetic DecodeBundle for testing
cpu::DecodeBundle make_decode_bundle(
    bool valid,
    cpu::Opcode op,
    cpu::Register rd,
    cpu::Register rs1,
    cpu::Register rs2,
    bool register_write,
    std::uint32_t pc = 0x1000,
    int32_t imm = 0
)
{
    cpu::DecodeBundle b{};
    b.valid = valid;
    b.pc = pc;
    b.instruction = 0x12345678;
    b.opcode = op;
    b.rd = rd;
    b.rs1 = rs1;
    b.rs2 = rs2;
    b.immediate = imm;
    b.register_write = register_write;
    b.alu_operation = cpu::ALUOperation::ADD;
    b.operand_a = cpu::OperandSource::REGISTER;
    b.operand_b = cpu::OperandSource::REGISTER;
    return b;
}

// ---------------------------------------------------------------------------
// 1. Source registers are translated through RAT
// ---------------------------------------------------------------------------
void test_source_registers_translated_through_rat()
{
    cpu::RegisterAliasTable rat(64);
    cpu::PhysicalRegisterFreeList free_list(64, 16);
    cpu::RenameUnit unit(rat, free_list);

    // Initial RAT mappings: R0->0, R1->1, R2->2, ...
    auto b1 = make_decode_bundle(true, cpu::Opcode::ADD, cpu::Register::R3, cpu::Register::R1, cpu::Register::R2, true);
    cpu::RenameBundle out1 = unit.rename(b1);

    assert(out1.valid);
    assert(out1.physical_rs1 == 1);
    assert(out1.physical_rs2 == 2);

    // Modify RAT mappings explicitly and verify translation
    rat.set(cpu::Register::R4, 40);
    rat.set(cpu::Register::R5, 55);

    auto b2 = make_decode_bundle(true, cpu::Opcode::SUB, cpu::Register::R6, cpu::Register::R4, cpu::Register::R5, true);
    cpu::RenameBundle out2 = unit.rename(b2);

    assert(out2.valid);
    assert(out2.physical_rs1 == 40);
    assert(out2.physical_rs2 == 55);

    std::cout << "  [PASS] Requirement 1: Source registers are translated through RAT.\n";
}

// ---------------------------------------------------------------------------
// 2. Destination gets a new physical register
// ---------------------------------------------------------------------------
void test_destination_gets_new_physical_register()
{
    cpu::RegisterAliasTable rat(64);
    cpu::PhysicalRegisterFreeList free_list(64, 16);
    cpu::RenameUnit unit(rat, free_list);

    std::size_t initial_free_count = free_list.size();
    assert(initial_free_count == 48); // 64 total - 16 initially mapped

    // Destination R3 should receive the first available physical register (16)
    auto b = make_decode_bundle(true, cpu::Opcode::ADD, cpu::Register::R3, cpu::Register::R1, cpu::Register::R2, true);
    cpu::RenameBundle out = unit.rename(b);

    assert(out.valid);
    assert(out.physical_rd == 16);
    assert(free_list.size() == initial_free_count - 1);

    // Next instruction should get 17
    auto b2 = make_decode_bundle(true, cpu::Opcode::ADD, cpu::Register::R4, cpu::Register::R1, cpu::Register::R2, true);
    cpu::RenameBundle out2 = unit.rename(b2);

    assert(out2.valid);
    assert(out2.physical_rd == 17);
    assert(free_list.size() == initial_free_count - 2);

    std::cout << "  [PASS] Requirement 2: Destination gets a new physical register.\n";
}

// ---------------------------------------------------------------------------
// 3. RAT is updated
// ---------------------------------------------------------------------------
void test_rat_is_updated()
{
    cpu::RegisterAliasTable rat(64);
    cpu::PhysicalRegisterFreeList free_list(64, 16);
    cpu::RenameUnit unit(rat, free_list);

    assert(rat.lookup(cpu::Register::R7) == 7);

    auto b = make_decode_bundle(true, cpu::Opcode::ADD, cpu::Register::R7, cpu::Register::R1, cpu::Register::R2, true);
    cpu::RenameBundle out = unit.rename(b);

    assert(out.valid);
    assert(out.physical_rd == 16);
    // RAT must be updated for R7 to point to physical register 16
    assert(rat.lookup(cpu::Register::R7) == 16);

    // Other entries in RAT should remain unchanged
    assert(rat.lookup(cpu::Register::R0) == 0);
    assert(rat.lookup(cpu::Register::R1) == 1);
    assert(rat.lookup(cpu::Register::R2) == 2);
    assert(rat.lookup(cpu::Register::R6) == 6);

    std::cout << "  [PASS] Requirement 3: RAT is updated.\n";
}

// ---------------------------------------------------------------------------
// 4. Two instructions can rename in one cycle
// ---------------------------------------------------------------------------
void test_two_wide_rename_in_one_cycle()
{
    cpu::RegisterAliasTable rat(64);
    cpu::PhysicalRegisterFreeList free_list(64, 16);
    cpu::RenameUnit unit(rat, free_list);

    auto in0 = make_decode_bundle(true, cpu::Opcode::ADD, cpu::Register::R3, cpu::Register::R1, cpu::Register::R2, true, 0x1000);
    auto in1 = make_decode_bundle(true, cpu::Opcode::SUB, cpu::Register::R6, cpu::Register::R4, cpu::Register::R5, true, 0x1004);

    cpu::RenameBundle out0;
    cpu::RenameBundle out1;

    unit.rename(in0, in1, out0, out1);

    assert(out0.valid);
    assert(out1.valid);

    // Both instructions renamed in one cycle
    assert(out0.pc == 0x1000);
    assert(out1.pc == 0x1004);

    assert(out0.physical_rs1 == 1);
    assert(out0.physical_rs2 == 2);
    assert(out0.physical_rd == 16);

    assert(out1.physical_rs1 == 4);
    assert(out1.physical_rs2 == 5);
    assert(out1.physical_rd == 17);

    assert(rat.lookup(cpu::Register::R3) == 16);
    assert(rat.lookup(cpu::Register::R6) == 17);

    std::cout << "  [PASS] Requirement 4: Two instructions can rename in one cycle.\n";
}

// ---------------------------------------------------------------------------
// 5. Lane 1 sees lane 0's RAT update
// ---------------------------------------------------------------------------
void test_lane1_sees_lane0_rat_update()
{
    cpu::RegisterAliasTable rat(64);
    cpu::PhysicalRegisterFreeList free_list(64, 16);
    cpu::RenameUnit unit(rat, free_list);

    // Case 5a: RAW dependency where Lane 1's rs1 is Lane 0's rd
    // Lane 0: ADD R1, R2, R3 (writes R1)
    // Lane 1: SUB R4, R1, R5 (reads R1, writes R4)
    auto in0 = make_decode_bundle(true, cpu::Opcode::ADD, cpu::Register::R1, cpu::Register::R2, cpu::Register::R3, true);
    auto in1 = make_decode_bundle(true, cpu::Opcode::SUB, cpu::Register::R4, cpu::Register::R1, cpu::Register::R5, true);

    cpu::RenameBundle out0;
    cpu::RenameBundle out1;

    unit.rename(in0, in1, out0, out1);

    assert(out0.valid);
    assert(out1.valid);
    assert(out0.physical_rd == 16);
    // Lane 1 must observe Lane 0's RAT update: rs1 translated to 16
    assert(out1.physical_rs1 == 16);
    assert(out1.physical_rs2 == 5);
    assert(out1.physical_rd == 17);

    // Case 5b: RAW dependency where Lane 1's rs2 is Lane 0's rd
    // Lane 0: ADD R8, R0, R0 (writes R8 -> gets 18)
    // Lane 1: ADD R9, R10, R8 (reads R8 as rs2)
    auto in0_b = make_decode_bundle(true, cpu::Opcode::ADD, cpu::Register::R8, cpu::Register::R0, cpu::Register::R0, true);
    auto in1_b = make_decode_bundle(true, cpu::Opcode::ADD, cpu::Register::R9, cpu::Register::R10, cpu::Register::R8, true);

    unit.rename(in0_b, in1_b, out0, out1);

    assert(out0.physical_rd == 18);
    assert(out1.physical_rs2 == 18);
    assert(out1.physical_rd == 19);

    // Case 5c: WAW dependency: both lane 0 and lane 1 write to the same destination register
    // Lane 0: ADD R1, R0, R0 (writes R1 -> gets 20)
    // Lane 1: ADDI R1, R1, 5 (reads R1 as rs1 -> gets 20, writes R1 -> gets 21)
    auto in0_c = make_decode_bundle(true, cpu::Opcode::ADD, cpu::Register::R1, cpu::Register::R0, cpu::Register::R0, true);
    auto in1_c = make_decode_bundle(true, cpu::Opcode::ADDI, cpu::Register::R1, cpu::Register::R1, cpu::Register::R0, true);

    unit.rename(in0_c, in1_c, out0, out1);

    assert(out0.physical_rd == 20);
    assert(out1.physical_rs1 == 20); // Lane 1 sees Lane 0's update for its source
    assert(out1.physical_rd == 21);  // Lane 1 gets its own new physical register
    assert(rat.lookup(cpu::Register::R1) == 21); // Final RAT mapping is Lane 1's destination

    std::cout << "  [PASS] Requirement 5: Lane 1 sees lane 0's RAT update.\n";
}

// ---------------------------------------------------------------------------
// 6. Physical-register exhaustion stalls/invalidates rename
// ---------------------------------------------------------------------------
void test_physical_register_exhaustion()
{
    // Empty free list (0 free registers)
    cpu::RegisterAliasTable rat(64);
    cpu::PhysicalRegisterFreeList free_list(16, 16); // 16 total, 16 mapped -> 0 free
    assert(free_list.empty());

    cpu::RenameUnit unit(rat, free_list);

    // Single rename with exhausted free list
    auto in = make_decode_bundle(true, cpu::Opcode::ADD, cpu::Register::R3, cpu::Register::R1, cpu::Register::R2, true);
    cpu::RenameBundle out = unit.rename(in);

    assert(!out.valid); // Invalidated
    assert(out.physical_rd == cpu::PhysicalRegisterFreeList::INVALID_REGISTER);
    assert(unit.stalled());
    assert(rat.lookup(cpu::Register::R3) == 3); // RAT untouched

    // 2-wide rename when free list has 0 registers
    auto in0 = make_decode_bundle(true, cpu::Opcode::ADD, cpu::Register::R3, cpu::Register::R1, cpu::Register::R2, true);
    auto in1 = make_decode_bundle(true, cpu::Opcode::SUB, cpu::Register::R6, cpu::Register::R4, cpu::Register::R5, true);

    cpu::RenameBundle out0;
    cpu::RenameBundle out1;
    unit.rename(in0, in1, out0, out1);

    assert(!out0.valid); // Lane 0 stalls / invalidated
    assert(!out1.valid); // Lane 1 stalls / invalidated
    assert(unit.stalled());
    assert(rat.lookup(cpu::Register::R3) == 3);
    assert(rat.lookup(cpu::Register::R6) == 6);

    // 2-wide rename when free list has exactly 1 register left
    cpu::RegisterAliasTable rat2(64);
    cpu::PhysicalRegisterFreeList free_list2(17, 16); // Exactly 1 free register (16)
    assert(free_list2.size() == 1);

    cpu::RenameUnit unit2(rat2, free_list2);

    unit2.rename(in0, in1, out0, out1);
    assert(out0.valid); // Lane 0 succeeds with the 1 available register
    assert(out0.physical_rd == 16);
    assert(rat2.lookup(cpu::Register::R3) == 16);

    assert(!out1.valid); // Lane 1 exhausts and is stalled / invalidated
    assert(out1.physical_rd == cpu::PhysicalRegisterFreeList::INVALID_REGISTER);
    assert(unit2.stalled());
    assert(rat2.lookup(cpu::Register::R6) == 6); // Lane 1 did not update RAT

    std::cout << "  [PASS] Requirement 6: Physical-register exhaustion stalls/invalidates rename.\n";
}

// ---------------------------------------------------------------------------
// 7. Invalid DecodeBundle produces a bubble
// ---------------------------------------------------------------------------
void test_invalid_decode_bundle_produces_bubble()
{
    cpu::RegisterAliasTable rat(64);
    cpu::PhysicalRegisterFreeList free_list(64, 16);
    cpu::RenameUnit unit(rat, free_list);

    std::size_t initial_free = free_list.size();

    // Single instruction bubble
    auto invalid_bundle = make_decode_bundle(false, cpu::Opcode::ADD, cpu::Register::R3, cpu::Register::R1, cpu::Register::R2, true);
    cpu::RenameBundle out = unit.rename(invalid_bundle);

    assert(!out.valid); // Bubble
    assert(free_list.size() == initial_free); // No register allocated
    assert(rat.lookup(cpu::Register::R3) == 3); // RAT untouched

    // 2-wide: Lane 0 invalid (bubble), Lane 1 valid
    auto valid_bundle = make_decode_bundle(true, cpu::Opcode::ADD, cpu::Register::R4, cpu::Register::R1, cpu::Register::R2, true);
    cpu::RenameBundle out0, out1;

    unit.rename(invalid_bundle, valid_bundle, out0, out1);
    assert(!out0.valid); // Lane 0 is bubble
    assert(out1.valid);  // Lane 1 renames successfully
    assert(out1.physical_rd == 16);
    assert(rat.lookup(cpu::Register::R4) == 16);

    // 2-wide: Lane 0 valid, Lane 1 invalid (bubble)
    auto valid_bundle2 = make_decode_bundle(true, cpu::Opcode::SUB, cpu::Register::R5, cpu::Register::R1, cpu::Register::R2, true);
    unit.rename(valid_bundle2, invalid_bundle, out0, out1);
    assert(out0.valid);  // Lane 0 renames successfully
    assert(out0.physical_rd == 17);
    assert(!out1.valid); // Lane 1 is bubble

    // 2-wide: Both invalid
    unit.rename(invalid_bundle, invalid_bundle, out0, out1);
    assert(!out0.valid);
    assert(!out1.valid);

    std::cout << "  [PASS] Requirement 7: Invalid DecodeBundle produces a bubble.\n";
}

// ---------------------------------------------------------------------------
// 8. Instructions with register_write=false don't allocate a physical register
// ---------------------------------------------------------------------------
void test_register_write_false_does_not_allocate()
{
    cpu::RegisterAliasTable rat(64);
    cpu::PhysicalRegisterFreeList free_list(64, 16);
    cpu::RenameUnit unit(rat, free_list);

    std::size_t initial_free = free_list.size();

    // Store instruction (SW): reads rs1 and rs2, does NOT write to a register
    auto sw_bundle = make_decode_bundle(true, cpu::Opcode::SW, cpu::Register::R0, cpu::Register::R1, cpu::Register::R2, false);
    cpu::RenameBundle out_sw = unit.rename(sw_bundle);

    assert(out_sw.valid);
    assert(!out_sw.register_write);
    assert(out_sw.physical_rs1 == 1);
    assert(out_sw.physical_rs2 == 2);
    assert(free_list.size() == initial_free); // Free list untouched!
    assert(rat.lookup(cpu::Register::R0) == 0); // RAT untouched!

    // Branch instruction (BEQ): reads rs1 and rs2, does NOT write to a register
    auto beq_bundle = make_decode_bundle(true, cpu::Opcode::BEQ, cpu::Register::R0, cpu::Register::R3, cpu::Register::R4, false);
    cpu::RenameBundle out_beq = unit.rename(beq_bundle);

    assert(out_beq.valid);
    assert(!out_beq.register_write);
    assert(out_beq.physical_rs1 == 3);
    assert(out_beq.physical_rs2 == 4);
    assert(free_list.size() == initial_free);

    // Resilience test: when free list has 0 registers, register_write=false STILL succeeds!
    cpu::RegisterAliasTable rat_empty(64);
    cpu::PhysicalRegisterFreeList free_list_empty(16, 16); // 0 free
    assert(free_list_empty.empty());

    cpu::RenameUnit unit_empty(rat_empty, free_list_empty);

    cpu::RenameBundle out_sw_empty = unit_empty.rename(sw_bundle);
    assert(out_sw_empty.valid); // Does NOT stall or invalidate!
    assert(!unit_empty.stalled());

    // 2-wide test with register_write=false on both lanes with 0 free registers
    cpu::RenameBundle out0, out1;
    unit_empty.rename(sw_bundle, beq_bundle, out0, out1);
    assert(out0.valid);
    assert(out1.valid);
    assert(!unit_empty.stalled());

    std::cout << "  [PASS] Requirement 8: Instructions with register_write=false don't allocate a physical register.\n";
}

} // namespace

int main()
{
    std::cout << "=========================================================\n";
    std::cout << "--- Testing Superscalar Rename Unit (2-wide & RAT)    ---\n";
    std::cout << "=========================================================\n";

    test_source_registers_translated_through_rat();
    test_destination_gets_new_physical_register();
    test_rat_is_updated();
    test_two_wide_rename_in_one_cycle();
    test_lane1_sees_lane0_rat_update();
    test_physical_register_exhaustion();
    test_invalid_decode_bundle_produces_bubble();
    test_register_write_false_does_not_allocate();

    std::cout << "\n[PASS] All 8 Superscalar Rename Unit tests passed successfully!\n";
    return 0;
}

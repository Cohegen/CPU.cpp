#include "../../superscalar/core/rename/PhysicalRegisterFreeList.hpp"
#include "../../superscalar/core/rename/RegisterAliasTable.hpp"
#include "../../superscalar/core/rename/RenameUnit.hpp"
#include "../../superscalar/core/dispatch/ReOrderBuffer.hpp"
#include "../../superscalar/core/dispatch/ROBEntry.hpp"
#include "../../include/isa/Registers.hpp"
#include "../../include/isa/Opcode.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

// Macro mimicking Catch2 REQUIRE while using assert / exception semantics
#define REQUIRE(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "FAILED: " #expr << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::abort(); \
        } \
    } while (0)

namespace {

// Helper to construct a synthetic DecodeBundle for testing
cpu::DecodeBundle make_decode_bundle(
    bool valid,
    cpu::Opcode op,
    cpu::Register rd,
    cpu::Register rs1,
    cpu::Register rs2,
    bool register_write,
    cpu::ControlFlow control_flow = cpu::ControlFlow::NONE,
    std::uint32_t pc = 0x1000
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
    b.register_write = register_write;
    b.control_flow = control_flow;
    b.alu_operation = cpu::ALUOperation::ADD;
    b.operand_a = cpu::OperandSource::REGISTER;
    b.operand_b = cpu::OperandSource::REGISTER;
    return b;
}

// ---------------------------------------------------------------------------
// Test 1: One speculative register is recovered
// ---------------------------------------------------------------------------
void test_one_speculative_register_recovered()
{
    std::cout << "Test 1: One speculative register is recovered ... ";

    // Initial free list: P16 ... P31 (16 registers total)
    constexpr std::size_t total_phys = 32;
    constexpr std::size_t initial_mapped = 16;
    cpu::PhysicalRegisterFreeList free_list(total_phys, initial_mapped);

    const std::size_t initial_size = free_list.size();
    REQUIRE(initial_size == 16);

    // allocate -> P16
    const auto p16 = free_list.allocate();
    REQUIRE(p16 == 16);
    REQUIRE(free_list.size() == initial_size - 1);

    // release P16
    free_list.release(p16);
    REQUIRE(free_list.size() == initial_size);

    // Verify P16 is eventually reusable: allocate through all available
    // registers and verify P16 is among them.
    bool p16_found = false;
    for (std::size_t i = 0; i < initial_size; ++i)
    {
        auto reg = free_list.allocate();
        if (reg == p16)
        {
            p16_found = true;
        }
    }
    REQUIRE(p16_found);
    REQUIRE(free_list.empty());

    std::cout << "PASSED\n";
}

// ---------------------------------------------------------------------------
// Test 2: Multiple squashed destinations are recovered
// ---------------------------------------------------------------------------
void test_multiple_squashed_destinations_recovered()
{
    std::cout << "Test 2: Multiple squashed destinations are recovered ... ";

    constexpr std::size_t total_phys = 32;
    constexpr std::size_t initial_mapped = 16;
    cpu::PhysicalRegisterFreeList free_list(total_phys, initial_mapped);

    const std::size_t initial_size = free_list.size();
    REQUIRE(initial_size == 16);

    // Simulate three speculative instructions:
    // ADD R1 -> P16
    // SUB R2 -> P17
    // ADD R3 -> P18
    const auto p16 = free_list.allocate();
    const auto p17 = free_list.allocate();
    const auto p18 = free_list.allocate();

    REQUIRE(p16 == 16);
    REQUIRE(p17 == 17);
    REQUIRE(p18 == 18);
    REQUIRE(free_list.size() == initial_size - 3);

    // Release all three
    free_list.release(p16);
    free_list.release(p17);
    free_list.release(p18);

    REQUIRE(free_list.size() == initial_size);

    // Verify all three can eventually be allocated again
    bool found_p16 = false;
    bool found_p17 = false;
    bool found_p18 = false;

    for (std::size_t i = 0; i < initial_size; ++i)
    {
        auto reg = free_list.allocate();
        if (reg == p16) found_p16 = true;
        if (reg == p17) found_p17 = true;
        if (reg == p18) found_p18 = true;
    }

    REQUIRE(found_p16);
    REQUIRE(found_p17);
    REQUIRE(found_p18);
    REQUIRE(free_list.empty());

    std::cout << "PASSED\n";
}

// ---------------------------------------------------------------------------
// Test 3: old_physical_rd is NOT released
// ---------------------------------------------------------------------------
void test_old_physical_rd_not_released()
{
    std::cout << "Test 3: old_physical_rd is NOT released ... ";

    cpu::PhysicalRegisterFreeList free_list(32, 16);
    cpu::RegisterAliasTable rat(32);

    // Before branch: R1 -> P4
    rat.set(cpu::Register::R1, 4);
    REQUIRE(rat.lookup(cpu::Register::R1) == 4);

    // Speculative: ADD R1 -> P16
    cpu::RenameUnit<32> unit(rat, free_list);
    auto b_add = make_decode_bundle(true, cpu::Opcode::ADD, cpu::Register::R1, cpu::Register::R2, cpu::Register::R3, true);
    cpu::RenameBundle renamed_add = unit.rename(b_add);

    REQUIRE(renamed_add.valid);
    REQUIRE(renamed_add.physical_rd == 16);
    REQUIRE(renamed_add.old_physical_rd == 4);

    // ROB entry setup
    cpu::ROBEntry entry{};
    entry.valid = true;
    entry.register_write = renamed_add.register_write;
    entry.physical_rd = renamed_add.physical_rd;
    entry.old_physical_rd = renamed_add.old_physical_rd;

    // After squash: release(entry.physical_rd), do NOT release old_physical_rd (P4)
    // The test establishes that free-list count increases by exactly one, not two.
    const auto before_squash = free_list.size();

    // Squash action for younger entry:
    if (entry.register_write)
    {
        free_list.release(entry.physical_rd);
        // CRITICAL: entry.old_physical_rd is NOT released on squash!
    }

    const auto after_squash = free_list.size();
    REQUIRE(after_squash == before_squash + 1);
    REQUIRE(after_squash != before_squash + 2);

    std::cout << "PASSED\n";
}

// ---------------------------------------------------------------------------
// Test 4: Instruction without a destination register
// ---------------------------------------------------------------------------
void test_instruction_without_destination_register()
{
    std::cout << "Test 4: Instruction without a destination register ... ";

    cpu::PhysicalRegisterFreeList free_list(32, 16);

    // Branches, stores, etc. don't allocate a destination PR:
    // BEQ
    // physical_rd = INVALID_REGISTER
    // register_write = false
    cpu::ROBEntry entry{};
    entry.valid = true;
    entry.register_write = false;
    entry.physical_rd = cpu::PhysicalRegisterFreeList::INVALID_REGISTER;

    // Squashing this entry must not change the FreeList
    const auto before = free_list.size();

    if (entry.register_write)
    {
        free_list.release(entry.physical_rd);
    }

    REQUIRE(free_list.size() == before);

    // Also verify safety if free_list.release() is directly called with INVALID_REGISTER
    free_list.release(cpu::PhysicalRegisterFreeList::INVALID_REGISTER);
    REQUIRE(free_list.size() == before);

    std::cout << "PASSED\n";
}

// ---------------------------------------------------------------------------
// Test 5: Full branch-recovery scenario ⭐
// ---------------------------------------------------------------------------
void test_full_branch_recovery_scenario()
{
    std::cout << "Test 5: Full branch-recovery scenario ... ";

    cpu::RegisterAliasTable rat(64);
    cpu::PhysicalRegisterFreeList free_list(64, 16);
    cpu::ReOrderBuffer<16> rob;
    rob.reset();

    // Initial RAT: R1 -> P1, R2 -> P2, R3 -> P3 (matches default reset)
    REQUIRE(rat.lookup(cpu::Register::R1) == 1);
    REQUIRE(rat.lookup(cpu::Register::R2) == 2);
    REQUIRE(rat.lookup(cpu::Register::R3) == 3);

    // FreeList initially has 64 - 16 = 48 registers: P16, P17, P18, ...
    const auto initial_free_count = free_list.size();
    REQUIRE(initial_free_count == 48);

    // 1. Rename unit
    cpu::RenameUnit<64> unit(rat, free_list);

    // 2. Branch instruction encountered at PC 0x1000
    auto b_branch = make_decode_bundle(
        true, cpu::Opcode::BEQ, cpu::Register::R0, cpu::Register::R1, cpu::Register::R2,
        false, cpu::ControlFlow::BRANCH, 0x1000
    );
    cpu::RenameBundle branch_renamed = unit.rename(b_branch);

    // Take RAT checkpoint at branch
    rat.checkpoint();
    REQUIRE(rat.checkpoint_count() == 1);

    cpu::ReOrderBuffer<16>::Index branch_rob_idx = 0;
    REQUIRE(rob.allocate(branch_renamed, branch_rob_idx));

    // 3. Perform speculative renames:
    // ADD R1 -> P16
    // SUB R2 -> P17
    // ADD R3 -> P18
    auto b_add1 = make_decode_bundle(true, cpu::Opcode::ADD, cpu::Register::R1, cpu::Register::R1, cpu::Register::R0, true, cpu::ControlFlow::NONE, 0x1004);
    auto b_sub2 = make_decode_bundle(true, cpu::Opcode::SUB, cpu::Register::R2, cpu::Register::R2, cpu::Register::R0, true, cpu::ControlFlow::NONE, 0x1008);
    auto b_add3 = make_decode_bundle(true, cpu::Opcode::ADD, cpu::Register::R3, cpu::Register::R3, cpu::Register::R0, true, cpu::ControlFlow::NONE, 0x100C);

    cpu::RenameBundle r_add1 = unit.rename(b_add1);
    cpu::RenameBundle r_sub2 = unit.rename(b_sub2);
    cpu::RenameBundle r_add3 = unit.rename(b_add3);

    REQUIRE(r_add1.valid);
    REQUIRE(r_add1.physical_rd == 16);
    REQUIRE(r_sub2.valid);
    REQUIRE(r_sub2.physical_rd == 17);
    REQUIRE(r_add3.valid);
    REQUIRE(r_add3.physical_rd == 18);

    // FreeList now has 3 fewer registers
    REQUIRE(free_list.size() == initial_free_count - 3);

    // Speculative RAT state
    REQUIRE(rat.lookup(cpu::Register::R1) == 16);
    REQUIRE(rat.lookup(cpu::Register::R2) == 17);
    REQUIRE(rat.lookup(cpu::Register::R3) == 18);

    // Allocate into ROB:
    // [BRANCH]
    // [ADD  R1 -> P16]
    // [SUB  R2 -> P17]
    // [ADD  R3 -> P18]
    cpu::ReOrderBuffer<16>::Index idx1 = 0, idx2 = 0, idx3 = 0;
    REQUIRE(rob.allocate(r_add1, idx1));
    REQUIRE(rob.allocate(r_sub2, idx2));
    REQUIRE(rob.allocate(r_add3, idx3));
    REQUIRE(rob.size() == 4);

    // 4. Mispredict occurs on BRANCH!
    // Recovery steps:
    // Step 1: restore RAT checkpoint
    rat.restore();
    REQUIRE(rat.checkpoint_count() == 0);

    // Step 2: squash younger ROB entries
    std::vector<cpu::ROBEntry> squashed = rob.squash_younger_than(branch_rob_idx);
    REQUIRE(squashed.size() == 3);

    // Step 3: release their physical_rd (if register_write == true)
    for (const auto& sq_entry : squashed)
    {
        if (sq_entry.register_write)
        {
            free_list.release(sq_entry.physical_rd);
        }
    }

    // 5. Final state verification:
    // RAT restored:
    REQUIRE(rat.lookup(cpu::Register::R1) == 1);
    REQUIRE(rat.lookup(cpu::Register::R2) == 2);
    REQUIRE(rat.lookup(cpu::Register::R3) == 3);

    // FreeList:
    // P16, P17, P18 available again, full capacity restored
    REQUIRE(free_list.size() == initial_free_count);

    // ROB:
    // Only [BRANCH] remains
    REQUIRE(rob.size() == 1);
    REQUIRE(rob.entry(branch_rob_idx) != nullptr);
    REQUIRE(rob.entry(branch_rob_idx)->pc == 0x1000);
    REQUIRE(rob.entry(branch_rob_idx)->branch == true);

    std::cout << "PASSED\n";
}

} // namespace

int main()
{
    std::cout << "=========================================================\n";
    std::cout << "--- PhysicalRegisterFreeList Recovery Behavior Tests  ---\n";
    std::cout << "=========================================================\n\n";

    test_one_speculative_register_recovered();
    test_multiple_squashed_destinations_recovered();
    test_old_physical_rd_not_released();
    test_instruction_without_destination_register();
    test_full_branch_recovery_scenario();

    std::cout << "\n[PASS] All 5 FreeList Recovery Behavior tests passed successfully!\n";
    return 0;
}

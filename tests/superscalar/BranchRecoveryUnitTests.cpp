// ─────────────────────────────────────────────────────────────────────────────
// BranchRecoveryUnitTests.cpp
//
// Tests for cpu::BranchRecoveryUnit.
//
// Test 1 – Misprediction recovery
//   Builds a realistic speculative window (BEQ + ADD + ADD + STORE), fires a
//   misprediction report and verifies that:
//     • The RAT is restored to its pre-branch snapshot.
//     • The ROB retains only the branch entry.
//     • Younger entries (ADD, ADD, STORE) are gone.
//     • The Issue Queue contains only the BEQ entry.
//     • Physical registers P16 and P17 (allocated by the two ADDs) are
//       returned to the free list.
//     • RecoveryControl::recover, fetch_decode_flush, fetch_redirect == true.
//     • RecoveryControl::redirect_pc == 100.
//
// Test 2 – Correct prediction (no-op)
//   Same structures, correct prediction → nothing changes and all flags stay
//   false.
// ─────────────────────────────────────────────────────────────────────────────

#include "../../superscalar/core/branch/BranchRecoveryUnit.hpp"
#include "../../superscalar/core/branch/BranchResult.hpp"
#include "../../superscalar/core/dispatch/ReOrderBuffer.hpp"
#include "../../superscalar/core/dispatch/ROBEntry.hpp"
#include "../../superscalar/core/issue/IssueQueue.hpp"
#include "../../superscalar/core/issue/IssueQueueEntry.hpp"
#include "../../superscalar/core/rename/RegisterAliasTable.hpp"
#include "../../superscalar/core/rename/PhysicalRegisterFreeList.hpp"
#include "../../superscalar/core/rename/RenameBundle.hpp"
#include "../../include/isa/Opcode.hpp"
#include "../../include/isa/Registers.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

// Allocate a ROB entry that represents a branch instruction (no register write).
static std::size_t alloc_branch_rob(cpu::ReOrderBuffer<16>& rob, std::uint32_t pc)
{
    cpu::RenameBundle b{};
    b.valid        = true;
    b.pc           = pc;
    b.opcode       = cpu::Opcode::BEQ;
    b.control_flow = cpu::ControlFlow::BRANCH;
    b.register_write = false;
    b.physical_rd   = cpu::PhysicalRegisterFreeList::INVALID_REGISTER;

    std::size_t idx = 0;
    assert(rob.allocate(b, idx));
    return idx;
}

// Allocate a ROB entry that represents an ADD (register-writing instruction).
static std::size_t alloc_add_rob(
    cpu::ReOrderBuffer<16>&        rob,
    std::uint32_t                  pc,
    cpu::RenameBundle::PhysicalRegister physical_rd,
    cpu::RenameBundle::PhysicalRegister old_physical_rd
)
{
    cpu::RenameBundle b{};
    b.valid          = true;
    b.pc             = pc;
    b.opcode         = cpu::Opcode::ADD;
    b.register_write = true;
    b.physical_rd    = physical_rd;
    b.old_physical_rd = old_physical_rd;

    std::size_t idx = 0;
    assert(rob.allocate(b, idx));
    return idx;
}

// Allocate a ROB entry that represents a STORE (no register write).
static std::size_t alloc_store_rob(cpu::ReOrderBuffer<16>& rob, std::uint32_t pc)
{
    cpu::RenameBundle b{};
    b.valid          = true;
    b.pc             = pc;
    b.opcode         = cpu::Opcode::SW;
    b.memory_write   = true;
    b.register_write = false;
    b.physical_rd    = cpu::PhysicalRegisterFreeList::INVALID_REGISTER;

    std::size_t idx = 0;
    assert(rob.allocate(b, idx));
    return idx;
}

// Insert an IssueQueue entry tagged with the given ROB index.
static void insert_iq_entry(
    cpu::IssueQueue<16>& iq,
    cpu::Opcode          opcode,
    std::size_t          rob_index,
    cpu::ControlFlow     cf = cpu::ControlFlow::NONE
)
{
    cpu::IssueQueueEntry e{};
    e.valid        = true;
    e.opcode       = opcode;
    e.rob_index    = rob_index;
    e.rs1_ready    = true;
    e.rs2_ready    = true;
    e.control_flow = cf;
    assert(iq.insert(e));
}

// ─────────────────────────────────────────────────────────────────────────────
// Build the shared speculative state used by both tests.
//
// Initial architectural → physical mappings (identity):
//   R0→P0, R1→P1, …, R15→P15
//
// Branch (ROB 0, BEQ) dispatched:
//   RAT checkpoint taken here  →  R1→P1, R2→P2 (and all others identity)
//
// Speculative renames:
//   ADD R1 : R1→P16  (ROB 1)
//   ADD R2 : R2→P17  (ROB 2)
//   STORE  : no rename (ROB 3)
//
// Free list after those allocations: P18, P19, …
// ─────────────────────────────────────────────────────────────────────────────
struct SpectState
{
    cpu::ReOrderBuffer<16>          rob{};
    cpu::RegisterAliasTable         rat{64};       // 64 physical regs
    cpu::PhysicalRegisterFreeList   free_list{64, 16}; // P0-P15 pre-mapped
    cpu::IssueQueue<16>             iq{};

    std::size_t rob_beq{};
    std::size_t rob_add1{};
    std::size_t rob_add2{};
    std::size_t rob_store{};

    SpectState()
    {
        // ── Initial RAT: identity mapping R0→P0 … R15→P15 ──────────────────
        // RegisterAliasTable::reset() already sets table_[i] = i, so the
        // default-constructed RAT already encodes the identity mapping.

        // ── Dispatch branch (ROB 0) ─────────────────────────────────────────
        rob_beq = alloc_branch_rob(rob, /*pc=*/0x00);

        // Take the RAT checkpoint for this branch.
        rat.checkpoint();

        // ── Speculative renames ──────────────────────────────────────────────
        // ADD R1 → P16
        const auto p16 = free_list.allocate(); // should give P16
        assert(p16 == 16u);
        rat.set(cpu::Register::R1, p16);

        // ADD R2 → P17
        const auto p17 = free_list.allocate(); // should give P17
        assert(p17 == 17u);
        rat.set(cpu::Register::R2, p17);

        // ── Dispatch speculative instructions to ROB ─────────────────────────
        rob_add1  = alloc_add_rob(rob, /*pc=*/0x04, /*prd=*/p16, /*old=*/1);
        rob_add2  = alloc_add_rob(rob, /*pc=*/0x08, /*prd=*/p17, /*old=*/2);
        rob_store = alloc_store_rob(rob, /*pc=*/0x0C);

        // ── Populate Issue Queue ─────────────────────────────────────────────
        insert_iq_entry(iq, cpu::Opcode::BEQ,  rob_beq,   cpu::ControlFlow::BRANCH);
        insert_iq_entry(iq, cpu::Opcode::ADD,  rob_add1);
        insert_iq_entry(iq, cpu::Opcode::ADD,  rob_add2);
        insert_iq_entry(iq, cpu::Opcode::SW,   rob_store);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Test 1 – Misprediction recovery
// ─────────────────────────────────────────────────────────────────────────────
void test_misprediction_recovery()
{
    SpectState s{};

    // Verify pre-recovery state.
    assert(s.rob.size() == 4);
    assert(s.iq.size()  == 4);
    assert(s.rat.lookup(cpu::Register::R1) == 16u); // P16 (speculative)
    assert(s.rat.lookup(cpu::Register::R2) == 17u); // P17 (speculative)

    // ── BEU reports a misprediction ──────────────────────────────────────────
    cpu::BranchResult result{};
    result.valid            = true;
    result.rob_index        = s.rob_beq;    // ROB 0

    result.taken            = true;
    result.target_pc        = 100;

    result.predicted_taken  = false;
    result.predicted_target = 0;

    result.mispredicted     = true;
    result.redirect_pc      = 100;

    // ── Perform recovery ─────────────────────────────────────────────────────
    cpu::BranchRecoveryUnit bru{};
    bru.recover(result, s.rob, s.rat, s.free_list, s.iq);

    const auto& ctrl = bru.control();

    // ── Verify RecoveryControl signals ───────────────────────────────────────
    assert(ctrl.recover           == true);
    assert(ctrl.fetch_decode_flush == true);
    assert(ctrl.fetch_redirect    == true);
    assert(ctrl.redirect_pc       == 100u);

    // ── Verify RAT restore ───────────────────────────────────────────────────
    // Checkpoint was taken before the speculative renames, so R1/R2 must be
    // back to their pre-branch physical registers P1 and P2.
    assert(s.rat.lookup(cpu::Register::R1) == 1u);
    assert(s.rat.lookup(cpu::Register::R2) == 2u);

    // All other mappings must be unchanged (still identity).
    assert(s.rat.lookup(cpu::Register::R0) == 0u);
    assert(s.rat.lookup(cpu::Register::R3) == 3u);

    // ── Verify ROB squash ────────────────────────────────────────────────────
    // Only the BEQ entry (ROB 0) must survive.
    assert(s.rob.size() == 1);

    // BEQ is still at its original slot and is valid.
    const cpu::ROBEntry* beq_entry = s.rob.entry(s.rob_beq);
    assert(beq_entry != nullptr);
    assert(beq_entry->valid);
    assert(beq_entry->opcode == cpu::Opcode::BEQ);

    // Younger instructions are gone.
    // The slots they occupied will now have their valid flag cleared.
    const cpu::ROBEntry* add1_entry = s.rob.entry(s.rob_add1);
    assert(add1_entry == nullptr || !add1_entry->valid);

    const cpu::ROBEntry* add2_entry = s.rob.entry(s.rob_add2);
    assert(add2_entry == nullptr || !add2_entry->valid);

    const cpu::ROBEntry* store_entry = s.rob.entry(s.rob_store);
    assert(store_entry == nullptr || !store_entry->valid);

    // ── Verify Issue Queue squash ─────────────────────────────────────────────
    // Only the BEQ entry must remain.
    assert(s.iq.size() == 1);
    assert( s.iq.contains_rob(s.rob_beq));
    assert(!s.iq.contains_rob(s.rob_add1));
    assert(!s.iq.contains_rob(s.rob_add2));
    assert(!s.iq.contains_rob(s.rob_store));

    // ── Verify FreeList reclaim ───────────────────────────────────────────────
    // Before recovery the free list had P18, P19, …
    // After recovery P16 and P17 must have been returned, so the free list must
    // now be larger by exactly 2.
    //
    // We probe by allocating three registers and checking that we get P16 or
    // P17 among them (order within the queue is implementation-defined, but
    // both must be available within the next few allocations).
    {
        const auto a = s.free_list.allocate();
        const auto b = s.free_list.allocate();
        const auto c = s.free_list.allocate();

        // At least two of the returned registers must be P16 or P17.
        const bool p16_back = (a == 16u || b == 16u || c == 16u);
        const bool p17_back = (a == 17u || b == 17u || c == 17u);

        assert(p16_back);
        assert(p17_back);
    }

    std::cout << "  [PASS] misprediction recovery\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 2 – Correct prediction: BranchRecoveryUnit must be a no-op
// ─────────────────────────────────────────────────────────────────────────────
void test_correct_prediction_no_op()
{
    SpectState s{};

    // Snapshot sizes before the (non-)recovery.
    const std::size_t rob_size_before = s.rob.size();
    const std::size_t iq_size_before  = s.iq.size();
    const auto        r1_before       = s.rat.lookup(cpu::Register::R1);
    const auto        r2_before       = s.rat.lookup(cpu::Register::R2);
    const std::size_t fl_size_before  = s.free_list.size();

    // Correct prediction: actual taken to 100, prediction also taken to 100.
    cpu::BranchResult result{};
    result.valid            = true;
    result.rob_index        = s.rob_beq;

    result.taken            = true;
    result.target_pc        = 100;

    result.predicted_taken  = true;
    result.predicted_target = 100;

    result.mispredicted     = false;  // ← correct prediction
    result.redirect_pc      = 0;

    cpu::BranchRecoveryUnit bru{};
    bru.recover(result, s.rob, s.rat, s.free_list, s.iq);

    const auto& ctrl = bru.control();

    // ── Control flags must all be false ──────────────────────────────────────
    assert(ctrl.recover           == false);
    assert(ctrl.fetch_decode_flush == false);
    assert(ctrl.fetch_redirect    == false);

    // ── Structures must be untouched ─────────────────────────────────────────
    assert(s.rob.size()                       == rob_size_before);
    assert(s.iq.size()                        == iq_size_before);
    assert(s.rat.lookup(cpu::Register::R1)    == r1_before);
    assert(s.rat.lookup(cpu::Register::R2)    == r2_before);
    assert(s.free_list.size()                 == fl_size_before);

    std::cout << "  [PASS] correct prediction is a no-op\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 3 – Invalid result: BranchRecoveryUnit must be a no-op
// ─────────────────────────────────────────────────────────────────────────────
void test_invalid_result_no_op()
{
    SpectState s{};

    const std::size_t rob_size_before = s.rob.size();
    const std::size_t iq_size_before  = s.iq.size();

    // result.valid == false → nothing should happen even if mispredicted is set.
    cpu::BranchResult result{};
    result.valid        = false;
    result.mispredicted = true;
    result.redirect_pc  = 999;

    cpu::BranchRecoveryUnit bru{};
    bru.recover(result, s.rob, s.rat, s.free_list, s.iq);

    const auto& ctrl = bru.control();

    assert(ctrl.recover           == false);
    assert(ctrl.fetch_decode_flush == false);
    assert(ctrl.fetch_redirect    == false);
    assert(s.rob.size()           == rob_size_before);
    assert(s.iq.size()            == iq_size_before);

    std::cout << "  [PASS] invalid BranchResult is a no-op\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 4 – Repeated calls: second call resets stale flags
// ─────────────────────────────────────────────────────────────────────────────
void test_second_call_resets_flags()
{
    // First recovery with a misprediction.
    SpectState s{};

    cpu::BranchResult bad{};
    bad.valid        = true;
    bad.rob_index    = s.rob_beq;
    bad.taken        = true;
    bad.target_pc    = 100;
    bad.mispredicted = true;
    bad.redirect_pc  = 100;

    cpu::BranchRecoveryUnit bru{};
    bru.recover(bad, s.rob, s.rat, s.free_list, s.iq);
    assert(bru.control().recover == true);

    // Second call with a valid-but-not-mispredicted result must reset all flags.
    // (Structures from the first recovery now hold valid post-recovery state.)
    cpu::BranchResult good{};
    good.valid        = true;
    good.mispredicted = false;

    bru.recover(good, s.rob, s.rat, s.free_list, s.iq);

    assert(bru.control().recover           == false);
    assert(bru.control().fetch_decode_flush == false);
    assert(bru.control().fetch_redirect    == false);

    std::cout << "  [PASS] second call resets stale control flags\n";
}

// ─────────────────────────────────────────────────────────────────────────────
int main()
{
    std::cout << "Running BranchRecoveryUnit Tests:\n";

    test_misprediction_recovery();
    test_correct_prediction_no_op();
    test_invalid_result_no_op();
    test_second_call_resets_flags();

    std::cout << "\nAll BranchRecoveryUnit tests PASSED successfully!\n";
    return 0;
}

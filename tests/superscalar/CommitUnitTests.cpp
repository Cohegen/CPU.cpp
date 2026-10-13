#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

#include "../../superscalar/core/commit/ArchitecturalRegisterFile.hpp"
#include "../../superscalar/core/commit/StoreBuffer.hpp"
#include "../../superscalar/core/commit/CommitUnit.hpp"
#include "../../superscalar/core/dispatch/ReOrderBuffer.hpp"
#include "../../superscalar/core/issue/PhysicalRegisterFile.hpp"
#include "../../superscalar/core/rename/PhysicalRegisterFreeList.hpp"
#include "../../superscalar/core/rename/RegisterAliasTable.hpp"
#include "../../superscalar/core/rename/RenameBundle.hpp"
#include "../../include/isa/Opcode.hpp"
#include "../../include/isa/Registers.hpp"

// ============================================================================
// Test 1: In-order commit and ARF retirement
// Instructions retire strictly in-order and write to the Architectural Register File.
// ============================================================================
void test_in_order_commit_and_arf_retirement()
{
    std::cout << "[Test 1] Testing In-Order Commit & ARF Retirement...\n";

    cpu::ReOrderBuffer<16> rob{};
    cpu::PhysicalRegisterFile<64> prf{};
    cpu::ArchitecturalRegisterFile<16> arf{};
    cpu::PhysicalRegisterFreeList free_list{64, 16};
    cpu::RegisterAliasTable rat{64};
    cpu::CommitUnit<16, 64, 16, 16> commit_unit{2};

    // Instruction 1: ADD R1 = R2 + R3 (ROB 0), allocated to P16, old P1
    cpu::RenameBundle b1{};
    b1.valid = true;
    b1.pc = 0x1000;
    b1.opcode = cpu::Opcode::ADD;
    b1.register_write = true;
    b1.rd = cpu::Register::R1;
    b1.physical_rd = 16;
    b1.old_physical_rd = 1;
    std::size_t idx1 = 0;
    assert(rob.allocate(b1, idx1));

    // Instruction 2: ADD R2 = R4 + R5 (ROB 1), allocated to P17, old P2
    cpu::RenameBundle b2{};
    b2.valid = true;
    b2.pc = 0x1004;
    b2.opcode = cpu::Opcode::ADD;
    b2.register_write = true;
    b2.rd = cpu::Register::R2;
    b2.physical_rd = 17;
    b2.old_physical_rd = 2;
    std::size_t idx2 = 0;
    assert(rob.allocate(b2, idx2));

    // Simulate execution out of order: Instruction 2 completes before Instruction 1
    prf.write(17, 200);
    rob.complete(idx2, 200);

    // Commit attempt: ROB head (Instruction 1) is NOT completed, so neither should commit!
    auto events = commit_unit.step(rob, prf, arf, free_list, rat);
    assert(events.empty());
    assert(arf.read(cpu::Register::R1) == 0);
    assert(arf.read(cpu::Register::R2) == 0);
    assert(commit_unit.committed_instruction_count() == 0);

    // Now Instruction 1 completes
    prf.write(16, 100);
    rob.complete(idx1, 100);

    // Commit step: both instructions should now commit in-order (width = 2)
    events = commit_unit.step(rob, prf, arf, free_list, rat);
    assert(events.size() == 2);
    assert(events[0].rob_index == idx1);
    assert(events[0].architectural_rd == cpu::Register::R1);
    assert(events[0].value == 100);

    assert(events[1].rob_index == idx2);
    assert(events[1].architectural_rd == cpu::Register::R2);
    assert(events[1].value == 200);

    // ARF must now reflect committed values
    assert(arf.read(cpu::Register::R1) == 100);
    assert(arf.read(cpu::Register::R2) == 200);
    assert(commit_unit.committed_instruction_count() == 2);
    assert(rob.empty());

    std::cout << "  [PASS] Instructions committed in-order and ARF updated correctly.\n";
}

// ============================================================================
// Test 2: Overwritten physical register reclamation
// On commit of a register write, old_physical_rd is formally returned to free list.
// ============================================================================
void test_overwritten_register_reclamation()
{
    std::cout << "[Test 2] Testing Overwritten Physical Register Reclamation...\n";

    cpu::ReOrderBuffer<16> rob{};
    cpu::PhysicalRegisterFile<64> prf{};
    cpu::ArchitecturalRegisterFile<16> arf{};
    cpu::PhysicalRegisterFreeList free_list{64, 16};
    cpu::RegisterAliasTable rat{64};
    cpu::CommitUnit<16, 64, 16, 16> commit_unit{2};

    // Free list initially has 64 - 16 = 48 registers (P16 to P63)
    const std::size_t initial_free_count = free_list.size();

    // Allocate P16 to R1 (overwriting initial identity P1)
    const auto p16 = free_list.allocate();
    assert(p16 == 16);
    assert(free_list.size() == initial_free_count - 1);

    cpu::RenameBundle b{};
    b.valid = true;
    b.pc = 0x1000;
    b.opcode = cpu::Opcode::ADD;
    b.register_write = true;
    b.rd = cpu::Register::R1;
    b.physical_rd = p16;
    b.old_physical_rd = 1; // P1 was the previous mapping for R1
    std::size_t idx = 0;
    assert(rob.allocate(b, idx));

    prf.write(p16, 42);
    rob.complete(idx, 42);

    // Before commit, P1 is NOT in the free list (still needed by older in-flight reads)
    // On commit, P1 is freed
    auto events = commit_unit.step(rob, prf, arf, free_list, rat);
    assert(events.size() == 1);
    assert(events[0].freed_physical_reg == 1);

    // Free list count is restored (P16 allocated, P1 reclaimed)
    assert(free_list.size() == initial_free_count);

    // Exhaust remaining free registers to verify P1 is indeed now available in the free list
    bool found_p1 = false;
    while (!free_list.empty())
    {
        if (free_list.allocate() == 1)
        {
            found_p1 = true;
            break;
        }
    }
    assert(found_p1);

    std::cout << "  [PASS] Overwritten physical register reclaimed on commit.\n";
}

// ============================================================================
// Test 3: Branch retirement and checkpoint deallocation
// Successfully completed branches reaching ROB head pop their RAT checkpoints.
// ============================================================================
void test_branch_retirement_checkpoint_pop()
{
    std::cout << "[Test 3] Testing Branch Retirement Checkpoint Deallocation...\n";

    cpu::ReOrderBuffer<16> rob{};
    cpu::PhysicalRegisterFile<64> prf{};
    cpu::ArchitecturalRegisterFile<16> arf{};
    cpu::PhysicalRegisterFreeList free_list{64, 16};
    cpu::RegisterAliasTable rat{64};
    cpu::CommitUnit<16, 64, 16, 16> commit_unit{2};

    // Take RAT checkpoint at branch dispatch
    const auto cp = rat.checkpoint();
    assert(rat.checkpoint_count() == 1);

    cpu::RenameBundle branch_bundle{};
    branch_bundle.valid = true;
    branch_bundle.pc = 0x2000;
    branch_bundle.opcode = cpu::Opcode::BEQ;
    branch_bundle.control_flow = cpu::ControlFlow::BRANCH;
    std::size_t branch_idx = 0;
    assert(rob.allocate(branch_bundle, branch_idx, &cp));

    assert(rob.entry(branch_idx)->has_checkpoint == true);

    // Branch completes correctly
    rob.complete(branch_idx);

    // Commit the branch
    auto events = commit_unit.step(rob, prf, arf, free_list, rat);
    assert(events.size() == 1);
    assert(events[0].branch == true);

    // RAT checkpoint stack must be popped upon retirement
    assert(rat.checkpoint_count() == 0);

    std::cout << "  [PASS] Branch retired and checkpoint deallocated.\n";
}

// ============================================================================
// Test 4: Store Buffer & Speculative Memory Commit
// Stores remain buffered and only mark committed when reaching ROB head.
// Squashed stores are removed and never committed.
// ============================================================================
void test_store_buffer_speculative_memory_commit()
{
    std::cout << "[Test 4] Testing Store Buffer & Memory Commit...\n";

    cpu::ReOrderBuffer<16> rob{};
    cpu::PhysicalRegisterFile<64> prf{};
    cpu::ArchitecturalRegisterFile<16> arf{};
    cpu::PhysicalRegisterFreeList free_list{64, 16};
    cpu::RegisterAliasTable rat{64};
    cpu::StoreBuffer<16> store_buffer{};
    cpu::CommitUnit<16, 64, 16, 16> commit_unit{2};

    // Store 1 (ROB 0): SW 100 -> [0x1000]
    cpu::RenameBundle sw1{};
    sw1.valid = true;
    sw1.pc = 0x3000;
    sw1.opcode = cpu::Opcode::SW;
    sw1.memory_write = true;
    std::size_t rob_sw1 = 0;
    assert(rob.allocate(sw1, rob_sw1));
    assert(store_buffer.allocate(rob_sw1, sw1.pc));

    // Store 2 (ROB 1): SW 200 -> [0x1004] (speculative on wrong path)
    cpu::RenameBundle sw2{};
    sw2.valid = true;
    sw2.pc = 0x3004;
    sw2.opcode = cpu::Opcode::SW;
    sw2.memory_write = true;
    std::size_t rob_sw2 = 0;
    assert(rob.allocate(sw2, rob_sw2));
    assert(store_buffer.allocate(rob_sw2, sw2.pc));

    // Both stores calculate addresses and data during execution
    assert(store_buffer.update(rob_sw1, 0x1000, 100));
    assert(store_buffer.update(rob_sw2, 0x1004, 200));

    // Test Store-to-Load Forwarding from StoreBuffer
    const auto fwd1 = store_buffer.forward(0x1000);
    assert(fwd1.has_value() && fwd1.value() == 100);

    const auto fwd2 = store_buffer.forward(0x1004);
    assert(fwd2.has_value() && fwd2.value() == 200);

    // Verify neither store can drain to memory before ROB commit
    assert(!store_buffer.pop_committed().has_value());

    // Complete Store 1 in ROB
    rob.complete(rob_sw1);

    // Store 2 is squashed due to branch misprediction
    std::vector<std::size_t> squashed = {rob_sw2};
    store_buffer.squash(squashed);

    assert(store_buffer.size() == 1); // Only sw1 remains

    // Commit Store 1
    auto events = commit_unit.step(rob, prf, arf, free_list, rat, &store_buffer);
    assert(events.size() == 1);
    assert(events[0].store == true);

    // Store 1 is now marked committed and ready to drain to memory
    const auto committed_store = store_buffer.pop_committed();
    assert(committed_store.has_value());
    assert(committed_store->address == 0x1000);
    assert(committed_store->data == 100);
    assert(store_buffer.empty());

    std::cout << "  [PASS] StoreBuffer preserved speculation and committed safely to memory.\n";
}

int main()
{
    std::cout << "========================================================\n";
    std::cout << "Starting Phase 1: In-Order Commit & Retirement Unit Tests\n";
    std::cout << "========================================================\n";

    test_in_order_commit_and_arf_retirement();
    test_overwritten_register_reclamation();
    test_branch_retirement_checkpoint_pop();
    test_store_buffer_speculative_memory_commit();

    std::cout << "========================================================\n";
    std::cout << "ALL Phase 1 Commit & Retirement Tests PASSED!\n";
    std::cout << "========================================================\n";

    return 0;
}

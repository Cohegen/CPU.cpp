#include "../../superscalar/core/issue/IssueQueue.hpp"
#include "../../superscalar/core/issue/IssueQueueEntry.hpp"
#include "../../superscalar/core/dispatch/ReOrderBuffer.hpp"
#include "../../superscalar/core/dispatch/ROBEntry.hpp"
#include "../../superscalar/core/rename/RenameBundle.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

// Helper to construct a synthetic IssueQueueEntry with a given ROB index
cpu::IssueQueueEntry make_iq_entry(std::size_t rob_index, std::uint32_t pc = 0x1000, bool issued = false)
{
    cpu::IssueQueueEntry entry{};
    entry.valid = true;
    entry.issued = issued;
    entry.pc = pc;
    entry.rob_index = rob_index;
    entry.rs1_ready = true;
    entry.rs2_ready = true;
    return entry;
}

// ===========================================================================
// Test 1 — No matching ROB entries
// IQ: ROB 1, ROB 2, ROB 3
// squash {7,8}
// -> nothing removed
// -> size remains 3
// ===========================================================================
void test_squash_no_matching_rob_entries()
{
    std::cout << "[Test 1] No matching ROB entries ... ";

    cpu::IssueQueue<16> queue;
    queue.reset();

    assert(queue.insert(make_iq_entry(1, 0x1000)));
    assert(queue.insert(make_iq_entry(2, 0x1004)));
    assert(queue.insert(make_iq_entry(3, 0x1008)));

    assert(queue.size() == 3);
    assert(queue.contains_rob(1));
    assert(queue.contains_rob(2));
    assert(queue.contains_rob(3));

    // Squash {7, 8}
    std::size_t removed = queue.squash({7, 8});

    // -> nothing removed
    // -> size remains 3
    assert(removed == 0);
    assert(queue.size() == 3);
    assert(queue.contains_rob(1));
    assert(queue.contains_rob(2));
    assert(queue.contains_rob(3));
    assert(!queue.contains_rob(7));
    assert(!queue.contains_rob(8));

    std::cout << "PASSED (0 removed, size=3)\n";
}

// ===========================================================================
// Test 2 — One matching entry
// IQ: ROB 3, ROB 7, ROB 9
// squash {7}
// -> ROB 7 removed
// -> size = 2
// ===========================================================================
void test_squash_one_matching_entry()
{
    std::cout << "[Test 2] One matching entry ... ";

    cpu::IssueQueue<16> queue;
    queue.reset();

    assert(queue.insert(make_iq_entry(3, 0x1000)));
    assert(queue.insert(make_iq_entry(7, 0x1004)));
    assert(queue.insert(make_iq_entry(9, 0x1008)));

    assert(queue.size() == 3);

    // Squash {7}
    std::size_t removed = queue.squash({7});

    // -> ROB 7 removed
    // -> size = 2
    assert(removed == 1);
    assert(queue.size() == 2);
    assert(queue.contains_rob(3));
    assert(queue.contains_rob(9));
    assert(!queue.contains_rob(7));

    std::cout << "PASSED (ROB 7 removed, size=2, ROB 3 & 9 remain)\n";
}

// ===========================================================================
// Test 3 — Multiple matching entries
// IQ: ROB 3, ROB 6, ROB 7, ROB 8, ROB 10
// squash {6,7,8}
// -> 6,7,8 removed
// -> 3,10 remain
// ===========================================================================
void test_squash_multiple_matching_entries()
{
    std::cout << "[Test 3] Multiple matching entries ... ";

    cpu::IssueQueue<16> queue;
    queue.reset();

    assert(queue.insert(make_iq_entry(3, 0x1000)));
    assert(queue.insert(make_iq_entry(6, 0x1004)));
    assert(queue.insert(make_iq_entry(7, 0x1008)));
    assert(queue.insert(make_iq_entry(8, 0x100C)));
    assert(queue.insert(make_iq_entry(10, 0x1010)));

    assert(queue.size() == 5);

    // Squash {6, 7, 8}
    std::size_t removed = queue.squash({6, 7, 8});

    // -> 6, 7, 8 removed
    // -> 3, 10 remain
    assert(removed == 3);
    assert(queue.size() == 2);
    assert(queue.contains_rob(3));
    assert(queue.contains_rob(10));
    assert(!queue.contains_rob(6));
    assert(!queue.contains_rob(7));
    assert(!queue.contains_rob(8));

    std::cout << "PASSED (6, 7, 8 removed, size=2, ROB 3 & 10 remain)\n";
}

// ===========================================================================
// Test 4 — Older instruction survives
// This is particularly important:
// branch ROB = 8
// IQ: ROB 5, ROB 6, ROB 7, ROB 8, ROB 9, ROB 10
// squashed = {9,10}
// -> 5,6,7,8 survive
// -> 9,10 removed
// ===========================================================================
void test_squash_older_instruction_survives()
{
    std::cout << "[Test 4] Older instruction survives ... ";

    cpu::IssueQueue<16> queue;
    queue.reset();

    constexpr std::size_t branch_rob = 8;
    (void)branch_rob;

    assert(queue.insert(make_iq_entry(5, 0x1000)));
    assert(queue.insert(make_iq_entry(6, 0x1004)));
    assert(queue.insert(make_iq_entry(7, 0x1008)));
    assert(queue.insert(make_iq_entry(8, 0x100C))); // Branch instruction itself
    assert(queue.insert(make_iq_entry(9, 0x1010))); // Younger than branch
    assert(queue.insert(make_iq_entry(10, 0x1014))); // Younger than branch

    assert(queue.size() == 6);

    // Only instructions younger than the branch are squashed: {9, 10}
    std::vector<std::size_t> squashed = {9, 10};
    std::size_t removed = queue.squash(squashed);

    // -> 5, 6, 7, 8 survive (including branch ROB 8)
    // -> 9, 10 removed
    assert(removed == 2);
    assert(queue.size() == 4);
    assert(queue.contains_rob(5));
    assert(queue.contains_rob(6));
    assert(queue.contains_rob(7));
    assert(queue.contains_rob(8));
    assert(!queue.contains_rob(9));
    assert(!queue.contains_rob(10));

    std::cout << "PASSED (5,6,7,8 survive, 9,10 removed, size=4)\n";
}

// ===========================================================================
// Test 5 — Issued entries also disappear
// An instruction being issued does not mean it becomes non-speculative.
// ROB 10 -> IQ entry, issued = true
// If ROB 10 is younger than a mispredicted branch, it still gets flushed.
// So:
// issued == execution state
// committed == architectural state
// An issued-but-uncommitted instruction remains speculative.
// ===========================================================================
void test_squash_issued_entries_also_disappear()
{
    std::cout << "[Test 5] Issued entries also disappear ... ";

    cpu::IssueQueue<16> queue;
    queue.reset();

    // Insert instructions:
    // ROB 8: branch (unissued)
    // ROB 9: unissued younger instruction
    // ROB 10: ALREADY ISSUED younger instruction (issued = true)
    assert(queue.insert(make_iq_entry(8, 0x1000, false)));
    assert(queue.insert(make_iq_entry(9, 0x1004, false)));
    assert(queue.insert(make_iq_entry(10, 0x1008, true))); // issued = true!

    assert(queue.size() == 3);

    // Verify ROB 10 is marked as issued in the IssueQueue
    auto* entry10 = queue.find_rob(10);
    assert(entry10 != nullptr);
    assert(entry10->issued == true);
    assert(entry10->valid == true);

    // Branch at ROB 8 mispredicts -> squash younger instructions {9, 10}
    std::size_t removed = queue.squash({9, 10});

    // Both younger instructions must be removed, including the issued one (ROB 10)
    assert(removed == 2);
    assert(queue.size() == 1);
    assert(queue.contains_rob(8));
    assert(!queue.contains_rob(9));
    assert(!queue.contains_rob(10));
    assert(queue.find_rob(10) == nullptr);

    // Verify architectural principle:
    // issued == execution state
    // committed == architectural state
    // Even though ROB 10 was issued, it had not committed and was speculative.
    std::cout << "PASSED (Issued entry ROB 10 flushed successfully)\n";
}

// ===========================================================================
// Test 6 — Wrapped ROB
// For example:
// ROB capacity = 8
// head = 6
// tail = 3
// logical order:
// 6 -> 7 -> 0 -> 1 -> 2
// If branch is at 7:
// younger = 0,1,2
// The Issue Queue must receive exactly:
// {0,1,2}
// Your existing squash_younger_than() already handles this correctly.
// ===========================================================================
void test_squash_wrapped_rob()
{
    std::cout << "[Test 6] Wrapped ROB ... ";

    // 1. Setup a ReOrderBuffer with Capacity = 8
    cpu::ReOrderBuffer<8> rob;
    rob.reset();

    // Advance head to 6 by allocating and committing 6 dummy instructions
    for (int i = 0; i < 6; ++i)
    {
        cpu::ReOrderBuffer<8>::Index idx = 0;
        cpu::RenameBundle b{};
        b.valid = true;
        b.pc = 0x0100 + i * 4;
        assert(rob.allocate(b, idx));
        rob.complete(idx);
        cpu::ROBEntry committed{};
        assert(rob.commit(committed));
    }
    // Now head_ = 6, tail_ = 6, count_ = 0
    assert(rob.head_index() == 6);
    assert(rob.empty());

    // Allocate 5 instructions in program order:
    // Slot 6: inst at 0x1000
    // Slot 7: branch at 0x1004 (mispredicting branch!)
    // Slot 0: wrapped inst at 0x1008
    // Slot 1: wrapped inst at 0x100C
    // Slot 2: wrapped inst at 0x1010
    // tail_ wraps to 3.
    cpu::ReOrderBuffer<8>::Index idx6 = 0, branch_idx7 = 0, idx0 = 0, idx1 = 0, idx2 = 0;

    auto alloc_inst = [&](std::uint32_t pc, cpu::ReOrderBuffer<8>::Index& out_idx) {
        cpu::RenameBundle b{};
        b.valid = true;
        b.pc = pc;
        assert(rob.allocate(b, out_idx));
    };

    alloc_inst(0x1000, idx6);
    alloc_inst(0x1004, branch_idx7);
    alloc_inst(0x1008, idx0);
    alloc_inst(0x100C, idx1);
    alloc_inst(0x1010, idx2);

    assert(idx6 == 6);
    assert(branch_idx7 == 7);
    assert(idx0 == 0);
    assert(idx1 == 1);
    assert(idx2 == 2);
    assert(rob.head_index() == 6);
    assert(rob.tail_index() == 3);
    assert(rob.size() == 5);

    // 2. Setup IssueQueue containing these instructions
    cpu::IssueQueue<16> queue;
    queue.reset();

    assert(queue.insert(make_iq_entry(6, 0x1000)));
    assert(queue.insert(make_iq_entry(7, 0x1004))); // branch
    assert(queue.insert(make_iq_entry(0, 0x1008))); // younger wrapped
    assert(queue.insert(make_iq_entry(1, 0x100C))); // younger wrapped
    assert(queue.insert(make_iq_entry(2, 0x1010))); // younger wrapped

    assert(queue.size() == 5);

    // 3. Squash younger than branch at ROB index 7 using rob.squash_younger_than()
    std::vector<cpu::ROBEntry> squashed_rob = rob.squash_younger_than(branch_idx7);

    // Verify squash_younger_than returns exactly {0, 1, 2} in logical program order
    assert(squashed_rob.size() == 3);
    assert(squashed_rob[0].rob_index == 0);
    assert(squashed_rob[1].rob_index == 1);
    assert(squashed_rob[2].rob_index == 2);
    assert(squashed_rob[0].pc == 0x1008);
    assert(squashed_rob[1].pc == 0x100C);
    assert(squashed_rob[2].pc == 0x1010);

    // 4. Issue Queue receives exactly {0, 1, 2} via the squashed entries
    std::size_t removed = queue.squash(squashed_rob);

    // -> 0, 1, 2 removed from IssueQueue
    // -> 6, 7 survive in IssueQueue
    assert(removed == 3);
    assert(queue.size() == 2);
    assert(queue.contains_rob(6));
    assert(queue.contains_rob(7));
    assert(!queue.contains_rob(0));
    assert(!queue.contains_rob(1));
    assert(!queue.contains_rob(2));

    // Also verify ROB state after squash
    assert(rob.size() == 2);
    assert(rob.entry(idx6) != nullptr && rob.entry(idx6)->valid);
    assert(rob.entry(branch_idx7) != nullptr && rob.entry(branch_idx7)->valid);
    assert(rob.tail_index() == 0);

    std::cout << "PASSED (Received {0,1,2}, 6 & 7 survived, size=2)\n";
}

} // namespace

int main()
{
    std::cout << "===================================================================\n";
    std::cout << "--- Superscalar Issue Queue Independent Unit Tests              ---\n";
    std::cout << "===================================================================\n\n";

    test_squash_no_matching_rob_entries();
    test_squash_one_matching_entry();
    test_squash_multiple_matching_entries();
    test_squash_older_instruction_survives();
    test_squash_issued_entries_also_disappear();
    test_squash_wrapped_rob();

    std::cout << "\n===================================================================\n";
    std::cout << "[PASS] All 6 independent IssueQueue tests completed successfully!\n";
    std::cout << "===================================================================\n";

    return 0;
}

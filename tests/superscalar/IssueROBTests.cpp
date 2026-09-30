#include "../../superscalar/core/issue/PhysicalRegisterFile.hpp"
#include "../../superscalar/core/issue/IssueQueue.hpp"
#include "../../superscalar/core/issue/IssueQueueEntry.hpp"
#include "../../superscalar/core/dispatch/ROBEntry.hpp"
#include "../../superscalar/core/dispatch/ReOrderBuffer.hpp"
#include "../../superscalar/core/dispatch/DispatchUnit.hpp"
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

// Helper to construct synthetic DecodeBundle
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

// ===========================================================================
// 1. PhysicalRegisterFile Unit Tests
// ===========================================================================

void test_prf_reset_state()
{
    // Test 1 — Reset state
    cpu::PhysicalRegisterFile<> registers;
    registers.reset();

    for (std::uint8_t p = 0; p < 32; ++p)
    {
        assert(registers.read(p) == 0);
        assert(!registers.ready(p));
    }

    std::cout << "  [PASS] Test 1: Reset state initializes all registers (value=0, ready=false).\n";
}

void test_prf_allocate_makes_register_unavailable()
{
    // Test 2 — Allocate makes register unavailable
    cpu::PhysicalRegisterFile<> registers;
    registers.reset();

    registers.allocate(16);

    // Expect: P16: value = 0, ready = false
    assert(registers.read(16) == 0);
    assert(!registers.ready(16));

    std::cout << "  [PASS] Test 2: Allocate makes register unavailable (value=0, ready=false).\n";
}

void test_prf_write_makes_register_ready()
{
    // Test 3 — Write makes register ready
    cpu::PhysicalRegisterFile<> registers;
    registers.reset();

    registers.allocate(16);
    registers.write(16, 42);

    // Expect: P16: value = 42, ready = true
    assert(registers.read(16) == 42);
    assert(registers.ready(16));

    std::cout << "  [PASS] Test 3: Write makes register ready (value=42, ready=true).\n";
}

void test_prf_overwrite_existing_value()
{
    // Test 4 — Overwrite an existing value
    cpu::PhysicalRegisterFile<> registers;
    registers.reset();

    registers.write(16, 42);
    registers.write(16, 99);

    // Expect: P16 = 99, ready = true
    assert(registers.read(16) == 99);
    assert(registers.ready(16));

    std::cout << "  [PASS] Test 4: Overwrite an existing value updates correctly (value=99, ready=true).\n";
}

void test_prf_allocate_resets_readiness()
{
    // Test 5 — Allocate resets readiness
    cpu::PhysicalRegisterFile<> registers;
    registers.reset();

    registers.write(16, 42);
    assert(registers.read(16) == 42);
    assert(registers.ready(16));

    registers.allocate(16);

    // Expect: P16: value = 0, ready = false
    assert(registers.read(16) == 0);
    assert(!registers.ready(16));

    std::cout << "  [PASS] Test 5: Allocate resets readiness for reused register (value=0, ready=false).\n";
}

void test_prf_invalid_physical_register()
{
    // Test 6 — Invalid physical register
    cpu::PhysicalRegisterFile<> registers;
    registers.reset();

    registers.write(10, 50);

    // Testing out-of-range index (255)
    assert(registers.read(255) == 0);
    assert(!registers.ready(255));
    registers.write(255, 100);
    registers.allocate(255);

    // Valid state must not be modified
    assert(registers.read(10) == 50);
    assert(registers.ready(10));
    assert(registers.read(255) == 0);
    assert(!registers.ready(255));

    std::cout << "  [PASS] Test 6: Invalid physical register safely handled without corrupting valid state.\n";
}

void test_prf_boundary_registers()
{
    // Test 7 — Boundary registers (P0 and P31)
    cpu::PhysicalRegisterFile<> registers;
    registers.reset();

    // P0
    registers.allocate(0);
    assert(registers.read(0) == 0);
    assert(!registers.ready(0));
    registers.write(0, 100);
    assert(registers.read(0) == 100);
    assert(registers.ready(0));

    // P31
    registers.allocate(31);
    assert(registers.read(31) == 0);
    assert(!registers.ready(31));
    registers.write(31, 200);
    assert(registers.read(31) == 200);
    assert(registers.ready(31));

    // Out-of-bounds boundary (P32)
    assert(registers.read(32) == 0);
    assert(!registers.ready(32));

    std::cout << "  [PASS] Test 7: Boundary registers P0 and P31 operate correctly.\n";
}

// ===========================================================================
// 2. Issue Queue Tests
// ===========================================================================

void test_iq_insert()
{
    // Test 8 — Insert
    cpu::IssueQueue<16> queue;
    queue.reset();

    cpu::IssueQueueEntry entry{};
    entry.physical_rs1 = 2; // P2
    entry.physical_rs2 = 3; // P3
    entry.rs1_ready = true;
    entry.rs2_ready = false;

    assert(queue.insert(entry));
    assert(queue.size() == 1);

    std::cout << "  [PASS] Test 8: IssueQueue insert increases size to 1.\n";
}

void test_iq_waiting_instruction_not_selected()
{
    // Test 9 — Waiting instruction isn't selected
    cpu::IssueQueue<16> queue;
    queue.reset();

    cpu::IssueQueueEntry entry{};
    entry.physical_rs1 = 2; // P2
    entry.physical_rs2 = 3; // P3
    entry.rs1_ready = true;
    entry.rs2_ready = false;

    assert(queue.insert(entry));

    // P2 = ready, P3 = not ready -> queue.select_ready() must return nullptr
    assert(queue.select_ready() == nullptr);

    std::cout << "  [PASS] Test 9: Waiting instruction with unresolved source operand is not selected.\n";
}

void test_iq_wakeup_source2()
{
    // Test 10 — Wakeup source 2
    cpu::IssueQueue<16> queue;
    queue.reset();

    cpu::IssueQueueEntry entry{};
    entry.physical_rs1 = 2; // P2
    entry.physical_rs2 = 3; // P3
    entry.rs1_ready = true;
    entry.rs2_ready = false;

    assert(queue.insert(entry));
    assert(queue.select_ready() == nullptr);

    // Wakeup P3 with value 100
    queue.wakeup(3, 100);

    auto* ready_entry = queue.select_ready();
    assert(ready_entry != nullptr);
    assert(ready_entry->rs2_ready == true);
    assert(ready_entry->rs2_value == 100);

    std::cout << "  [PASS] Test 10: Wakeup resolves source 2 and enables selection of ready instruction.\n";
}

// ===========================================================================
// 3. Two Instructions with a Dependency
// ===========================================================================

void test_two_instructions_with_dependency()
{
    // I0: ADD R1, R2, R3 -> P16 <- P2 + P3
    // I1: SUB R4, R1, R5 -> P17 <- P16 - P5
    cpu::IssueQueue<16> queue;
    queue.reset();

    // I0: P2 ready, P3 ready -> READY
    cpu::IssueQueueEntry i0{};
    i0.pc = 0x1000;
    i0.physical_rd = 16;
    i0.physical_rs1 = 2;
    i0.physical_rs2 = 3;
    i0.rs1_ready = true;
    i0.rs2_ready = true;
    i0.rs1_value = 10;
    i0.rs2_value = 20;

    // I1: P16 not ready, P5 ready -> WAIT
    cpu::IssueQueueEntry i1{};
    i1.pc = 0x1004;
    i1.physical_rd = 17;
    i1.physical_rs1 = 16;
    i1.physical_rs2 = 5;
    i1.rs1_ready = false; // Waiting on P16 from I0
    i1.rs2_ready = true;
    i1.rs2_value = 5;

    assert(queue.insert(i0));
    assert(queue.insert(i1));
    assert(queue.size() == 2);

    // Initially, only I0 is ready
    auto* selected = queue.select_ready();
    assert(selected != nullptr);
    assert(selected->pc == 0x1000);
    assert(selected->physical_rd == 16);

    // Simulate I0 execution and completion
    queue.mark_issued(selected);

    // I1 is still not ready before wakeup
    assert(queue.select_ready() == nullptr);

    // Simulate I0 result production: P16 = 42
    queue.wakeup(16, 42);

    // Now I1 becomes ready
    auto* next_selected = queue.select_ready();
    assert(next_selected != nullptr);
    assert(next_selected->pc == 0x1004);
    assert(next_selected->physical_rd == 17);
    assert(next_selected->rs1_ready == true);
    assert(next_selected->rs1_value == 42);
    assert(next_selected->rs2_ready == true);

    std::cout << "  [PASS] Section 3: Two instructions with RAW dependency resolve out-of-order via wakeup.\n";
}

// ===========================================================================
// 4. Issue Queue Capacity Tests
// ===========================================================================

void test_iq_capacity()
{
    cpu::IssueQueue<4> queue;
    queue.reset();

    cpu::IssueQueueEntry e1{}; e1.physical_rd = 1;
    cpu::IssueQueueEntry e2{}; e2.physical_rd = 2;
    cpu::IssueQueueEntry e3{}; e3.physical_rd = 3;
    cpu::IssueQueueEntry e4{}; e4.physical_rd = 4;
    cpu::IssueQueueEntry e5{}; e5.physical_rd = 5;

    assert(queue.insert(e1));
    assert(queue.insert(e2));
    assert(queue.insert(e3));
    assert(queue.insert(e4));

    // Queue is full
    assert(queue.size() == 4);
    assert(queue.full());

    // Fifth insert must fail
    assert(!queue.insert(e5));

    // Remove one entry by index
    queue.remove(1);
    assert(queue.size() == 3);
    assert(!queue.full());

    // Insertion must work again
    assert(queue.insert(e5));
    assert(queue.size() == 4);
    assert(queue.full());

    std::cout << "  [PASS] Section 4: IssueQueue capacity enforcement, rejection on full, and removal.\n";
}

// ===========================================================================
// 5. ROB Tests
// ===========================================================================

void test_rob_allocate()
{
    // Test 11 — Allocate
    cpu::ReOrderBuffer<16> rob;
    rob.reset();

    cpu::RenameBundle bundle{};
    bundle.valid = true;
    bundle.pc = 0x1000;
    bundle.instruction = 0x00210093; // ADDI
    bundle.physical_rd = 16;
    bundle.register_write = true;

    std::size_t index = 999;
    assert(rob.allocate(bundle, index));
    assert(index == 0);
    assert(rob.size() == 1);

    const auto* entry = rob.entry(index);
    assert(entry != nullptr);
    assert(entry->valid == true);
    assert(entry->completed == false);
    assert(entry->physical_rd == 16);

    std::cout << "  [PASS] Test 11: ROB allocate creates valid, incomplete entry.\n";
}

void test_rob_cannot_commit_incomplete()
{
    // Test 12 — Can't commit incomplete instruction
    cpu::ReOrderBuffer<16> rob;
    rob.reset();

    cpu::RenameBundle bundle{};
    bundle.valid = true;
    bundle.physical_rd = 16;

    std::size_t index = 0;
    assert(rob.allocate(bundle, index));

    cpu::ROBEntry committed{};
    assert(!rob.commit(committed));

    std::cout << "  [PASS] Test 12: Cannot commit incomplete instruction.\n";
}

void test_rob_complete_then_commit()
{
    // Test 13 — Complete then commit
    cpu::ReOrderBuffer<16> rob;
    rob.reset();

    cpu::RenameBundle bundle{};
    bundle.valid = true;
    bundle.physical_rd = 16;

    std::size_t index = 0;
    assert(rob.allocate(bundle, index));

    rob.complete(index, 123);

    cpu::ROBEntry committed{};
    assert(rob.commit(committed));

    assert(committed.result == 123);
    assert(committed.completed == true);
    assert(committed.physical_rd == 16);
    assert(rob.size() == 0);

    std::cout << "  [PASS] Test 13: Complete then commit commits result and frees ROB entry.\n";
}

void test_rob_in_order_retirement()
{
    // Test 14 — In-order retirement
    cpu::ReOrderBuffer<16> rob;
    rob.reset();

    cpu::RenameBundle b0{}; b0.valid = true; b0.pc = 0x1000; b0.physical_rd = 16;
    cpu::RenameBundle b1{}; b1.valid = true; b1.pc = 0x1004; b1.physical_rd = 17;
    cpu::RenameBundle b2{}; b2.valid = true; b2.pc = 0x1008; b2.physical_rd = 18;

    std::size_t i0 = 0, i1 = 0, i2 = 0;
    assert(rob.allocate(b0, i0));
    assert(rob.allocate(b1, i1));
    assert(rob.allocate(b2, i2));

    // Complete I1 and I2, but not I0
    rob.complete(i1, 200);
    rob.complete(i2, 300);

    cpu::ROBEntry committed{};
    // Commit must fail because I0 at head is incomplete
    assert(!rob.commit(committed));

    // Now complete I0
    rob.complete(i0, 100);

    // Commits must occur in exact program order: I0, then I1, then I2
    assert(rob.commit(committed));
    assert(committed.pc == 0x1000);
    assert(committed.physical_rd == 16);
    assert(committed.result == 100);

    assert(rob.commit(committed));
    assert(committed.pc == 0x1004);
    assert(committed.physical_rd == 17);
    assert(committed.result == 200);

    assert(rob.commit(committed));
    assert(committed.pc == 0x1008);
    assert(committed.physical_rd == 18);
    assert(committed.result == 300);

    assert(rob.empty());
    assert(!rob.commit(committed));

    std::cout << "  [PASS] Test 14: In-order retirement strictly maintained despite out-of-order completion.\n";
}

// ===========================================================================
// 6. ROB Circular-Buffer Tests
// ===========================================================================

void test_rob_circular_buffer()
{
    cpu::ReOrderBuffer<4> rob;
    rob.reset();

    cpu::RenameBundle b0{}; b0.valid = true; b0.pc = 0x1000; b0.physical_rd = 16;
    cpu::RenameBundle b1{}; b1.valid = true; b1.pc = 0x1004; b1.physical_rd = 17;
    cpu::RenameBundle b2{}; b2.valid = true; b2.pc = 0x1008; b2.physical_rd = 18;
    cpu::RenameBundle b3{}; b3.valid = true; b3.pc = 0x100C; b3.physical_rd = 19;

    std::size_t i0 = 0, i1 = 0, i2 = 0, i3 = 0;
    assert(rob.allocate(b0, i0));
    assert(rob.allocate(b1, i1));
    assert(rob.allocate(b2, i2));
    assert(rob.allocate(b3, i3));

    assert(i0 == 0 && i1 == 1 && i2 == 2 && i3 == 3);
    assert(rob.full());

    // Head is at 0, tail is wrapped to 0 (full)
    assert(rob.head_index() == 0);

    // Complete and commit I0 and I1
    rob.complete(i0, 10);
    rob.complete(i1, 20);

    cpu::ROBEntry committed{};
    assert(rob.commit(committed));
    assert(committed.pc == 0x1000);
    assert(rob.commit(committed));
    assert(committed.pc == 0x1004);

    // Now head is at index 2, size is 2
    assert(rob.head_index() == 2);
    assert(rob.size() == 2);
    assert(!rob.full());

    // Allocate I4 and I5
    cpu::RenameBundle b4{}; b4.valid = true; b4.pc = 0x1010; b4.physical_rd = 20;
    cpu::RenameBundle b5{}; b5.valid = true; b5.pc = 0x1014; b5.physical_rd = 21;

    std::size_t i4 = 0, i5 = 0;
    assert(rob.allocate(b4, i4));
    assert(rob.allocate(b5, i5));

    // Storage physically wrapped:
    // slots 0: I4, 1: I5, 2: I2, 3: I3
    assert(i4 == 0);
    assert(i5 == 1);
    assert(rob.full());

    assert(rob.raw_entry(0).pc == 0x1010);
    assert(rob.raw_entry(1).pc == 0x1014);
    assert(rob.raw_entry(2).pc == 0x1008);
    assert(rob.raw_entry(3).pc == 0x100C);

    // Commit remaining instructions in circular FIFO order: I2, I3, I4, I5
    rob.complete(i2, 30);
    rob.complete(i3, 40);
    rob.complete(i4, 50);
    rob.complete(i5, 60);

    assert(rob.commit(committed));
    assert(committed.pc == 0x1008);
    assert(rob.commit(committed));
    assert(committed.pc == 0x100C);
    assert(rob.commit(committed));
    assert(committed.pc == 0x1010);
    assert(rob.commit(committed));
    assert(committed.pc == 0x1014);

    assert(rob.empty());

    std::cout << "  [PASS] Section 6: Circular buffer storage wrapping verified successfully.\n";
}

// ===========================================================================
// 7. Full Subsystem Integration Test
// ===========================================================================

void test_full_subsystem_integration()
{
    // Entire pipeline chain:
    // DecodeBundle -> RenameUnit -> RenameBundle -> DispatchUnit -> ROB & IssueQueue -> Physical RF

    // Architectural registers: 64 physical registers, 16 initially mapped (R0..R15 -> P0..P15)
    cpu::RegisterAliasTable rat(64);
    cpu::PhysicalRegisterFreeList free_list(64, 16);
    cpu::RenameUnit rename_unit(rat, free_list);

    // Physical Register File (64 registers)
    cpu::PhysicalRegisterFile<64> rf;
    rf.reset();

    // Architectural base registers initialized as ready
    for (std::uint8_t p = 0; p < 16; ++p)
    {
        rf.write(p, p * 10); // P2 = 20, P3 = 30, P5 = 50, etc.
    }
    assert(rf.ready(2));
    assert(rf.ready(3));
    assert(rf.ready(5));

    // ROB and Issue Queue
    cpu::ReOrderBuffer<16> rob;
    cpu::IssueQueue<16> queue;
    cpu::DispatchUnit<16, 16, 64> dispatch_unit(rob, queue, rf);

    // 1. Decode bundles:
    // I0: ADD R1, R2, R3
    // I1: SUB R4, R1, R5
    auto d0 = make_decode_bundle(true, cpu::Opcode::ADD, cpu::Register::R1, cpu::Register::R2, cpu::Register::R3, true, 0x1000);
    auto d1 = make_decode_bundle(true, cpu::Opcode::SUB, cpu::Register::R4, cpu::Register::R1, cpu::Register::R5, true, 0x1004);

    // 2. Rename stage: 2-wide rename
    cpu::RenameBundle r0, r1;
    rename_unit.rename(d0, d1, r0, r1);

    // Verify Rename:
    // I0 gets P16
    assert(r0.valid);
    assert(r0.physical_rs1 == 2); // R2 -> P2
    assert(r0.physical_rs2 == 3); // R3 -> P3
    assert(r0.physical_rd == 16);

    // RAT: R1 -> P16
    assert(rat.lookup(cpu::Register::R1) == 16);

    // I1 sees R1 -> P16 from Lane 0 RAT update!
    assert(r1.valid);
    assert(r1.physical_rs1 == 16); // R1 -> P16
    assert(r1.physical_rs2 == 5);  // R5 -> P5
    assert(r1.physical_rd == 17);  // R4 gets P17
    assert(rat.lookup(cpu::Register::R4) == 17);

    // 3. Dispatch stage: dispatch both into ROB and IssueQueue
    std::size_t rob_idx0 = 0, rob_idx1 = 0;
    bool dispatched = dispatch_unit.dispatch(r0, r1, rob_idx0, rob_idx1);
    assert(dispatched);

    // Verify ROB allocation
    assert(rob.size() == 2);
    assert(rob.entry(rob_idx0)->physical_rd == 16);
    assert(rob.entry(rob_idx1)->physical_rd == 17);

    // Verify Physical RF: P16 and P17 are allocated and NOT ready
    assert(!rf.ready(16));
    assert(!rf.ready(17));

    // Verify Issue Queue:
    assert(queue.size() == 2);

    // Issue Queue selection:
    // I0 has P2 (ready) and P3 (ready) -> READY
    // I1 has P16 (NOT ready) and P5 (ready) -> WAITING
    auto* selected = queue.select_ready();
    assert(selected != nullptr);
    assert(selected->pc == 0x1000);
    assert(selected->physical_rd == 16);
    assert(selected->rs1_ready == true);
    assert(selected->rs2_ready == true);

    // 4. Execute I0:
    // I0 issues and computes 20 + 30 = 50
    queue.mark_issued(selected);

    // Before wakeup, queue cannot select I1
    assert(queue.select_ready() == nullptr);

    // I0 produces P16 = 50:
    // - Physical RF receives P16 = 50 and becomes ready
    // - ROB records I0 completion
    // - Issue Queue wakes up P16
    rf.write(16, 50);
    assert(rf.ready(16));
    assert(rf.read(16) == 50);

    rob.complete(rob_idx0, 50);

    queue.wakeup(16, 50);

    // 5. I1 now becomes ready!
    auto* ready_i1 = queue.select_ready();
    assert(ready_i1 != nullptr);
    assert(ready_i1->pc == 0x1004);
    assert(ready_i1->physical_rd == 17);
    assert(ready_i1->rs1_ready == true);
    assert(ready_i1->rs1_value == 50);
    assert(ready_i1->rs2_ready == true);
    assert(ready_i1->rs2_value == 50); // P5 = 50

    // I1 issues and computes 50 - 50 = 0
    queue.mark_issued(ready_i1);
    rf.write(17, 0);
    rob.complete(rob_idx1, 0);

    // 6. ROB In-order commit
    cpu::ROBEntry committed;
    assert(rob.commit(committed));
    assert(committed.pc == 0x1000);
    assert(committed.physical_rd == 16);
    assert(committed.result == 50);

    assert(rob.commit(committed));
    assert(committed.pc == 0x1004);
    assert(committed.physical_rd == 17);
    assert(committed.result == 0);

    assert(rob.empty());

    std::cout << "  [PASS] Section 7: Full subsystem integration test (Decode->Rename->Dispatch->ROB/IQ/PRF->Wakeup->Retire) passed!\n";
}

} // namespace

int main()
{
    std::cout << "===================================================================\n";
    std::cout << "--- Superscalar Issue Queue, PRF, ROB & Subsystem Unit Tests    ---\n";
    std::cout << "===================================================================\n\n";

    std::cout << "[1] PhysicalRegisterFile unit tests:\n";
    test_prf_reset_state();
    test_prf_allocate_makes_register_unavailable();
    test_prf_write_makes_register_ready();
    test_prf_overwrite_existing_value();
    test_prf_allocate_resets_readiness();
    test_prf_invalid_physical_register();
    test_prf_boundary_registers();

    std::cout << "\n[2] Issue Queue tests:\n";
    test_iq_insert();
    test_iq_waiting_instruction_not_selected();
    test_iq_wakeup_source2();

    std::cout << "\n[3] Dependency Resolution (Wakeup) test:\n";
    test_two_instructions_with_dependency();

    std::cout << "\n[4] Issue Queue capacity tests:\n";
    test_iq_capacity();

    std::cout << "\n[5] ROB tests:\n";
    test_rob_allocate();
    test_rob_cannot_commit_incomplete();
    test_rob_complete_then_commit();
    test_rob_in_order_retirement();

    std::cout << "\n[6] ROB circular-buffer test:\n";
    test_rob_circular_buffer();

    std::cout << "\n[7] Full subsystem integration test:\n";
    test_full_subsystem_integration();

    std::cout << "\n===================================================================\n";
    std::cout << "[PASS] All 7 test suites (15 tests total) completed successfully!\n";
    std::cout << "===================================================================\n";

    return 0;
}

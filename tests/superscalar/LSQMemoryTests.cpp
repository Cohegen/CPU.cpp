#include <iostream>
#include <cassert>
#include <cstdint>
#include <vector>

// Phase 2 components
#include "../../superscalar/core/memory/LoadStoreQueue.hpp"
#include "../../superscalar/core/memory/data/DataMemory.hpp"
#include "../../superscalar/core/execution/MemoryExecutionUnit.hpp"

// Existing pipeline components
#include "../../superscalar/core/dispatch/ReOrderBuffer.hpp"
#include "../../superscalar/core/issue/IssueQueue.hpp"
#include "../../superscalar/core/issue/PhysicalRegisterFile.hpp"
#include "../../superscalar/core/rename/PhysicalRegisterFreeList.hpp"
#include "../../superscalar/core/rename/RegisterAliasTable.hpp"
#include "../../superscalar/core/commit/StoreBuffer.hpp"

using namespace cpu;

// ============================================================
// Test 1: Load from Data Memory (no forwarding)
// ============================================================
void test_load_from_memory()
{
    std::cout << "[Test 1] Testing Load from Data Memory (no forwarding)..." << std::endl;

    constexpr std::size_t ROB_CAP = 16;
    constexpr std::size_t IQ_CAP = 16;
    constexpr std::size_t PRF_COUNT = 64;
    constexpr std::size_t LSQ_CAP = 32;
    constexpr std::size_t SB_CAP = 16;
    constexpr std::size_t DM_SIZE = 256;

    ReOrderBuffer<ROB_CAP> rob;
    IssueQueue<IQ_CAP> iq;
    PhysicalRegisterFile<PRF_COUNT> prf;
    LoadStoreQueue<LSQ_CAP> lsq;
    StoreBuffer<SB_CAP> sb;
    DataMemory<DM_SIZE> dmem;

    // Pre-populate data memory: word at byte address 0x10 = 0xDEADBEEF
    dmem.write(0x10, 0xDEADBEEF);

    // Allocate a load instruction in the ROB
    RenameBundle load_bundle{};
    load_bundle.valid = true;
    load_bundle.pc = 0x100;
    load_bundle.opcode = Opcode::LW;
    load_bundle.register_write = true;
    load_bundle.rd = Register::R1;
    load_bundle.physical_rd = 20;
    load_bundle.old_physical_rd = 1;
    load_bundle.memory_read = true;

    ReOrderBuffer<ROB_CAP>::Index rob_idx = 0;
    assert(rob.allocate(load_bundle, rob_idx));

    // Allocate in LSQ
    assert(lsq.allocate(LoadStoreQueue<LSQ_CAP>::OpType::LOAD, rob_idx, 0x100, 20));

    // Insert into issue queue
    IssueQueueEntry iq_entry{};
    iq_entry.valid = true;
    iq_entry.pc = 0x100;
    iq_entry.opcode = Opcode::LW;
    iq_entry.physical_rd = 20;
    iq_entry.physical_rs1 = 0;
    iq_entry.physical_rs2 = 0;
    iq_entry.rs1_ready = true;
    iq_entry.rs2_ready = true;
    iq_entry.rs1_value = 0x10;  // Base address
    iq_entry.immediate = 0;      // Offset
    iq_entry.memory_read = true;
    iq_entry.rob_index = rob_idx;
    assert(iq.insert(iq_entry));

    // Mark rs1 ready in PRF
    prf.write(0, 0x10);

    // Execute
    MemoryExecutionUnit<ROB_CAP, IQ_CAP, PRF_COUNT, LSQ_CAP, SB_CAP, DM_SIZE>
        mem_unit(rob, iq, prf, lsq, sb, dmem);

    auto results = mem_unit.execute();
    assert(results.size() == 1);
    assert(results[0].valid);
    assert(results[0].is_load);
    assert(results[0].data == 0xDEADBEEF);
    assert(results[0].address == 0x10);

    // Verify PRF was updated
    assert(prf.read(20) == 0xDEADBEEF);
    assert(prf.ready(20));

    // Verify ROB entry is completed
    const auto* rob_entry = rob.entry(rob_idx);
    assert(rob_entry->completed);
    assert(rob_entry->result == 0xDEADBEEF);

    // Verify IQ is empty (entry was consumed)
    assert(iq.empty());

    std::cout << "  [PASS] Load from data memory works correctly." << std::endl;
}

// ============================================================
// Test 2: Store-to-Load Forwarding
// ============================================================
void test_store_to_load_forwarding()
{
    std::cout << "[Test 2] Testing Store-to-Load Forwarding..." << std::endl;

    constexpr std::size_t ROB_CAP = 16;
    constexpr std::size_t IQ_CAP = 16;
    constexpr std::size_t PRF_COUNT = 64;
    constexpr std::size_t LSQ_CAP = 32;
    constexpr std::size_t SB_CAP = 16;
    constexpr std::size_t DM_SIZE = 256;

    ReOrderBuffer<ROB_CAP> rob;
    IssueQueue<IQ_CAP> iq;
    PhysicalRegisterFile<PRF_COUNT> prf;
    LoadStoreQueue<LSQ_CAP> lsq;
    StoreBuffer<SB_CAP> sb;
    DataMemory<DM_SIZE> dmem;

    // Data memory has old value at address 0x20
    dmem.write(0x20, 0x11111111);

    // --- Store instruction (older, ROB index 0) ---
    RenameBundle store_bundle{};
    store_bundle.valid = true;
    store_bundle.pc = 0x200;
    store_bundle.opcode = Opcode::SW;
    store_bundle.memory_write = true;

    ReOrderBuffer<ROB_CAP>::Index store_rob_idx = 0;
    assert(rob.allocate(store_bundle, store_rob_idx));

    // Allocate store in LSQ and StoreBuffer
    assert(lsq.allocate(LoadStoreQueue<LSQ_CAP>::OpType::STORE, store_rob_idx, 0x200));
    assert(sb.allocate(store_rob_idx, 0x200));

    // Simulate store execution: address = 0x20, data = 0xCAFEBABE
    lsq.set_address(store_rob_idx, 0x20);
    lsq.set_store_data(store_rob_idx, 0xCAFEBABE);
    lsq.mark_executed(store_rob_idx);
    sb.update(store_rob_idx, 0x20, 0xCAFEBABE);
    rob.complete(store_rob_idx, 0);

    // --- Load instruction (younger, ROB index 1) ---
    RenameBundle load_bundle{};
    load_bundle.valid = true;
    load_bundle.pc = 0x204;
    load_bundle.opcode = Opcode::LW;
    load_bundle.register_write = true;
    load_bundle.rd = Register::R2;
    load_bundle.physical_rd = 25;
    load_bundle.old_physical_rd = 2;
    load_bundle.memory_read = true;

    ReOrderBuffer<ROB_CAP>::Index load_rob_idx = 0;
    assert(rob.allocate(load_bundle, load_rob_idx));

    // Allocate load in LSQ
    assert(lsq.allocate(LoadStoreQueue<LSQ_CAP>::OpType::LOAD, load_rob_idx, 0x204, 25));

    // Insert load into issue queue
    IssueQueueEntry iq_entry{};
    iq_entry.valid = true;
    iq_entry.pc = 0x204;
    iq_entry.opcode = Opcode::LW;
    iq_entry.physical_rd = 25;
    iq_entry.physical_rs1 = 0;
    iq_entry.physical_rs2 = 0;
    iq_entry.rs1_ready = true;
    iq_entry.rs2_ready = true;
    iq_entry.rs1_value = 0x20;  // Same address as the store
    iq_entry.immediate = 0;
    iq_entry.memory_read = true;
    iq_entry.rob_index = load_rob_idx;
    assert(iq.insert(iq_entry));

    // Execute load
    MemoryExecutionUnit<ROB_CAP, IQ_CAP, PRF_COUNT, LSQ_CAP, SB_CAP, DM_SIZE>
        mem_unit(rob, iq, prf, lsq, sb, dmem);

    auto results = mem_unit.execute();
    assert(results.size() == 1);
    assert(results[0].valid);
    assert(results[0].is_load);

    // KEY ASSERTION: Load should get forwarded value 0xCAFEBABE, NOT the
    // old memory value 0x11111111
    assert(results[0].data == 0xCAFEBABE);

    // Verify PRF was updated with forwarded value
    assert(prf.read(25) == 0xCAFEBABE);

    std::cout << "  [PASS] Store-to-Load forwarding works correctly." << std::endl;
}

// ============================================================
// Test 3: Memory Order Violation Detection
// ============================================================
void test_memory_order_violation()
{
    std::cout << "[Test 3] Testing Memory Order Violation Detection..." << std::endl;

    constexpr std::size_t LSQ_CAP = 32;

    LoadStoreQueue<LSQ_CAP> lsq;

    // Allocate a store (older, ROB index 0)
    assert(lsq.allocate(LoadStoreQueue<LSQ_CAP>::OpType::STORE, 0, 0x300));

    // Allocate a load (younger, ROB index 1)
    assert(lsq.allocate(LoadStoreQueue<LSQ_CAP>::OpType::LOAD, 1, 0x304, 30));

    // Load executes FIRST (out of order) — address 0x40
    lsq.set_address(1, 0x40);
    lsq.complete_load(1, 0x99);
    lsq.mark_executed(1);

    // Store resolves its address LATER — same address 0x40!
    lsq.set_address(0, 0x40);
    lsq.set_store_data(0, 0xAA);

    // Check for violation
    auto violation = lsq.check_order_violation(0);
    assert(violation.violation);
    assert(violation.violating_load_rob_index == 1);

    std::cout << "  [PASS] Memory order violation detected correctly." << std::endl;
}

// ============================================================
// Test 4: No Violation When Addresses Differ
// ============================================================
void test_no_violation_different_addresses()
{
    std::cout << "[Test 4] Testing No Violation When Addresses Differ..." << std::endl;

    constexpr std::size_t LSQ_CAP = 32;

    LoadStoreQueue<LSQ_CAP> lsq;

    // Store to address 0x40, load from address 0x80 — no conflict
    assert(lsq.allocate(LoadStoreQueue<LSQ_CAP>::OpType::STORE, 0, 0x400));
    assert(lsq.allocate(LoadStoreQueue<LSQ_CAP>::OpType::LOAD, 1, 0x404, 31));

    lsq.set_address(1, 0x80);
    lsq.complete_load(1, 0x55);
    lsq.mark_executed(1);

    lsq.set_address(0, 0x40);
    lsq.set_store_data(0, 0x66);

    auto violation = lsq.check_order_violation(0);
    assert(!violation.violation);

    std::cout << "  [PASS] No violation when addresses differ." << std::endl;
}

// ============================================================
// Test 5: Store Execution and ROB Completion
// ============================================================
void test_store_execution()
{
    std::cout << "[Test 5] Testing Store Execution and ROB Completion..." << std::endl;

    constexpr std::size_t ROB_CAP = 16;
    constexpr std::size_t IQ_CAP = 16;
    constexpr std::size_t PRF_COUNT = 64;
    constexpr std::size_t LSQ_CAP = 32;
    constexpr std::size_t SB_CAP = 16;
    constexpr std::size_t DM_SIZE = 256;

    ReOrderBuffer<ROB_CAP> rob;
    IssueQueue<IQ_CAP> iq;
    PhysicalRegisterFile<PRF_COUNT> prf;
    LoadStoreQueue<LSQ_CAP> lsq;
    StoreBuffer<SB_CAP> sb;
    DataMemory<DM_SIZE> dmem;

    // Allocate store in ROB
    RenameBundle store_bundle{};
    store_bundle.valid = true;
    store_bundle.pc = 0x500;
    store_bundle.opcode = Opcode::SW;
    store_bundle.memory_write = true;

    ReOrderBuffer<ROB_CAP>::Index rob_idx = 0;
    assert(rob.allocate(store_bundle, rob_idx));

    // Allocate in LSQ and StoreBuffer
    assert(lsq.allocate(LoadStoreQueue<LSQ_CAP>::OpType::STORE, rob_idx, 0x500));
    assert(sb.allocate(rob_idx, 0x500));

    // Insert store into issue queue
    IssueQueueEntry iq_entry{};
    iq_entry.valid = true;
    iq_entry.pc = 0x500;
    iq_entry.opcode = Opcode::SW;
    iq_entry.physical_rs1 = 5;
    iq_entry.physical_rs2 = 6;
    iq_entry.rs1_ready = true;
    iq_entry.rs2_ready = true;
    iq_entry.rs1_value = 0x30;    // Base address
    iq_entry.rs2_value = 0xBEEF;  // Store data
    iq_entry.immediate = 0;
    iq_entry.memory_write = true;
    iq_entry.rob_index = rob_idx;
    assert(iq.insert(iq_entry));

    // Execute
    MemoryExecutionUnit<ROB_CAP, IQ_CAP, PRF_COUNT, LSQ_CAP, SB_CAP, DM_SIZE>
        mem_unit(rob, iq, prf, lsq, sb, dmem);

    auto results = mem_unit.execute();
    assert(results.size() == 1);
    assert(results[0].valid);
    assert(results[0].is_store);
    assert(results[0].address == 0x30);
    assert(results[0].data == 0xBEEF);

    // Verify ROB entry is completed (stores complete in ROB immediately)
    const auto* rob_entry = rob.entry(rob_idx);
    assert(rob_entry->completed);

    // Verify LSQ entry was updated with address and data
    const auto* lsq_entry = lsq.find(rob_idx);
    assert(lsq_entry != nullptr);
    assert(lsq_entry->address_valid);
    assert(lsq_entry->address == 0x30);
    assert(lsq_entry->data_valid);
    assert(lsq_entry->data == 0xBEEF);
    assert(lsq_entry->executed);

    // Verify StoreBuffer was updated
    const auto& sb_entries = sb.entries();
    bool found = false;
    for (const auto& e : sb_entries)
    {
        if (e.valid && e.rob_index == rob_idx)
        {
            assert(e.address == 0x30);
            assert(e.data == 0xBEEF);
            assert(e.address_ready);
            assert(e.data_ready);
            found = true;
        }
    }
    assert(found);

    // Data memory should NOT be written yet (store is speculative)
    assert(dmem.read(0x30).value_or(0) == 0);

    std::cout << "  [PASS] Store executes correctly: LSQ/SB updated, memory untouched." << std::endl;
}

// ============================================================
// Test 6: LSQ Squash on Misprediction
// ============================================================
void test_lsq_squash()
{
    std::cout << "[Test 6] Testing LSQ Squash on Misprediction..." << std::endl;

    constexpr std::size_t LSQ_CAP = 32;

    LoadStoreQueue<LSQ_CAP> lsq;

    // Allocate several entries
    assert(lsq.allocate(LoadStoreQueue<LSQ_CAP>::OpType::STORE, 0, 0x600));
    assert(lsq.allocate(LoadStoreQueue<LSQ_CAP>::OpType::LOAD,  1, 0x604, 10));
    assert(lsq.allocate(LoadStoreQueue<LSQ_CAP>::OpType::STORE, 2, 0x608));
    assert(lsq.allocate(LoadStoreQueue<LSQ_CAP>::OpType::LOAD,  3, 0x60C, 11));
    assert(lsq.size() == 4);

    // Squash ROB entries 2 and 3 (younger than a mispredicted branch at ROB 1)
    std::vector<std::size_t> squashed = {2, 3};
    lsq.squash(squashed);

    assert(lsq.size() == 2);
    assert(lsq.find(0) != nullptr);
    assert(lsq.find(1) != nullptr);
    assert(lsq.find(2) == nullptr);
    assert(lsq.find(3) == nullptr);

    std::cout << "  [PASS] LSQ squash removes correct entries." << std::endl;
}

// ============================================================
// Test 7: DataMemory Basic Read/Write
// ============================================================
void test_data_memory()
{
    std::cout << "[Test 7] Testing DataMemory Basic Read/Write..." << std::endl;

    DataMemory<256> dmem;

    // Write and read back by byte address
    assert(dmem.write(0x00, 100));
    assert(dmem.write(0x04, 200));
    assert(dmem.write(0x08, 300));

    assert(dmem.read(0x00).value() == 100);
    assert(dmem.read(0x04).value() == 200);
    assert(dmem.read(0x08).value() == 300);

    // Word-index access
    assert(dmem.read_word(0) == 100);
    assert(dmem.read_word(1) == 200);
    assert(dmem.read_word(2) == 300);

    // Out of bounds
    assert(!dmem.read(256 * 4).has_value());
    assert(!dmem.write(256 * 4, 999));

    // Reset
    dmem.reset();
    assert(dmem.read(0x00).value() == 0);

    std::cout << "  [PASS] DataMemory read/write works correctly." << std::endl;
}

// ============================================================
// Test 8: End-to-End Store-then-Load-then-Commit
// ============================================================
void test_store_commit_to_memory()
{
    std::cout << "[Test 8] Testing Store Commit to Data Memory..." << std::endl;

    constexpr std::size_t SB_CAP = 16;
    constexpr std::size_t DM_SIZE = 256;

    StoreBuffer<SB_CAP> sb;
    DataMemory<DM_SIZE> dmem;

    // Allocate and execute a store
    assert(sb.allocate(0, 0x700));
    sb.update(0, 0x40, 0xFEEDFACE);

    // Data memory is untouched before commit
    assert(dmem.read(0x40).value_or(0) == 0);

    // Mark committed (as if ROB head retired this store)
    sb.mark_committed(0);

    // Pop committed store and write to memory
    auto committed = sb.pop_committed();
    assert(committed.has_value());
    assert(committed->address == 0x40);
    assert(committed->data == 0xFEEDFACE);

    dmem.write(committed->address, committed->data);

    // Now data memory has the value
    assert(dmem.read(0x40).value() == 0xFEEDFACE);

    std::cout << "  [PASS] Store committed to data memory correctly." << std::endl;
}

// ============================================================
// Main
// ============================================================
int main()
{
    std::cout << "========================================================" << std::endl;
    std::cout << "Starting Phase 2: LSQ & Memory Disambiguation Tests" << std::endl;
    std::cout << "========================================================" << std::endl;

    test_load_from_memory();
    test_store_to_load_forwarding();
    test_memory_order_violation();
    test_no_violation_different_addresses();
    test_store_execution();
    test_lsq_squash();
    test_data_memory();
    test_store_commit_to_memory();

    std::cout << "========================================================" << std::endl;
    std::cout << "ALL Phase 2 LSQ & Memory Tests PASSED!" << std::endl;
    std::cout << "========================================================" << std::endl;

    return 0;
}

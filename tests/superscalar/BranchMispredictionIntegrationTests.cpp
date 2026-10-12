#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

// Core components
#include "../../superscalar/core/branch/BranchPredictor.hpp"
#include "../../superscalar/core/branch/BranchPredictionUnit.hpp"
#include "../../superscalar/core/branch/BranchExecutionUnit.hpp"
#include "../../superscalar/core/branch/BranchRecoveryUnit.hpp"
#include "../../superscalar/core/branch/BranchResult.hpp"

#include "../../superscalar/core/fetch/FetchUnit.hpp"
#include "../../superscalar/core/fetch/FetchDecodeReg.hpp"
#include "../../superscalar/core/decode/DecodeUnit.hpp"
#include "../../superscalar/core/decode/DecodeBundle.hpp"
#include "../../superscalar/core/rename/RegisterAliasTable.hpp"
#include "../../superscalar/core/rename/PhysicalRegisterFreeList.hpp"
#include "../../superscalar/core/rename/RenameUnit.hpp"
#include "../../superscalar/core/rename/RenameBundle.hpp"
#include "../../superscalar/core/dispatch/ReOrderBuffer.hpp"
#include "../../superscalar/core/dispatch/DispatchUnit.hpp"
#include "../../superscalar/core/issue/IssueQueue.hpp"
#include "../../superscalar/core/issue/IssueUnit.hpp"
#include "../../superscalar/core/issue/PhysicalRegisterFile.hpp"
#include "../../superscalar/core/execution/ExecutionRequest.hpp"

#include "../../include/isa/Opcode.hpp"
#include "../../include/isa/Registers.hpp"

// ============================================================================
// Test 1: Prediction preservation
// The prediction made during fetch reaches the branch's execution request unchanged.
// ============================================================================
void test_prediction_preservation()
{
    std::cout << "[Test 1] Testing Prediction Preservation...\n";

    cpu::BranchPredictor<256> predictor{};
    const std::uint32_t branch_pc = 0x1000;
    const std::uint32_t branch_target = 0x1020;

    // Train predictor so it predicts taken with branch_target
    predictor.update(branch_pc, /*actual_taken=*/true, branch_target);
    predictor.update(branch_pc, /*actual_taken=*/true, branch_target);

    cpu::BranchPredictionUnit<256> bpu{predictor};
    const auto bpu_res = bpu.evaluate(
        branch_pc, /*valid0=*/true, cpu::ControlFlow::BRANCH,
        branch_pc + 4, /*valid1=*/false, cpu::ControlFlow::NONE
    );

    assert(bpu_res.lane0.taken == true);
    assert(bpu_res.lane0.target == branch_target);

    // Form DecodeBundle
    cpu::DecodeBundle decode_bundle{};
    decode_bundle.valid = true;
    decode_bundle.pc = branch_pc;
    decode_bundle.opcode = cpu::Opcode::BEQ;
    decode_bundle.control_flow = cpu::ControlFlow::BRANCH;
    decode_bundle.prediction.valid = true;
    decode_bundle.prediction.predicted_taken = bpu_res.lane0.taken;
    decode_bundle.prediction.predicted_target = bpu_res.lane0.target;

    // Pass through Rename
    cpu::RegisterAliasTable rat{64};
    cpu::PhysicalRegisterFreeList free_list{64, 16};
    cpu::PhysicalRegisterFile<64> prf{};
    cpu::RenameUnit<64> rename_unit{rat, free_list, prf};

    cpu::RenameBundle rename_bundle = rename_unit.rename(decode_bundle);
    assert(rename_bundle.valid);
    assert(rename_bundle.prediction.valid == true);
    assert(rename_bundle.prediction.predicted_taken == true);
    assert(rename_bundle.prediction.predicted_target == branch_target);

    // Pass through Dispatch
    cpu::ReOrderBuffer<16> rob{};
    cpu::IssueQueue<16> iq{};
    cpu::DispatchUnit<16, 16, 64> dispatch_unit{rob, iq, prf};

    const auto rat_checkpoint = rat.checkpoint();
    std::size_t dispatched_rob_idx = 0;
    const bool dispatched = dispatch_unit.dispatch(rename_bundle, dispatched_rob_idx, &rat_checkpoint);
    assert(dispatched);
    assert(iq.size() == 1);

    // Check IssueQueueEntry
    const auto* iq_entry = iq.find_rob(dispatched_rob_idx);
    assert(iq_entry != nullptr);
    assert(iq_entry->predicted_taken == true);
    assert(iq_entry->predicted_target == branch_target);

    // Issue to ExecutionRequest
    cpu::IssueUnit issue_unit{1};
    std::vector<cpu::ExecutionRequest> requests = issue_unit.issue(iq);
    assert(requests.size() == 1);
    assert(requests[0].valid);
    assert(requests[0].predicted_taken == true);
    assert(requests[0].predicted_target == branch_target);

    std::cout << "  [PASS] Prediction preservation verified.\n";
}

// ============================================================================
// Test 2: Correct prediction
// When the branch resolves correctly, no recovery occurs.
// ============================================================================
void test_correct_prediction()
{
    std::cout << "[Test 2] Testing Correct Prediction (No Recovery)...\n";

    cpu::ReOrderBuffer<16> rob{};
    cpu::RegisterAliasTable rat{64};
    cpu::PhysicalRegisterFreeList free_list{64, 16};
    cpu::IssueQueue<16> iq{};
    cpu::BranchRecoveryUnit<16, 16> bru{};

    // Allocate branch entry in ROB
    cpu::RenameBundle branch_bundle{};
    branch_bundle.valid = true;
    branch_bundle.pc = 0x2000;
    branch_bundle.opcode = cpu::Opcode::BEQ;
    branch_bundle.control_flow = cpu::ControlFlow::BRANCH;
    branch_bundle.prediction.valid = true;
    branch_bundle.prediction.predicted_taken = true;
    branch_bundle.prediction.predicted_target = 0x2040;

    const auto cp = rat.checkpoint();
    std::size_t branch_rob_idx = 0;
    assert(rob.allocate(branch_bundle, branch_rob_idx, &cp));

    // Execute branch: BEQ rs1==rs2 (equal -> taken), actual target 0x2040
    cpu::BranchExecutionUnit<32> beu{};
    const auto res = beu.evaluate(
        cpu::Opcode::BEQ,
        0x2000,
        /*rs1=*/10,
        /*rs2=*/10,
        /*imm=*/0x40,
        /*predicted_taken=*/true,
        /*predicted_target=*/0x2040,
        branch_rob_idx
    );

    assert(res.valid);
    assert(res.taken == true);
    assert(res.target == 0x2040);
    assert(res.mispredicted == false);

    // Call recovery unit
    const bool recovered = bru.recover(res, rob, rat, free_list, iq);
    assert(!recovered);
    assert(bru.control().recover == false);
    assert(bru.control().fetch_decode_flush == false);
    assert(bru.control().fetch_redirect == false);

    std::cout << "  [PASS] Correct prediction produces no recovery.\n";
}

// ============================================================================
// Test 3: Misprediction recovery
// When prediction is wrong, RAT is restored and younger ROB/IQ entries squashed.
// ============================================================================
void test_misprediction_recovery()
{
    std::cout << "[Test 3] Testing Misprediction Recovery (RAT restore, ROB & IQ squash)...\n";

    cpu::ReOrderBuffer<16> rob{};
    cpu::RegisterAliasTable rat{64};
    cpu::PhysicalRegisterFreeList free_list{64, 16};
    cpu::IssueQueue<16> iq{};
    cpu::BranchRecoveryUnit<16, 16> bru{};

    // RAT pre-branch: identity mapping
    assert(rat.lookup(cpu::Register::R1) == 1);
    assert(rat.lookup(cpu::Register::R2) == 2);

    // Checkpoint taken at branch dispatch
    const auto cp = rat.checkpoint();

    cpu::RenameBundle branch_bundle{};
    branch_bundle.valid = true;
    branch_bundle.pc = 0x3000;
    branch_bundle.opcode = cpu::Opcode::BEQ;
    branch_bundle.control_flow = cpu::ControlFlow::BRANCH;
    branch_bundle.prediction.valid = true;
    branch_bundle.prediction.predicted_taken = false; // Predicted NOT taken
    branch_bundle.prediction.predicted_target = 0;

    std::size_t branch_rob_idx = 0;
    assert(rob.allocate(branch_bundle, branch_rob_idx, &cp));

    cpu::IssueQueueEntry branch_iq{};
    branch_iq.valid = true;
    branch_iq.opcode = cpu::Opcode::BEQ;
    branch_iq.rob_index = branch_rob_idx;
    assert(iq.insert(branch_iq));

    // Speculatively rename instructions on the wrong path
    const auto p16 = free_list.allocate(); // P16
    rat.set(cpu::Register::R1, p16);

    cpu::RenameBundle add_bundle{};
    add_bundle.valid = true;
    add_bundle.pc = 0x3004;
    add_bundle.opcode = cpu::Opcode::ADD;
    add_bundle.register_write = true;
    add_bundle.physical_rd = p16;
    add_bundle.old_physical_rd = 1;

    std::size_t add_rob_idx = 0;
    assert(rob.allocate(add_bundle, add_rob_idx));

    cpu::IssueQueueEntry add_iq{};
    add_iq.valid = true;
    add_iq.opcode = cpu::Opcode::ADD;
    add_iq.rob_index = add_rob_idx;
    assert(iq.insert(add_iq));

    // Verify speculative state exists
    assert(rob.size() == 2);
    assert(iq.size() == 2);
    assert(rat.lookup(cpu::Register::R1) == p16);

    // Execute branch: rs1==rs2 (taken), misprediction!
    cpu::BranchExecutionUnit<32> beu{};
    const auto res = beu.evaluate(
        cpu::Opcode::BEQ,
        0x3000,
        /*rs1=*/5,
        /*rs2=*/5,
        /*imm=*/0x20,
        /*predicted_taken=*/false,
        /*predicted_target=*/0,
        branch_rob_idx
    );

    assert(res.valid);
    assert(res.taken == true);
    assert(res.mispredicted == true);
    assert(res.redirect_pc == 0x3020);

    // Perform recovery
    const bool recovered = bru.recover(res, rob, rat, free_list, iq);
    assert(recovered);
    assert(bru.control().recover == true);
    assert(bru.control().redirect_pc == 0x3020);

    // RAT restored to pre-branch state
    assert(rat.lookup(cpu::Register::R1) == 1);

    // ROB squashed younger entries
    assert(rob.size() == 1);
    assert(rob.entry(branch_rob_idx) != nullptr);
    assert(rob.entry(branch_rob_idx)->valid == true);
    assert(rob.entry(add_rob_idx) == nullptr || !rob.entry(add_rob_idx)->valid);

    // Issue Queue squashed younger entries
    assert(iq.size() == 1);
    assert(iq.contains_rob(branch_rob_idx));
    assert(!iq.contains_rob(add_rob_idx));

    std::cout << "  [PASS] Misprediction recovery restores RAT and squashes ROB/IQ.\n";
}

// ============================================================================
// Test 4: Physical register safety
// Registers belonging to squashed instructions are released, while registers
// still needed by surviving instructions are preserved.
// ============================================================================
void test_physical_register_safety()
{
    std::cout << "[Test 4] Testing Physical Register Safety...\n";

    cpu::ReOrderBuffer<16> rob{};
    cpu::RegisterAliasTable rat{64};
    cpu::PhysicalRegisterFreeList free_list{64, 16};
    cpu::IssueQueue<16> iq{};
    cpu::BranchRecoveryUnit<16, 16> bru{};

    // Surviving instruction: dispatched before the branch
    const auto surviving_prd = free_list.allocate(); // P16
    rat.set(cpu::Register::R3, surviving_prd);

    cpu::RenameBundle surviving_bundle{};
    surviving_bundle.valid = true;
    surviving_bundle.pc = 0x4000;
    surviving_bundle.opcode = cpu::Opcode::ADD;
    surviving_bundle.register_write = true;
    surviving_bundle.physical_rd = surviving_prd;
    surviving_bundle.old_physical_rd = 3;

    std::size_t surviving_rob_idx = 0;
    assert(rob.allocate(surviving_bundle, surviving_rob_idx));

    // Branch dispatched after surviving instruction
    const auto cp = rat.checkpoint();

    cpu::RenameBundle branch_bundle{};
    branch_bundle.valid = true;
    branch_bundle.pc = 0x4004;
    branch_bundle.opcode = cpu::Opcode::BEQ;
    branch_bundle.control_flow = cpu::ControlFlow::BRANCH;
    branch_bundle.prediction.valid = true;
    branch_bundle.prediction.predicted_taken = false;

    std::size_t branch_rob_idx = 0;
    assert(rob.allocate(branch_bundle, branch_rob_idx, &cp));

    // Squashed instructions: dispatched after branch
    const auto squashed_prd1 = free_list.allocate(); // P17
    rat.set(cpu::Register::R1, squashed_prd1);
    cpu::RenameBundle squashed_b1{};
    squashed_b1.valid = true;
    squashed_b1.pc = 0x4008;
    squashed_b1.opcode = cpu::Opcode::ADD;
    squashed_b1.register_write = true;
    squashed_b1.physical_rd = squashed_prd1;
    squashed_b1.old_physical_rd = 1;
    std::size_t squashed_rob1 = 0;
    assert(rob.allocate(squashed_b1, squashed_rob1));

    const auto squashed_prd2 = free_list.allocate(); // P18
    rat.set(cpu::Register::R2, squashed_prd2);
    cpu::RenameBundle squashed_b2{};
    squashed_b2.valid = true;
    squashed_b2.pc = 0x400C;
    squashed_b2.opcode = cpu::Opcode::ADD;
    squashed_b2.register_write = true;
    squashed_b2.physical_rd = squashed_prd2;
    squashed_b2.old_physical_rd = 2;
    std::size_t squashed_rob2 = 0;
    assert(rob.allocate(squashed_b2, squashed_rob2));

    const std::size_t free_count_before_recovery = free_list.size();

    // Trigger branch misprediction
    cpu::BranchExecutionUnit<32> beu{};
    const auto res = beu.evaluate(
        cpu::Opcode::BEQ,
        0x4004,
        /*rs1=*/1,
        /*rs2=*/1,
        /*imm=*/0x100,
        /*predicted_taken=*/false,
        /*predicted_target=*/0,
        branch_rob_idx
    );
    assert(res.mispredicted);

    bru.recover(res, rob, rat, free_list, iq);

    // Free list should have grown by exactly 2 (squashed_prd1 and squashed_prd2)
    assert(free_list.size() == free_count_before_recovery + 2);

    // Allocated registers after recovery should be squashed_prd1 and squashed_prd2,
    // and NOT surviving_prd!
    const auto reg_a = free_list.allocate();
    const auto reg_b = free_list.allocate();

    assert(reg_a != surviving_prd);
    assert(reg_b != surviving_prd);
    assert(reg_a == squashed_prd1 || reg_a == squashed_prd2);
    assert(reg_b == squashed_prd1 || reg_b == squashed_prd2);

    // Surviving instruction is still preserved in ROB
    assert(rob.size() == 2); // surviving instruction + branch
    assert(rob.entry(surviving_rob_idx) != nullptr);
    assert(rob.entry(surviving_rob_idx)->valid);
    assert(rob.entry(surviving_rob_idx)->physical_rd == surviving_prd);

    std::cout << "  [PASS] Physical registers of squashed instructions released; surviving preserved.\n";
}

// ============================================================================
// Test 5: Frontend recovery
// The wrong-path instructions are flushed, and fetching resumes at actual next PC.
// ============================================================================
void test_frontend_recovery()
{
    std::cout << "[Test 5] Testing Frontend Recovery (Flush & Fetch Redirect)...\n";

    logic::Wire clock{logic::LogicState::LOW};
    logic::Wire reset{logic::LogicState::LOW};
    logic::Wire enable{logic::LogicState::HIGH};
    logic::Wire flush{logic::LogicState::LOW};

    // Buses for Fetch/Decode Reg
    logic::Bus<32> fetch_pc0{};
    logic::Bus<32> fetch_inst0{};
    logic::Wire fetch_val0{logic::LogicState::HIGH};

    logic::Bus<32> fetch_pc1{};
    logic::Bus<32> fetch_inst1{};
    logic::Wire fetch_val1{logic::LogicState::HIGH};

    logic::Bus<32> decode_pc0{};
    logic::Bus<32> decode_inst0{};
    logic::Wire decode_val0{logic::LogicState::LOW};

    logic::Bus<32> decode_pc1{};
    logic::Bus<32> decode_inst1{};
    logic::Wire decode_val1{logic::LogicState::LOW};

    cpu::FetchDecodeReg<32, 32> fd_reg{
        clock, reset, enable, flush,
        fetch_pc0, fetch_inst0, fetch_val0,
        fetch_pc1, fetch_inst1, fetch_val1,
        decode_pc0, decode_inst0, decode_val0,
        decode_pc1, decode_inst1, decode_val1
    };

    // Buses and wires for FetchUnit
    logic::Bus<32> f_pc0{};
    logic::Bus<32> f_pc1{};
    logic::Bus<32> f_inst0{};
    logic::Bus<32> f_inst1{};
    logic::Wire f_hit0{logic::LogicState::HIGH};
    logic::Wire f_hit1{logic::LogicState::HIGH};
    logic::Wire f_val0{logic::LogicState::LOW};
    logic::Wire f_val1{logic::LogicState::LOW};
    logic::Bus<32> next_pc{};
    logic::Bus<32> redirect_pc{};
    logic::Wire redirect{logic::LogicState::LOW};

    cpu::FetchUnit<32, 32> fetch_unit{
        clock, reset,
        f_pc0, f_pc1,
        f_inst0, f_inst1,
        f_hit0, f_hit1,
        f_val0, f_val1,
        next_pc,
        redirect_pc,
        redirect
    };

    // Step 1: Clock a cycle to put instructions into FetchDecodeReg
    fetch_pc0.write_value(0x5000);
    fetch_inst0.write_value(0x11111111);
    fetch_val0.write(logic::LogicState::HIGH);

    fetch_pc1.write_value(0x5004);
    fetch_inst1.write_value(0x22222222);
    fetch_val1.write(logic::LogicState::HIGH);

    clock.write(logic::LogicState::LOW);
    fd_reg.evaluate();
    clock.write(logic::LogicState::HIGH);
    fd_reg.evaluate();

    assert(decode_val0.read() == logic::LogicState::HIGH);
    assert(decode_val1.read() == logic::LogicState::HIGH);
    assert(decode_pc0.read_value() == 0x5000);

    // Step 2: Simulate branch recovery unit firing
    cpu::BranchResult mispred_res{};
    mispred_res.valid = true;
    mispred_res.mispredicted = true;
    mispred_res.redirect_pc = 0x6000;
    mispred_res.rob_index = 0;

    cpu::ReOrderBuffer<16> rob{};
    cpu::RegisterAliasTable rat{64};
    cpu::PhysicalRegisterFreeList free_list{64, 16};
    cpu::IssueQueue<16> iq{};

    const auto cp = rat.checkpoint();
    cpu::RenameBundle b{};
    b.valid = true;
    b.control_flow = cpu::ControlFlow::BRANCH;
    std::size_t idx = 0;
    rob.allocate(b, idx, &cp);

    cpu::BranchRecoveryUnit<16, 16> bru{};
    bru.recover(mispred_res, rob, rat, free_list, iq);

    assert(bru.control().recover == true);
    assert(bru.control().fetch_decode_flush == true);
    assert(bru.control().fetch_redirect == true);
    assert(bru.control().redirect_pc == 0x6000);

    // Step 3: Apply flush to FetchDecodeReg
    flush.write(logic::LogicState::HIGH);
    clock.write(logic::LogicState::LOW);
    fd_reg.evaluate();
    clock.write(logic::LogicState::HIGH);
    fd_reg.evaluate();

    assert(decode_val0.read() == logic::LogicState::LOW);
    assert(decode_val1.read() == logic::LogicState::LOW);

    // Step 4: Apply redirect to FetchUnit
    redirect.write(logic::LogicState::HIGH);
    redirect_pc.write_value(bru.control().redirect_pc);

    clock.write(logic::LogicState::LOW);
    fetch_unit.evaluate();
    clock.write(logic::LogicState::HIGH);
    fetch_unit.evaluate();

    assert(f_pc0.read_value() == 0x6000);
    assert(f_pc1.read_value() == 0x6004);

    std::cout << "  [PASS] Frontend flushed and fetch redirected to target PC.\n";
}

// ============================================================================
// Test 6: Predictor training
// Predictor training occurs exactly once per resolved branch.
// ============================================================================
void test_predictor_training()
{
    std::cout << "[Test 6] Testing Predictor Training (Once Per Resolved Branch)...\n";

    cpu::BranchPredictor<256> predictor{cpu::BranchPredictionState::WEAK_NOT_TAKEN};
    const std::uint32_t branch_pc = 0x7000;

    // Verify initial counter state
    assert(predictor.state(branch_pc) == cpu::BranchPredictionState::WEAK_NOT_TAKEN);

    cpu::BranchExecutionUnit<32> beu{};

    // First branch resolution: BEQ taken
    const auto res1 = beu.evaluate_with_predictor(
        cpu::Opcode::BEQ,
        branch_pc,
        /*rs1=*/1,
        /*rs2=*/1,
        /*imm=*/16,
        predictor
    );
    assert(res1.taken == true);
    // After 1 taken outcome: WEAK_NOT_TAKEN (1) -> WEAK_TAKEN (2)
    assert(predictor.state(branch_pc) == cpu::BranchPredictionState::WEAK_TAKEN);

    // Second branch resolution: BEQ taken again
    const auto res2 = beu.evaluate_with_predictor(
        cpu::Opcode::BEQ,
        branch_pc,
        /*rs1=*/1,
        /*rs2=*/1,
        /*imm=*/16,
        predictor
    );
    assert(res2.taken == true);
    // After 2nd taken outcome: WEAK_TAKEN (2) -> STRONG_TAKEN (3)
    assert(predictor.state(branch_pc) == cpu::BranchPredictionState::STRONG_TAKEN);

    // Third branch resolution: BEQ not taken
    const auto res3 = beu.evaluate_with_predictor(
        cpu::Opcode::BEQ,
        branch_pc,
        /*rs1=*/1,
        /*rs2=*/2,
        /*imm=*/16,
        predictor
    );
    assert(res3.taken == false);
    // After 3rd (not taken) outcome: STRONG_TAKEN (3) -> WEAK_TAKEN (2)
    assert(predictor.state(branch_pc) == cpu::BranchPredictionState::WEAK_TAKEN);

    std::cout << "  [PASS] Predictor trained exactly once per resolved branch.\n";
}

int main()
{
    std::cout << "========================================================\n";
    std::cout << "Starting End-to-End Branch Misprediction Integration Tests\n";
    std::cout << "========================================================\n";

    test_prediction_preservation();
    test_correct_prediction();
    test_misprediction_recovery();
    test_physical_register_safety();
    test_frontend_recovery();
    test_predictor_training();

    std::cout << "========================================================\n";
    std::cout << "ALL 6 Branch Misprediction Integration Tests PASSED!\n";
    std::cout << "========================================================\n";

    return 0;
}

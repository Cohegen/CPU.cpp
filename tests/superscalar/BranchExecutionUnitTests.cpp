#include "../../superscalar/core/branch/BranchExecutionUnit.hpp"
#include "../../superscalar/core/branch/BranchResult.hpp"
#include "../../superscalar/core/branch/BranchPredictor.hpp"
#include "../../superscalar/core/issue/IssueQueueEntry.hpp"
#include "../../superscalar/core/execution/ExecutionRequest.hpp"
#include "../../superscalar/core/dispatch/ReOrderBuffer.hpp"
#include "../../include/isa/Opcode.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>

void test_beq_equal_taken()
{
    cpu::BranchExecutionUnit<32> beu{};
    const std::uint32_t pc = 0x00001000;
    const std::uint32_t rs1 = 42;
    const std::uint32_t rs2 = 42;
    const std::int32_t imm = 8;

    const auto result = beu.evaluate(cpu::Opcode::BEQ, pc, rs1, rs2, imm);

    assert(result.valid);
    assert(result.taken); // BEQ equal -> taken
    assert(result.target_pc == 0x00001008);
    assert(result.next_pc == 0x00001008);
    std::cout << "  [PASS] BEQ equal      -> taken\n";
}

void test_beq_unequal_not_taken()
{
    cpu::BranchExecutionUnit<32> beu{};
    const std::uint32_t pc = 0x00001000;
    const std::uint32_t rs1 = 42;
    const std::uint32_t rs2 = 99;
    const std::int32_t imm = 8;

    const auto result = beu.evaluate(cpu::Opcode::BEQ, pc, rs1, rs2, imm);

    assert(result.valid);
    assert(!result.taken); // BEQ unequal -> not taken
    assert(result.fallthrough_pc == 0x00001004);
    assert(result.next_pc == 0x00001004);
    std::cout << "  [PASS] BEQ unequal    -> not taken\n";
}

void test_bne_equal_not_taken()
{
    cpu::BranchExecutionUnit<32> beu{};
    const std::uint32_t pc = 0x00002000;
    const std::uint32_t rs1 = 100;
    const std::uint32_t rs2 = 100;
    const std::int32_t imm = 16;

    const auto result = beu.evaluate(cpu::Opcode::BNE, pc, rs1, rs2, imm);

    assert(result.valid);
    assert(!result.taken); // BNE equal -> not taken
    assert(result.fallthrough_pc == 0x00002004);
    assert(result.next_pc == 0x00002004);
    std::cout << "  [PASS] BNE equal      -> not taken\n";
}

void test_bne_unequal_taken()
{
    cpu::BranchExecutionUnit<32> beu{};
    const std::uint32_t pc = 0x00002000;
    const std::uint32_t rs1 = 100;
    const std::uint32_t rs2 = 200;
    const std::int32_t imm = 16;

    const auto result = beu.evaluate(cpu::Opcode::BNE, pc, rs1, rs2, imm);

    assert(result.valid);
    assert(result.taken); // BNE unequal -> taken
    assert(result.target_pc == 0x00002010);
    assert(result.next_pc == 0x00002010);
    std::cout << "  [PASS] BNE unequal    -> taken\n";
}

void test_j_taken()
{
    cpu::BranchExecutionUnit<32> beu{};
    const std::uint32_t pc = 0x00003000;
    const std::uint32_t rs1 = 0;
    const std::uint32_t rs2 = 0;
    const std::int32_t imm = 32;

    const auto result = beu.evaluate(cpu::Opcode::J, pc, rs1, rs2, imm);

    assert(result.valid);
    assert(result.taken); // J -> taken (unconditional)
    assert(result.target_pc == 0x00003020);
    assert(result.next_pc == 0x00003020);
    std::cout << "  [PASS] J              -> taken\n";
}

void test_target_pc_plus_immediate()
{
    cpu::BranchExecutionUnit<32> beu{};

    // Target calculation: target = PC + immediate
    const std::uint32_t pc1 = 0x00400000;
    const std::int32_t imm1 = 0x40;
    const auto res1 = beu.evaluate(cpu::Opcode::J, pc1, 0, 0, imm1);
    assert(res1.target_pc == 0x00400040);
    assert(res1.target_pc == pc1 + static_cast<std::uint32_t>(imm1));

    const std::uint32_t pc2 = 0x00008000;
    const std::int32_t imm2 = 0x1234;
    const auto res2 = beu.evaluate(cpu::Opcode::BEQ, pc2, 5, 5, imm2);
    assert(res2.target_pc == 0x00009234);
    assert(res2.target_pc == pc2 + static_cast<std::uint32_t>(imm2));

    std::cout << "  [PASS] target = PC + immediate\n";
}

void test_forward_target()
{
    cpu::BranchExecutionUnit<32> beu{};
    const std::uint32_t pc = 0x00001000;
    const std::int32_t forward_offset = +64; // Positive immediate forward branch

    const auto result = beu.evaluate(cpu::Opcode::BEQ, pc, 7, 7, forward_offset);

    assert(result.taken);
    assert(result.target_pc > pc); // Forward jump
    assert(result.target_pc == 0x00001040);
    assert(result.next_pc == 0x00001040);
    std::cout << "  [PASS] forward target\n";
}

void test_negative_target()
{
    cpu::BranchExecutionUnit<32> beu{};
    const std::uint32_t pc = 0x00001000;
    const std::int32_t negative_offset = -16; // Negative immediate backward branch (loop)

    const auto result = beu.evaluate(cpu::Opcode::BEQ, pc, 7, 7, negative_offset);

    assert(result.taken);
    assert(result.target_pc < pc); // Backward loop target
    assert(result.target_pc == 0x00000FF0);
    assert(result.next_pc == 0x00000FF0);
    std::cout << "  [PASS] negative target\n";
}

void test_correct_prediction()
{
    cpu::BranchExecutionUnit<32> beu{};

    // Case 1: Predicted taken, actual is taken
    {
        const std::uint32_t pc = 0x00004000;
        const std::int32_t imm = 24;
        const std::uint32_t expected_target = pc + imm;
        const auto result = beu.evaluate(
            cpu::Opcode::BEQ, pc, 10, 10, imm,
            true, expected_target
        );
        assert(result.taken);
        assert(!result.mispredicted); // Correctly predicted taken
        assert(result.redirect_pc == 0);
    }

    // Case 2: Predicted not taken, actual is not taken
    {
        const std::uint32_t pc = 0x00004000;
        const std::int32_t imm = 24;
        const auto result = beu.evaluate(
            cpu::Opcode::BEQ, pc, 10, 20, imm,
            false, 0
        );
        assert(!result.taken);
        assert(!result.mispredicted); // Correctly predicted not taken
        assert(result.redirect_pc == 0);
    }

    // Case 3: Trained 2-bit branch predictor
    {
        cpu::BranchPredictor<16> predictor{cpu::BranchPredictionState::STRONG_TAKEN};
        const std::uint32_t pc = 0x00005000;
        const std::int32_t imm = 12;
        const auto result = beu.evaluate_with_predictor(
            cpu::Opcode::BEQ, pc, 1, 1, imm, predictor
        );
        assert(result.taken);
        assert(!result.mispredicted);
    }

    std::cout << "  [PASS] correct prediction\n";
}

void test_wrong_prediction()
{
    cpu::BranchExecutionUnit<32> beu{};

    // Case 1: Predicted not taken, but actual branch is taken
    {
        const std::uint32_t pc = 0x00006000;
        const std::int32_t imm = 40;
        const auto result = beu.evaluate(
            cpu::Opcode::BEQ, pc, 9, 9, imm,
            false, 0 // Predicted not taken
        );
        assert(result.taken);
        assert(result.mispredicted); // Mispredicted!
        assert(result.redirect_pc == result.target_pc); // Redirect to taken target
        assert(result.redirect_pc == 0x00006028);
    }

    // Case 2: Predicted taken, but actual branch is not taken
    {
        const std::uint32_t pc = 0x00006000;
        const std::int32_t imm = 40;
        const auto result = beu.evaluate(
            cpu::Opcode::BEQ, pc, 9, 10, imm,
            true, 0x00006028 // Predicted taken
        );
        assert(!result.taken);
        assert(result.mispredicted); // Mispredicted!
        assert(result.redirect_pc == result.fallthrough_pc); // Redirect to fall-through PC
        assert(result.redirect_pc == 0x00006004);
    }

    // Case 3: Predicted taken with incorrect target address
    {
        const std::uint32_t pc = 0x00006000;
        const std::int32_t imm = 40;
        const auto result = beu.evaluate(
            cpu::Opcode::BEQ, pc, 9, 9, imm,
            true, 0x12345678 // Wrong predicted target
        );
        assert(result.taken);
        assert(result.mispredicted); // Target mismatch is a misprediction!
        assert(result.redirect_pc == result.target_pc);
    }

    std::cout << "  [PASS] wrong prediction\n";
}

void test_issue_queue_and_rob_integration()
{
    cpu::BranchExecutionUnit<32> beu{};
    cpu::ReOrderBuffer<16> rob{};

    // Dispatch a branch to ROB
    cpu::RenameBundle bundle{};
    bundle.valid = true;
    bundle.pc = 0x00007000;
    bundle.opcode = cpu::Opcode::BEQ;
    bundle.control_flow = cpu::ControlFlow::BRANCH;
    std::size_t rob_idx = 0;
    assert(rob.allocate(bundle, rob_idx));

    // Construct issue queue entry for the branch
    cpu::IssueQueueEntry entry{};
    entry.valid = true;
    entry.pc = 0x00007000;
    entry.opcode = cpu::Opcode::BEQ;
    entry.rs1_value = 15;
    entry.rs2_value = 15;
    entry.immediate = 20;
    entry.rob_index = rob_idx;
    entry.control_flow = cpu::ControlFlow::BRANCH;

    const auto res = beu.evaluate(entry, false, 0);
    assert(res.taken);
    assert(res.target_pc == 0x00007014);

    // Update ROB entry
    auto* rob_entry = rob.entry(res.rob_index);
    assert(rob_entry != nullptr);
    rob_entry->branch = true;
    rob_entry->branch_taken = res.taken;
    rob_entry->branch_target = res.target_pc;
    rob.complete(res.rob_index, res.next_pc);

    assert(rob_entry->completed);
    assert(rob_entry->branch_taken);
    assert(rob_entry->branch_target == 0x00007014);

    std::cout << "  [PASS] IssueQueue and ROB integration\n";
}

int main()
{
    std::cout << "Running Superscalar Branch Tests:\n";
    test_beq_equal_taken();
    test_beq_unequal_not_taken();
    test_bne_equal_not_taken();
    test_bne_unequal_taken();
    test_j_taken();
    test_target_pc_plus_immediate();
    test_forward_target();
    test_negative_target();
    test_correct_prediction();
    test_wrong_prediction();
    test_issue_queue_and_rob_integration();

    std::cout << "\nAll superscalar branch tests PASSED successfully!\n";
    return 0;
}

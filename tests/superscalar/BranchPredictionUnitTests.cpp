#include "../../superscalar/core/branch/BranchPredictionUnit.hpp"
#include "../../superscalar/core/branch/BranchPredictor.hpp"
#include "../../superscalar/core/decode/DecodeTypes.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>

// ─────────────────────────────────────────────────────────────────────────────
// Test 1 — no control flow
// ─────────────────────────────────────────────────────────────────────────────
void test_no_control_flow()
{
    cpu::BranchPredictor<256> predictor{};
    cpu::BranchPredictionUnit<256> bpu{predictor};

    const std::uint32_t pc0 = 0x100;
    const std::uint32_t pc1 = 0x104;

    const auto result = bpu.evaluate(
        pc0, true, cpu::ControlFlow::NONE,
        pc1, true, cpu::ControlFlow::NONE
    );

    assert(!result.redirect);
    assert(!result.lane0_is_control_flow);
    assert(!result.lane1_is_control_flow);

    std::cout << "  [PASS] Test 1: no control flow -> redirect == false\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 2 — lane 0 taken
// ─────────────────────────────────────────────────────────────────────────────
void test_lane0_taken()
{
    cpu::BranchPredictor<256> predictor{};
    cpu::BranchPredictionUnit<256> bpu{predictor};

    const std::uint32_t pc0 = 0x100;
    const std::uint32_t target0 = 0x400;
    const std::uint32_t pc1 = 0x104;

    // Train predictor for 0x100 -> 0x400
    predictor.update(pc0, true, target0);
    predictor.update(pc0, true, target0);

    const auto result = bpu.evaluate(
        pc0, true, cpu::ControlFlow::BRANCH,
        pc1, true, cpu::ControlFlow::NONE
    );

    assert(result.redirect);
    assert(result.redirect_lane == 0);
    assert(result.redirect_pc == 0x400);

    std::cout << "  [PASS] Test 2: lane 0 taken -> redirect == true, lane == 0, pc == 0x400\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 3 — lane 1 taken
// ─────────────────────────────────────────────────────────────────────────────
void test_lane1_taken()
{
    cpu::BranchPredictor<256> predictor{};
    cpu::BranchPredictionUnit<256> bpu{predictor};

    const std::uint32_t pc0 = 0x100;
    const std::uint32_t pc1 = 0x104;
    const std::uint32_t target1 = 0x500;

    // Train predictor for 0x104 -> target1
    predictor.update(pc1, true, target1);
    predictor.update(pc1, true, target1);

    const auto result = bpu.evaluate(
        pc0, true, cpu::ControlFlow::NONE,
        pc1, true, cpu::ControlFlow::BRANCH
    );

    assert(result.redirect);
    assert(result.redirect_lane == 1);
    assert(result.redirect_pc == target1);

    std::cout << "  [PASS] Test 3: lane 1 taken -> redirect == true, lane == 1, pc == target1\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 4 — both taken (verifies oldest-instruction priority)
// ─────────────────────────────────────────────────────────────────────────────
void test_both_taken_oldest_priority()
{
    cpu::BranchPredictor<256> predictor{};
    cpu::BranchPredictionUnit<256> bpu{predictor};

    const std::uint32_t pc0 = 0x100;
    const std::uint32_t target0 = 0x400;
    const std::uint32_t pc1 = 0x104;
    const std::uint32_t target1 = 0x800;

    // Train predictor for both branches
    predictor.update(pc0, true, target0);
    predictor.update(pc0, true, target0);
    predictor.update(pc1, true, target1);
    predictor.update(pc1, true, target1);

    const auto result = bpu.evaluate(
        pc0, true, cpu::ControlFlow::BRANCH,
        pc1, true, cpu::ControlFlow::BRANCH
    );

    assert(result.redirect);
    assert(result.redirect_lane == 0);
    assert(result.redirect_pc == 0x400);

    std::cout << "  [PASS] Test 4: both taken -> redirect_lane == 0, pc == 0x400 (oldest priority)\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 5 — lane 0 BTB miss, lane 1 BTB hit
// ─────────────────────────────────────────────────────────────────────────────
void test_lane0_btb_miss_lane1_btb_hit()
{
    cpu::BranchPredictor<256> predictor{};
    cpu::BranchPredictionUnit<256> bpu{predictor};

    const std::uint32_t pc0 = 0x100;
    const std::uint32_t pc1 = 0x104;
    const std::uint32_t target1 = 0x800;

    // Lane 0: counter predicts taken, but BTB entry is missing/empty (hit == false)
    predictor.set_state(pc0, cpu::BranchPredictionState::STRONG_TAKEN);
    assert(predictor.predict(pc0).taken == true);
    assert(predictor.predict(pc0).btb_hit == false);

    // Lane 1: counter taken and BTB hit
    predictor.update(pc1, true, target1);
    predictor.update(pc1, true, target1);
    assert(predictor.predict(pc1).taken == true);
    assert(predictor.predict(pc1).btb_hit == true);

    const auto result = bpu.evaluate(
        pc0, true, cpu::ControlFlow::BRANCH,
        pc1, true, cpu::ControlFlow::BRANCH
    );

    assert(result.redirect);
    assert(result.redirect_lane == 1);
    assert(result.redirect_pc == target1);

    std::cout << "  [PASS] Test 5: lane 0 BTB miss, lane 1 BTB hit -> redirect_lane == 1, pc == lane1_target\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 6 — prediction metadata preserved
// ─────────────────────────────────────────────────────────────────────────────
void test_prediction_metadata_preserved()
{
    cpu::BranchPredictor<256> predictor{};
    cpu::BranchPredictionUnit<256> bpu{predictor};

    const std::uint32_t pc0 = 0x100;
    const std::uint32_t pc1 = 0x104;

    predictor.update(pc0, true, 0x400);
    predictor.update(pc0, true, 0x400);
    predictor.update(pc1, false, 0x0);

    const auto pred0 = predictor.predict(pc0);
    const auto pred1 = predictor.predict(pc1);

    const auto result = bpu.evaluate(
        pc0, true, cpu::ControlFlow::BRANCH,
        pc1, true, cpu::ControlFlow::BRANCH
    );

    assert(result.lane0 == pred0);
    assert(result.lane1 == pred1);

    assert(result.lane0.taken == pred0.taken);
    assert(result.lane0.target == pred0.target);
    assert(result.lane0.btb_hit == pred0.btb_hit);

    assert(result.lane1.taken == pred1.taken);
    assert(result.lane1.target == pred1.target);
    assert(result.lane1.btb_hit == pred1.btb_hit);

    std::cout << "  [PASS] Test 6: prediction metadata preserved exactly as returned by predictor\n";
}

int main()
{
    std::cout << "Running BranchPredictionUnit Tests:\n";

    test_no_control_flow();
    test_lane0_taken();
    test_lane1_taken();
    test_both_taken_oldest_priority();
    test_lane0_btb_miss_lane1_btb_hit();
    test_prediction_metadata_preserved();

    std::cout << "\nAll BranchPredictionUnit tests PASSED successfully!\n";
    return 0;
}

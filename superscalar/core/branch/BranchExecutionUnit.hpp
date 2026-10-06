#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include <logic/combinational/adders/RippleCarryAdder.hpp>
#include <logic/signals/bus.hpp>
#include <logic/signals/logicState.hpp>
#include <logic/signals/wire.hpp>
#include <logic/simulator/Component.hpp>

#if __has_include("components/ALUInterface.hpp")
#include "components/ALUInterface.hpp"
#elif __has_include("ALUInterface.hpp")
#include "ALUInterface.hpp"
#else
#include "../../../components/ALUInterface.hpp"
#endif

#if __has_include("components/ControlSignals.hpp")
#include "components/ControlSignals.hpp"
#elif __has_include("ControlSignals.hpp")
#include "ControlSignals.hpp"
#else
#include "../../../components/ControlSignals.hpp"
#endif

#if __has_include("isa/Opcode.hpp")
#include "isa/Opcode.hpp"
#elif __has_include("Opcode.hpp")
#include "Opcode.hpp"
#else
#include "../../../include/isa/Opcode.hpp"
#endif

#include "BranchResult.hpp"
#include "BranchPredictor.hpp"

#if __has_include("superscalar/core/issue/IssueQueueEntry.hpp")
#include "superscalar/core/issue/IssueQueueEntry.hpp"
#elif __has_include("issue/IssueQueueEntry.hpp")
#include "issue/IssueQueueEntry.hpp"
#elif __has_include("IssueQueueEntry.hpp")
#include "IssueQueueEntry.hpp"
#else
#include "../issue/IssueQueueEntry.hpp"
#endif

#if __has_include("superscalar/core/execution/ExecutionRequest.hpp")
#include "superscalar/core/execution/ExecutionRequest.hpp"
#elif __has_include("execution/ExecutionRequest.hpp")
#include "execution/ExecutionRequest.hpp"
#elif __has_include("ExecutionRequest.hpp")
#include "ExecutionRequest.hpp"
#else
#include "../execution/ExecutionRequest.hpp"
#endif

namespace cpu
{

template <std::size_t DataWidth = 32>
class BranchExecutionUnit : public logic::Component
{
public:
    BranchExecutionUnit()
        : BranchExecutionUnit(
              owned_input_valid_,
              owned_pc_,
              owned_operand_a_,
              owned_operand_b_,
              owned_immediate_,
              owned_target_,
              owned_branch_taken_,
              owned_output_valid_
          )
    {
    }

    BranchExecutionUnit(
        logic::Wire& input_valid,
        logic::Bus<DataWidth>& pc,
        logic::Bus<DataWidth>& operand_a,
        logic::Bus<DataWidth>& operand_b,
        logic::Bus<DataWidth>& immediate,
        logic::Bus<DataWidth>& target,
        logic::Wire& branch_taken,
        logic::Wire& output_valid
    )
        : input_valid_(input_valid),
          pc_(pc),
          operand_a_(operand_a),
          operand_b_(operand_b),
          immediate_(immediate),
          target_(target),
          branch_taken_out_(branch_taken),
          output_valid_(output_valid)
    {
        alu_ = std::make_unique<ALUInterface<DataWidth>>(
            operand_a_, operand_b_, ALUOperation::SUB, sub_result_, zero_, carry_
        );
        target_adder_ = std::make_unique<logic::RippleCarryAdder<DataWidth>>(
            pc_, immediate_, target_carry_in_, target_, target_carry_out_
        );
        target_carry_in_.write(logic::LogicState::LOW);
    }

    void set_instruction(
        Opcode opcode,
        std::uint32_t pc,
        std::uint32_t rs1_val,
        std::uint32_t rs2_val,
        std::int32_t imm,
        std::size_t rob_index = 0
    ) noexcept
    {
        opcode_ = opcode;
        rob_index_ = rob_index;

        input_valid_.write(logic::LogicState::HIGH);
        pc_.write_value(pc);
        operand_a_.write_value(rs1_val);
        operand_b_.write_value(rs2_val);
        immediate_.write_value(static_cast<std::uint32_t>(imm));
    }

    void set_prediction(bool predicted_taken, std::uint32_t predicted_target = 0) noexcept
    {
        predicted_taken_ = predicted_taken;
        predicted_target_ = predicted_target;
    }

    void evaluate() noexcept override
    {
        // 1. Evaluate target address through RippleCarryAdder: target = PC + immediate
        target_adder_->evaluate();

        // 2. Evaluate equality comparison through ALUInterface subtraction: rs1 - rs2
        alu_->evaluate();

        // 3. Resolve condition
        const bool is_zero = (zero_.read() == logic::LogicState::HIGH);
        switch (opcode_)
        {
            case Opcode::BEQ:
                actual_taken_ = is_zero;
                break;
            case Opcode::BNE:
                actual_taken_ = !is_zero;
                break;
            case Opcode::J:
                actual_taken_ = true;
                break;
            default:
                actual_taken_ = false;
                break;
        }

        branch_taken_out_.write(
            actual_taken_ ? logic::LogicState::HIGH : logic::LogicState::LOW
        );
        output_valid_.write(input_valid_.read());

        actual_target_ = static_cast<std::uint32_t>(target_.read_value());
        fallthrough_pc_ = static_cast<std::uint32_t>(pc_.read_value()) + 4U;
        next_pc_ = actual_taken_ ? actual_target_ : fallthrough_pc_;

        // 4. Verify prediction
        if (predicted_taken_ != actual_taken_)
        {
            mispredicted_ = true;
        }
        else if (actual_taken_ && (predicted_target_ != 0U) && (predicted_target_ != actual_target_))
        {
            mispredicted_ = true;
        }
        else
        {
            mispredicted_ = false;
        }

        redirect_pc_ = mispredicted_ ? next_pc_ : 0U;
    }

    [[nodiscard]]
    BranchResult result() const noexcept
    {
        BranchResult res{};
        res.valid = (output_valid_.read() == logic::LogicState::HIGH);
        res.rob_index = rob_index_;
        res.pc = static_cast<std::uint32_t>(pc_.read_value());
        res.opcode = opcode_;
        res.taken = actual_taken_;
        res.target_pc = actual_target_;
        res.fallthrough_pc = fallthrough_pc_;
        res.next_pc = next_pc_;
        res.predicted_taken = predicted_taken_;
        res.predicted_target = predicted_target_;
        res.mispredicted = mispredicted_;
        res.redirect_pc = redirect_pc_;
        return res;
    }

    [[nodiscard]]
    BranchResult evaluate(
        Opcode opcode,
        std::uint32_t pc,
        std::uint32_t rs1_val,
        std::uint32_t rs2_val,
        std::int32_t imm,
        bool predicted_taken = false,
        std::uint32_t predicted_target = 0,
        std::size_t rob_index = 0
    )
    {
        set_instruction(opcode, pc, rs1_val, rs2_val, imm, rob_index);
        set_prediction(predicted_taken, predicted_target);
        evaluate();
        return result();
    }

    [[nodiscard]]
    BranchResult evaluate(
        const IssueQueueEntry& entry,
        bool predicted_taken = false,
        std::uint32_t predicted_target = 0
    )
    {
        return evaluate(
            entry.opcode,
            entry.pc,
            entry.rs1_value,
            entry.rs2_value,
            entry.immediate,
            predicted_taken,
            predicted_target,
            entry.rob_index
        );
    }

    [[nodiscard]]
    BranchResult evaluate(
        const ExecutionRequest& request,
        bool predicted_taken = false,
        std::uint32_t predicted_target = 0
    )
    {
        return evaluate(
            request.opcode,
            request.pc,
            request.rs1_value,
            request.rs2_value,
            request.immediate,
            predicted_taken,
            predicted_target,
            request.rob_index
        );
    }

    template <std::size_t TableSize>
    [[nodiscard]]
    BranchResult evaluate_with_predictor(
        Opcode opcode,
        std::uint32_t pc,
        std::uint32_t rs1_val,
        std::uint32_t rs2_val,
        std::int32_t imm,
        BranchPredictor<TableSize>& predictor,
        std::size_t rob_index = 0
    )
    {
        const auto prediction = predictor.predict(pc);
        const auto res = evaluate(
            opcode, pc, rs1_val, rs2_val, imm,
            prediction.taken, prediction.target, rob_index
        );
        predictor.update(pc, res.taken, res.target_pc);
        return res;
    }

private:
    logic::Wire owned_input_valid_{logic::LogicState::LOW};
    logic::Wire owned_output_valid_{logic::LogicState::LOW};
    logic::Wire owned_branch_taken_{logic::LogicState::LOW};
    logic::Bus<DataWidth> owned_pc_{};
    logic::Bus<DataWidth> owned_operand_a_{};
    logic::Bus<DataWidth> owned_operand_b_{};
    logic::Bus<DataWidth> owned_immediate_{};
    logic::Bus<DataWidth> owned_target_{};

    logic::Wire& input_valid_;
    logic::Bus<DataWidth>& pc_;
    logic::Bus<DataWidth>& operand_a_;
    logic::Bus<DataWidth>& operand_b_;
    logic::Bus<DataWidth>& immediate_;
    logic::Bus<DataWidth>& target_;
    logic::Wire& branch_taken_out_;
    logic::Wire& output_valid_;

    logic::Wire zero_{logic::LogicState::LOW};
    logic::Wire carry_{logic::LogicState::LOW};
    logic::Bus<DataWidth> sub_result_{};
    std::unique_ptr<ALUInterface<DataWidth>> alu_;

    logic::Wire target_carry_in_{logic::LogicState::LOW};
    logic::Wire target_carry_out_{logic::LogicState::LOW};
    std::unique_ptr<logic::RippleCarryAdder<DataWidth>> target_adder_;

    Opcode opcode_{Opcode::NOP};
    std::size_t rob_index_{0};
    bool predicted_taken_{false};
    std::uint32_t predicted_target_{0};

    bool actual_taken_{false};
    std::uint32_t actual_target_{0};
    std::uint32_t fallthrough_pc_{0};
    std::uint32_t next_pc_{0};
    bool mispredicted_{false};
    std::uint32_t redirect_pc_{0};
};

} // namespace cpu

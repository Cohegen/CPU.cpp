#pragma once

#include <cstddef>
#include <cstdint>

#include <logic/signals/bus.hpp>
#include <logic/signals/logicState.hpp>
#include <logic/signals/wire.hpp>
#include <logic/simulator/Component.hpp>

#include "../../../components/ALUInterface.hpp"
#include "../issue/IssueQueueEntry.hpp"

namespace cpu
{

struct ExecuteResult
{
    bool valid{false};
    std::uint32_t result{0};
    std::size_t rob_index{0};
    IssueQueueEntry::PhysicalRegister physical_rd{0};
};


template <std::size_t DataWidth = 32>
class ALUExecutionUnit : public logic::Component
{
public:
    ALUExecutionUnit(
        logic::Wire& input_valid,
        logic::Bus<DataWidth>& operand_a,
        logic::Bus<DataWidth>& operand_b,
        logic::Bus<DataWidth>& result,
        logic::Wire& output_valid
    )
        : input_valid_(input_valid),
          operand_a_(operand_a),
          operand_b_(operand_b),
          result_(result),
          output_valid_(output_valid),
          alu_(operand_a_, operand_b_, ALUOperation::NONE, result_, zero_, carry_)
    {
    }

    void set_operation(ALUOperation operation) noexcept
    {
        operation_ = operation;
    }

    void set_destination(
        std::size_t rob_index,
        IssueQueueEntry::PhysicalRegister physical_rd
    ) noexcept
    {
        rob_index_ = rob_index;
        physical_rd_ = physical_rd;
    }

    void evaluate() noexcept override
    {
        alu_.set_operation(operation_);
        alu_.evaluate();
        output_valid_.write(input_valid_.read());
    }

    [[nodiscard]] ExecuteResult result_bundle() const noexcept
    {
        return ExecuteResult{
            output_valid_.read() == logic::LogicState::HIGH,
            static_cast<std::uint32_t>(result_.read_value()),
            rob_index_,
            physical_rd_
        };
    }

private:
    logic::Wire& input_valid_;
    logic::Bus<DataWidth>& operand_a_;
    logic::Bus<DataWidth>& operand_b_;
    logic::Bus<DataWidth>& result_;
    logic::Wire& output_valid_;
    logic::Wire zero_;
    logic::Wire carry_;
    ALUOperation operation_{ALUOperation::NONE};
    std::size_t rob_index_{0};
    IssueQueueEntry::PhysicalRegister physical_rd_{0};
    ALUInterface<DataWidth> alu_;
};

} // namespace cpu

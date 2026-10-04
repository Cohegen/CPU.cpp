#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <logic/signals/bus.hpp>
#include <logic/signals/logicState.hpp>
#include <logic/signals/wire.hpp>

#include "ALUExecutionUnit.hpp"
#include "../dispatch/ReOrderBuffer.hpp"
#include "../issue/IssueQueue.hpp"
#include "../issue/PhysicalRegisterFile.hpp"
#include "../rename/PhysicalRegisterFreeList.hpp"

namespace cpu
{

// Executes up to two ready integer ALU operations each cycle.  Write-back
// broadcasts values to the PRF and issue queue, then completes matching ROB
// entries.  Retirement remains ordered at the ROB head.
template <std::size_t ROBCapacity = 16, std::size_t IQCapacity = 16,
          std::size_t PRFCount = 64, std::size_t DataWidth = 32>
class IntegerExecutionPipeline
{
public:
    using ROB = ReOrderBuffer<ROBCapacity>;
    using IQ = IssueQueue<IQCapacity>;
    using PRF = PhysicalRegisterFile<PRFCount>;

    struct CycleResult
    {
        std::size_t issued{0};
        std::size_t retired{0};
    };

    IntegerExecutionPipeline(ROB& rob, IQ& issue_queue, PRF& prf,
                             PhysicalRegisterFreeList& free_list) noexcept
        : rob_(rob), issue_queue_(issue_queue), prf_(prf), free_list_(free_list),
          lanes_{}
    {
    }

    [[nodiscard]] CycleResult cycle() noexcept
    {
        CycleResult result{};
        std::array<typename IQ::Index, 2> selected{};
        std::size_t selected_count = 0;

        for (typename IQ::Index index = 0; index < issue_queue_.capacity() && selected_count < 2; ++index)
        {
            const auto* entry = issue_queue_.entry(index);
            if (entry != nullptr && ready_for_integer_execution(*entry))
            {
                selected[selected_count++] = index;
            }
        }

        for (std::size_t lane_index = 0; lane_index < selected_count; ++lane_index)
        {
            const auto* entry = issue_queue_.entry(selected[lane_index]);
            execute_lane(lane_index, *entry);
            issue_queue_.remove(selected[lane_index]);
            ++result.issued;
        }

        for (std::size_t lane_index = 0; lane_index < selected_count; ++lane_index)
        {
            write_back(lanes_[lane_index].result_bundle());
        }

        ROBEntry committed{};
        while (result.retired < 2 && rob_.commit(committed))
        {
            if (committed.register_write)
            {
                free_list_.release(committed.old_physical_rd);
            }
            ++result.retired;
        }
        return result;
    }

private:
    struct Lane
    {
        logic::Wire valid{logic::LogicState::LOW};
        logic::Wire completed{logic::LogicState::LOW};
        logic::Bus<DataWidth> operand_a{};
        logic::Bus<DataWidth> operand_b{};
        logic::Bus<DataWidth> result{};
        ALUExecutionUnit<DataWidth> unit;

        Lane() : unit(valid, operand_a, operand_b, result, completed) {}
    };

    [[nodiscard]] static bool ready_for_integer_execution(const IssueQueueEntry& entry) noexcept
    {
        return entry.valid && !entry.issued && entry.rs1_ready && entry.rs2_ready &&
               !entry.memory_read && !entry.memory_write &&
               entry.control_flow == ControlFlow::NONE && !entry.halt &&
               entry.alu_operation != ALUOperation::NONE;
    }

    [[nodiscard]] static std::uint32_t operand_value(
        OperandSource source, std::uint32_t register_value, std::int32_t immediate
    ) noexcept
    {
        switch (source)
        {
            case OperandSource::REGISTER: return register_value;
            case OperandSource::IMMEDIATE: return static_cast<std::uint32_t>(immediate);
            case OperandSource::NONE: return 0;
        }
        return 0;
    }

    void execute_lane(std::size_t lane_index, const IssueQueueEntry& entry) noexcept
    {
        auto& lane = lanes_[lane_index];
        lane.valid.write(logic::LogicState::HIGH);
        lane.operand_a.write_value(operand_value(entry.operand_a, entry.rs1_value, entry.immediate));
        lane.operand_b.write_value(operand_value(entry.operand_b, entry.rs2_value, entry.immediate));
        lane.unit.set_operation(entry.alu_operation);
        lane.unit.set_destination(entry.rob_index, entry.physical_rd);
        lane.unit.evaluate();
        lane.valid.write(logic::LogicState::LOW);
    }

    void write_back(const ExecuteResult& executed) noexcept
    {
        if (!executed.valid)
        {
            return;
        }
        const auto* rob_entry = rob_.entry(executed.rob_index);
        if (rob_entry == nullptr || !rob_entry->valid)
        {
            return;
        }
        if (rob_entry->register_write)
        {
            prf_.write(executed.physical_rd, executed.result);
            issue_queue_.wakeup(executed.physical_rd, executed.result);
        }
        rob_.complete(executed.rob_index, executed.result);
    }

    ROB& rob_;
    IQ& issue_queue_;
    PRF& prf_;
    PhysicalRegisterFreeList& free_list_;
    std::array<Lane, 2> lanes_;
};

} // namespace cpu

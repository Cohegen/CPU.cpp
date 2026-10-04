#pragma once
#include <cstddef>
#include "../rename/RenameBundle.hpp"
#include "ReOrderBuffer.hpp"
#include "../issue/IssueQueue.hpp"
#include "../issue/PhysicalRegisterFile.hpp"

namespace cpu
{
    template<std::size_t ROBCapacity = 16, std::size_t IQCapacity = 16, std::size_t PRFCount = 32>
    class DispatchUnit
    {
    public:
        using ROBIndex = std::size_t;

        explicit DispatchUnit(ReOrderBuffer<ROBCapacity>& rob) noexcept
            : rob_(rob), issue_queue_(nullptr), prf_(nullptr)
        {
        }

        DispatchUnit(
            ReOrderBuffer<ROBCapacity>& rob,
            IssueQueue<IQCapacity>& issue_queue,
            PhysicalRegisterFile<PRFCount>& prf
        ) noexcept
            : rob_(rob), issue_queue_(&issue_queue), prf_(&prf)
        {
        }

        // Helper to construct an IssueQueueEntry from RenameBundle and PRF
        [[nodiscard]]
        IssueQueueEntry make_issue_entry(const RenameBundle& bundle, ROBIndex rob_index) const noexcept
        {
            IssueQueueEntry entry{};
            entry.valid = bundle.valid;
            entry.pc = bundle.pc;
            entry.instruction = bundle.instruction;
            entry.opcode = bundle.opcode;

            entry.physical_rd = bundle.physical_rd;
            entry.physical_rs1 = bundle.physical_rs1;
            entry.physical_rs2 = bundle.physical_rs2;

            entry.immediate = bundle.immediate;
            entry.alu_operation = bundle.alu_operation;
            entry.operand_a = bundle.operand_a;
            entry.operand_b = bundle.operand_b;

            entry.register_write = bundle.register_write;
            entry.memory_read = bundle.memory_read;
            entry.memory_write = bundle.memory_write;
            entry.control_flow = bundle.control_flow;
            entry.rob_index = rob_index;
            entry.halt = bundle.halt;

            if (prf_ != nullptr)
            {
                if (bundle.operand_a == OperandSource::REGISTER)
                {
                    entry.rs1_ready = prf_->ready(bundle.physical_rs1);
                    entry.rs1_value = prf_->read(bundle.physical_rs1);
                }
                else
                {
                    entry.rs1_ready = true;
                }

                if (bundle.operand_b == OperandSource::REGISTER)
                {
                    entry.rs2_ready = prf_->ready(bundle.physical_rs2);
                    entry.rs2_value = prf_->read(bundle.physical_rs2);
                }
                else
                {
                    entry.rs2_ready = true;
                }
            }
            else
            {
                entry.rs1_ready = (bundle.operand_a != OperandSource::REGISTER);
                entry.rs2_ready = (bundle.operand_b != OperandSource::REGISTER);
            }

            return entry;
        }

        // Single instruction dispatch
        [[nodiscard]]
        bool dispatch(const RenameBundle& input, ROBIndex& rob_index) noexcept
        {
            if (!input.valid)
            {
                return true;
            }

            if (rob_.full())
            {
                return false;
            }
            if (issue_queue_ != nullptr && issue_queue_->full())
            {
                return false;
            }

            if (!rob_.allocate(input, rob_index))
            {
                return false;
            }

            if (issue_queue_ != nullptr)
            {
                IssueQueueEntry entry = make_issue_entry(input, rob_index);
                if (!issue_queue_->insert(entry))
                {
                    return false;
                }
            }

            return true;
        }

        [[nodiscard]]
        bool dispatch(const RenameBundle& input) noexcept
        {
            ROBIndex rob_index = 0;
            return dispatch(input, rob_index);
        }

        // 2-wide dispatch
        [[nodiscard]]
        bool dispatch(
            const RenameBundle& input0,
            const RenameBundle& input1,
            ROBIndex& rob_index0,
            ROBIndex& rob_index1
        ) noexcept
        {
            const std::size_t valid_count = static_cast<std::size_t>(input0.valid) + static_cast<std::size_t>(input1.valid);

            if (valid_count == 0)
            {
                return true;
            }

            // Checking if there's enough room in ROB
            if (rob_.capacity() - rob_.size() < valid_count)
            {
                return false;
            }

            // Checking if there's enough room in IssueQueue
            if (issue_queue_ != nullptr && (issue_queue_->capacity() - issue_queue_->size() < valid_count))
            {
                return false;
            }

            // Dispatch lane 0 first
            if (input0.valid)
            {
                if (!dispatch(input0, rob_index0))
                {
                    return false;
                }
            }

            // Dispatch lane 1
            if (input1.valid)
            {
                if (!dispatch(input1, rob_index1))
                {
                    return false;
                }
            }

            return true;
        }

    private:
        ReOrderBuffer<ROBCapacity>& rob_;
        IssueQueue<IQCapacity>* issue_queue_{nullptr};
        PhysicalRegisterFile<PRFCount>* prf_{nullptr};
    };

    template<std::size_t ROBCapacity>
    DispatchUnit(ReOrderBuffer<ROBCapacity>&) -> DispatchUnit<ROBCapacity, 16, 32>;

    template<std::size_t ROBCapacity, std::size_t IQCapacity, std::size_t PRFCount>
    DispatchUnit(ReOrderBuffer<ROBCapacity>&, IssueQueue<IQCapacity>&, PhysicalRegisterFile<PRFCount>&)
        -> DispatchUnit<ROBCapacity, IQCapacity, PRFCount>;
}

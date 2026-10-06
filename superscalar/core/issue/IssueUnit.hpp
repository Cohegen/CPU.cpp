#pragma once

#include <cstddef>
#include <vector>

#include "../execution/ExecutionRequest.hpp"
#include "IssueQueue.hpp"

namespace cpu {

class IssueUnit
{
public:
    explicit IssueUnit(std::size_t issue_width = 1) noexcept
        : issue_width_(issue_width)
    {
    }

    template<std::size_t Capacity>
    std::vector<ExecutionRequest>
    issue(IssueQueue<Capacity>& issue_queue);

    void reset() noexcept
    {
    }

private:
    std::size_t issue_width_{};
};

template<std::size_t Capacity>
std::vector<ExecutionRequest>
IssueUnit::issue(IssueQueue<Capacity>& issue_queue)
{
    std::vector<ExecutionRequest> requests;
    requests.reserve(issue_width_);

    for (std::size_t i = 0; i < issue_width_; ++i)
    {
        auto* entry = issue_queue.select_ready();

        if (entry == nullptr)
        {
            break;
        }

        ExecutionRequest request{};

        request.valid = true;

        request.pc = entry->pc;
        request.instruction = entry->instruction;
        request.opcode = entry->opcode;

        request.physical_rs1 = entry->physical_rs1;
        request.physical_rs2 = entry->physical_rs2;
        request.physical_rd = entry->physical_rd;

        request.rs1_value = entry->rs1_value;
        request.rs2_value = entry->rs2_value;

        request.immediate = entry->immediate;

        request.alu_operation = entry->alu_operation;

        request.operand_a = entry->operand_a;
        request.operand_b = entry->operand_b;

        request.memory_read = entry->memory_read;
        request.memory_write = entry->memory_write;

        request.control_flow = entry->control_flow;

        request.rob_index = entry->rob_index;

        request.halt = entry->halt;

        issue_queue.mark_issued(entry);

        requests.push_back(request);

        /*
         Remove the entry after successfully creating the
         execution request.
         */
        issue_queue.remove(entry);
    }

    return requests;
}

}

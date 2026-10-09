/*
BranchRecoveryUnit decides which speculative
state should be discarded
*/

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "BranchResult.hpp"
#include "../rename/RegisterAliasTable.hpp"
#include "../rename/PhysicalRegisterFreeList.hpp"
#include "../rob/ReOrderBuffer.hpp"
#include "../issue/IssueQueue.hpp"

namespace cpu
{

template<
    std::size_t ROBCapacity = 16,
    std::size_t IssueQueueCapacity = 16
>
class BranchRecoveryUnit
{
public:

    using ROB = ReOrderBuffer<ROBCapacity>;
    using IssueQueueType = IssueQueue<IssueQueueCapacity>;

    struct Control
    {
        bool recover{false};

        bool fetch_redirect{false};
        std::uint32_t redirect_pc{0};

        bool fetch_decode_flush{false};
    };

    void reset() noexcept
    {
        control_ = {};
    }

    [[nodiscard]]
    const Control& control() const noexcept
    {
        return control_;
    }

    [[nodiscard]]
    bool recover(
        const BranchResult& branch_result,
        ROB& rob,
        RegisterAliasTable& rat,
        PhysicalRegisterFreeList& free_list,
        IssueQueueType& issue_queue
    ) noexcept
    {
        // Recovery signals are pulses
        control_ = {};

 
        // No valid branch resolution.
        if (!branch_result.valid)
            return false;

        // Correct prediction: nothing to recover.

        if (!branch_result.mispredicted)
            return false;


        // Locate the branch in the ROB

        auto* branch_entry =
            rob.entry(branch_result.rob_index);

        if (branch_entry == nullptr)
            return false;

        if (!branch_entry->valid)
            return false;

        if (!branch_entry->branch)
            return false;

        if (!branch_entry->has_checkpoint)
            return false;

       
        // 1. Restore RAT
        rat.restore(branch_entry->checkpoint);

        // Squash younger ROB entries
        std::vector<typename ROB::Index> squashed_indices;

        const auto squashed_entries =rob.squash_younger_than(branch_result.rob_index,squashed_indices);

     
        // 3. Return physical registers belonging to squashed instruction
        for (const auto& entry : squashed_entries)
        {
            if (!entry.register_write)
                continue;

            free_list.release(entry.physical_rd);
        }

  
        // 4. Remove corresponding Issue Queue entries
        issue_queue.squash(squashed_indices);

     
        // 5. Flush frontend instructions
        control_.fetch_decode_flush = true;

        
        // 6. Redirect Fetch to the actual next PC
        control_.fetch_redirect = true;
        control_.redirect_pc =
            branch_result.redirect_pc;

        control_.recover = true;

        return true;
    }

private:

    Control control_{};
};

} // namespace cpu
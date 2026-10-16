#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "ArchitecturalRegisterFile.hpp"
#include "StoreBuffer.hpp"
#include "../dispatch/ReOrderBuffer.hpp"
#include "../dispatch/ROBEntry.hpp"
#include "../issue/PhysicalRegisterFile.hpp"
#include "../rename/PhysicalRegisterFreeList.hpp"
#include "../rename/RegisterAliasTable.hpp"

namespace cpu
{
    template<
        std::size_t ROBCapacity = 16,
        std::size_t PRFCount = 64,
        std::size_t ARFCount = 16,
        std::size_t StoreBufferCapacity = 16
    >
    class CommitUnit
    {
    public:
        using ROB = ReOrderBuffer<ROBCapacity>;
        using PRF = PhysicalRegisterFile<PRFCount>;
        using ARF = ArchitecturalRegisterFile<ARFCount>;
        using SB  = StoreBuffer<StoreBufferCapacity>;

        struct CommitEvent
        {
            bool valid{false};
            std::size_t rob_index{0};
            std::uint32_t pc{0};
            Opcode opcode{Opcode::NOP};
            bool register_write{false};
            Register architectural_rd{Register::R0};
            typename PRF::PhysicalRegister physical_rd{0};
            typename PRF::PhysicalRegister freed_physical_reg{0};
            std::uint32_t value{0};
            bool branch{false};
            bool store{false};
            bool halt{false};
        };

        explicit CommitUnit(std::size_t commit_width = 2) noexcept
            : commit_width_(commit_width)
        {
        }

        void reset() noexcept
        {
            committed_instruction_count_ = 0;
            halted_ = false;
        }

        [[nodiscard]]
        std::size_t committed_instruction_count() const noexcept
        {
            return committed_instruction_count_;
        }

        [[nodiscard]]
        bool halted() const noexcept
        {
            return halted_;
        }

        // Commit up to commit_width_ instructions in-order from ROB head
        std::vector<CommitEvent> step(
            ROB& rob,
            PRF& prf,
            ARF& arf,
            PhysicalRegisterFreeList& free_list,
            RegisterAliasTable& rat,
            SB* store_buffer = nullptr
        )
        {
            std::vector<CommitEvent> events;
            events.reserve(commit_width_);

            for (std::size_t i = 0; i < commit_width_; ++i)
            {
                if (rob.empty())
                {
                    break;
                }

                const auto* head_entry = rob.head();
                if (head_entry == nullptr || !head_entry->valid || !head_entry->completed)
                {
                    // In-order commit stops at first uncompleted instruction
                    break;
                }

                ROBEntry committed_rob_entry{};
                if (!rob.commit(committed_rob_entry))
                {
                    break;
                }

                CommitEvent ev{};
                ev.valid = true;
                ev.rob_index = committed_rob_entry.rob_index;
                ev.pc = committed_rob_entry.pc;
                ev.opcode = committed_rob_entry.opcode;
                ev.register_write = committed_rob_entry.register_write;
                ev.architectural_rd = committed_rob_entry.architectural_rd;
                ev.physical_rd = committed_rob_entry.physical_rd;
                ev.freed_physical_reg = committed_rob_entry.old_physical_rd;
                ev.branch = committed_rob_entry.branch;
                ev.store = committed_rob_entry.memory_write;
                ev.halt = committed_rob_entry.halt;

                // Architectural Register File (ARF) vs PRF Commit
                if (committed_rob_entry.register_write)
                {
                    const auto val = prf.read(committed_rob_entry.physical_rd);
                    ev.value = val;
                    arf.write(committed_rob_entry.architectural_rd, val);

                    // Overwritten physical register (old_physical_rd) is formally released
                    if (committed_rob_entry.old_physical_rd != PhysicalRegisterFreeList::INVALID_REGISTER)
                    {
                        free_list.release_back(committed_rob_entry.old_physical_rd);
                    }
                }

                // Branch Retirement: Pop/deallocate RAT checkpoint
                if (committed_rob_entry.branch && committed_rob_entry.has_checkpoint)
                {
                    rat.pop_checkpoint();
                }

                // Store Buffer / Memory Commit
                if (committed_rob_entry.memory_write && store_buffer != nullptr)
                {
                    (void)store_buffer->mark_committed(committed_rob_entry.rob_index);
                }

                if (committed_rob_entry.halt)
                {
                    halted_ = true;
                }

                ++committed_instruction_count_;
                events.push_back(ev);

                if (halted_)
                {
                    break;
                }
            }

            return events;
        }

    private:
        std::size_t commit_width_{2};
        std::size_t committed_instruction_count_{0};
        bool halted_{false};
    };
}

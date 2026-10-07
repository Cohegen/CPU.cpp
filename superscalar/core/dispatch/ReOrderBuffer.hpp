#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include "ROBEntry.hpp"
#include "../rename/RenameBundle.hpp"

namespace cpu
{
    template<std::size_t Capacity =16>
    class ReOrderBuffer
    {
        public:
          using Entry = ROBEntry;
          using Index = std::size_t;

          static_assert(Capacity>0,"ROB capacity must be greater than zero" );

          //state
          [[nodiscard]]
          bool empty() const noexcept
          {
            return count_ ==0;
          }

          [[nodiscard]]
          bool full() const noexcept
          {
            return count_ == Capacity;
          }

          [[nodiscard]]
          std::size_t size() const noexcept
          {
            return count_;
          }

          [[nodiscard]]
          constexpr std::size_t capacity() const noexcept
          {
            return Capacity;
          }

          //allocate an entry
          [[nodiscard]]
          bool allocate(const RenameBundle& bundle, Index& index) noexcept
          {
            if(!bundle.valid)
            {
                return false;
            }
            if(full())
            {
                return false;
            }
            index = tail_;
            Entry& entry = entries_[tail_];
            entry = Entry{};
            entry.valid = true;
            entry.completed = false;
            entry.rob_index = index;
            entry.pc = bundle.pc;
            entry.instruction = bundle.instruction;
            entry.opcode = bundle.opcode;

            //destination
            entry.register_write = bundle.register_write;
            entry.physical_rd = bundle.physical_rd;
            entry.old_physical_rd = bundle.old_physical_rd;

            //memory
            entry.memory_read = bundle.memory_read;
            entry.memory_write = bundle.memory_write;

            //control flow 
            entry.branch = bundle.control_flow == ControlFlow::BRANCH;
            entry.jump = bundle.control_flow == ControlFlow::JUMP;

            //halt
            entry.halt = bundle.halt;

            //advance tail
            tail_ = increment(tail_);
            ++count_;
            return true;
          }

          [[nodiscard]]
          bool allocate(const RenameBundle& bundle) noexcept
          {
            Index ignored = 0;
            return allocate(bundle, ignored);
          }

          [[nodiscard]]
          bool allocate(Index& index) noexcept
          {
            RenameBundle bundle{};
            bundle.valid = true;
            return allocate(bundle, index);
          }

          [[nodiscard]]
          bool allocate() noexcept
          {
            Index ignored = 0;
            return allocate(ignored);
          }

          //marks instruction complete
          void complete(Index index, std::uint32_t result) noexcept
          {
            if(index >= Capacity)
            {
                return;
            }
            Entry& entry = entries_[index];
            if(!entry.valid)
            {
                return;
            }
            entry.result = result;
            entry.completed = true;
          }

          void complete(Index index) noexcept
          {
            complete(index, 0);
          }

          //commits head entry
          [[nodiscard]]
          bool commit(Entry& committed_entry) noexcept
          {
            if(empty())
            {
                return false;
            }
            Entry& entry = entries_[head_];
            if(!entry.valid)
            {
                return false;
            }
            if(!entry.completed)
            {
                return false;
            }
            committed_entry = entry;
            entry = Entry{};
            head_ = increment(head_);
            --count_;
            return true;
          }

          [[nodiscard]]
          bool commit() noexcept
          {
            Entry ignored{};
            return commit(ignored);
          }

          //inspects head
          [[nodiscard]]
          const Entry* head() const noexcept
          {
            if(empty())
            {
                return nullptr;
            }
            return &entries_[head_];
          }

          [[nodiscard]]
          Entry* head() noexcept
          {
            if(empty())
            {
                return nullptr;
            }
            return &entries_[head_];
          }

          //inspects entry by index
          const Entry* entry(Index index) const noexcept
          {
            if(index >= Capacity)
            {
                return nullptr;
            }
            return &entries_[index];
          }

          [[nodiscard]]
          Entry* entry(Index index) noexcept
          {
            if(index >= Capacity)
            {
                return nullptr;
            }
            return &entries_[index];
          }

          [[nodiscard]]
          const Entry& raw_entry(Index index) const noexcept
          {
            return entries_[index];
          }

          [[nodiscard]]
          Index head_index() const noexcept
          {
            return head_;
          }

          [[nodiscard]]
          Index tail_index() const noexcept
          {
            return tail_;
          }

          [[nodiscard]]
          std::vector<ROBEntry>squash_younger_than(Index branch_index)noexcept
          {
            std::vector<ROBEntry> squashed;

            if (count_ == 0)
            {
                return squashed;
            }
        
            // Locate the branch in logical ROB order.
           
        
            Index current = head_;
            std::size_t branch_position = 0;
            bool found = false;
        
            for (std::size_t position = 0;
                 position < count_;
                 ++position)
            {
                if (current == branch_index)
                {
                    branch_position = position;
                    found = true;
                    break;
                }
        
                current = next_index(current);
            }
        
            if (!found)
            {
                return squashed;
            }
        
            // Number of entries younger than the branch.
            const std::size_t younger_count =
                count_ - branch_position - 1;
        
            
            // Removing younger entries in program order.
          
        
            Index current_younger = next_index(branch_index);
        
            for (std::size_t i = 0;
                 i < younger_count;
                 ++i)
            {
                if (entries_[current_younger].valid)
                {
                    entries_[current_younger].rob_index = current_younger;
                    squashed.push_back(entries_[current_younger]);
                }
        
                entries_[current_younger] = ROBEntry{};
        
                current_younger =
                    next_index(current_younger);
            }
        
           
            // Branch becomes the youngest surviving ROB entry.
            
        
            tail_ = next_index(branch_index);
        
            count_ -= younger_count;
        
            return squashed;
        
          }

          [[nodiscard]]
          std::vector<Index> squash_younger_indices(Index branch_index) noexcept
          {
              auto squashed = squash_younger_than(branch_index);
              std::vector<Index> indices;
              indices.reserve(squashed.size());
              for (const auto& entry : squashed)
              {
                  indices.push_back(entry.rob_index);
              }
              return indices;
          }
          [[nodiscard]]
          Index next_index(Index index)const noexcept
          {
            return (index +1) % Capacity;
          }

          //reset
          void reset() noexcept
          {
            for(auto& entry : entries_)
            {
                entry = Entry{};
            }
            head_ = 0;
            tail_ = 0;
            count_ = 0;
          }

        private:
           [[nodiscard]]
           static constexpr Index increment(Index index) noexcept
           {
             return (index + 1) % Capacity;
           }

           std::array<Entry, Capacity> entries_{};
           Index head_{0};
           Index tail_{0};
           std::size_t count_{0};
    };
}

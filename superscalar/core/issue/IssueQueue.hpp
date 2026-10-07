#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <vector>
#include "IssueQueueEntry.hpp"

#if __has_include("../dispatch/ROBEntry.hpp")
#include "../dispatch/ROBEntry.hpp"
#elif __has_include("ROBEntry.hpp")
#include "ROBEntry.hpp"
#elif __has_include("dispatch/ROBEntry.hpp")
#include "dispatch/ROBEntry.hpp"
#endif

namespace cpu {
    template<std::size_t Capacity=16>
    class IssueQueue 
    {
        public:
          using Entry = IssueQueueEntry;
          using Index = std::size_t;
          using PhysicalRegister = Entry::PhysicalRegister;

          static_assert(Capacity>0,"Issue Queue capacity must be greater than zero" );

          //queue state
          [[nodiscard]]
          bool empty() const noexcept 
          {
            return count_ == 0;
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

          //insert instruction
          [[nodiscard]]
          bool insert(Entry entry) noexcept
          {
            if(full())
            {
                return false;
            }
            entry.valid = true;
            for(Index i=0; i<Capacity; ++i)
            {
                if(!entries_[i].valid)
                {
                    entries_[i] = entry;
                    ++count_;
                    return true;
                }
            }
            return false;
          }

          //finding a ready instruction
          [[nodiscard]]
          Entry* select_ready() noexcept
          {
            for(auto& entry: entries_)
            {
                if(!entry.valid)
                {
                    continue;
                }
                if(entry.issued)
                {
                    continue;
                }
                if(entry.rs1_ready && entry.rs2_ready)
                {
                    return &entry;
                }
            }
            return nullptr;
          }

          [[nodiscard]]
          const Entry* select_ready() const noexcept
          {
            for(const auto& entry: entries_)
            {
                if(!entry.valid)
                {
                    continue;
                }
                if(entry.issued)
                {
                    continue;
                }
                if(entry.rs1_ready && entry.rs2_ready)
                {
                    return &entry;
                }
            }
            return nullptr;
          }

          //mark instruction as issued
          void mark_issued(Index index) noexcept
          {
            if(index >= Capacity)
            {
                return;
            }
            if(!entries_[index].valid)
            {
                return;
            }
            entries_[index].issued = true;
          }

          void mark_issued(const Entry* entry_ptr) noexcept
          {
            if(!entry_ptr) return;
            for(Index i = 0; i < Capacity; ++i)
            {
                if(&entries_[i] == entry_ptr)
                {
                    mark_issued(i);
                    return;
                }
            }
          }

          //removes instruction
          void remove(Index index) noexcept
          {
            if(index >= Capacity)
            {
                return;
            }
            if(!entries_[index].valid)
            {
                return;
            }
            entries_[index] = Entry{};
            --count_;
          }

          bool remove(const Entry* entry_ptr) noexcept
          {
            if(!entry_ptr) return false;
            for(Index i = 0; i < Capacity; ++i)
            {
                if(&entries_[i] == entry_ptr)
                {
                    remove(i);
                    return true;
                }
            }
            return false;
          }

       
          /*Squashing (Branch Misprediction Recovery)
          */
          // Removes all valid entries matching the given rob_index.
          //entries are removed regardless of whether issued is true or false.
          // An issued-but-uncommitted instruction remains speculative.
          std::size_t squash(std::size_t rob_index) noexcept
          {
              std::size_t removed = 0;
              for(Index i = 0; i < Capacity; ++i)
              {
                  if(entries_[i].valid && entries_[i].rob_index == rob_index)
                  {
                      entries_[i] = Entry{};
                      --count_;
                      ++removed;
                  }
              }
              return removed;
          }

          // Squash entries matching an initializer list of ROB indices, e.g. squash({7, 8}).
          std::size_t squash(std::initializer_list<std::size_t> rob_indices) noexcept
          {
              std::size_t removed = 0;
              for(std::size_t rob_idx : rob_indices)
              {
                  removed += squash(rob_idx);
              }
              return removed;
          }

          // Squash entries matching a vector of ROB indices.
          std::size_t squash(const std::vector<std::size_t>& rob_indices) noexcept
          {
              std::size_t removed = 0;
              for(std::size_t rob_idx : rob_indices)
              {
                  removed += squash(rob_idx);
              }
              return removed;
          }

          // Squash entries matching squashed ROBEntry list from ReOrderBuffer::squash_younger_than.
          std::size_t squash(const std::vector<ROBEntry>& squashed_entries) noexcept
          {
              std::size_t removed = 0;
              for(const auto& entry : squashed_entries)
              {
                  removed += squash(entry.rob_index);
              }
              return removed;
          }

          // Squash an entry matching a single ROBEntry.
          std::size_t squash(const ROBEntry& rob_entry) noexcept
          {
              return squash(rob_entry.rob_index);
          }

          // Query whether an entry with rob_index exists in the queue
          [[nodiscard]]
          bool contains_rob(std::size_t rob_index) const noexcept
          {
              for(const auto& entry : entries_)
              {
                  if(entry.valid && entry.rob_index == rob_index)
                  {
                      return true;
                  }
              }
              return false;
          }

          // Find an entry with rob_index
          [[nodiscard]]
          const Entry* find_rob(std::size_t rob_index) const noexcept
          {
              for(const auto& entry : entries_)
              {
                  if(entry.valid && entry.rob_index == rob_index)
                  {
                      return &entry;
                  }
              }
              return nullptr;
          }

          [[nodiscard]]
          Entry* find_rob(std::size_t rob_index) noexcept
          {
              for(auto& entry : entries_)
              {
                  if(entry.valid && entry.rob_index == rob_index)
                  {
                      return &entry;
                  }
              }
              return nullptr;
          }

          //waking-up a physical register
          void wakeup(PhysicalRegister physical_register,std::uint32_t value)noexcept
          {
            for(auto& entry:entries_)
            {
                if(!entry.valid)
                {
                    continue;
                }
                //source 1
                if(!entry.rs1_ready && entry.physical_rs1 == physical_register)
                {
                    entry.rs1_value = value;
                    entry.rs1_ready = true;
                }

                //souce 2
                if(!entry.rs2_ready && entry.physical_rs2 == physical_register)
                {
                    entry.rs2_value = value;
                    entry.rs2_ready = true;
                }
            }
          }

          //inspecting entry
          [[nodiscard]]
          Entry* entry(Index index)noexcept
          {
            if(index >= Capacity)
            {
                return nullptr;
            }
            return &entries_[index];
          }

          [[nodiscard]]
          const Entry* entry(Index index)const noexcept
          {
            if(index >= Capacity)
            {
                return nullptr;
            }
            return &entries_[index];
          }

          //reset
          void reset() noexcept
          {
            for(auto& entry: entries_)
            {
                entry =Entry{};
            }
            count_ = 0;
          }

          private:
            std::array<Entry,Capacity>entries_{};
            std::size_t count_{0};
    };

}

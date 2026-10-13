#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <optional>

namespace cpu
{
    template<std::size_t Capacity = 32>
    class LoadStoreQueue
    {
    public:
        enum class OpType : std::uint8_t { NONE, LOAD, STORE };

        struct Entry
        {
            bool valid{false};
            OpType type{OpType::NONE};
            std::size_t rob_index{0};
            std::uint32_t pc{0};

            // Address
            bool address_valid{false};
            std::uint32_t address{0};

            // Data (for stores: value to write; for loads: loaded value)
            bool data_valid{false};
            std::uint32_t data{0};

            // Execution state
            bool executed{false};
            bool completed{false};

            // Physical destination register (for loads)
            std::uint8_t physical_rd{0};
        };

        // Result of a load forwarding check
        struct LoadResult
        {
            bool hit{false};        // Did we find forwarding data?
            bool must_wait{false};   // Must stall: older store address not yet known
            std::uint32_t data{0};   // Forwarded data if hit
        };

        // Result of an order violation check
        struct ViolationCheck
        {
            bool violation{false};
            std::size_t violating_load_rob_index{0};
            std::uint32_t violating_load_pc{0};
        };

        LoadStoreQueue() noexcept
        {
            reset();
        }

        void reset() noexcept
        {
            entries_.clear();
        }

        [[nodiscard]]
        bool empty() const noexcept
        {
            return entries_.empty();
        }

        [[nodiscard]]
        std::size_t size() const noexcept
        {
            return entries_.size();
        }

        [[nodiscard]]
        bool full() const noexcept
        {
            return entries_.size() >= Capacity;
        }

        // Allocate a load or store entry during dispatch (maintains program order)
        [[nodiscard]]
        bool allocate(OpType type, std::size_t rob_index, std::uint32_t pc,
                      std::uint8_t physical_rd = 0) noexcept
        {
            if (full())
            {
                return false;
            }
            Entry entry{};
            entry.valid = true;
            entry.type = type;
            entry.rob_index = rob_index;
            entry.pc = pc;
            entry.physical_rd = physical_rd;
            entries_.push_back(entry);
            return true;
        }

        // Set the address of a load/store after address calculation
        bool set_address(std::size_t rob_index, std::uint32_t address) noexcept
        {
            auto* e = find(rob_index);
            if (e == nullptr)
            {
                return false;
            }
            e->address = address;
            e->address_valid = true;
            return true;
        }

        // Set store data
        bool set_store_data(std::size_t rob_index, std::uint32_t data) noexcept
        {
            auto* e = find(rob_index);
            if (e == nullptr || e->type != OpType::STORE)
            {
                return false;
            }
            e->data = data;
            e->data_valid = true;
            return true;
        }

        // Execute a load: check older stores for forwarding
        // Scans backward from the load's position through older entries
        [[nodiscard]]
        LoadResult execute_load(std::size_t load_rob_index) const noexcept
        {
            LoadResult result{};

            // Find the load's position in the queue
            std::size_t load_pos = entries_.size();
            for (std::size_t i = 0; i < entries_.size(); ++i)
            {
                if (entries_[i].valid && entries_[i].rob_index == load_rob_index)
                {
                    load_pos = i;
                    break;
                }
            }

            if (load_pos >= entries_.size())
            {
                return result; // Load not found
            }

            const auto& load_entry = entries_[load_pos];
            if (!load_entry.address_valid)
            {
                return result; // Load address not yet known
            }

            // Search backward through older entries for stores
            for (std::size_t i = load_pos; i > 0; --i)
            {
                const auto& older = entries_[i - 1];
                if (!older.valid || older.type != OpType::STORE)
                {
                    continue;
                }

                // Older store with unknown address — must conservatively wait
                if (!older.address_valid)
                {
                    result.must_wait = true;
                    return result;
                }

                // Older store with matching address
                if (older.address == load_entry.address)
                {
                    if (older.data_valid)
                    {
                        // Forward data from older store
                        result.hit = true;
                        result.data = older.data;
                        return result;
                    }
                    else
                    {
                        // Matching address but data not ready — must wait
                        result.must_wait = true;
                        return result;
                    }
                }
            }

            // No older store conflicts — load goes to memory
            return result;
        }

        // Complete a load with a value (from forwarding or memory)
        bool complete_load(std::size_t rob_index, std::uint32_t data) noexcept
        {
            auto* e = find(rob_index);
            if (e == nullptr || e->type != OpType::LOAD)
            {
                return false;
            }
            e->data = data;
            e->data_valid = true;
            e->completed = true;
            return true;
        }

        // Mark entry as executed
        bool mark_executed(std::size_t rob_index) noexcept
        {
            auto* e = find(rob_index);
            if (e == nullptr)
            {
                return false;
            }
            e->executed = true;
            return true;
        }

        // When a store resolves its address, check if any younger load
        // already executed to the same address (memory order violation)
        [[nodiscard]]
        ViolationCheck check_order_violation(std::size_t store_rob_index) const noexcept
        {
            ViolationCheck result{};

            // Find the store's position
            std::size_t store_pos = entries_.size();
            for (std::size_t i = 0; i < entries_.size(); ++i)
            {
                if (entries_[i].valid && entries_[i].rob_index == store_rob_index)
                {
                    store_pos = i;
                    break;
                }
            }

            if (store_pos >= entries_.size())
            {
                return result;
            }

            const auto& store_entry = entries_[store_pos];
            if (!store_entry.address_valid)
            {
                return result;
            }

            // Search forward through younger entries for loads that already executed
            for (std::size_t i = store_pos + 1; i < entries_.size(); ++i)
            {
                const auto& younger = entries_[i];
                if (!younger.valid || younger.type != OpType::LOAD)
                {
                    continue;
                }

                // Younger load executed to the same address — violation!
                if (younger.executed &&
                    younger.address_valid &&
                    younger.address == store_entry.address)
                {
                    result.violation = true;
                    result.violating_load_rob_index = younger.rob_index;
                    result.violating_load_pc = younger.pc;
                    return result;
                }
            }

            return result;
        }

        // Remove a committed entry
        bool remove(std::size_t rob_index) noexcept
        {
            for (auto it = entries_.begin(); it != entries_.end(); ++it)
            {
                if (it->valid && it->rob_index == rob_index)
                {
                    entries_.erase(it);
                    return true;
                }
            }
            return false;
        }

        // Squash entries matching the given ROB indices
        void squash(const std::vector<std::size_t>& squashed_rob_indices) noexcept
        {
            for (const auto rob_idx : squashed_rob_indices)
            {
                for (auto it = entries_.begin(); it != entries_.end(); )
                {
                    if (it->valid && it->rob_index == rob_idx)
                    {
                        it = entries_.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
            }
        }

        // Access entries for testing
        [[nodiscard]]
        const std::vector<Entry>& entries() const noexcept
        {
            return entries_;
        }

        // Find entry by ROB index
        [[nodiscard]]
        const Entry* find(std::size_t rob_index) const noexcept
        {
            for (const auto& entry : entries_)
            {
                if (entry.valid && entry.rob_index == rob_index)
                {
                    return &entry;
                }
            }
            return nullptr;
        }

        [[nodiscard]]
        Entry* find(std::size_t rob_index) noexcept
        {
            for (auto& entry : entries_)
            {
                if (entry.valid && entry.rob_index == rob_index)
                {
                    return &entry;
                }
            }
            return nullptr;
        }

    private:
        std::vector<Entry> entries_;
    };
}

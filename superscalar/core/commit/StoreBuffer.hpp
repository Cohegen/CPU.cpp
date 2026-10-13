#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <optional>

#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/signals/clock.hpp>

#include "../dispatch/ROBEntry.hpp"

namespace cpu
{
    template<std::size_t Capacity = 16>
    class StoreBuffer
    {
    public:
        struct Entry
        {
            bool valid{false};
            std::size_t rob_index{0};
            std::uint32_t pc{0};
            std::uint32_t address{0};
            std::uint32_t data{0};
            bool address_ready{false};
            bool data_ready{false};
            bool committed{false}; // Marked true when store instruction commits in ROB
        };

        StoreBuffer() noexcept
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

        // Allocate a speculative store slot during dispatch
        [[nodiscard]]
        bool allocate(std::size_t rob_index, std::uint32_t pc) noexcept
        {
            if (full())
            {
                return false;
            }
            Entry entry{};
            entry.valid = true;
            entry.rob_index = rob_index;
            entry.pc = pc;
            entries_.push_back(entry);
            return true;
        }

        // Execution stage updates address and store data
        bool update(std::size_t rob_index, std::uint32_t address, std::uint32_t data) noexcept
        {
            for (auto& entry : entries_)
            {
                if (entry.valid && entry.rob_index == rob_index)
                {
                    entry.address = address;
                    entry.data = data;
                    entry.address_ready = true;
                    entry.data_ready = true;
                    return true;
                }
            }
            return false;
        }

        // Forwarding check for speculative loads
        [[nodiscard]]
        std::optional<std::uint32_t> forward(std::uint32_t address) const noexcept
        {
            // Search youngest to oldest for the matching address
            for (auto it = entries_.rbegin(); it != entries_.rend(); ++it)
            {
                if (it->valid && it->address_ready && it->address == address)
                {
                    if (it->data_ready)
                    {
                        return it->data;
                    }
                    return std::nullopt; // Waiting on store data
                }
            }
            return std::nullopt;
        }

        // Commit: called when the store instruction reaches ROB head
        [[nodiscard]]
        bool mark_committed(std::size_t rob_index) noexcept
        {
            for (auto& entry : entries_)
            {
                if (entry.valid && entry.rob_index == rob_index)
                {
                    entry.committed = true;
                    return true;
                }
            }
            return false;
        }

        // Pop the committed store entry at the head to write to memory
        [[nodiscard]]
        std::optional<Entry> pop_committed() noexcept
        {
            if (!entries_.empty() && entries_.front().valid && entries_.front().committed)
            {
                Entry entry = entries_.front();
                entries_.erase(entries_.begin());
                return entry;
            }
            return std::nullopt;
        }

        // Squash younger stores on misprediction
        void squash(const std::vector<std::size_t>& squashed_rob_indices) noexcept
        {
            for (const auto rob_idx : squashed_rob_indices)
            {
                for (auto it = entries_.begin(); it != entries_.end(); )
                {
                    if (it->valid && !it->committed && it->rob_index == rob_idx)
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

        [[nodiscard]]
        const std::vector<Entry>& entries() const noexcept
        {
            return entries_;
        }

    private:
        std::vector<Entry> entries_{};
    };
}

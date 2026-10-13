#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "../memory/LoadStoreQueue.hpp"
#include "../memory/data/DataMemory.hpp"
#include "../commit/StoreBuffer.hpp"
#include "../dispatch/ReOrderBuffer.hpp"
#include "../issue/IssueQueue.hpp"
#include "../issue/IssueQueueEntry.hpp"
#include "../issue/PhysicalRegisterFile.hpp"
#include "ExecutionRequest.hpp"

namespace cpu
{

template<
    std::size_t ROBCapacity = 16,
    std::size_t IQCapacity = 16,
    std::size_t PRFCount = 64,
    std::size_t LSQCapacity = 32,
    std::size_t StoreBufferCapacity = 16,
    std::size_t DataMemorySize = 1024
>
class MemoryExecutionUnit
{
public:
    using ROB = ReOrderBuffer<ROBCapacity>;
    using IQ  = IssueQueue<IQCapacity>;
    using PRF = PhysicalRegisterFile<PRFCount>;
    using LSQ = LoadStoreQueue<LSQCapacity>;
    using SB  = StoreBuffer<StoreBufferCapacity>;
    using DM  = DataMemory<DataMemorySize>;

    struct MemoryResult
    {
        bool valid{false};
        bool is_load{false};
        bool is_store{false};
        std::size_t rob_index{0};
        std::uint8_t physical_rd{0};
        std::uint32_t address{0};
        std::uint32_t data{0};
        bool order_violation{false};
        std::size_t violating_load_rob_index{0};
    };

    MemoryExecutionUnit(
        ROB& rob, IQ& issue_queue, PRF& prf,
        LSQ& lsq, SB& store_buffer, DM& data_memory
    ) noexcept
        : rob_(rob),
          issue_queue_(issue_queue),
          prf_(prf),
          lsq_(lsq),
          store_buffer_(store_buffer),
          data_memory_(data_memory)
    {
    }

    // Execute one memory operation per cycle from the issue queue
    std::vector<MemoryResult> execute() noexcept
    {
        std::vector<MemoryResult> results;

        // Find a ready memory operation in the issue queue
        for (typename IQ::Index index = 0; index < issue_queue_.capacity(); ++index)
        {
            auto* entry = issue_queue_.entry(index);
            if (entry != nullptr && ready_for_memory_execution(*entry))
            {
                // Capture entry data before removing from IQ
                IssueQueueEntry captured = *entry;
                issue_queue_.remove(index);

                MemoryResult result;
                if (captured.memory_read)
                {
                    result = process_load(captured);
                }
                else if (captured.memory_write)
                {
                    result = process_store(captured);
                }

                if (result.valid)
                {
                    write_back(result);
                    results.push_back(result);
                }

                // Single memory port: only one op per cycle
                break;
            }
        }

        return results;
    }

private:
    [[nodiscard]]
    static bool ready_for_memory_execution(const IssueQueueEntry& entry) noexcept
    {
        return entry.valid &&
               !entry.issued &&
               entry.rs1_ready &&
               entry.rs2_ready &&
               (entry.memory_read || entry.memory_write);
    }

    [[nodiscard]]
    static std::uint32_t calculate_address(const IssueQueueEntry& entry) noexcept
    {
        return entry.rs1_value + static_cast<std::uint32_t>(entry.immediate);
    }

    MemoryResult process_load(const IssueQueueEntry& entry) noexcept
    {
        MemoryResult result{};
        result.valid = true;
        result.is_load = true;
        result.rob_index = entry.rob_index;
        result.physical_rd = entry.physical_rd;

        const std::uint32_t address = calculate_address(entry);
        result.address = address;

        // Set address in LSQ
        lsq_.set_address(entry.rob_index, address);

        // Check LSQ for store-to-load forwarding
        auto fwd = lsq_.execute_load(entry.rob_index);

        if (fwd.hit)
        {
            // Data forwarded from an older store
            result.data = fwd.data;
        }
        else
        {
            // No forwarding available — read from data memory
            auto mem_data = data_memory_.read(address);
            result.data = mem_data.value_or(0);
        }

        // Complete and mark executed in LSQ
        lsq_.complete_load(entry.rob_index, result.data);
        lsq_.mark_executed(entry.rob_index);

        return result;
    }

    MemoryResult process_store(const IssueQueueEntry& entry) noexcept
    {
        MemoryResult result{};
        result.valid = true;
        result.is_store = true;
        result.rob_index = entry.rob_index;

        const std::uint32_t address = calculate_address(entry);
        const std::uint32_t store_data = entry.rs2_value;
        result.address = address;
        result.data = store_data;

        // Set address and data in LSQ
        lsq_.set_address(entry.rob_index, address);
        lsq_.set_store_data(entry.rob_index, store_data);

        // Update the StoreBuffer (for commit-time memory write)
        store_buffer_.update(entry.rob_index, address, store_data);

        // Check for memory order violations
        auto violation = lsq_.check_order_violation(entry.rob_index);
        if (violation.violation)
        {
            result.order_violation = true;
            result.violating_load_rob_index = violation.violating_load_rob_index;
        }

        // Mark executed in LSQ
        lsq_.mark_executed(entry.rob_index);

        return result;
    }

    void write_back(const MemoryResult& result) noexcept
    {
        if (!result.valid)
        {
            return;
        }

        const auto* rob_entry = rob_.entry(result.rob_index);
        if (rob_entry == nullptr || !rob_entry->valid)
        {
            return;
        }

        if (result.is_load)
        {
            // Write loaded data to PRF and wake up dependents
            prf_.write(result.physical_rd, result.data);
            issue_queue_.wakeup(result.physical_rd, result.data);
        }

        // Mark ROB entry complete
        rob_.complete(result.rob_index, result.data);
    }

    ROB& rob_;
    IQ&  issue_queue_;
    PRF& prf_;
    LSQ& lsq_;
    SB&  store_buffer_;
    DM&  data_memory_;
};

} // namespace cpu

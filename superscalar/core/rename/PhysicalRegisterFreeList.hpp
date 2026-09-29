#pragma once

#include <cstddef>
#include <cstdint>
#include <queue>

namespace cpu
{
    class PhysicalRegisterFreeList
    {
    public:
        using PhysicalRegister = std::uint8_t;
        static constexpr PhysicalRegister INVALID_REGISTER = 0xFF;

        explicit PhysicalRegisterFreeList(
            std::size_t physical_register_count = 64,
            std::size_t initially_mapped_registers = 16
        )
        {
            reset(physical_register_count, initially_mapped_registers);
        }

        void reset(
            std::size_t physical_register_count = 64,
            std::size_t initially_mapped_registers = 16
        )
        {
            while (!free_registers_.empty())
            {
                free_registers_.pop();
            }

            for (std::size_t i = initially_mapped_registers; i < physical_register_count; ++i)
            {
                free_registers_.push(static_cast<PhysicalRegister>(i));
            }
        }

        [[nodiscard]]
        bool empty() const noexcept
        {
            return free_registers_.empty();
        }

        [[nodiscard]]
        std::size_t size() const noexcept
        {
            return free_registers_.size();
        }

        [[nodiscard]]
        bool can_allocate(std::size_t count = 1) const noexcept
        {
            return free_registers_.size() >= count;
        }

        [[nodiscard]]
        PhysicalRegister allocate() noexcept
        {
            if (free_registers_.empty())
            {
                return INVALID_REGISTER;
            }
            const PhysicalRegister physical_register = free_registers_.front();
            free_registers_.pop();
            return physical_register;
        }

        void release(PhysicalRegister physical_register) noexcept
        {
            if (physical_register == INVALID_REGISTER)
            {
                return;
            }
            free_registers_.push(physical_register);
        }

    private:
        std::queue<PhysicalRegister> free_registers_;
    };
}
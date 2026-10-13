#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "../../include/isa/Registers.hpp"

namespace cpu
{
    template<std::size_t ArchitecturalRegisterCount = 16>
    class ArchitecturalRegisterFile
    {
    public:
        using Value = std::uint32_t;

        ArchitecturalRegisterFile() noexcept
        {
            reset();
        }

        void reset() noexcept
        {
            registers_.fill(0);
        }

        [[nodiscard]]
        Value read(Register reg) const noexcept
        {
            const std::size_t index = static_cast<std::size_t>(reg);
            if (index >= ArchitecturalRegisterCount)
            {
                return 0;
            }
            return registers_[index];
        }

        void write(Register reg, Value value) noexcept
        {
            const std::size_t index = static_cast<std::size_t>(reg);
            if (index >= ArchitecturalRegisterCount)
            {
                return;
            }
            registers_[index] = value;
        }

        [[nodiscard]]
        Value operator[](Register reg) const noexcept
        {
            return read(reg);
        }

        [[nodiscard]]
        static constexpr std::size_t size() noexcept
        {
            return ArchitecturalRegisterCount;
        }

    private:
        std::array<Value, ArchitecturalRegisterCount> registers_{};
    };
}

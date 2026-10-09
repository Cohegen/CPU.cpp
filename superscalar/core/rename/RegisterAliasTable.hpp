#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#if __has_include("isa/Registers.hpp")
#include "isa/Registers.hpp"
#elif __has_include("Registers.hpp")
#include "Registers.hpp"
#else
#include "../../../include/isa/Registers.hpp"
#endif

namespace cpu
{
    class RegisterAliasTable
    {
    public:
        
        using PhysicalRegister = std::uint8_t;
        static constexpr std::size_t ArchitecturalRegisterCount = REGISTER_COUNT;
        struct CheckPoint
        {
           std::array<PhysicalRegister,16>mappings{};
           bool valid{false};
        };
        CheckPoint checkpoint() const noexcept
        {
            CheckPoint snapshot{};

            for(std::size_t i=0;i<mappings_.size();++i)
            {
                snapshot.mappings[i] = mappings_[i];
            }
            snapshot.valid =true;
            return snapshot;
        }

        void restore(const CheckPoint& checkpoint)noexcept
        {
            if(!checkpoint.valid)
            {
                return;
            }
            for(std::size_t i=0;i<mappings_.size();++i)
            {
                mappings_[i] = checkpoint.mappings[i];
            }
        }
        explicit RegisterAliasTable(std::size_t physical_reg_count = 64)
            : physical_reg_count_(physical_reg_count)
        {
            reset();
        }

        void reset() noexcept
        {
            for (std::size_t i = 0; i < ArchitecturalRegisterCount; ++i)
            {
                table_[i] = static_cast<PhysicalRegister>(i);
            }
        }

        [[nodiscard]]
        PhysicalRegister lookup(Register architectural_register) const noexcept
        {
            const std::size_t index = static_cast<std::size_t>(architectural_register);
            if (index >= ArchitecturalRegisterCount)
            {
                return 0;
            }
            return table_[index];
        }

        [[nodiscard]]
        PhysicalRegister operator[](Register architectural_register) const noexcept
        {
            return lookup(architectural_register);
        }

        void set(Register architectural_register, PhysicalRegister physical_register) noexcept
        {
            const std::size_t index = static_cast<std::size_t>(architectural_register);
            if (index >= ArchitecturalRegisterCount)
            {
                return;
            }
            if (physical_register >= physical_reg_count_)
            {
                return;
            }
            table_[index] = physical_register;
        }

        [[nodiscard]]
        std::size_t physical_reg_count() const noexcept
        {
            return physical_reg_count_;
        }

        [[nodiscard]]
        const std::array<PhysicalRegister, ArchitecturalRegisterCount>& table() const noexcept
        {
            return table_;
        }

    private:
        std::array<PhysicalRegister, ArchitecturalRegisterCount> table_{};
        std::size_t physical_reg_count_{64};
    };
}
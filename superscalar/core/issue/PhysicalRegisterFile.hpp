#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace cpu
{
    template<std::size_t PhysicalRegisterCount=32>
    class PhysicalRegisterFile
    {
        public:
           using PhysicalRegister = std::uint8_t;
           using Value = std::uint32_t;

           static constexpr PhysicalRegister INVALID_REGISTER = 0xFF;
           static_assert(PhysicalRegisterCount > 0,"Physical register count must be greater than zero" );
           static_assert( PhysicalRegisterCount <= 255,  "Physical register count must fit in uint8_t");

           //reset
           void reset() noexcept
           {
             for(auto& register_state :registers_)
             {
                register_state.value = 0;
                register_state.ready = false;
             }
           }

           //read value
           [[nodiscard]]
           Value read( PhysicalRegister physical_register) const noexcept
           {
            if(!valid_register(physical_register))
            {
                return 0;
            }
            return registers_[physical_register].value;
           }

           //checking readiness
           bool ready(PhysicalRegister physical_register)const noexcept
           {
              if(!valid_register(physical_register))
              {
                return false;

              }
              return registers_[physical_register].ready;
           }

           //allocate a destination
           void allocate(PhysicalRegister physical_register)noexcept
           {
              if(!valid_register(physical_register))
              {
                return;
              }
              registers_[physical_register].value = 0;
              registers_[physical_register].ready = false;
           }

           //write completed result
           void write(PhysicalRegister physical_register,Value value)noexcept
           {
               if(!valid_register(physical_register))
               {
                return;
               }
               registers_[physical_register].value = value;
               registers_[physical_register].ready = true;
           }

           //explicitly marks register as ready
           void set_ready(PhysicalRegister physical_register,bool ready_state)noexcept
           {
            if(!valid_register(physical_register))
            {
                return;
            }
            registers_[physical_register].ready = ready_state;
           }

           //number of physical registers
           [[nodiscard]]
           constexpr std::size_t size() const noexcept
           {
              return PhysicalRegisterCount;
           }
        private:
           struct RegisterState
           {
             Value value{0};
             bool ready{false};
           };

           [[nodiscard]]
           static constexpr bool valid_register(PhysicalRegister physical_register)noexcept
           {
               return physical_register < PhysicalRegisterCount;
           }

           std::array<RegisterState,PhysicalRegisterCount>registers_{};

    };

}
#pragma once

#include <cstddef>

#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/simulator/Component.hpp>

namespace cpu
{
    template <
     std::size_t CPUAddressWidth=32,
     std::size_t InstructionWidth=32
    >
    class InstructionMemoryInterface : public logic::Component
    {
        public:
              InstructionMemoryInterface(
                logic::Wire& request,
                logic::Bus<CPUAddressWidth>&address,
                logic::Wire& response,
                logic::Bus<InstructionWidth>& instruction
              ): request_(request),
                 address_(address),
                 response_(response),
                 instruction_(instruction){}

              void evaluate() noexcept override
              {
              }
        private:
          logic::Wire& request_;
          logic::Bus<CPUAddressWidth>& address_;

          logic::Wire& response_;
          logic::Bus<InstructionWidth>& instruction_;
    };
}

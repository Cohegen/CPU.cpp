#pragma once

#include <cstddef>
#include <cstdint>
#include<vector>
#include <logic/signals/clock.hpp>
#include <logic/signals/wire.hpp>
#include "single_cycle_cpu/SingleCycleDatapath.hpp"

namespace cpu{
    template<
    std::size_t AddressWidth =32,
    std::size_t DataWidth = 32,
    std::size_t InstructionWidth =32,
    std::size_t RegisterAddressWidth =4,
    std::size_t InstructionMemoryAddressWidth =8,
    std::size_t DataMemoryAddressWidth =8
    >
    class CPU{
        public:
          CPU():datapath_(clock_,reset_){}

          void reset() noexcept{
            reset_.write(logic::LogicState::HIGH);

            datapath_.evaluate();

            reset_.write(logic::LogicState::LOW);
          }

          void step() noexcept{
            if(halted_){
                return;
            }
            datapath_.evaluate();

            //rising edge
            clock_.tick();

            //updates in the datapath
            datapath_.evaluate();

            //checking whether the instruction that has been executed was a halt
            if(datapath_.control_signals().halt){
                halted_ = true;
                
            }
           
          }

          void run() noexcept{
            while(!halted_){
                step();
            }
          }

          [[nodiscard]]
          bool halted() const noexcept{
            return halted_;
          }

          void load_program(const std::vector<std::size_t>& instructions)noexcept{
            datapath_.load_instructions(instructions);
          }

          [[nodiscard]]
          const SingleCycleDatapath<
                 AddressWidth,
                 DataWidth,
                 InstructionWidth,
                 RegisterAddressWidth,
                 InstructionMemoryAddressWidth,
                 DataMemoryAddressWidth
               >& datapath() const noexcept{
                return datapath_;
               }

        private:
          logic::Clock clock_;
          logic::Wire reset_;

          bool halted_{false};
          SingleCycleDatapath<
              AddressWidth,
              DataWidth,
              InstructionWidth,
              RegisterAddressWidth,
              InstructionMemoryAdressWidth,
              DataMemoryAdressWidth
          >datapath_;
          

    };
}
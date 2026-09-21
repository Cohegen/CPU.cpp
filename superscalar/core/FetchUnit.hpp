#pragma once

#include <cstddef>
#include <cstdint>

#include <logic/simulator/Component.hpp>
#include <logic/signals/bus.hpp>
#include <logic/signals/clock.hpp>
#include <logic/signals/wire.hpp>
#include <logic/combinational/adders/RippleCarryAdder.hpp>


#include "../../components/InstructionMemory.hpp"
#include "../../components/ProgramCounter.hpp"

#include "../core/FetchBundle.hpp"

namespace cpu
{
    template<std::size_t InstructionWidth=32,std::size_t CPUAddressWidth=32>
    class SuperScalarFetchUnit:public Component{
      static_assert(CPUAddressWidth>0,"CPUAddressWidth must be greater than zero");
      static_assert(InstructionWidth>0,"InstructionWidth must be greater than zero");
      static_assert(InstructionWidth % 8==0,"InstructionWidth must bea multiple of 8");
        public:
          static constexpr std::size_t InstructionBytes = InstructionWidth /8;
          static constexpr std::size_t FetchBytes = 2* InstructionBytes;
          SuperScalarFetchUnit(
            logic::Clock& clock,
            logic::Wire& reset,
            logic::Bus<InstructionWidth>&pc0,
            logic::Bus<InstructionWidth>&pc1,
            logic::Bus<InstructionWidth>&instruction0,
            logic::Bus<InstructionWidth>&instruction1,
            logic::Wire& cache_hit0,
            logic::Wire& cache_hit1,
            logic::Wire& valid0,
            logic::Wire& valid1,
            logic::Bus<CPUAddressWidth>next_pc
          ): clock_(clock),
             reset_(reset),
             pc0_(pc0),
             pc1_(pc1),
             instruction0_(instruction0),
             instruction1_(instruction1),
             cache_hit0_(cache_hit0),
             cache_hit1_(cache_hit1),
             nextpc_(nextpc),
             pc_register_(next_pc,clock_,pc_register_output_){}


          void evaluate() noexcept override
          {
            /*
            Updating the program counter register
            */
            pc_register_.evaluate();

            /*
            Reading the current PC
            */
            const std::size_t pc = bus_to_value(pc_register_output_);

            /*
            Generating the two instruction address

            Lane 0 =pc
            Lane 1 = pc + InstructionBytes
            */
            const std::size_t pc0 = pc;
            const std::size_t pc1 = pc +InstructionBytes;

            value_to_bus(pc0,pc0_);
            value_to_bus(pc1,pc1_);

            /*
            Sequentail next pc

            A 2-wide fetch consumes two 32-bit instructions :
               PC = instr0
               PC+4 = instruction 1
            */
            value_to_bus(pc+FetchBytes,nextpc_);

            valid0_.write(cache_hit0_.read());
            valid1_.write(cache_hit1_.read());

            /*
            Reset
            */
            if(reset_.read() == logic::LogicState::HIGH){
              value_to_bus(0,nextpc_);
              valid0_.write(logic::LogicState::LOW);
              valid1_.write(logic::LogicState::LOW);
            }
          }

          [[nodiscard]]
          std::size_t pc() const noexcept{
            return bus_to_value(pc_register_output_);
          }

        private:

         
          //inputs
         logic::Clock& clock_;
         logic::Wire& reset_;
         logic::Bus<InstructionWidth>cache_hit0_;
         logic::Bus<InstructionWidth>cache_hit1_;
         logic::Bus<InstructionWidth>instruction0_;
         logic::Bus<InstructionWidth>instruction1_;
         logic::Bus<InstructionWidth>fetch_bundle_;
         logic::Bus<InstructionWidth>& PC_,
         logic::Wire& redirect_;
         logic::Bus<InstructionWidth>& redictTarget_;
         logic::Wire& stall_;

         //outputs
         logic::Bus<InstructionWidth> instruction0_;
         logic::Bus<InstructionWidth>instruction1_;
          logic::Bus<InstructionWidth>pc0_;
          logic::Wire& valid0_;
          logic::Wire& valid1_;
          logic::Bus<InstructionWidth> pc1_;

          logic::Bus<InstructionWidth> nextpc_;
          logic::Bus<CPUAddressWidth>pc_register_output_;
          logic::Register<CPUAddressWidth> pc_register_;

          template<std::size_t Width>
          [[nodiscard]]
          static std::size_t bus_to_value(const logic::Bus<Width>&bus)noexcept{
            std::size_t value=0;
            for(std::size_t i=0;i<Width;++i){
              if(bus[i].read() == logic::LogicState::HIGH){
                value |= (1ULL << i);
              }

            }
            return value;
          }

        template<std::size_t Width>
        static void value_to_bus(std::size_t value,logic::Bus<Width>&bus)noexcept{
          for(std::size_t i=0;i<Width;++i){
            bus[i].write(
              (value & (1ULL << i))
                  ? logic::LogicState::HIGH
                  : logic::LogicState::LOW
          );
        }
        
          
    };
}
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
    template<std::size_t InstructionWidth=32>
    class SuperScalarFetchUnit:public Component{

        public:
          SuperScalarFetchUnit(
            logic::Clock& clock,
            logic::Wire& reset


          ): 

        private:

         
          //inputs
         logic::Clock& clock_;
         logic::Wire& reset_;
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

          //internal hardware components
          FetchBundle fetch_bundle_;
          ProgramCounter<InstructionWidth> program_counter_;
          InstructionMemory<InstructionWidth> instruction_memory_;
          logic::RippleCarryAdder<InstructionWidth> pcplus4_adder_;
          logic::RippleCarryAdder<InstructionWidth> pcplus8_adder_;
          //intenal buses
          logic::Bus<InstructionWidth>pc2instr_mem_;
          logic::Bus<InstructionWidth>instruction_address0_;
          logic::Bus<InstructionWidth>instruction_address1_;
          
    };
}
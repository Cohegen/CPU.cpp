#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <logic/combinational/adders/RippleCarryAdder.hpp>
#include <logic/combinational/multiplexers/Mux.hpp>
#include <logic/sequential/memory/RegisterFile.hpp>
#include <logic/signals/bus.hpp>
#include <logic/signals/clock.hpp>
#include <logic/signals/logicState.hpp>
#include <logic/signals/wire.hpp>
#include <logic/simulator/Component.hpp>

#include "../../components/ALUInterface.hpp"
#include "../../components/ALUOperandMux.hpp"
#include "../../components/ControlSignals.hpp"
#include "../../components/DataMemory.hpp"
#include "../../components/InstructionMemory.hpp"
#include "../../components/ProgramCounter.hpp"
#include "../../components/WriteBackMux.hpp"
#include "../../include/isa/DecodedInstruction.hpp"
#include "../../include/isa/Instruction.hpp"
#include "../../include/isa/InstructionDecoder.hpp"
#include "../core/ControlSignals.hpp"
#include "../core/ControlUnit.hpp"
#include "../core/HazardUnit.hpp"
#include "../multiplexers/ForwardAE_mux.hpp"
#include "../multiplexers/ForwardBE_mux.hpp"
#include "../registers/EX_MEM.hpp"
#include "../registers/ID_EX.hpp"
#include "../registers/IF_ID.hpp"
#include "../registers/MEM_WB.hpp"

namespace cpu
{
    template<std::size_t InstructionWidth=32>
    class SuperScalarFetchUnit:public Component{

        public:

        private:
          //inputs
         logic::Clock& clock_;
         logic::Wire& reset_;
         logic::Bus<InstructionWidth>& PC,
         logic::Wire& redirect_;
         logic::Wire& redictTarget_;
         logic::Wire& stall_;

         //outputs
         logic::Bus<InstructionWidth> instruction0_;
         logic::Bus<InstructionWidth>instruction1_;
          logic::Bus<InstructionWidth>pc0_;
          logic::Bus<InstructionWidth>valid0_;
          logic::Bus<InstructionWidth>valid1_;

          logic::Bus<InstructionWidth> nextpc_;
    };
}
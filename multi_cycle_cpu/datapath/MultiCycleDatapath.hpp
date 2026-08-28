#pragma once

#include "components/ProgramCounter.hpp"
#include "core/registers/InstructionRegister.hpp"
#include "core/registers/MemoryDataRegister.hpp"
#include "core/registers/OperandA_reg.hpp"
#include "core/registers/OperandB_reg.hpp"
#include "core/registers/ALU_out_reg.hpp"
#include <logic/signals/wire.hpp>
#include <logic/signals/bus.hpp>
#include <logic/signals/clock.hpp>
#include <logic/sequential/memory/RegisterFile.hpp>
#include "components/ALUInterface.hpp"
#include <logic/sequential/memory/Memory.hpp>
#include "core/ControlSignals.hpp"
#include <logic/simulator/Component.hpp>
#include "core/muxes/ALUSrcA_mux.hpp"
#include "core/muxes/ALUSrcB_mux.hpp"

namespace cpu{
    template<
         std::size_t AddressWidth=32,
         std::size_t DataWidth=32,
         std::size_t InstructionWidth=32,
         std::size_t RegisterAddressWidth =5,
         std::size_t MemoryAddressWidth=32
       >
    class MultlCycleDatapath:public Component{

        public:
          MultlCycleDatapath(){}

          void evaluate(
            const MultiCycleControlSignals& control
          );

          void clock();

        private:

        //external signals
        logic::Clock& clock_;
        logic::Wire& reset_;
        logic::Wire pc_enable_;
        logic::Wire clock_signal_;
        logic::Bus<AddressWidth>pc_next_;

        //instruction path
        logic::Bus<AddressWidth>pc_;
        logic::Bus<InstructionWidth>instruction_;

        //next instruction
        logic::Bus<AddressWidth>pc_increment_;

        //register address
        logic::Bus<RegisterAddressWidth>rs1_address_;
        logic::Bus<RegisterAddressWidth>rs2_address_;
        logic::Bus<RegisterAddressWidth>rd_address_;

        //register data
        logic::Bus<DataWidth> rs1_data_;
        logic::Bus<DataWidth> rs2_data_;
        logic::Bus<DataWidth> register_write_data_;
        logic::Wire register_write_enable_;

        //ALU datapath
        ALUSrcA<DataWidth> mux_a;
        ALUSrcB<DataWidth>mux_b;

        logic::Wire alu_zero_;
        logic::Wire alu_carry_;

        //Control Signals
        ControlSignals control_;


        //registers
        ProgramCounter<AddressWidth>pc;
        InstructionRegiste<InstructionWidth> ir;
        MemoryDataRegister<DataWidth> mdr;

        OperandA_reg<DataWidth> regA;
        OperandB_reg<DataWidth>regB;

        ALU_out_reg<DataWidth> aluOut;

        //Register file
        logic::RegisterFile<RegisterAddressWidth,DataWidth> registerFile;

        //ALU
        ALUInterface<DataWidth> alu;


        //memory
        logic::Memory<N> memory;
    };
}

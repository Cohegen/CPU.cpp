/*
An implementation of a pipelied processor datapath based on
ARM architecture
*/

#pragma once
#include "../../components/ControlSignals.hpp"
#include "../../components/ProgramCounter.hpp"
#include "../../components/InstructionMemory.hpp"
#include "../../include/isa/InstructionDecoder.hpp"
#include "ControlUnit.hpp"
#include "../../components/ALUOperandMux.hpp"
#include "../../components/ALUInterface.hpp"
#include "../../components/DataMemory.hpp"
#include "../../components/WriteBackMux.hpp"
#include "../registers/EX_MEM.hpp"
#include "../registers/ID_EX.hpp"
#include "../registers/IF_ID.hpp"
#include "../registers/MEM_WB.hpp"
#include <logic/simulator/Component.hpp>

#include <logic/signals/wire.hpp>
#include <logic/signals/bus.hpp>
#include <logic/signals/clock.hpp>
#include <logic/combinational/adders/RippleCarryAdder.hpp>
#include <logic/sequential/memory/RegisterFile.hpp>
#include <logic/sequential/memory/memory.hpp>
#include <cstdint>
#include <vector>

namespace cpu{
    template<std::size_t AddressWidth=32,std::size_t DataWidth=32,
      std::size_t InstructionWidth=32,std::size_t RegisterAddressWidth=4,
       std::size_t InstructionMemoryAdressWidth=8,std::size_t DataMemoryAddressWidth=8>

    class PipelinedCycleDatapath:public logic::Component{
        public:
          PipelinedCycleDatapath(
            logic::Clock& clock,
            logic::Wire& reset
          ): clock_(clock),
             reset_(reset),
             program_counter_(clock_signal_,reset_,pc_enable_,pc_next_,pc_),
             instruction_mem_(pc_,instruction_),
             pc_adder_(pc_,pc_constant_one_,pc_adder_carry_in_,pc_increment_,pc_adder_carry_out_),
             if_id_(instruction_,pc_next_,clock_,reset_,enable_),
             register_file_(clock_,reset_,register_write_enable_,rs1_address_,rs2_address_,rd_address_,register_write_data_,rs1_data_,rs2_data_),
             

        private:
           //external signals
           logic::Clock& clock_;
           logic::Wire& reset_;
           logic::Wire pc_enable_;
           logic::Wire clock_signal_;
           logic::Bus<AddressWidth>pc_next_;
 
           //instruction path
           logic::Bus<AddressWidth>pc_;
           logic::Bus<InstructionWidth> instruction_;
 
           //next instruction path
           logic::Bus<AddressWidth>pc_increment_;
           logic::Bus<AddressWidth> pc_constant_one_;
 
           logic::Wire pc_adder_carry_in_;
           logic::Wire pc_adder_carry_out_;
 
           logic::RippleCarryAdder<AddressWidth>pc_adder_;
 
           //Program counter multiplexers
           logic::Bus<AddressWidth>selected_pc_next_;
           logic::Mux<AddressWidth> pc_next_mux_;
 
           //Instruction decoding
           DecodedInstruction decoded_instruction_;
 
           //register addresses
           logic::Bus<RegisterAddressWidth>rs1_address_;
           logic::Bus<RegisterAddressWidth>rs2_address_;
           logic::Bus<RegisterAddressWidth>rd_address_;
 
           //register data
           logic::Bus<DataWidth> rs1_data_;
           logic::Bus<DataWidth> rs2_data_;
           logic::Bus<DataWidth> register_write_data_;
           logic::Wire register_write_enable_;
 
           //ALU datapath
           logic::Bus<DataWidth>immediate_;
           logic::Bus<DataWidth>alu_operand_b_;
           logic::Bus<DataWidth>alu_result_;
 
           logic::Wire alu_zero_;
           logic::Wire alu_carry_;
           logic::Wire alu_source_immediate_;
 
           //control signals
           ControlSignals control_;
 
           //datapath components
           ProgramCounter<AddressWidth> program_counter_;
           InstructionMemory<AddressWidth,InstructionMemoryAddressWidth,InstructionWidth> instruction_mem_;
           logic::RegisterFile<RegisterAddressWidth, DataWidth> register_file_;
           ALUOperandMux<DataWidth> alu_operand_mux_;
           ALUInterface<DataWidth> alu_interface_;
           IF_ID<InstructionWidth,AddressWidth> if_id_;
           EX_MEM<DataWidth,RegisterAddressWidth>ex_mem_;
           ID_EX<AddressWidth,DataWidth,RegisterAddressWidth> id_ex_;
           MEM_WB<DataWidth,RegisterAddressWidth> mem_wb_;
 
           //Memory and Writeback control/interconnect signals
           logic::Wire memory_read_enable_;
           logic::Wire memory_write_enable_;
           logic::Wire memory_to_register_;
           logic::Bus<DataMemoryAddressWidth> data_memory_address_;
           logic::Bus<DataWidth> memory_read_data_;
 
           DataMemory<DataMemoryAddressWidth, DataWidth> data_mem_;
           WriteBackMux<DataWidth> writeback_mux_;
 
           //branching components
           logic::Bus<AddressWidth> branch_target_;
           logic::Bus<AddressWidth> branch_offset_;
           logic::Wire branch_adder_carry_in_;
           logic::Wire branch_adder_carry_out_;
           logic::RippleCarryAdder<AddressWidth>branch_adder_;
           logic::Wire branch_taken_;
    };
}
/*
Implementation of the single cycle processor datapath
*/

#pragma once
#include "ControlSignals.hpp"
#include "ProgramCounter.hpp"
#include "InstructionMemory.hpp"
#include "isa/InstructionDecoder.hpp"
#include <logic/sequential/memory/RegisterFile.hpp>
#include "ControlUnit.hpp"
#include "ALUOperandMux.hpp"
#include "ALUInterface.hpp"
#include <logic/simulator/Component.hpp>
#include <logic/signals/clock.hpp>
#include <logic/signals/wire.hpp>
#include <logic/signals/bus.hpp>
#include <logic/combinational/adders/RippleCarryAdder.hpp>
#include <vector>

namespace cpu{
    template<
       std::size_t AddressWidth = 32,
       std::size_t DataWidth = 32,
       std::size_t InstructionWidth=32,
       std::size_t RegisterAddressWidth =4,
       std::size_t InstructionMemoryAddressWidth=8
    
    >
    class SingleCycleDatapath:public logic::Component{

        public:
          SingleCycleDatapath(
             logic::Wire& clock,
             logic::Wire& reset
          ): clock_(clock),
             reset_(reset),
             program_counter_(clock_,reset_,pc_enable_,pc_next_,pc_),
             instruction_mem_(pc_,instruction_),
             pc_adder_(pc_,pc_constant_one_,pc_adder_carry_in_,pc_increment_,pc_adder_carry_out_)
             
             {
                // Constant 1 for PC increment
             for (std::size_t i = 0; i < AddressWidth; ++i)
                {
                    pc_constant_one_[i].write(
                       i == 0
                         ? logic::LogicState::HIGH
                          : logic::LogicState::LOW
                   );
                }

                   // No carry-in for PC + 1
                 pc_adder_carry_in_.write(
                              logic::LogicState::LOW
                          );

                  // PC enabled by default for now
                        pc_enable_.write(
                         logic::LogicState::HIGH
                        );
             }

            void evaluate() noexcept override{
               program_counter_.evaluate();
               instruction_mem_.evaluate();
               pc_adder_.evaluate();

               for(std::size_t i = 0; i < AddressWidth; ++i) {
                   pc_next_[i].write(pc_increment_[i].read());
               }
            }

            void load_instructions(const std::vector<std::size_t>& instructions) noexcept {
                instruction_mem_.load(instructions);
            }

            [[nodiscard]]
            logic::Bus<AddressWidth>& pc() noexcept {
                return pc_;
            }

            [[nodiscard]]
            const logic::Bus<AddressWidth>& pc() const noexcept {
                return pc_;
            }

            [[nodiscard]]
            logic::Bus<InstructionWidth>& instruction() noexcept {
                return instruction_;
            }

            [[nodiscard]]
            const logic::Bus<InstructionWidth>& instruction() const noexcept {
                return instruction_;
            }

            [[nodiscard]]
            InstructionMemory<AddressWidth, InstructionMemoryAddressWidth, InstructionWidth>& instruction_memory() noexcept {
                return instruction_mem_;
            }

            [[nodiscard]]
            const InstructionMemory<AddressWidth, InstructionMemoryAddressWidth, InstructionWidth>& instruction_memory() const noexcept {
                return instruction_mem_;
            }

        
        private:
           //external signals
          logic::Wire& clock_;
          logic::Wire& reset_;
          logic::Wire pc_enable_;
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

          //Instruction decoding
          DecodedInstruction decoded_instruction_;

          //register addresses
          logic::Bus<RegisterAddressWidth>rs1_address_;
          logic::Bus<RegisterAddressWidth>rs2_address_;
          logic::Bus<RegisterAddressWidth>rd_address_;

          //register data
          logic::Bus<DataWidth> rs1_data_;
          logic::Bus<DataWidth> rs2_data_;

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

          
          
    };
}
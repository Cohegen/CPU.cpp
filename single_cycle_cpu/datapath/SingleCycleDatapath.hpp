/*
Implementation of the single cycle processor datapath
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
    template<
       std::size_t AddressWidth = 32,
       std::size_t DataWidth = 32,
       std::size_t InstructionWidth=32,
       std::size_t RegisterAddressWidth =4,
       std::size_t InstructionMemoryAddressWidth=8,
       std::size_t DataMemoryAddressWidth=8

    >
    class SingleCycleDatapath:public logic::Component{


        public:
          SingleCycleDatapath(
             logic::Clock& clock,
             logic::Wire& reset
          ): clock_(clock),
             reset_(reset),
             program_counter_(clock_signal_,reset_,pc_enable_,pc_next_,pc_),
             instruction_mem_(pc_,instruction_),
             pc_adder_(pc_,pc_constant_one_,pc_adder_carry_in_,pc_increment_,pc_adder_carry_out_),
             register_file_(
                clock_,
                reset_,
                register_write_enable_,
                rs1_address_,
                rs2_address_,
                rd_address_,
                register_write_data_,
                rs1_data_,
                rs2_data_
             ),
             alu_operand_mux_(
                rs2_data_,
                immediate_,
                alu_source_immediate_,
                alu_operand_b_
             ),
             alu_interface_(
                rs1_data_,
                alu_operand_b_,
                control_.alu_operation,
                alu_result_,
                alu_zero_,
                alu_carry_
             ),
             data_mem_(
                clock_,
                reset_,
                memory_read_enable_,
                memory_write_enable_,
                data_memory_address_,
                rs2_data_,
                memory_read_data_
             ),

             writeback_mux_(
                alu_result_,
                memory_read_data_,
                memory_to_register_,
                register_write_data_
             ),
             branch_adder_(
                pc_,
                branch_offset_,
                branch_adder_carry_in_,
                branch_target_,
                branch_adder_carry_out_
             ),
             pc_next_mux_(
                pc_increment_,
                branch_target_,
                branch_taken_,
                pc_next_
             )
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

                   //setting signal of branch adder carry in
                   branch_adder_carry_in_.write(
                          logic::LogicState::LOW
                   );

                   for(std::size_t i=0;i<AddressWidth;++i){
                     branch_offset_[i].write(
                         immediate_[i].read()
                     );
                   }
             }

             void evaluate() noexcept override
             {
                //propagating external clock
                clock_signal_.write(clock_.state());

                 // Fetch instruction at the current PC before updating PC.
                 instruction_mem_.evaluate();

                 //decode stage
                 Instruction instruction_word(
                    static_cast<std::uint32_t>(instruction_.read_value())
                 );

                 decoded_instruction_ = InstructionDecoder::decode(instruction_word);
                  control_ = ControlUnit::generate(decoded_instruction_);

                  //disable PC increment when HALT is executed
                  pc_enable_.write(
                      control_.halt
                          ? logic::LogicState::LOW
                          : logic::LogicState::HIGH
                  );

                  //driving decoded instruction signals
                 rs1_address_.write_value( static_cast<std::size_t>(decoded_instruction_.rs1)
                );

                rs2_address_.write_value(
                    static_cast<std::size_t>(decoded_instruction_.rs2)
                );

                rd_address_.write_value(
                    static_cast<std::size_t>(decoded_instruction_.rd)
                );

                //connecting immediate
                immediate_.write_value(
                    static_cast<std::uint32_t>(
                        decoded_instruction_.immediate
                    )
                );

                //driving control signals
                alu_source_immediate_.write(
                    control_.alu_source_immediate
                        ? logic::LogicState::HIGH
                        : logic::LogicState::LOW
                );

                memory_read_enable_.write(
                    control_.memory_read
                        ? logic::LogicState::HIGH
                        : logic::LogicState::LOW
                );

                memory_write_enable_.write(
                    control_.memory_write
                        ? logic::LogicState::HIGH
                        : logic::LogicState::LOW
                );

                memory_to_register_.write(
                    control_.memory_read
                        ? logic::LogicState::HIGH
                        : logic::LogicState::LOW
                );
                //reading register operands
                register_write_enable_.write(logic::LogicState::LOW);
                register_file_.evaluate();

                //selecting ALU operand B
                alu_operand_mux_.evaluate();

                //executing ALU operation
                alu_interface_.set_operation(control_.alu_operation);
                alu_interface_.evaluate();

                //data memory address
                for (std::size_t i = 0; i < DataMemoryAddressWidth; ++i)
                {
                    data_memory_address_[i].write(alu_result_[i].read());
                }

                //performing memory operation
                data_mem_.evaluate();

                //selecting value to write back
                writeback_mux_.evaluate();

                //register file write
                register_write_enable_.write(
                    control_.register_write
                        ? logic::LogicState::HIGH
                        : logic::LogicState::LOW
                );
                register_file_.evaluate();

                //generating branch target
                for(std::size_t i=0;i<AddressWidth;++i){
                    branch_offset_[i].write(
                        immediate_[i].read()
                    );
                }
                branch_adder_.evaluate();

                  //determining branch or jump condition
                  bool next_pc_select = false;

                  if(control_.branch){
                      if(decoded_instruction_.opcode == Opcode::BEQ){
                          next_pc_select = alu_zero_.read() == logic::LogicState::HIGH;
                      }else if(decoded_instruction_.opcode == Opcode::BNE){
                          next_pc_select = alu_zero_.read() == logic::LogicState::LOW;
                      }
                  } else if (control_.jump) {
                      next_pc_select = true;
                  }
                  branch_taken_.write(
                      next_pc_select ? logic::LogicState::HIGH : logic::LogicState::LOW
                  );

                // Generate PC + 1 from the current PC.
                pc_adder_.evaluate();

                //selecting next PC state
                pc_next_mux_.evaluate();

                //updating program counter
                program_counter_.evaluate();

               
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

            [[nodiscard]]
            const DecodedInstruction& decoded_instruction() const noexcept {
                return decoded_instruction_;
            }

            [[nodiscard]]
            const logic::Bus<RegisterAddressWidth>& rs1_address() const noexcept {
                return rs1_address_;
            }

            [[nodiscard]]
            const logic::Bus<RegisterAddressWidth>& rs2_address() const noexcept {
                return rs2_address_;
            }

            [[nodiscard]]
            const logic::Bus<RegisterAddressWidth>& rd_address() const noexcept {
                return rd_address_;
            }

            [[nodiscard]]
            const logic::Bus<DataWidth>& immediate() const noexcept {
                return immediate_;
            }

            [[nodiscard]]
            const logic::Bus<DataWidth>& rs1_data() const noexcept {
                return rs1_data_;
            }

            [[nodiscard]]
            const logic::Bus<DataWidth>& rs2_data() const noexcept {
                return rs2_data_;
            }

            [[nodiscard]]
            const logic::Bus<DataWidth>& alu_operand_b() const noexcept {
                return alu_operand_b_;
            }

            [[nodiscard]]
            const logic::Bus<DataWidth>& alu_result() const noexcept {
                return alu_result_;
            }

            [[nodiscard]]
            const logic::Wire& alu_zero() const noexcept {
                return alu_zero_;
            }

            [[nodiscard]]
            const logic::Wire& alu_carry() const noexcept {
                return alu_carry_;
            }

            [[nodiscard]]
            const logic::Bus<DataWidth>& memory_read_data() const noexcept {
                return memory_read_data_;
            }

            [[nodiscard]]
            const logic::Bus<DataWidth>& register_write_data() const noexcept {
                return register_write_data_;
            }

            [[nodiscard]]
            const logic::Wire& memory_to_register() const noexcept {
                return memory_to_register_;
            }

            [[nodiscard]]
            DataMemory<DataMemoryAddressWidth, DataWidth>& data_memory() noexcept {
                return data_mem_;
            }

            [[nodiscard]]
            const DataMemory<DataMemoryAddressWidth, DataWidth>& data_memory() const noexcept {
                return data_mem_;
            }


            [[nodiscard]]
            const ControlSignals& control_signals() const noexcept {
                return control_;
            }

            [[nodiscard]]
            bool halted() const noexcept {
                return control_.halt;
            }

            void write_register_for_test(
                cpu::Register destination,
                std::uint32_t value
            ) noexcept {
                rd_address_.write_value(
                    static_cast<std::size_t>(destination)
                );
                register_write_data_.write_value(value);
                register_write_enable_.write(logic::LogicState::HIGH);

                register_file_.evaluate();
                clock_.tick();
                register_file_.evaluate();
                clock_.tick();
                register_file_.evaluate();

                register_write_enable_.write(logic::LogicState::LOW);
                register_file_.evaluate();
            }

            [[nodiscard]]
            std::uint32_t read_register_for_test(
                cpu::Register source
            ) noexcept {
                rs1_address_.write_value(
                    static_cast<std::size_t>(source)
                );
                register_file_.evaluate();

                return static_cast<std::uint32_t>(
                    rs1_data_.read_value()
                );
            }


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













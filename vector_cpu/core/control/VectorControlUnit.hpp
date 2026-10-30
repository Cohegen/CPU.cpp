#pragma once
#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/signals/logicState.hpp>
#include <logic/simulator/Component.hpp>
#include <logic/combinational/comparators/Comparator.hpp>
#include <logic/gates/AND.hpp>
#include <logic/gates/OR.hpp>
#include <logic/gates/NOT.hpp>
#include <logic/combinational/decoders/Decoder.hpp>
#include <cstddef>
#include <cstdint>
#include <array>

namespace cpu {
    template<
      std::size_t InstrWidth=32,
      std::size_t AddressWidth = 5,
      std::size_t OPLEN=7,
      std::size_t FUNCT=3
      
    >
    class VectorControlUnit:  public logic::Component
    {
        static_assert(InstrWidth==32,"Instruction Width must be 32 bits" );
        static_assert(AddressWidth==5,"AddressWidth must be 5 bits" );
        static_assert(OPLEN==7,"Opcode length must be 7" );
        static_assert(FUNCT==3,"Funct3 must be 3 bits" );
        
        public:
          VectorControlUnit(
            logic::Bus<InstrWidth>& instruction,
            logic::Wire& reset,
            logic::Bus<AddressWidth>& rs1,
            logic::Bus<AddressWidth>& rs2,
            logic::Bus<AddressWidth>& rd,
            logic::Bus<OPLEN>& instruction_opcode,
            logic::Bus<FUNCT>& funct3,
            logic::Bus<3>& alu_opcode,
            logic::Wire& write_enable,
            logic::Wire& valid_instruction
          ):instruction_(instruction),
            reset_(reset),
            rs1_(rs1),
            rs2_(rs2),
            rd_(rd),
            instruction_opcode_(instruction_opcode),
            funct3_(funct3),
            alu_opcode_(alu_opcode),
            write_enable_(write_enable),
            valid_instruction_(valid_instruction),
            comparator_(instruction_opcode_,expected_opcode_,opcode_valid_,greater_,less_),
            decoder_(funct3_,decoder_enable_,decoded_operations_),
            not_gate_(reset_,reset_n_),
            or_gate1_(
                decoded_operations_[0],
                decoded_operations_[1],
                results_[0]
            ),
            or_gate2_(
                results_[0],
                decoded_operations_[2],
                results_[1]
            ),
            or_gate3_(
                results_[1],
                decoded_operations_[3],
                results_[2]
            ),
            or_gate4_(
                results_[2],
                decoded_operations_[4],
                operation_valid_
            ),
            and_gate1_(
                opcode_valid_,
                operation_valid_,
                valid_instruction_
            ),
            and_gate2_(
                valid_instruction_,
                reset_n_,
                write_enable_
            )
          {
              expected_opcode_.write_value(0b1010111);
              decoder_enable_.write(logic::LogicState::HIGH);
          }

        void evaluate() noexcept override
        {
            //extracting instruction fields
            extract_register_address();
            extract_opcode();
            extract_funct3();
            extract_alu_opcode();

            //validating the major opcode
            comparator_.evaluate();

            //decoding the requested operation
            decoder_enable_.write(logic::LogicState::HIGH);
            decoder_.evaluate();

            //determining whether funct3 represents a supported vector operation
            evaluate_operation_validity();

            //checking instruction validity
            and_gate1_.evaluate();

            //generating write enable signals
            not_gate_.evaluate();
            and_gate2_.evaluate();
        }

        void extract_register_address()
        {
            for(std::size_t i{0};i<AddressWidth;++i)
            {
                rd_[i].write(
                    instruction_[27+i].read()
                );
                rs1_[i].write(
                    instruction_[22+i].read()
                );
                rs2_[i].write(
                    instruction_[17+i].read()
                );
            }
        }

        void extract_opcode()
        {
            for(std::size_t i{0};i<OPLEN;++i)
            {
                instruction_opcode_[i].write(
                    instruction_[0+i].read()
                );
            }
        }

        void decode_opcode()
        {
            comparator_.evaluate();
        }

        void extract_funct3()
        {
            for(std::size_t i{0};i<FUNCT;++i)
            {
                funct3_[i].write(
                    instruction_[14+i].read()
                );
            }
        }

        void extract_alu_opcode()
        {
          for(std::size_t i{0};i<3;++i)
          {
            alu_opcode_[i].write(
               funct3_[i].read()
             );
          }
        }
        
        private:
           void evaluate_operation_validity()
           {
               or_gate1_.evaluate();
               or_gate2_.evaluate();
               or_gate3_.evaluate();
               or_gate4_.evaluate();
           }

           //inputs
           logic::Bus<InstrWidth>& instruction_;
           logic::Wire& reset_;

           //outputs
           logic::Bus<AddressWidth>& rs1_;
           logic::Bus<AddressWidth>& rs2_;
           logic::Bus<AddressWidth>& rd_;

           logic::Bus<OPLEN>& instruction_opcode_;
           logic::Bus<FUNCT>& funct3_;
           logic::Bus<3>& alu_opcode_;
           
           logic::Wire& write_enable_;
           logic::Wire& valid_instruction_;

           //internal hardware components,wires and buses
           logic::Bus<OPLEN> expected_opcode_;

           logic::Wire opcode_valid_;
           logic::Wire greater_;
           logic::Wire less_;
           logic::Wire decoder_enable_;

           logic::Wire operation_valid_;
           logic::Bus<8> decoded_operations_;

           logic::Wire reset_n_;
           logic::Bus<3> results_;

           logic::Comparator<OPLEN> comparator_;
           logic::Decoder<3,8> decoder_;
           logic::NotGate not_gate_;

           //OR gate network
           logic::OrGate or_gate1_;
           logic::OrGate or_gate2_;
           logic::OrGate or_gate3_;
           logic::OrGate or_gate4_;

           //instruction validation and write control
           logic::ANDGate and_gate1_;
           logic::ANDGate and_gate2_; //checks instruction validity
    };
}

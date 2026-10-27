#pragma once
#include "../core/memory/VectorRegisterFile.hpp"
#include "../core/execution/VectorExecutionUnit.hpp"
#include "../core/configuration/VectorMaskWBUnit.hpp"


namespace cpu {
   
    class VectorDatapath
    {
        public:
            VectorDatapath(
                logic::Bus<5>& rs1,
                logic::Bus<5>& rs2,
                logic::Bus<5>& rd,
                logic::Bus<128>& write_data,
                logic::Bus<5> write_addr
                logic::Bus<3>& opcode,
                logic::Bus<8> vl,
                logic::Clock& clock,
                logic::Wire& enable,
                logic::Bus<128>result
            ): 
                rs1_(rs1),
                rs2_(rs2),
                rd_(rd),
                write_data_(write_data),
                write_addr_(write_addr),
                opcode_(opcode),
                vl_(vl),
                clock_(clock),
                enable_(enable),
                result_(result),
                vector_reg_file_(rs1_,rs2_,rd_,write_data_,write_addr_,enable_,reset_,clock_),

            {
               
                
            }

            void evaluate() noexcept
            {
                operand_a_.write(
                    vector_reg_file_.read_data1().read_value()
                );
                operand_b_.write(
                    vector_reg_file_.read_data2().read_value()
                );

                old_dest_.write(
                    vector_reg_file_.read_data3_().read_value()
                );
            }
        private:
          logic::Bus<5>& rs1_;
          logic::Bus<5>& rs2_;
          logic::Bus<5>& rd_;
          logic::Bus<128>& write_data_;
          logic::Bus<5>& write_addr_;
          logic::Bus<3>& opcode_;
          logic::Bus<8>& vl_;
          logic::Wire& enable_;
          logic::Clock& clock_;
          logic::Wire& reset_;

          //output
          logic::Bus<128>& result_;
          //internal buses
          logic::Bus<128> operand_a_;
          logic::Bus<128> operand_b_;
          logic::Bus<128> old_dest_;

          logic::Bus<128> execution_result_;
          logic::Bus<128> writeback_result_;

          logic::Bus<4> lane_mask_;
          
          //internal hardware components
          VectorRegisterFile<32,128> vector_reg_file_;
          VectorExecutionUnit<128,4,32> vec_execution_unit_;
          VectorMaskedWriteBackUnit vec_masked_unit_;
    };
}
/*
Holds the rs1 value after instruction is decoded
*/

#pragma once
#include <cstddef>

#include <logic/sequential/registers/register.hpp>
#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/simulator/Component.hpp>
#include <logic/signals/clock.hpp>
#include <logic/combinational/multiplexers/Mux.hpp>

namespace cpu{
    template<std::size_t DataWidth=32>
    class OperandA_reg:public logic::Component{
        public:
           OperandA_reg(
            logic::Bus<DataWidth>& data,
            logic::Wire& clock,
            logic::Wire& reset,
            logic::Wire& a_write
           ):  
              read_data1(data),
              clock_(clock),
              A_write_(a_write),
              reset_(reset),
              load_mux(ALU_operand_A,read_data1,A_write_,load_output),
              reset_mux(load_output,zero_bus_,reset_,op_A_input),
              op_A_reg(op_A_input,clock_,ALU_operand_A)
              {
                zero_bus_.write(logic::LogicState::LOW);
              }

          void evaluate() noexcept {
            load_mux.evaluate();
            reset_mux.evaluate();
            op_A_reg.evaluate();

          }

         const logic::Bus<DataWidth> read() const{
            return ALU_operand_A;
         }
            
        private:
          //inputs
          logic::Bus<DataWidth>& read_data1; // data from the register file readData1 port
          logic::Wire& clock_;
          logic::Wire& A_write_; // write enable signal
          logic::Wire& reset_;
          logic::Bus<DataWidth>zero_bus_;
          logic::Bus<DataWidth> op_A_input;

          //output
          logic::Bus<DataWidth> load_output;
          logic::Bus<DataWidth>ALU_operand_A;

          //muxes
          logic::Mux<DataWidth> load_mux;
          logic::Mux<DataWidth> reset_mux;
          //register
          logic::Register<DataWidth> op_A_reg;


    };
}
/*
Holds rs2 value after decode 
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
    class OperandB_reg:public logic::Component{
        public:
         OperandB_reg(
            logic::Bus<DataWidth>& data,
            logic::Wire& clock,
            logic::Wire& B_write,
            logic::Wire& reset
         ):  readData2_(data), 
             clock_(clock),
             B_write_(B_write),
             reset_(),
             load_mux(reg_output_,readData2_,B_write_,load_output_),
             reset_mux(load_output_,zero_bus_,reset_,op_B_input_),
             operand_B_reg(op_B_input_,clock_,reg_output_)
           {
              zero_bus_.write(logic::LogicState::LOW);
           }

          void evaluate(){
            load_mux.evaluate();
            reset_mux.evaluate();
            operand_B_reg.evaluate();
          }

          const logic::Bus<DataWidth> read() const{
             return reg_output_;
          }
         
        
        private:
         //inputs
         logic::Bus<DataWidth> readData2_;
         logic::Wire& clock_;
         logic::Wire& B_write_;
         logic::Wire& reset_;
         logic::Bus<DataWidth>zero_bus_;
         logic::Bus<DataWidth>op_B_input_;
         
         //outputs
         logic::Bus<DataWidth>load_output_;
         logic::Bus<DataWidth>reg_output_;

         //muxes
         logic::Mux<DataWidth> load_mux;
         logic::Mux<DataWidth> reset_mux;

         //register
         logic::Register<DataWidth> operand_B_reg;
    };
}
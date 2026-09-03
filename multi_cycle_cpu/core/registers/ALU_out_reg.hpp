/*
Holds the ALU result
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
    template<std::size_t DataWidth =32>
    class ALU_out_reg: public logic::Component{
        public:
          ALU_out_reg(
            logic::Bus<DataWidth>&alu_data,
            logic::Wire& clock,
            logic::Wire& out_write,
            logic::Wire& reset
          ) : alu_data_(alu_data),
              clock_(clock),
              out_write_(out_write),
              reset_(reset),
              load_mux(alu_result_,alu_data,out_write_,load_output_),
              reset_mux(load_output_,zero_bus_,reset_,alu_out_reg_in),
              alu_out_reg(alu_out_reg_in,clock_,alu_result_)
            {
                zero_bus_.write(logic::LogicState::LOW);
            }
             void evaluate() noexcept {
                load_mux.evaluate();
                reset_mux.evaluate();
                alu_out_reg.evaluate();
             }

             logic::Bus<DataWidth>& output() noexcept {
                return alu_result_;
             }

             const logic::Bus<DataWidth>& output() const noexcept {
                return alu_result_;
             }

             const logic::Bus<DataWidth> read() const {
                return alu_result_;
             }

        private:
           //inputs
           logic::Bus<DataWidth>&alu_data_;
           logic::Wire& clock_;
           logic::Wire& out_write_;
           logic::Wire& reset_;
           logic::Bus<DataWidth>alu_out_reg_in;
           logic::Bus<DataWidth> zero_bus_;
           logic::Bus<DataWidth> load_output_;
           logic::Bus<DataWidth> alu_result_;

           //muxes
           logic::Mux<DataWidth>load_mux;
           logic::Mux<DataWidth> reset_mux;

           //register
           logic::Register<DataWidth>alu_out_reg;
    };
}
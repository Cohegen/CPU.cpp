#pragma once
#include <logic/sequential/registers/register.hpp>
#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/simulator/Component.hpp>
#include <logic/signals/clock.hpp>
#include <logic/combinational/multiplexers/Mux.hpp>
#include "../core/ControlSignals.hpp"


namespace cpu{
    template<std::size_t DataWdith=32,std::size_t N=5>
    class MEM_WB:public Component{
        public:
          MEM_WB(
            logic::Wire& enable,
            logic::Clock& clock,
            logic::Wire& reset,
            logic::Bus<DataWdith>& ALUResult,
            logic::Bus<DataWdith>&readData2,
            logic::Bus<N>&rd,
          ):  enable_(enable),
              clock_(clock),
              reset_(reset),
              ALUResult_(ALUResult),
              readData2_(readData2),
              rd_(rd),
              reset_mux_alu_result_(ALUResult_,zero_alu_result_,reset_,reset_out_alu_result),
              enable_mux_alu_result_(reset_out_alu_result,ALUResult_out_,enable_,alu_result_reg_in),
              ALUResult_reg_(alu_result_reg_in,clock_,ALUResult_out_),

              reset_mux_readData2_(readData2_,zero_readData2_,reset_,reset_out_readData2),
              enable_mux_readData2_(reset_out_readData2_,readData2_out_,enable_,readData2_reg_in),
              readData2_reg_(readData2_reg_in,clock_,readData2_out_),

              reset_mux_rd_(rd_,zero_rd_,reset_,reset_out_rd),
              enable_mux_rd_(reset_out_rd,rd_out_,enable_,rd_reg_in),
              rd_reg_(rd_reg_in,clock_,rd_out_)
          {
            zero_alu_result_.write(logic::LogicState::LOW);
            zero_rd_.write(logic::LogicState::LOW);
            zero_readData2_.write(logic::LogicState::LOW);
          }

          void evaluate() noexcept{
              //alu result
              reset_mux_alu_result_.evaluate(logic::LogicState::LOW);
              enable_mux_alu_result_.evaluate(logic::LogicState::LOW);
              ALUResult_reg_.evaluate(logic::LogicState::LOW);

              //readData2
              reset_mux_readData2_.evaluate();
              enable_mux_readData2_.evaluate();
              readData2_reg_.evaluate();

              //rd
              reset_mux_rd_.evaluate();
              enable_mux_rd_.evaluate();
              rd_reg_.evaluate();
          }


        private:
          //external signals
          logic::Wire& enable_;
          logic::Clock& clock_;
          logic::Wire& reset_;

          //Control signals
          PipelinedControlSignals controls_;

          //pipeline registers
          Register<DataWidth> ALUResult_reg_;
          Register<DataWidth> readData2_reg_;
          Register<N> rd_reg_;

          //inputs
          logic::Bus<DataWdith>& ALUResult_;
          logic::Bus<DataWdith>&readData2_;
          logic::Bus<N>&rd_;

          //outputs
          logic::Bus<DataWidth>&ALUResult_out_;
          logic::Bus<DataWdith>&readData2_out_;
          logic::Bus<N>&rd_out_;

          //internal buses
          logic::Bus<DataWdith> zero_alu_result_;
          logic::Bus<DataWdith> zero_readData2_;
          logic::Bus<N> zero_rd_;
          logic::Bus<DataWdith>alu_result_reg_in;
          logic::Bus<DataWdith>readData2_reg_in;
          logic::Bus<N> rd_reg_in;
          logic::Bus<DataWdith>reset_out_alu_result;
          logic::Bus<DataWdith>reset_out_readData2;
          logic::Bus<N>reset_out_rd;

          //muxes
           //1. for ALUResult
           logic::Mux<DataWdith>enable_mux_alu_result_;
           logic::Mux<DataWdith>reset_mux_alu_result_;

           logic::Mux<DataWdith>enable_mux_readData2_;
           logic::Mux<DataWdith>reset_mux_readData2_;

           logic::Mux<N>enable_mux_rd_;
           logic::Mux<N>reset_mux_rd_;
    };
}
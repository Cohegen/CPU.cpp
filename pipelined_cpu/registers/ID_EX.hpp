#pragma once
#include <logic/sequential/registers/register.hpp>
#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/simulator/Component.hpp>
#include <logic/signals/clock.hpp>
#include <logic/combinational/multiplexers/Mux.hpp>
#include "../core/ControlSignals.hpp"


namespace cpu{
    template<std::size_t AddressWidth=32,std::size_t DataWidth=32>
    class ID_EX:public Component{
        public:
          ID_EX(
           logic::Bus<AddressWidth>&pcplus4,
           logic::Bus<DataWidth>&readData1,
           logic::Bus<DataWidth>&readData2,
           logic::Bus<DataWidth>&immediate,
           logic::Bus<5>&rs,
           logic::Bus<5>&rt,
           logic::Bus<5>&rd,
           logic::Wire& enable,
          logic::Clock& clock,
          logic::Wire& reset,

          ): pcplus4_(pcplus4),
             readData1_(readData1),
             readData2_(readData2),
             immediate_(immediate),
             rs_(rs),
             rt_(rt),
             rd_(rd),
             enable_(enable),clock_(clock),
             reset_(reset),
             reset_mux_pc(pcplus4_,zero_pc_,reset_,reset_out_pc_),
             enable_mux_pc(reset_out_pc_,pcplus4_out_,enable_,pcplus4_reg_in_),
             pcplus4_reg(pcplus4_reg_in_,clock_,pcplus4_out_),

             reset_mux_imm(immediate_,zero_imm_,reset_,reset_out_imm_),
             enable_mux_imm(reset_out_imm_,immediate_out_,enable_,imm_reg_in_),
             immediate_reg_(imm_reg_in_,clock_,immediate_out_),

             reset_mux_rd1(readData1_,zero_rd1_,reset_,reset_out_rd1_),
             enable_mux_rd1(reset_out_rd1_,read_data1_out_,enable_,rd1_reg_in_),
             readData1_reg_(rd1_reg_in_,clock_,read_data1_out_),   
             
             reset_mux_rd2(readData2_,zero_rd2_,reset_,reset_out_rd2_),
             enable_mux_rd2(reset_out_rd2_,read_data2_out_,enable_,rd2_reg_in_),
             readData2_reg_(rd2_reg_in_,clock_,read_data2_out_),

             reset_mux_rs(rs_,zero_rs_,reset_,reset_out_rs_),
             enable_mux_rs(reset_out_rs_,rs_out_,enable_,rs_reg_in_),
             rs_reg(rs_reg_in_,clock_,rs_out_),

             reset_mux_rt(rt_,zero_rt_,reset_,reset_out_rt_),
             enable_mux_rt(reset_mux_rt,rt_out_,enable_,rt_reg_in_),
             rt_reg(rt_reg_in_,clock_,rt_out_),

             reset_mux_rd(rd_,zero_rd_,reset_,reset_out_rd_),
             enable_mux_rd(reset_mux_rd,rd_out_,enable_,rd_reg_in_),
             rd_reg(rd_reg_in_,clock_,rd_out_)
            { 
                zero_imm_.write(logic::LogicState::LOW);
                zero_pc_.write(logic::LogicState::LOW);
                zero_rd1_.write(logic::LogicState::LOW);
                zero_rd2_.write(logic::LogicState::LOW);
                zero_rs_.write(logic::LogicState::LOW);
                zero_rt_.write(logic::LogicState::LOW);
            }

            void evaluate() noexcept{
                //pc
                reset_mux_pc.evaluate();
                enable_mux_pc.evaluate();
                pcplus4_reg.evaluate();

                //immediate
                reset_mux_imm.evaluate();
                enable_mux_imm.evaluate();
                immediate_reg_.evaluate();

                //rd1
                reset_mux_rd1.evaluate();
                enable_mux_rd1.evaluate();
                readData1_reg_.evaluate();

                //rd2
                reset_mux_rd2.evaluate();
                enable_mux_rd2.evaluate();
                readData2_reg_.evaluate();

                //rs
                reset_mux_rs.evaluate();
                enable_mux_rs.evaluate();
                rs_reg.evaluate();

                //rt
                reset_mux_rt.evaluate();
                enable_mux_rt.evaluate();
                rt_reg.evaluate();

                //rd
                reset_mux_rd.evaluate();
                enable_mux_rd.evaluate();
                rd_reg.evaluate();
            }

        private:

          //Signals
          logic::Wire& enable_;
          logic::Clock& clock_;
          logic::Wire& reset_;

          //control Signals
          PipelinedControlSignals controls_;

          //pipeline registers
          Register<AddressWidth> pcplus4_reg;//stores the pc+4 value
          Register<DataWidth>readData1_reg_;//stores the first register operand
          Register<DataWidth>readData2_reg_;//stores the second register operand
          Register<DataWidth>immediate_reg_; //stores sign extended immediate
          Register<5>rs_reg; //first source register number
          Register<5> rt_reg; //second source register number
          Register<5> rd_reg; //destination register number

          //inputs
          logic::Bus<AddressWidth>&pcplus4_;
          logic::Bus<DataWidth>&readData1_;
          logic::Bus<DataWidth>&readData2_;
          logic::Bus<DataWidth>&immediate_;
          logic::Bus<5>&rs_;
          logic::Bus<5>&rt_;
          logic::Bus<5>&rd_;

          //outputs
          logic::Bus<AddressWidth>&pcplus4_out_;
          logic::Bus<DataWidth>&read_data1_out_;
          logic::Bus<DataWidth>&read_data2_out_;
          logic::Bus<DataWidth>&immediate_out_;
          logic::Bus<5>&rs_out_;
          logic::Bus<5>&rt_out_;
          logic::Bus<5>&rd_out_;

           //internal buses
           logic::Bus<InstructionWidth> zero_rd1_;
           logic::Bus<InstructionWidth> zero_rd2_;
           logic::Bus<InstructionWidth> zero_imm_;
           logic::Bus<InstructionWidth> zero_rs_;
           logic::Bus<InstructionWidth> zero_rt_;
           logic::Bus<InstructionWidth> zero_rd_;
           logic::Bus<AddressWidth>zero_pc_;
           logic::Bus<InstructionWidth>rd1_reg_in_;
           logic::Bus<AddressWidth>pcplus4_reg_in_;
           logic::Bus<InstructionWidth>rd2_reg_in_;
           logic::Bus<InstructionWidth>imm_reg_in_;
           logic::Bus<InstructionWidth>rs_reg_in_;
           logic::Bus<InstructionWidth>rt_reg_in_;
           logic::Bus<InstructionWidth>rd_reg_in_;
           logic::Bus<N>reset_out_rd1_;
           logic::Bus<N>reset_out_pc_;
           logic::Bus<N>reset_out_rd2_;
           logic::Bus<N>reset_out_imm_;
           logic::Bus<N>reset_out_rs_;
           logic::Bus<N>reset_out_rt_;
           logic::Bus<N>reset_out_rd_;
          /*
          Multiplexers for the registers
          */
          //1. for pcplus4_reg
          logic::Mux<AddressWidth>enable_mux_pc;
          logic::Mux<AddressWidth>reset_mux_pc;

          //2.for readData1
          logic::Mux<DataWidth>enable_mux_rd1;
          logic::Mux<DataWidth>reset_mux_rd1;

          //3. for readData2
          logic::Mux<DataWidth>enable_mux_rd2;
          logic::Mux<DataWidth>reset_mux_rd2;

          //4. for immediate register
          logic::Mux<DataWidth>enable_mux_imm;
          logic::Mux<DataWidth>reset_mux_imm;

          //5. for source1
          logic::Mux<5>enable_mux_rs;
          logic::Mux<5>reset_mux_rs;

          //6. for source2
          logic::Mux<5>enable_mux_rt;
          logic::Mux<5>reset_mux_rt;

          //7. for destination
          logic::Mux<5>enable_mux_rd;
          logic::Mux<5>reset_mux_rd;





    };
}
#pragma once
#include <logic/sequential/registers/register.hpp>
#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/simulator/Component.hpp>
#include <logic/signals/clock.hpp>
#include <logic/combinational/multiplexers/Mux.hpp>

namespace cpu{
    template<std::size_t InstructionWidth=32,std::size_t AddressWidth=32>
    class IF_ID:public Component{

        public:
          IF_ID(
            logic::Bus<InstructionWidth>&instruction_in,
            logic::Bus<AddressWidth>&pcplus4_in,
            logic::Wire&enable,
            logic::Wire& reset,
            logic::Clock& clock,
          ): instruction_in_(instruction_in),
          pcplus4_in_(pcplus4_in),
          enable_(enable),
          reset_(reset),
          clock_(clock), 
          reset_mux_instr(instruction_in_,zero_instr_,reset_,reset_out_instr_),
          enable_mux_instr(reset_out_,instruction_out_,enable_,instr_reg_in_),
          instruction_register_(instr_reg_in_,clock_,instruction_out_),

          reset_mux_pc(pcplus4_in_,zero_instr_,reset_,reset_out_pc_),
          enable_mux_pc(reset_mux_pc_,pcplus4_out_,enable_,pcplus4_reg_in_),
          pcplus4_register_(pcplus4_reg_in_,clock_,pcplus4_out_)
           {
            zero_instr_.write(logic::logicState::LOW);
            zero_pc_.write(logic::logicState::LOW);
           }

           void evaluate() noexcept override{
            //instruction part
            reset_mux_instr.evaluate();
            enable_mux_instr.evaluate();
            instruction_register_.evaluate();

            //pc section
            reset_mux_pc.evaluate();
            enable_mux_pc.evaluate();
            pcplus4_register_.evaluate();
           }

        private:
          //pipeline registers
          Register<InstructionWidth> instruction_register_;
          Register<AddressWidth> pcplus4_register_;

          //signals
          logic::Wire& enable_;
          logic::Clock& clock_;
          logic::Wire& reset_;

          //inputs
          logic::Bus<InstructionWidth>& instruction_in_;
          logic::Bus<AddressWidth>& pcplus4_in_;

          //outputs
          logic::Bus<InstructionWidth>&instruction_out_;
          logic::Bus<AddressWidth>& pcplus4_out_;

          //internal buses
          logic::Bus<InstructionWidth> zero_instr_;
          logic::Bus<AddressWidth>zero_pc_;
          logic::Bus<InstructionWidth>instr_reg_in_;
          logic::Bus<AddressWidth>pcplus4_reg_in_;
          logic::Bus<N>reset_out_instr_;
          logic::Bus<N>reset_out_pc_;
          
          
          //muxes for instructon_reg
          logic::Mux<InstructionWidth> enable_mux_instr;
          logic::Mux<InstructionWidth> reset_mux_instr;

          //muxes for pcplus4
          logic::Mux<AddressWidth> enable_mux_pc;
          logic::Mux<AddressWidth> reset_mux_pc;
          
    };
}
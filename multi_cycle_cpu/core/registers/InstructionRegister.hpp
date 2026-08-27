/*
Temporary register which holds the currently fetched instruction

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
    template <std::size_t InstructionWidth = 32>
    class InstructionRegister:public logic::Component{
        public:
         InstructionRegister(
            logic::Bus<InstructionWidth>&input,
            logic::Wire& clock,
            logic::Wire& ir_write,
            logic::Wire& reset
         ): input_(input),
            clock_(clock),
            ir_write_(ir_write),
            reset_(reset),
            load_mux(ir_output_,input_,ir_write_,load_output_),
            reset_mux(load_output_,zero_bus,reset_,ir_input_),
            ir_(ir_input_,clock_,ir_output_){
            zero_bus.write(logic::LogicState::LOW);
         }

        
        void evaluate() noexcept {
            load_mux.evaluate();
            reset_mux.evaluate();
            ir_.evaluate();
        }
        const logic::Bus<InstructionWidth> read() const{
            return ir_output_;
            }

      

        
        private:
             //inputs
             logic::Bus<InstructionWidth>&input_;
             logic::Wire& clock_;
             logic::Wire& ir_write_;
             logic::Wire& reset_;
             logic::Bus<InstructionWidth>ir_output_;
             logic::Bus<InstructionWidth> load_output_;
             logic::Bus<InstructionWidth>ir_input_;
             logic::Bus<InstructionWidth> zero_bus;

             //muxes
             logic::Mux<InstructionWidth> load_mux;
             logic::Mux<InstructionWidth> reset_mux;
    
             //register
             logic::Register<InstructionWidth> ir_;
        };

       


    }


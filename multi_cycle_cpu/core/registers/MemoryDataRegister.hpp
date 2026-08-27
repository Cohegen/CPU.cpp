/*
Holds data from memory
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
    template<std::size_t DataWidth= 32>
    class MemoryDataRegister:public logic::Component{
        public:
           MemoryDataRegister(
            logic::Bus<DataWidth>& data,
            logic::Wire& clock,
            logic::Wire& mdr_write,
            logic::Wire& reset
           ): memory_data_(data),
              clock_(clock),
              mdr_write_(mdr_write),
              reset_(reset),
              load_mux(mdr_output_,memory_data_,mdr_write_,load_output_),
              reset_mux(load_output_,zero_bus_,reset_,mdr_input_),
              mdr_(mdr_input_,clock_,mdr_output_)
           {
            zero_bus_.write(logic::LogicState::LOW);
           }

           void evaluate() noexcept {
            load_mux.evaluate();
            reset_mux.evaluate();
            mdr_.evaluate();
           }

           const logic::Bus<DataWidth>read() const{
              return mdr_output_;
           }

        private:
        //inputs
        logic::Bus<DataWidth>& memory_data_;
        logic::Wire& clock_;
        logic::Wire& mdr_write_;
        logic::Wire& reset_;
        logic::Bus<DataWidth>zero_bus_;
        logic::Bus<DataWidth> mdr_input_;
        //outputs
        logic::Bus<DataWidth> load_output_;
        logic::Bus<DataWidth>mdr_output_;

        //muxes
        logic::Mux<DataWidth> load_mux;
        logic::Mux<DataWidth> reset_mux;

        // memory data register
        logic::Register<DataWidth> mdr_;

    };
}
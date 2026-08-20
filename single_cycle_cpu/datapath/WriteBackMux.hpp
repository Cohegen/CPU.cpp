
#pragma  once

#include <logic/combinational/multiplexers/Mux.hpp>

#include <logic/signals/wire.hpp>
#include <logic/signals/bus.hpp>
#include <logic/simulator/Component.hpp>

namespace cpu {
    template<std::size_t N=32>
    class WriteBackMux :public logic::Component
    {
        public:
          WriteBackMux(
            logic::Bus<N>& alu_result,
            logic::Bus<N>& memory_data,
            logic::Wire& memory_to_register,
            logic::Bus<N>& output
          ):
            alu_result_(alu_result),
            memory_data_(memory_data),
            memory_to_register_(memory_to_register),
            output_(output),
            mux_(
                alu_result_,
                memory_data_,
                memory_to_register_,
                output_
            )
          
          {}

          void evaluate() noexcept override{
            mux_.evaluate();
          }

        private:
        //muxes inputs
        logic::Bus<N>& alu_result_;
        logic::Bus<N>& memory_data_;
        logic::Wire& memory_to_register_;
        logic::Bus<N>& output_;

        //Internal components
        logic::Mux<N> mux_;

    };
}
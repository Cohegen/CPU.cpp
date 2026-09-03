#pragma once
#include <cstddef>
#include <logic/signals/bus.hpp>
#include <logic/combinational/multiplexers/Mux3.hpp>
#include <logic/simulator/Component.hpp>
#include <logic/signals/wire.hpp>

namespace cpu {
    template<std::size_t DataWidth=32,std::size_t N=32>
    class OutMux:public logic::Component{
        public:
          static_assert(DataWidth == N, "DataWidth must be equal to N");

          OutMux(
             logic::Bus<DataWidth>&alu_result,
             logic::Bus<DataWidth>&alu_out,
             logic::Bus<DataWidth>&PcJump,
             logic::Wire& select0,
             logic::Wire& select1,
             logic::Bus<N>& pc_val
          ):  alu_result_(alu_result),
              alu_out_(alu_out),
              PCJump_(PcJump),
              select0_(select0),
              select1_(select1),
              pc_val_(pc_val),
              OutMux_(alu_result_,alu_out_,PCJump_,select0_,select1_,pc_val_)
             {}

           void evaluate() noexcept override{
             OutMux_.evaluate();
           }
        private:
           //inputs
           logic::Bus<DataWidth>&alu_result_;
           logic::Bus<DataWidth>&alu_out_;
           logic::Bus<DataWidth>&PCJump_;
           logic::Wire& select0_;
           logic::Wire& select1_;

           //output
           logic::Bus<N>&pc_val_;

           //Mux
           logic::Mux3<N>OutMux_;
    };
}
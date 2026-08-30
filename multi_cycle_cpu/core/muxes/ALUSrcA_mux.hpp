/*
selects PC or A depending on the operation
*/
#pragma once
#include <logic/signals/bus.hpp>
#include <logic/combinational/multiplexers/Mux.hpp>
#include <logic/simulator/Component.hpp>
#include <logic/signals/wire.hpp>

namespace cpu{
    template<std::size_t AdressWidth=32,std::size_t DataWidth=32,std::size_t N=32>
    class ALUSrcA_mux:public logic::Component{
        public:
          static_assert(AdressWidth == N, "AdressWidth must be equal to N");
          static_assert(DataWidth == N, "DataWidth must be equal to N");

          ALUSrcA_mux(
            logic::Bus<AdressWidth>&pc_val,
            logic::Bus<DataWidth>& data,
            logic::Wire& ALUSrcA,
            logic::Bus<N>& srcA
          ):pc_value_(pc_val),
            readData1_(data),
            ALUSrcA_(ALUSrcA),
            SrcA_(srcA),
            ALUSrcAMux(pc_val,readData1_,ALUSrcA_,SrcA_)
           {}

          void evaluate() noexcept override{
             ALUSrcAMux.evaluate();
          }

        private:
         //inputs
         logic::Bus<AdressWidth>& pc_value_;
         logic::Bus<DataWidth>&readData1_;
         logic::Wire& ALUSrcA_;

         //output
         logic::Bus<N>&SrcA_;

         //mux
         logic::Mux<N>ALUSrcAMux;
    };
}
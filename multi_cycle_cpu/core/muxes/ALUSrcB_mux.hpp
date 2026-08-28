/*
selects B, constant 4,sign extended immediate or shifted immediate
*/
#pragma once
#include <cstddef>
#include <logic/signals/bus.hpp>
#include <logic/combinational/multiplexers/Mux4.hpp>
#include <logic/simulator/component.hpp>
#include <logic/signals/wire.hpp>

namespace cpu {
    template<std::size_t DataWidth=32,std::size_t N=32>
    class ALUSrcB_mux:public Component{
        public:
         ALUSrcB_mux(
            logic::Bus<DataWidth>&data,
            logic::Bus<DataWidth>&val,
            logic::Bus<DataWidth>&sign,
            logic::Bus<DataWidth>&shift,
            logic::Wire& alusrcB,
            logic::Bus<N>SrcB,
         ):  readData2_(data),
             constant_(val),
             sign_extended_(sign),
             shifted_val_(shift),
             ALUSrcB_(alusrcB),
             SrcB_(SrcB),
             ALUSrcBMux_(readData2_,constant_,sign_extended_,shifted_val_,ALUSrcB_,SrcB_)
            {}

           void evaluate() noexcept{
            ALUSrcBMux_.evaluate();
           }
        private:
          //inputs
          logic::Bus<DataWidth>&readData2_;
          logic::Bus<DataWidth>&constant_;
          logic::Bus<DataWidth>&sign_extended_;
          logic::Bus<DataWidth>&shifted_val_;
          logic::Wire& ALUSrcB_;

          //output
          logic::Bus<N>SrcB_;

          //Mux
          logic::Mux4<N>ALUSrcBMux_;
    };
}
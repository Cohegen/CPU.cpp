/*
chooses the next PC value
*/
#pragma once
#include <cstddef>
#include <logic/signals/bus.hpp>
#include <logic/combinational/multiplexers/Mux.hpp>
#include <logic/simulator/component.hpp>
#include <logic/signals/wire.hpp>

namespace cpu{
    template<std::size_t AddressWidth=32,std::size_t DataWidth=32,std::size_t N=32>
    class PCSource_mux:public logic::Component{
        public:
          static_assert(AddressWidth == N, "AddressWidth must be equal to N");
          static_assert(DataWidth == N, "DataWidth must be equal to N");

          PCSource_mux(
            logic::Bus<AddressWidth>&pc_val,
            logic::Bus<DataWidth>& alu_result,
            logic::Wire& IorD,
            logic::Bus<N>& address
          ):  pc_value_(pc_val),
              alu_result_(alu_result),
              IorD_(IorD),
              address_(address),
              pc_source_mux(pc_value_,alu_result_,IorD_,address_)
           {}

          void evaluate() noexcept override{
              pc_source_mux.evaluate();
          }
        private:
           //inputs
           logic::Bus<AddressWidth>&pc_value_;
           logic::Bus<DataWidth>& alu_result_;
           logic::Wire& IorD_;

           //output
           logic::Bus<N>&address_;

           //mux
           logic::Mux<N>pc_source_mux;

    };
}
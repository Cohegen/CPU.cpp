#pragma once
#include <cstddef>
#include <logic/signals/bus.hpp>
#include <logic/combinational/multiplexers/Mux.hpp>
#include <logic/simulator/Component.hpp>
#include <logic/signals/wire.hpp>

namespace cpu{
    template<std::size_t RegisterAddressWidth=4, std::size_t N=4>
    class RegDestMux:public logic::Component{
        public:
          static_assert(RegisterAddressWidth == N, "RegisterAddressWidth must be equal to N");

          RegDestMux(
            logic::Bus<RegisterAddressWidth>& rt,
            logic::Bus<RegisterAddressWidth>& rd,
            logic::Wire& RegDst,
            logic::Bus<N>& reg_dest
          ):  rt_(rt),
              rd_(rd),
              RegDst_(RegDst),
              reg_dest_(reg_dest),
              reg_dest_mux(rt_, rd_, RegDst_, reg_dest_)
           {}

          void evaluate() noexcept override{
              reg_dest_mux.evaluate();
          }
        private:
           //inputs
           logic::Bus<RegisterAddressWidth>& rt_;
           logic::Bus<RegisterAddressWidth>& rd_;
           logic::Wire& RegDst_;

           //output
           logic::Bus<N>& reg_dest_;

           //mux
           logic::Mux<N> reg_dest_mux;

    };
}
#pragma once
#include <logic/combinational/comparators/Comparator.hpp>
#include <logic/combinational/multiplexers/Mux.hpp>
#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>

namespace cpu {
    template<std::size_t Width>
    class MinimumUnit
    {

        public:
           MinimumUnit(
            logic::Bus<Width>&a,
            logic::Bus<Width>&b,
            logic::Bus<Width>&minimum
           ):a_(a),
             b_(b),
             minimum_(minimum),
             comparator_(a_,b_,equal_,greater_,less_),
             mux_(a_,b_,greater_,minimum_)
           {}

          void evaluate() noexcept
          {
            comparator_.evaluate();
            mux_.evaluate();
          }
        private:
          //circuitry inputs
          logic::Bus<Width>& a_;
          logic::Bus<Width>& b_;

          //outputs
          logic::Bus<Width>& minimum_;

          //internal signals
          logic::Wire equal_;
          logic::Wire greater_;
          logic::Wire less_;

          //internal hardware components
          logic::Comparator<Width>comparator_;
          logic::Mux<Width> mux_;

          
    };
    
}
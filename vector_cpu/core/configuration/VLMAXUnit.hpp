/*
Calculates the VLMAX

VLMAX represents the maximum number of elements that fit
into the vector register group
*/
#pragma once
#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/gates/NOT.hpp>
#include <logic/combinational/decoders/Decoder.hpp>

namespace cpu
{
    class VLMAXUnit {
    public:
        VLMAXUnit(
            logic::Bus<2>& sew,
            logic::Bus<8>& vlmax,
            logic::Wire& valid
        ) : sew_(sew),
            vlmax_(vlmax),
            valid_(valid),
            decoder_(sew_, decoded_),
            not_valid_(decoded_[3], valid_)
        {}

        void evaluate() noexcept
        {
            decoder_.evaluate();
            not_valid_.evaluate();

            // Mapping decoder outputs to VLMAX:
            // SEW = 00 (8-bit)  -> VLMAX = 16 (bit 4)
            // SEW = 01 (16-bit) -> VLMAX = 8  (bit 3)
            // SEW = 10 (32-bit) -> VLMAX = 4  (bit 2)
            // SEW = 11 (RSVD)   -> VLMAX = 0
            for (std::size_t i = 0; i < 8; ++i)
            {
                if (i != 2 && i != 3 && i != 4)
                {
                    vlmax_[i].write(logic::LogicState::LOW);
                }
            }

            vlmax_[4].write(decoded_[0].read());
            vlmax_[3].write(decoded_[1].read());
            vlmax_[2].write(decoded_[2].read());
        }

    private:
        // Inputs
        logic::Bus<2>& sew_;

        // Outputs
        logic::Bus<8>& vlmax_;
        logic::Wire& valid_;

        // Internal components
        logic::Bus<4> decoded_;
        logic::Decoder2to4 decoder_;
        logic::NotGate not_valid_;
    };
}

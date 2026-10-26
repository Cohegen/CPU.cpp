#pragma once

/*
An implementation of the Vector Masked Writeback unit
*/

#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/combinational/multiplexers/Mux.hpp>
#include <logic/simulator/Component.hpp>
#include <array>
#include <cstddef>

namespace cpu {
    class VectorMaskedWriteBackUnit : public logic::Component
    {
    public:
        VectorMaskedWriteBackUnit(
            logic::Bus<128>& execution_result,
            logic::Bus<128>& old_destination,
            logic::Bus<4>& lane_mask,
            logic::Bus<128>& writeback_result
        ) : 
            execution_result_(execution_result),
            old_destination_(old_destination),
            lane_mask_(lane_mask),
            writeback_result_(writeback_result),
            muxes_{{
                {old_dest_bus_[0], split_bus_[0], lane_mask_[0], output_lane_[0]},
                {old_dest_bus_[1], split_bus_[1], lane_mask_[1], output_lane_[1]},
                {old_dest_bus_[2], split_bus_[2], lane_mask_[2], output_lane_[2]},
                {old_dest_bus_[3], split_bus_[3], lane_mask_[3], output_lane_[3]}
            }}
        {}

        void evaluate() noexcept override
        {
            for (std::size_t lane{0}; lane < 4; ++lane)
            {
                for (std::size_t bit{0}; bit < 32; ++bit)
                {
                    const std::size_t source_idx = lane * 32 + bit;
                    split_bus_[lane][bit].write(
                        execution_result_[source_idx].read()
                    );
                    old_dest_bus_[lane][bit].write(
                        old_destination_[source_idx].read()
                    );
                }
            }

            for (auto& mux : muxes_)
            {
                mux.evaluate();
            }

            // combining results
            for (std::size_t lane = 0; lane < 4; ++lane)
            {
                for (std::size_t bit = 0; bit < 32; ++bit)
                {
                    const std::size_t dest_idx = lane * 32 + bit;

                    writeback_result_[dest_idx].write(
                        output_lane_[lane][bit].read()
                    );
                }
            }
        }

    private:
        // inputs
        logic::Bus<128>& execution_result_;
        logic::Bus<128>& old_destination_;
        logic::Bus<4>& lane_mask_;

        // output
        logic::Bus<128>& writeback_result_;

        // internal hardware components
        std::array<logic::Bus<32>, 4> split_bus_;
        std::array<logic::Bus<32>, 4> old_dest_bus_;
        std::array<logic::Bus<32>, 4> output_lane_;

        std::array<logic::Mux<32>, 4> muxes_;
    };
}

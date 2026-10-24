#pragma once
#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include "VectorLane.hpp"
#include <array>
namespace cpu {
    template<
    std::size_t VLEN=128,
    std::size_t num_lanes=4,
    std::size_t lane_width=32
    >
    class VectorExecutionUnit
    {
        static_assert(VLEN==num_lanes*lane_width,"Low" );
        static_assert(num_lanes==4 );
        static_assert(lane_width==32 );
        public:
           VectorExecutionUnit(
            logic::Bus<VLEN>&vec_a,
            logic::Bus<VLEN>&vec_b,
            logic::Bus<3>&opcode,
            logic::Bus<VLEN>&result_vec
           ):
             vec_a_(vec_a),
             vec_b_(vec_b),
             opcode(opcode),
             result_vec_(result_vec)
            {
                
            }

            void evaluate()noexcept
            {
                //splitting logic
                for(std::size_t lane=0;lane<num_lanes;++lane)
                {
                    for(std::size_t bit=0;bit<lane_width;++bit)
                    {
                        const std::size_t source_idx = lane*lane_width+bit;

                        operand_a_[lane][bit].write(
                            vec_a_[source_idx].read()
                        );

                        operand_b_[lane][bit].write(
                            vec_b_[source_idx].read()
                        );
                    }
                }


                for(std::size_t idx{0};idx<4;++idx)
                {
                    vector_lanes_[idx].evaluate();
                }

                //combining results
                for(std::size_t lane=0;lane<num_lanes;++lane)
                {
                    for(std::size_t bit=0;bit<lane_width;++bit)
                    {
                        const std::size_t dest_idx = lane*lane_width+bit;

                        result_vec_[dest_idx].write(
                            lane_results_[lane][bit].read();
                        );
                    }
                }
            }
        private:
          logic::Bus<VLEN>&vec_a_;
          logic::Bus<VLEN>&vec_b_;
          logic::Bus<3>&opcode;
          logic::Bus<VLEN>&result_vec_;

         

          //4 32-bit operand a buses
          std::array<logic::Bus<lane_width>,num_lanes>operand_a_;

          //4 32-bit operand b bus
          std::array<logic::Bus<lane_width>,num_lanes>operand_b_;

          //4 32-bit lane results
          std::array<logic::Bus<lane_width>,num_lanes> lane_results_;

          //4 zero and carry flag wires
          std::array<logic::Wire,num_lanes>zero_flags_;
          std::array<logic::Wire,num_lanes>carry_flags_;

          //internal hardware components
          std::array<VectorLane,num_lanes>vector_lanes_{
                VectorLane{operand_a_[0], operand_b_[0], opcode,
                           lane_results_[0], zero_flags_[0], carry_flags_[0]},
            
                VectorLane{operand_a_[1], operand_b_[1], opcode,
                           lane_results_[1], zero_flags_[1], carry_flags_[1]},
            
                VectorLane{operand_a_[2], operand_b_[2], opcode,
                           lane_results_[2], zero_flags_[2], carry_flags_[2]},
            
                VectorLane{operand_a_[3], operand_b_[3], opcode,
                           lane_results_[3], zero_flags_[3], carry_flags_[3]}
            
          };
    };
}
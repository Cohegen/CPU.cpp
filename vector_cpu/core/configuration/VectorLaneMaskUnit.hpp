#pragma once
#include <logic/combinational/comparators/Comparator.hpp>
#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/simulator/Component.hpp>
#include <array>
#include <cstddef>

namespace cpu {

    class VectorLaneMaskUnit : public logic::Component
    {
    public:
        VectorLaneMaskUnit(
            logic::Bus<8>& vector_length,
            logic::Bus<4>& lane_activation
        ) :
            vector_length_(vector_length),
            lane_activation_(lane_activation),
            comparators_{
                logic::Comparator<8>{
                    indices_[0],
                    vector_length_,
                    equal_flags_[0],
                    greater_flags_[0],
                    lane_activation_[0]
                },
                logic::Comparator<8>{
                    indices_[1],
                    vector_length_,
                    equal_flags_[1],
                    greater_flags_[1],
                    lane_activation_[1]
                },
                logic::Comparator<8>{
                    indices_[2],
                    vector_length_,
                    equal_flags_[2],
                    greater_flags_[2],
                    lane_activation_[2]
                },
                logic::Comparator<8>{
                    indices_[3],
                    vector_length_,
                    equal_flags_[3],
                    greater_flags_[3],
                    lane_activation_[3]
                }
            }
        {
            initialize_indices();
        }

        void evaluate() noexcept override
        {
            for (auto& comp : comparators_)
            {
                comp.evaluate();
            }
        }

        [[nodiscard]] const logic::Bus<8>& get_lane_index(std::size_t lane) const
        {
            return indices_.at(lane);
        }

        [[nodiscard]] const std::array<logic::Bus<8>, 4>& indices() const noexcept
        {
            return indices_;
        }

        [[nodiscard]] logic::LogicState get_equal_flag(std::size_t lane) const
        {
            return equal_flags_.at(lane).read();
        }

        [[nodiscard]] logic::LogicState get_greater_flag(std::size_t lane) const
        {
            return greater_flags_.at(lane).read();
        }

    private:
        // Inputs
        logic::Bus<8>& vector_length_;

        // Outputs
        logic::Bus<4>& lane_activation_;

        // Internal hardware components
        std::array<logic::Bus<8>, 4> indices_;
        std::array<logic::Wire, 4> greater_flags_;
        std::array<logic::Wire, 4> equal_flags_;
        std::array<logic::Wire, 4> less_flags_;

        std::array<logic::Comparator<8>, 4> comparators_;

        void initialize_indices()
        {
            for (std::size_t lane = 0; lane < 4; ++lane)
            {
                for (std::size_t bit = 0; bit < 8; ++bit)
                {
                    const bool bit_value = (lane >> bit) & 1U;
                    indices_[lane][bit].write(
                        bit_value ? logic::LogicState::HIGH : logic::LogicState::LOW
                    );
                }
            }
        }
    };

} // namespace cpu

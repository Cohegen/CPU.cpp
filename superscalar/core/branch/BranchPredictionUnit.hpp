/*
An implementation of a Two-Wide Branch Prediction Unit
*/
#pragma once
#include <cstddef>
#include <cstdint>

#include "BranchExecutionUnit.hpp"
#include "BranchPredictor.hpp"

namespace cpu {
    struct BranchPredictionResult
    {
        BranchPrediction lane0{};
        BranchPrediction lane1{};

        bool lane0_is_control_flow{false};
        bool lane1_is_control_flow{false};

        bool redirect{false};
        std::uint8_t redirect_lane{0};
        std::uint32_t redirect_pc{0};
    };

    struct PredictionInfo
    {
        bool valid{false};
        bool predicted_taken{false};
        std::uint32_t predicted_target{0};
    };

    template<std::size_t TableSize = 256>
    class BranchPredictionUnit
    {
        public:
        using Predictor = BranchPredictor<TableSize>;

        explicit BranchPredictionUnit(Predictor& predictor) noexcept : predictor_(predictor){


        }

        [[nodiscard]]
        BranchPredictionResult evaluate(
            std::uint32_t pc0,
            bool valid0,
            ControlFlow control_flow0,
            std::uint32_t pc1,
            bool valid1,
            ControlFlow control_flow1
        )const noexcept
        {
            BranchPredictionResult result{};
            result.lane0_is_control_flow = valid0 && is_control_flow(control_flow0);
            result.lane1_is_control_flow = valid1 && is_control_flow(control_flow1);

            if(result.lane0_is_control_flow)
            {
                result.lane0 = predictor_.predict(pc0);
            }
            if(result.lane1_is_control_flow)
            {
                result.lane1 = predictor_.predict(pc1);
            }

            /*
            Lane 0 is older than lane 1
            This lane 0 always has priority if it
            predicts a valid taken target
            */
            if(result.lane0_is_control_flow && result.lane0.taken && result.lane0.btb_hit)
            {
                result.redirect = true;
                result.redirect_lane = 0;
                result.redirect_pc = result.lane0.target;
            }else if(result.lane0_is_control_flow && result.lane1.taken && result.lane1.btb_hit)
            {
                result.redirect = true;
                result.redirect_lane = 1;
                result.redirect_pc = result.lane1.target;
            }

            return result;
        }

        private:
           [[nodiscard]]
           static constexpr bool is_control_flow(ControlFlow control_flow)noexcept
           {
            return control_flow != ControlFlow::NONE;
           }
           Predictor& predictor_;
    };

}
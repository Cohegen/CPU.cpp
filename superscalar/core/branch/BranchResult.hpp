#pragma once
#include <cstdint>

namespace cpu
{
    struct BranchResult
    {
        bool valid{false};
        
        //actual branch outcome
        bool taken{false};

        //actual target when the branch is taken
        std::uint32_t target{};

        //PC of the instruction
        std::uint32_t pc{};

        //PC+4 
        std:::uint32_t fallthrough{};

        //Prediction information
        bool predicted_taken{false};
        std::uint32_t predicted_target{};

        //set when the actual execution disagrees with prediction
        bool mispredicted{false};
    };
}
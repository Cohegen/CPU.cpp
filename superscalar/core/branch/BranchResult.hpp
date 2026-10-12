#pragma once
#include <cstddef>
#include <cstdint>
#include "../../include/isa/Opcode.hpp"

namespace cpu
{
    struct BranchResult
    {
        bool valid{false};
        std::size_t rob_index{0};
        std::uint32_t pc{};
        Opcode opcode{Opcode::NOP};
        
        // actual branch outcome
        bool taken{false};

        // actual target when the branch is taken
        std::uint32_t target{};
        std::uint32_t target_pc{};

        // PC+4 
        std::uint32_t fallthrough{};
        std::uint32_t fallthrough_pc{};

        // next PC according to branch outcome
        std::uint32_t next_pc{};

        // Prediction information
        bool predicted_taken{false};
        std::uint32_t predicted_target{};

        // set when the actual execution disagrees with prediction
        bool mispredicted{false};

        // redirect PC when mispredicted
        std::uint32_t redirect_pc{};
    };
}
#pragma once

#include <cstddef>
#include <cstdint>

#if __has_include("isa/Opcode.hpp")
#include "isa/Opcode.hpp"
#elif __has_include("Opcode.hpp")
#include "Opcode.hpp"
#else
#include "../../../include/isa/Opcode.hpp"
#endif

namespace cpu
{

struct BranchResult
{
    bool valid{false};
    std::size_t rob_index{0};
    std::uint32_t pc{0};
    Opcode opcode{Opcode::NOP};

    // Actual branch resolution
    bool taken{false};
    std::uint32_t target_pc{0};
    std::uint32_t fallthrough_pc{0};
    std::uint32_t next_pc{0};

    // Prediction verification
    bool predicted_taken{false};
    std::uint32_t predicted_target{0};
    bool mispredicted{false};
    std::uint32_t redirect_pc{0};
};

} // namespace cpu

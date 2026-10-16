#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "../include/isa/Opcode.hpp"
#include "../include/isa/Registers.hpp"
#include "../superscalar/datapath/SuperScalarCPU.hpp"

namespace cpu {

// Top-level convenience alias for 32-bit default configuration
using SuperScalarCPU32 = SuperScalarCPU<32, 32, 64, 16, 32, 16, 1024, 256, 8>;

} // namespace cpu

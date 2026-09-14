#pragma once

#include "SingleCycleCPU.hpp"
#include "MultiCycleCPU.hpp"
#include "PipelinedCPU.hpp"

namespace cpu {

    template<
        std::size_t AddressWidth = 32,
        std::size_t DataWidth = 32,
        std::size_t InstructionWidth = 32,
        std::size_t RegisterAddressWidth = 4,
        std::size_t InstructionMemoryAddressWidth = 8,
        std::size_t DataMemoryAddressWidth = 8
    >
    using CPU = SingleCycleCPU<
        AddressWidth,
        DataWidth,
        InstructionWidth,
        RegisterAddressWidth,
        InstructionMemoryAddressWidth,
        DataMemoryAddressWidth
    >;

} // namespace cpu
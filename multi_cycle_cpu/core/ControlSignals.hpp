#pragma once
#include <cstdint>
namespace cpu{
    enum class ALUSrcA : std::uin8_t{
        pc,
        RegA
    };
    enum class ALUSrcB:std::unint8_t{
        regB,
        constant,
        sign_extend,
        shifted
    };

    enum class PCSource: std::uint8_t{
        aluResult,
        aluOut,
        jumpTarget
    };

    enum class WriteBackSource:std::uint8_t{
        aluOut,
        memoryData
    };

    struct MultiCycleControlSignals{
        bool pcWrite;
        bool irWrite;
        bool mdrWrite;

        bool aWrite;
        bool bWrite;
        bool aluOutWrite;

        bool regWrite;
        bool memWrite;

        ALUSrcA aluSrcA;
        ALUSrcB aluSrcB;
        PCSource pcSource;
        WriteBackSource writebackSource;
    };

}

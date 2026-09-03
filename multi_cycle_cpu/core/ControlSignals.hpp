#pragma once
#include <cstdint>

#include "../../components/ControlSignals.hpp"

namespace cpu{
    enum class ALUSrcA : std::uint8_t{
        pc,
        RegA
    };
    enum class ALUSrcB:std::uint8_t{
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
        bool pcWrite = false;
        bool pcWriteCond = false;
        bool bne = false;
        bool irWrite = false;
        bool mdrWrite = false;

        bool aWrite = false;
        bool bWrite = false;
        bool aluOutWrite = false;

        bool regWrite = false;
        bool memWrite = false;
        bool memRead = false;
        bool iorD = false;
        bool regDst = true;
        bool halt = false;

        ALUSrcA aluSrcA = ALUSrcA::pc;
        ALUSrcB aluSrcB = ALUSrcB::regB;
        PCSource pcSource = PCSource::aluResult;
        WriteBackSource writebackSource = WriteBackSource::aluOut;
        ALUOperation aluOperation = ALUOperation::ADD;
    };

}

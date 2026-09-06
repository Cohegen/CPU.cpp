/*
Control signals of the pipelined processor
*/
#pragma once

#include <cstdint>
#include "../../components/ControlSignals.hpp"

namespace cpu {

    struct EXControlSignals {
        bool regDst = false;
        bool aluSrc = false;
        ALUOperation aluOp = ALUOperation::ADD;

        bool operator==(const EXControlSignals&) const noexcept = default;
    };

    struct MEMControlSignals {
        bool branch = false;
        bool memRead = false;
        bool memWrite = false;

        bool operator==(const MEMControlSignals&) const noexcept = default;
    };

    struct WBControlSignals {
        bool regWrite = false;
        bool memToReg = false;

        bool operator==(const WBControlSignals&) const noexcept = default;
    };

    struct PipelinedControlSignals {
        // EX stage
        bool regDst = false;
        bool aluSrc = false;
        ALUOperation aluOp = ALUOperation::ADD;

        // MEM stage
        bool branch = false;
        bool memRead = false;
        bool memWrite = false;

        // WB stage
        bool regWrite = false;
        bool memToReg = false;

        bool operator==(const PipelinedControlSignals&) const noexcept = default;

        EXControlSignals ex() const noexcept {
            return {regDst, aluSrc, aluOp};
        }

        MEMControlSignals mem() const noexcept {
            return {branch, memRead, memWrite};
        }

        WBControlSignals wb() const noexcept {
            return {regWrite, memToReg};
        }
    };
}


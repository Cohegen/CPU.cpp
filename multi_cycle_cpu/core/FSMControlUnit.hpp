#pragma once

#include <cstdint>
#include <string_view>

#include "../../components/ControlSignals.hpp"
#include "../../include/isa/DecodedInstruction.hpp"
#include "../../include/isa/InstructionFormat.hpp"
#include "../../include/isa/Opcode.hpp"
#include "ControlSignals.hpp"

namespace cpu {

/*
 Autonomous Finite State Machine (FSM) Control Unit for the Multi-Cycle CPU
 
 Implements the classic multi-cycle control architecture based on Harris and Harris / Patterson and Hennessy
 Instructions progress through distinct micro-states across multiple clock cycles:
    - FETCH (1 cycle): Memory read of instruction, IR write, PC <= PC + 1.
    - DECODE (1 cycle): Read register file operands A and B, precompute branch target.
    - EXECUTE / MEM_ADDR / BRANCH / JUMP (1 cycle): ALU operation or address calculation.
    - MEM_READ / MEM_WRITE (1 cycle for memory instructions).
    - MEM_WB / ALU_WB (1 cycle for register writeback).
 */
class FSMControlUnit {
public:
    enum class State : std::uint8_t {
        FETCH,
        DECODE,
        MEM_ADDR,
        MEM_READ,
        MEM_WB,
        MEM_WRITE,
        EXECUTE_R,
        ALU_WB,
        EXECUTE_I,
        BRANCH,
        JUMP,
        HALT
    };

    FSMControlUnit() noexcept = default;

    /*
     Reset the FSM back to the initial FETCH state.
     */
    void reset() noexcept {
        current_state_ = State::FETCH;
        next_state_ = State::FETCH;
    }

    [[nodiscard]]
    State current_state() const noexcept {
        return current_state_;
    }

    [[nodiscard]]
    State next_state() const noexcept {
        return next_state_;
    }

    [[nodiscard]]
    bool is_halted() const noexcept {
        return current_state_ == State::HALT;
    }

    [[nodiscard]]
    static constexpr std::string_view state_name(State s) noexcept {
        switch (s) {
            case State::FETCH:     return "FETCH";
            case State::DECODE:    return "DECODE";
            case State::MEM_ADDR:  return "MEM_ADDR";
            case State::MEM_READ:  return "MEM_READ";
            case State::MEM_WB:    return "MEM_WB";
            case State::MEM_WRITE: return "MEM_WRITE";
            case State::EXECUTE_R: return "EXECUTE_R";
            case State::ALU_WB:    return "ALU_WB";
            case State::EXECUTE_I: return "EXECUTE_I";
            case State::BRANCH:    return "BRANCH";
            case State::JUMP:      return "JUMP";
            case State::HALT:      return "HALT";
            default:               return "UNKNOWN";
        }
    }

    /*
     Generates Moore/Mealy control signals for the current state.
     */
    [[nodiscard]]
    MultiCycleControlSignals generate(const DecodedInstruction& instruction) const noexcept {
        MultiCycleControlSignals signals{};

        switch (current_state_) {
            case State::FETCH:
                // Reading instruction from memory, latch into IR, increment PC
                signals.memRead = true;
                signals.irWrite = true;
                signals.iorD = false;
                signals.aluSrcA = ALUSrcA::pc;
                signals.aluSrcB = ALUSrcB::constant; // PC + 1
                signals.aluOperation = ALUOperation::ADD;
                signals.pcWrite = true;
                signals.pcSource = PCSource::aluResult;
                break;

            case State::DECODE:
                // Reading register operands into A and B, precompute branch target (PC + imm) into ALU_out
                signals.aWrite = true;
                signals.bWrite = true;
                signals.aluOutWrite = true;
                signals.aluSrcA = ALUSrcA::pc;
                signals.aluSrcB = ALUSrcB::sign_extend;
                signals.aluOperation = ALUOperation::ADD;
                break;

            case State::MEM_ADDR:
                // Computing base + offset (A + immediate) and store into ALU_out
                signals.aluSrcA = ALUSrcA::RegA;
                signals.aluSrcB = ALUSrcB::sign_extend;
                signals.aluOperation = ALUOperation::ADD;
                signals.aluOutWrite = true;
                break;

            case State::MEM_READ:
                // Reading memory at address ALU_out into Memory Data Register (MDR)
                signals.memRead = true;
                signals.mdrWrite = true;
                signals.iorD = true;
                break;

            case State::MEM_WB:
                // Writing MDR content into destination register (rd)
                signals.regWrite = true;
                signals.regDst = true;
                signals.writebackSource = WriteBackSource::memoryData;
                break;

            case State::MEM_WRITE:
                // Writing register operand B into memory at address ALU_out
                signals.memWrite = true;
                signals.iorD = true;
                break;

            case State::EXECUTE_R:
                // ALU operation on RegA and RegB, save into ALU_out
                signals.aluSrcA = ALUSrcA::RegA;
                signals.aluSrcB = ALUSrcB::regB;
                signals.aluOperation = to_alu_operation(instruction.opcode);
                signals.aluOutWrite = true;
                break;

            case State::EXECUTE_I:
                // ALU operation on RegA and sign-extended immediate, save into ALU_out
                signals.aluSrcA = ALUSrcA::RegA;
                signals.aluSrcB = ALUSrcB::sign_extend;
                if (instruction.opcode == Opcode::LI) {
                    signals.aluOperation = ALUOperation::PASS_B;
                } else {
                    signals.aluOperation = ALUOperation::ADD;
                }
                signals.aluOutWrite = true;
                break;

            case State::ALU_WB:
                // Writing ALU_out result into destination register (rd)
                signals.regWrite = true;
                signals.regDst = true;
                signals.writebackSource = WriteBackSource::aluOut;
                break;

            case State::BRANCH:
                // Comparing operands A and B (SUB), conditionally updating PC if branch condition holds
                signals.aluSrcA = ALUSrcA::RegA;
                signals.aluSrcB = ALUSrcB::regB;
                signals.aluOperation = ALUOperation::SUB;
                signals.pcWriteCond = true;
                signals.bne = (instruction.opcode == Opcode::BNE);
                signals.pcSource = PCSource::aluOut; // target precomputed in DECODE
                break;

            case State::JUMP:
                // Unconditional jump to immediate address
                signals.pcWrite = true;
                signals.pcSource = PCSource::jumpTarget;
                break;

            case State::HALT:
                // Halt the processor
                signals.halt = true;
                break;
        }

        return signals;
    }

    /*
     Computes the next state transition based on current state and instruction.
     */
    void update_next_state(const DecodedInstruction& instruction) noexcept {
        switch (current_state_) {
            case State::FETCH:
                next_state_ = State::DECODE;
                break;

            case State::DECODE:
                switch (instruction.opcode) {
                    case Opcode::ADD:
                    case Opcode::SUB:
                    case Opcode::AND:
                    case Opcode::OR:
                    case Opcode::XOR:
                    case Opcode::NOT:
                        next_state_ = State::EXECUTE_R;
                        break;

                    case Opcode::LW:
                    case Opcode::SW:
                        next_state_ = State::MEM_ADDR;
                        break;

                    case Opcode::LI:
                    case Opcode::ADDI:
                        next_state_ = State::EXECUTE_I;
                        break;

                    case Opcode::BEQ:
                    case Opcode::BNE:
                        next_state_ = State::BRANCH;
                        break;

                    case Opcode::J:
                        next_state_ = State::JUMP;
                        break;

                    case Opcode::HALT:
                        next_state_ = State::HALT;
                        break;

                    case Opcode::NOP:
                    default:
                        next_state_ = State::FETCH;
                        break;
                }
                break;

            case State::MEM_ADDR:
                if (instruction.opcode == Opcode::LW) {
                    next_state_ = State::MEM_READ;
                } else {
                    next_state_ = State::MEM_WRITE;
                }
                break;

            case State::MEM_READ:
                next_state_ = State::MEM_WB;
                break;

            case State::MEM_WB:
            case State::MEM_WRITE:
            case State::ALU_WB:
            case State::BRANCH:
            case State::JUMP:
                next_state_ = State::FETCH;
                break;

            case State::EXECUTE_R:
            case State::EXECUTE_I:
                next_state_ = State::ALU_WB;
                break;

            case State::HALT:
                next_state_ = State::HALT;
                break;
        }
    }

    /*
      Clocks the FSM, advancing currentState to nextState.
     */
    void clock() noexcept {
        current_state_ = next_state_;
    }

    /*
    brief Computes next state logic and advances the clock in one step.
     */
    void step(const DecodedInstruction& instruction) noexcept {
        update_next_state(instruction);
        clock();
    }

private:
    [[nodiscard]]
    static ALUOperation to_alu_operation(Opcode opcode) noexcept {
        switch (opcode) {
            case Opcode::ADD: return ALUOperation::ADD;
            case Opcode::SUB: return ALUOperation::SUB;
            case Opcode::AND: return ALUOperation::AND;
            case Opcode::OR:  return ALUOperation::OR;
            case Opcode::XOR: return ALUOperation::XOR;
            case Opcode::NOT: return ALUOperation::NOT;
            default:          return ALUOperation::ADD;
        }
    }

    State current_state_{State::FETCH};
    State next_state_{State::FETCH};
};

} 

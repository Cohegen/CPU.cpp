#include "../../single_cycle_cpu/datapath/ControlUnit.hpp"
#include <cassert>
#include <iostream>

void test_control_unit()
{
    // Helper lambda to test expected control signals for an opcode
    auto check = [](cpu::Opcode op,
                    cpu::InstructionFormat fmt,
                    bool exp_reg_write,
                    bool exp_imm,
                    cpu::ALUOperation exp_alu_op,
                    bool exp_mem_read,
                    bool exp_mem_write,
                    bool exp_branch,
                    bool exp_jump,
                    bool exp_halt) {
        cpu::DecodedInstruction inst{
            op,
            fmt,
            cpu::Register::R1,
            cpu::Register::R2,
            cpu::Register::R3,
            100
        };

        auto signals = cpu::ControlUnit::generate(inst);

        assert(signals.register_write == exp_reg_write);
        assert(signals.alu_source_immediate == exp_imm);
        assert(signals.alu_operation == exp_alu_op);
        assert(signals.memory_read == exp_mem_read);
        assert(signals.memory_write == exp_mem_write);
        assert(signals.branch == exp_branch);
        assert(signals.jump == exp_jump);
        assert(signals.halt == exp_halt);
    };

    // 1. NOP
    check(cpu::Opcode::NOP,  cpu::InstructionFormat::I_TYPE, false, false, cpu::ALUOperation::ADD,    false, false, false, false, false);

    // 2. R-type arithmetic
    check(cpu::Opcode::ADD,  cpu::InstructionFormat::R_TYPE, true,  false, cpu::ALUOperation::ADD,    false, false, false, false, false);
    check(cpu::Opcode::SUB,  cpu::InstructionFormat::R_TYPE, true,  false, cpu::ALUOperation::SUB,    false, false, false, false, false);

    // 3. Logical operations
    check(cpu::Opcode::AND,  cpu::InstructionFormat::R_TYPE, true,  false, cpu::ALUOperation::AND,    false, false, false, false, false);
    check(cpu::Opcode::OR,   cpu::InstructionFormat::R_TYPE, true,  false, cpu::ALUOperation::OR,     false, false, false, false, false);
    check(cpu::Opcode::XOR,  cpu::InstructionFormat::R_TYPE, true,  false, cpu::ALUOperation::XOR,    false, false, false, false, false);
    check(cpu::Opcode::NOT,  cpu::InstructionFormat::R_TYPE, true,  false, cpu::ALUOperation::NOT,    false, false, false, false, false);

    // 4. LI
    check(cpu::Opcode::LI,   cpu::InstructionFormat::I_TYPE, true,  true,  cpu::ALUOperation::PASS_B, false, false, false, false, false);

    // 5. ADDI
    check(cpu::Opcode::ADDI, cpu::InstructionFormat::I_TYPE, true,  true,  cpu::ALUOperation::ADD,    false, false, false, false, false);

    // 6. LW
    check(cpu::Opcode::LW,   cpu::InstructionFormat::I_TYPE, true,  true,  cpu::ALUOperation::ADD,    true,  false, false, false, false);

    // 7. SW
    check(cpu::Opcode::SW,   cpu::InstructionFormat::S_TYPE, false, true,  cpu::ALUOperation::ADD,    false, true,  false, false, false);

    // 8. BEQ
    check(cpu::Opcode::BEQ,  cpu::InstructionFormat::B_TYPE, false, false, cpu::ALUOperation::SUB,    false, false, true,  false, false);

    // 9. BNE
    check(cpu::Opcode::BNE,  cpu::InstructionFormat::B_TYPE, false, false, cpu::ALUOperation::SUB,    false, false, true,  false, false);

    // 10. J
    check(cpu::Opcode::J,    cpu::InstructionFormat::J_TYPE, false, false, cpu::ALUOperation::ADD,    false, false, false, true,  false);

    // 11. HALT
    check(cpu::Opcode::HALT, cpu::InstructionFormat::I_TYPE, false, false, cpu::ALUOperation::ADD,    false, false, false, false, true);
}

int main()
{
    test_control_unit();
    std::cout << "ControlUnit tests passed!\n";
    return 0;
}

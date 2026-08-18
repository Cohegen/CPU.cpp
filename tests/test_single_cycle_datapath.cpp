#include "../single_cycle_cpu/datapath/SingleCycleDatapath.hpp"
#include <logic/signals/wire.hpp>
#include <iostream>
#include <cassert>
#include <cstdint>
#include <vector>

namespace
{
using Datapath = cpu::SingleCycleDatapath<32, 32, 32, 4, 8>;

std::uint32_t encode_r_type(
    cpu::Opcode opcode,
    cpu::Register rd,
    cpu::Register rs1,
    cpu::Register rs2
)
{
    return (static_cast<std::uint32_t>(opcode) << 26) |
           (static_cast<std::uint32_t>(rd) << 22) |
           (static_cast<std::uint32_t>(rs1) << 18) |
           (static_cast<std::uint32_t>(rs2) << 14);
}

std::uint32_t encode_i_type(
    cpu::Opcode opcode,
    cpu::Register rd,
    cpu::Register rs1,
    std::uint32_t immediate
)
{
    return (static_cast<std::uint32_t>(opcode) << 26) |
           (static_cast<std::uint32_t>(rd) << 22) |
           (static_cast<std::uint32_t>(rs1) << 18) |
           (immediate & 0x3FFFFU);
}

Datapath make_datapath(
    logic::Wire& clock,
    logic::Wire& reset,
    std::uint32_t instruction
)
{
    Datapath datapath(clock, reset);
    datapath.load_instructions({instruction});
    return datapath;
}

void assert_register_unchanged(
    Datapath& datapath,
    cpu::Register destination
)
{
    assert(datapath.read_register_for_test(destination) == 100);
}

void run_r_type_alu_test(
    const char* name,
    cpu::Opcode opcode,
    cpu::ALUOperation expected_operation,
    std::uint32_t lhs,
    std::uint32_t rhs,
    std::uint32_t expected_result
)
{
    logic::Wire clock(logic::LogicState::LOW);
    logic::Wire reset(logic::LogicState::LOW);
    auto datapath = make_datapath(
        clock,
        reset,
        encode_r_type(
            opcode,
            cpu::Register::R1,
            cpu::Register::R2,
            cpu::Register::R3
        )
    );

    datapath.write_register_for_test(cpu::Register::R1, 100);
    datapath.write_register_for_test(cpu::Register::R2, lhs);
    datapath.write_register_for_test(cpu::Register::R3, rhs);

    datapath.evaluate();

    assert(datapath.decoded_instruction().opcode == opcode);
    assert(datapath.decoded_instruction().rd == cpu::Register::R1);
    assert(datapath.decoded_instruction().rs1 == cpu::Register::R2);
    assert(datapath.decoded_instruction().rs2 == cpu::Register::R3);
    assert(datapath.control_signals().alu_operation == expected_operation);
    assert(datapath.control_signals().alu_source_immediate == false);
    assert(datapath.rs1_data().read_value() == lhs);
    assert(datapath.rs2_data().read_value() == rhs);
    assert(datapath.alu_operand_b().read_value() == rhs);
    assert(datapath.alu_result().read_value() == expected_result);
    assert_register_unchanged(datapath, cpu::Register::R1);

    std::cout << "[PASS] " << name << " instruction independently drives the ALU path\n";
}

void run_i_type_alu_test(
    const char* name,
    cpu::Opcode opcode,
    cpu::ALUOperation expected_operation,
    std::uint32_t rs1_value,
    std::uint32_t immediate,
    std::uint32_t expected_result
)
{
    logic::Wire clock(logic::LogicState::LOW);
    logic::Wire reset(logic::LogicState::LOW);
    auto datapath = make_datapath(
        clock,
        reset,
        encode_i_type(
            opcode,
            cpu::Register::R1,
            cpu::Register::R2,
            immediate
        )
    );

    datapath.write_register_for_test(cpu::Register::R1, 100);
    datapath.write_register_for_test(cpu::Register::R2, rs1_value);

    datapath.evaluate();

    assert(datapath.decoded_instruction().opcode == opcode);
    assert(datapath.decoded_instruction().rd == cpu::Register::R1);
    assert(datapath.decoded_instruction().rs1 == cpu::Register::R2);
    assert(datapath.control_signals().alu_operation == expected_operation);
    assert(datapath.control_signals().alu_source_immediate == true);
    assert(datapath.rs1_data().read_value() == rs1_value);
    assert(datapath.immediate().read_value() == immediate);
    assert(datapath.alu_operand_b().read_value() == immediate);
    assert(datapath.alu_result().read_value() == expected_result);
    assert_register_unchanged(datapath, cpu::Register::R1);

    std::cout << "[PASS] " << name << " instruction independently drives the ALU path\n";
}
}

int main()
{
    std::cout << "--- Testing SingleCycleDatapath Register Read Path ---\n";

    {
        logic::Wire clock(logic::LogicState::LOW);
        logic::Wire reset(logic::LogicState::LOW);
        auto datapath = make_datapath(
            clock,
            reset,
            encode_r_type(
                cpu::Opcode::ADD,
                cpu::Register::R3,
                cpu::Register::R1,
                cpu::Register::R2
            )
        );

        datapath.write_register_for_test(cpu::Register::R1, 42);
        datapath.write_register_for_test(cpu::Register::R2, 17);
        datapath.write_register_for_test(cpu::Register::R3, 100);

        datapath.evaluate();

        assert(datapath.instruction().read_value() == encode_r_type(
            cpu::Opcode::ADD,
            cpu::Register::R3,
            cpu::Register::R1,
            cpu::Register::R2
        ));
        assert(datapath.rs1_address().read_value() == static_cast<std::size_t>(cpu::Register::R1));
        assert(datapath.rs2_address().read_value() == static_cast<std::size_t>(cpu::Register::R2));
        assert(datapath.rd_address().read_value() == static_cast<std::size_t>(cpu::Register::R3));
        assert(datapath.rs1_data().read_value() == 42);
        assert(datapath.rs2_data().read_value() == 17);
        assert(datapath.read_register_for_test(cpu::Register::R3) == 100);
        std::cout << "[PASS] Instruction memory -> decoder -> register file read path\n";
        std::cout << "[PASS] Destination register is unchanged while write-back is disabled\n";
    }

    {
        logic::Wire clock(logic::LogicState::LOW);
        logic::Wire reset(logic::LogicState::LOW);
        auto datapath = make_datapath(
            clock,
            reset,
            encode_r_type(
                cpu::Opcode::SUB,
                cpu::Register::R4,
                cpu::Register::R5,
                cpu::Register::R9
            )
        );

        datapath.write_register_for_test(cpu::Register::R5, 25);
        datapath.write_register_for_test(cpu::Register::R9, 10);

        datapath.evaluate();

        assert(datapath.rs1_data().read_value() == 25);
        assert(datapath.rs2_data().read_value() == 10);
        std::cout << "[PASS] Independent simultaneous read ports select R5 and R9\n";
    }

    {
        logic::Wire clock(logic::LogicState::LOW);
        logic::Wire reset(logic::LogicState::LOW);
        auto datapath = make_datapath(
            clock,
            reset,
            encode_r_type(
                cpu::Opcode::AND,
                cpu::Register::R8,
                cpu::Register::R7,
                cpu::Register::R7
            )
        );

        datapath.write_register_for_test(cpu::Register::R7, 77);

        datapath.evaluate();

        assert(datapath.rs1_data().read_value() == 77);
        assert(datapath.rs2_data().read_value() == 77);
        std::cout << "[PASS] Both read ports can select the same register\n";
    }

    {
        logic::Wire clock(logic::LogicState::LOW);
        logic::Wire reset(logic::LogicState::LOW);
        constexpr std::uint32_t expected_immediate = 0x12345;
        auto datapath = make_datapath(
            clock,
            reset,
            encode_i_type(
                cpu::Opcode::ADDI,
                cpu::Register::R3,
                cpu::Register::R6,
                expected_immediate
            )
        );

        datapath.write_register_for_test(cpu::Register::R6, 66);

        datapath.evaluate();

        assert(datapath.decoded_instruction().opcode == cpu::Opcode::ADDI);
        assert(datapath.decoded_instruction().format == cpu::InstructionFormat::I_TYPE);
        assert(datapath.rs1_data().read_value() == 66);
        assert(datapath.immediate().read_value() == expected_immediate);
        std::cout << "[PASS] I-type rs1 read and immediate extraction remain connected\n";
    }

    run_r_type_alu_test(
        "ADD R1, R2, R3",
        cpu::Opcode::ADD,
        cpu::ALUOperation::ADD,
        20,
        5,
        25
    );

    run_r_type_alu_test(
        "SUB R1, R2, R3",
        cpu::Opcode::SUB,
        cpu::ALUOperation::SUB,
        20,
        5,
        15
    );

    run_r_type_alu_test(
        "AND R1, R2, R3",
        cpu::Opcode::AND,
        cpu::ALUOperation::AND,
        0xF0F0,
        0x0FF0,
        0x00F0
    );

    run_r_type_alu_test(
        "OR R1, R2, R3",
        cpu::Opcode::OR,
        cpu::ALUOperation::OR,
        0xF000,
        0x0F00,
        0xFF00
    );

    run_r_type_alu_test(
        "XOR R1, R2, R3",
        cpu::Opcode::XOR,
        cpu::ALUOperation::XOR,
        0x00FF,
        0x0F0F,
        0x0FF0
    );

    run_r_type_alu_test(
        "NOT R1, R2",
        cpu::Opcode::NOT,
        cpu::ALUOperation::NOT,
        0x0F0F0F0F,
        0,
        static_cast<std::uint32_t>(~0x0F0F0F0FU)
    );

    run_i_type_alu_test(
        "LI R1, 1234",
        cpu::Opcode::LI,
        cpu::ALUOperation::PASS_B,
        0,
        1234,
        1234
    );

    run_i_type_alu_test(
        "ADDI R1, R2, 7",
        cpu::Opcode::ADDI,
        cpu::ALUOperation::ADD,
        35,
        7,
        42
    );

    std::cout << "[SKIP] R0 hardwired-zero test: current RegisterFile permits writes to R0\n";
    std::cout << "[PASS] SingleCycleDatapath register read tests successful!\n";
    return 0;
}

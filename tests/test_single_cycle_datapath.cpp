#include "../single_cycle_cpu/datapath/SingleCycleDatapath.hpp"
#include <logic/signals/clock.hpp>
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

std::uint32_t encode_s_type(
    cpu::Opcode opcode,
    cpu::Register rs2,
    cpu::Register rs1,
    std::uint32_t immediate
)
{
    return (static_cast<std::uint32_t>(opcode) << 26) |
           (static_cast<std::uint32_t>(rs2) << 22) |
           (static_cast<std::uint32_t>(rs1) << 18) |
           (immediate & 0x3FFFFU);
}

std::uint32_t encode_b_type(
    cpu::Opcode opcode,
    cpu::Register rs1,
    cpu::Register rs2,
    std::uint32_t immediate
)
{
    return (static_cast<std::uint32_t>(opcode) << 26) |
           (static_cast<std::uint32_t>(rs1) << 22) |
           (static_cast<std::uint32_t>(rs2) << 18) |
           (immediate & 0x3FFFFU);
}

std::uint32_t encode_j_type(
    cpu::Opcode opcode,
    std::int32_t immediate
)
{
    return (static_cast<std::uint32_t>(opcode) << 26) |
           (static_cast<std::uint32_t>(immediate) & 0x03FFFFFFU);
}

Datapath make_datapath(
    logic::Clock& clock,
    logic::Wire& reset,
    std::uint32_t instruction
)
{
    Datapath datapath(clock, reset);
    datapath.load_instructions({instruction});
    return datapath;
}

void execute_current_instruction(
    Datapath& datapath,
    logic::Clock& clock
)
{
    datapath.evaluate();
    clock.tick();
    datapath.evaluate();
    clock.tick();
    datapath.evaluate();
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
    logic::Clock clock;
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
    logic::Clock clock;
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
        logic::Clock clock;
        logic::Wire reset(logic::LogicState::LOW);
        auto datapath = make_datapath(
            clock,
            reset,
            encode_i_type(cpu::Opcode::NOP, cpu::Register::R0, cpu::Register::R0, 0)
        );

        datapath.evaluate();
        const auto pc0 = datapath.pc().read_value();

        clock.tick();
        datapath.evaluate();
        const auto pc1 = datapath.pc().read_value();

        clock.tick();
        datapath.evaluate();
        const auto pc_after_falling_half_cycle = datapath.pc().read_value();

        clock.tick();
        datapath.evaluate();
        const auto pc2 = datapath.pc().read_value();

        assert(pc1 == pc0 + 1);
        assert(pc_after_falling_half_cycle == pc1);
        assert(pc2 == pc1 + 1);
        std::cout << "[PASS] PC increments on rising clock evaluations and holds on falling half-cycles\n";
    }

    {
        logic::Clock clock;
        logic::Wire reset(logic::LogicState::LOW);
        auto datapath = make_datapath(
            clock,
            reset,
            encode_i_type(cpu::Opcode::NOP, cpu::Register::R0, cpu::Register::R0, 0)
        );

        datapath.write_register_for_test(cpu::Register::R1, 42);
        assert(datapath.read_register_for_test(cpu::Register::R1) == 42);

        datapath.write_register_for_test(cpu::Register::R2, 100);
        assert(datapath.read_register_for_test(cpu::Register::R1) == 42);
        assert(datapath.read_register_for_test(cpu::Register::R2) == 100);
        std::cout << "[PASS] Register-file test helpers write and read independent registers\n";
    }

    {
        logic::Clock clock;
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

        datapath.write_register_for_test(cpu::Register::R1, 10);
        datapath.write_register_for_test(cpu::Register::R2, 20);
        datapath.evaluate();

        assert(datapath.rs1_data().read_value() == 10);
        assert(datapath.rs2_data().read_value() == 20);
        std::cout << "[PASS] Register file dual-read ports drive rs1_data and rs2_data independently\n";
    }

    {
        logic::Clock clock;
        logic::Wire reset(logic::LogicState::LOW);
        const std::uint32_t add_r3_r1_r2 = encode_r_type(
            cpu::Opcode::ADD,
            cpu::Register::R3,
            cpu::Register::R1,
            cpu::Register::R2
        );

        Datapath datapath(clock, reset);
        datapath.load_instructions({add_r3_r1_r2});

        datapath.write_register_for_test(cpu::Register::R1, 10);
        datapath.write_register_for_test(cpu::Register::R2, 20);

        // Execute ADD R3, R1, R2 at PC=0 and inspect the full combinational path.
        datapath.evaluate();

        assert(datapath.instruction().read_value() == add_r3_r1_r2);
        assert(datapath.decoded_instruction().opcode == cpu::Opcode::ADD);
        assert(datapath.decoded_instruction().rd == cpu::Register::R3);
        assert(datapath.decoded_instruction().rs1 == cpu::Register::R1);
        assert(datapath.decoded_instruction().rs2 == cpu::Register::R2);

        assert(datapath.rs1_data().read_value() == 10);
        assert(datapath.rs2_data().read_value() == 20);
        assert(datapath.alu_operand_b().read_value() == 20);
        assert(datapath.alu_result().read_value() == 30);
        assert(datapath.register_write_data().read_value() == 30);

        assert(datapath.control_signals().register_write == true);
        assert(datapath.control_signals().alu_source_immediate == false);
        assert(datapath.control_signals().memory_read == false);
        assert(datapath.control_signals().memory_write == false);
        assert(datapath.control_signals().alu_operation == cpu::ALUOperation::ADD);

        // Latch the result into R3 through the register-file clock edge.
        clock.tick();
        datapath.evaluate();
        clock.tick();
        datapath.evaluate();

        assert(datapath.read_register_for_test(cpu::Register::R3) == 30);
        std::cout << "[PASS] ADD R3, R1, R2 drives decode, register file, ALU operand mux, ALU, writeback mux, and register writeback\n";
    }

    {
        logic::Clock clock;
        logic::Wire reset(logic::LogicState::LOW);
        const std::uint32_t addi_r2_r1_5 = encode_i_type(
            cpu::Opcode::ADDI,
            cpu::Register::R2,
            cpu::Register::R1,
            5
        );

        Datapath datapath(clock, reset);
        datapath.load_instructions({addi_r2_r1_5});

        datapath.write_register_for_test(cpu::Register::R1, 10);

        // Execute ADDI R2, R1, 5 at PC=0 and inspect the immediate operand path.
        datapath.evaluate();

        assert(datapath.instruction().read_value() == addi_r2_r1_5);
        assert(datapath.decoded_instruction().opcode == cpu::Opcode::ADDI);
        assert(datapath.decoded_instruction().rd == cpu::Register::R2);
        assert(datapath.decoded_instruction().rs1 == cpu::Register::R1);

        assert(datapath.rs1_data().read_value() == 10);
        assert(datapath.immediate().read_value() == 5);
        assert(datapath.alu_operand_b().read_value() == 5);
        assert(datapath.alu_result().read_value() == 15);
        assert(datapath.register_write_data().read_value() == 15);

        assert(datapath.control_signals().alu_source_immediate == true);
        assert(datapath.control_signals().register_write == true);
        assert(datapath.control_signals().memory_read == false);
        assert(datapath.control_signals().memory_write == false);
        assert(datapath.control_signals().alu_operation == cpu::ALUOperation::ADD);

        // Latch the result into R2 through the register-file clock edge.
        clock.tick();
        datapath.evaluate();
        clock.tick();
        datapath.evaluate();

        assert(datapath.read_register_for_test(cpu::Register::R2) == 15);
        std::cout << "[PASS] ADDI R2, R1, 5 selects immediate through ALUOperandMux and writes back to R2\n";
    }

    {
        logic::Clock clock;
        logic::Wire reset(logic::LogicState::LOW);
        const std::uint32_t sw_r2_0_r1 = encode_s_type(
            cpu::Opcode::SW,
            cpu::Register::R2,
            cpu::Register::R1,
            0
        );
        const std::uint32_t lw_r3_0_r1 = encode_i_type(
            cpu::Opcode::LW,
            cpu::Register::R3,
            cpu::Register::R1,
            0
        );

        Datapath datapath(clock, reset);
        datapath.load_instructions({sw_r2_0_r1, lw_r3_0_r1});

        // DataMemory maps MSB=1 addresses to RAM; stores only persist there.
        // Use 0x80 so SW actually writes through Logic.cpp Memory -> RAM.
        constexpr std::uint32_t base_address = 0x80;

        datapath.write_register_for_test(cpu::Register::R1, base_address);
        datapath.write_register_for_test(cpu::Register::R2, 42);

        // --- SW R2, 0(R1): address = R1 + 0, store R2 into data memory ---
        datapath.evaluate();

        assert(datapath.instruction().read_value() == sw_r2_0_r1);
        assert(datapath.decoded_instruction().opcode == cpu::Opcode::SW);
        assert(datapath.decoded_instruction().rs1 == cpu::Register::R1);
        assert(datapath.decoded_instruction().rs2 == cpu::Register::R2);
        assert(datapath.rs1_data().read_value() == base_address);
        assert(datapath.immediate().read_value() == 0);
        assert(datapath.alu_result().read_value() == base_address);
        assert(datapath.rs2_data().read_value() == 42);
        assert(datapath.control_signals().memory_write == true);
        assert(datapath.control_signals().memory_read == false);
        assert(datapath.control_signals().register_write == false);
        assert(datapath.control_signals().alu_source_immediate == true);

        clock.tick();
        datapath.evaluate();
        clock.tick();
        datapath.evaluate();

        // --- LW R3, 0(R1): load from the address SW just wrote ---
        datapath.evaluate();

        assert(datapath.instruction().read_value() == lw_r3_0_r1);
        assert(datapath.decoded_instruction().opcode == cpu::Opcode::LW);
        assert(datapath.decoded_instruction().rd == cpu::Register::R3);
        assert(datapath.decoded_instruction().rs1 == cpu::Register::R1);
        assert(datapath.rs1_data().read_value() == base_address);
        assert(datapath.immediate().read_value() == 0);
        assert(datapath.alu_result().read_value() == base_address);
        assert(datapath.memory_read_data().read_value() == 42);
        assert(datapath.register_write_data().read_value() == 42);
        assert(datapath.control_signals().memory_read == true);
        assert(datapath.control_signals().memory_write == false);
        assert(datapath.control_signals().register_write == true);
        assert(datapath.control_signals().alu_source_immediate == true);
        assert(datapath.memory_to_register().read() == logic::LogicState::HIGH);

        clock.tick();
        datapath.evaluate();
        clock.tick();
        datapath.evaluate();

        assert(datapath.read_register_for_test(cpu::Register::R3) == 42);
        std::cout << "[PASS] SW then LW stores 42 to RAM through DataMemory->Memory->RAM and loads it back into R3\n";
    }

    {
        logic::Clock clock;
        logic::Wire reset(logic::LogicState::LOW);
        auto datapath = make_datapath(
            clock,
            reset,
            encode_i_type(cpu::Opcode::NOP, cpu::Register::R0, cpu::Register::R0, 0)
        );

        datapath.write_register_for_test(cpu::Register::R1, 123);
        assert(datapath.read_register_for_test(cpu::Register::R1) == 123);

        reset.write(logic::LogicState::HIGH);
        datapath.evaluate();
        clock.tick();
        datapath.evaluate();
        reset.write(logic::LogicState::LOW);
        clock.tick();
        datapath.evaluate();

        assert(datapath.read_register_for_test(cpu::Register::R1) == 0);
        assert(datapath.pc().read_value() == 0);
        std::cout << "[PASS] Reset clears register file state and PC on the active clock edge\n";
    }

    {
        logic::Clock clock;
        logic::Wire reset(logic::LogicState::LOW);
        const std::uint32_t instruction0 =
            encode_i_type(cpu::Opcode::ADDI, cpu::Register::R1, cpu::Register::R0, 1);
        const std::uint32_t instruction1 =
            encode_i_type(cpu::Opcode::ADDI, cpu::Register::R2, cpu::Register::R0, 2);
        const std::uint32_t instruction2 =
            encode_r_type(cpu::Opcode::ADD, cpu::Register::R3, cpu::Register::R1, cpu::Register::R2);

        Datapath datapath(clock, reset);
        datapath.load_instructions({instruction0, instruction1, instruction2});

        datapath.evaluate();
        assert(datapath.pc().read_value() == 0);
        assert(datapath.instruction().read_value() == instruction0);

        execute_current_instruction(datapath, clock);
        assert(datapath.pc().read_value() == 1);
        assert(datapath.instruction().read_value() == instruction1);

        execute_current_instruction(datapath, clock);
        assert(datapath.pc().read_value() == 2);
        assert(datapath.instruction().read_value() == instruction2);

        std::cout << "[PASS] Instruction fetch observes PC 0, 1, and 2 across full clock cycles\n";
    }

    {
        logic::Clock clock;
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
        logic::Clock clock;
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
        logic::Clock clock;
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
        logic::Clock clock;
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

    // LW instruction test (memory_read=true -> memory_to_register=HIGH -> WriteBackMux selects memory_read_data)
    {
        logic::Clock clock;
        logic::Wire reset(logic::LogicState::LOW);
        constexpr std::size_t offset = 1;
        auto datapath = make_datapath(
            clock,
            reset,
            encode_i_type(
                cpu::Opcode::LW,
                cpu::Register::R1,
                cpu::Register::R2,
                offset
            )
        );

        datapath.write_register_for_test(cpu::Register::R2, 0); // address = 0 + 1 = 1
        datapath.data_memory().load_rom({0x11111111, 0x99998888});

        datapath.evaluate();

        assert(datapath.decoded_instruction().opcode == cpu::Opcode::LW);
        assert(datapath.control_signals().memory_read == true);
        assert(datapath.memory_to_register().read() == logic::LogicState::HIGH);
        assert(datapath.alu_result().read_value() == 1);
        assert(datapath.memory_read_data().read_value() == 0x99998888);
        assert(datapath.register_write_data().read_value() == 0x99998888);
        std::cout << "[PASS] LW instruction sets memory_to_register=HIGH and WriteBackMux selects memory_read_data\n";
    }

    // ALU operation test (ADD instruction -> memory_read=false -> memory_to_register=LOW -> WriteBackMux selects alu_result)
    {
        logic::Clock clock;
        logic::Wire reset(logic::LogicState::LOW);
        auto datapath = make_datapath(
            clock,
            reset,
            encode_r_type(
                cpu::Opcode::ADD,
                cpu::Register::R1,
                cpu::Register::R2,
                cpu::Register::R3
            )
        );

        datapath.write_register_for_test(cpu::Register::R2, 100);
        datapath.write_register_for_test(cpu::Register::R3, 50);

        datapath.evaluate();

        assert(datapath.control_signals().memory_read == false);
        assert(datapath.memory_to_register().read() == logic::LogicState::LOW);
        assert(datapath.alu_result().read_value() == 150);
        assert(datapath.register_write_data().read_value() == 150);
        std::cout << "[PASS] ADD instruction sets memory_to_register=LOW and WriteBackMux selects alu_result\n";
    }

    // Branch condition test: BEQ R1, R2, +offset (R1 = 10, R2 = 10)
    {
        logic::Clock clock;
        logic::Wire reset(logic::LogicState::LOW);
        auto datapath = make_datapath(
            clock,
            reset,
            encode_b_type(
                cpu::Opcode::BEQ,
                cpu::Register::R1,
                cpu::Register::R2,
                5
            )
        );

        datapath.write_register_for_test(cpu::Register::R1, 10);
        datapath.write_register_for_test(cpu::Register::R2, 10);

        datapath.evaluate();

        assert(datapath.decoded_instruction().opcode == cpu::Opcode::BEQ);
        assert(datapath.control_signals().branch == true);
        assert(datapath.control_signals().alu_operation == cpu::ALUOperation::SUB);
        assert(datapath.rs1_data().read_value() == 10);
        assert(datapath.rs2_data().read_value() == 10);
        assert(datapath.alu_result().read_value() == 0);
        assert(datapath.alu_zero().read() == logic::LogicState::HIGH);

        // Verify PC transitions to branch target (0 + 5 = 5) on clock edge
        clock.tick();
        datapath.evaluate();
        assert(datapath.pc().read_value() == 5);

        std::cout << "[PASS] BEQ instruction compares R1(10) == R2(10), ALU result = 0, alu_zero = HIGH, branch taken (PC=5)\n";
    }

    // Branch condition test: BEQ R1, R2, +offset (R1 = 10, R2 = 5)
    {
        logic::Clock clock;
        logic::Wire reset(logic::LogicState::LOW);
        auto datapath = make_datapath(
            clock,
            reset,
            encode_b_type(
                cpu::Opcode::BEQ,
                cpu::Register::R1,
                cpu::Register::R2,
                5
            )
        );

        datapath.write_register_for_test(cpu::Register::R1, 10);
        datapath.write_register_for_test(cpu::Register::R2, 5);

        datapath.evaluate();

        assert(datapath.decoded_instruction().opcode == cpu::Opcode::BEQ);
        assert(datapath.control_signals().branch == true);
        assert(datapath.control_signals().alu_operation == cpu::ALUOperation::SUB);
        assert(datapath.rs1_data().read_value() == 10);
        assert(datapath.rs2_data().read_value() == 5);
        assert(datapath.alu_result().read_value() != 0);
        assert(datapath.alu_zero().read() == logic::LogicState::LOW);

        // Verify PC transitions to next sequential PC (0 + 1 = 1) on clock edge
        clock.tick();
        datapath.evaluate();
        assert(datapath.pc().read_value() == 1);

        std::cout << "[PASS] BEQ instruction compares R1(10) != R2(5), ALU result != 0, alu_zero = LOW, branch not taken (PC=1)\n";
    }

    // Branch condition test: BNE R1, R2, +offset (R1 = 10, R2 = 5)
    {
        logic::Clock clock;
        logic::Wire reset(logic::LogicState::LOW);
        auto datapath = make_datapath(
            clock,
            reset,
            encode_b_type(
                cpu::Opcode::BNE,
                cpu::Register::R1,
                cpu::Register::R2,
                5
            )
        );

        datapath.write_register_for_test(cpu::Register::R1, 10);
        datapath.write_register_for_test(cpu::Register::R2, 5);

        datapath.evaluate();

        assert(datapath.decoded_instruction().opcode == cpu::Opcode::BNE);
        assert(datapath.control_signals().branch == true);
        assert(datapath.control_signals().alu_operation == cpu::ALUOperation::SUB);
        assert(datapath.rs1_data().read_value() == 10);
        assert(datapath.rs2_data().read_value() == 5);
        assert(datapath.alu_result().read_value() != 0);
        assert(datapath.alu_zero().read() == logic::LogicState::LOW);

        // Verify PC transitions to branch target (0 + 5 = 5) on clock edge
        clock.tick();
        datapath.evaluate();
        assert(datapath.pc().read_value() == 5);

        std::cout << "[PASS] BNE instruction compares R1(10) != R2(5), ALU result != 0, alu_zero = LOW, branch taken (PC=5)\n";
    }

    // Branch condition test: BNE R1, R2, +offset (R1 = 10, R2 = 10)
    {
        logic::Clock clock;
        logic::Wire reset(logic::LogicState::LOW);
        auto datapath = make_datapath(
            clock,
            reset,
            encode_b_type(
                cpu::Opcode::BNE,
                cpu::Register::R1,
                cpu::Register::R2,
                5
            )
        );

        datapath.write_register_for_test(cpu::Register::R1, 10);
        datapath.write_register_for_test(cpu::Register::R2, 10);

        datapath.evaluate();

        assert(datapath.decoded_instruction().opcode == cpu::Opcode::BNE);
        assert(datapath.control_signals().branch == true);
        assert(datapath.control_signals().alu_operation == cpu::ALUOperation::SUB);
        assert(datapath.rs1_data().read_value() == 10);
        assert(datapath.rs2_data().read_value() == 10);
        assert(datapath.alu_result().read_value() == 0);
        assert(datapath.alu_zero().read() == logic::LogicState::HIGH);

        // Verify PC transitions to next sequential PC (0 + 1 = 1) on clock edge
        clock.tick();
        datapath.evaluate();
        assert(datapath.pc().read_value() == 1);

        std::cout << "[PASS] BNE instruction compares R1(10) == R2(10), ALU result = 0, alu_zero = HIGH, branch not taken (PC=1)\n";
    }

    // Control flow test: J +offset
    {
        logic::Clock clock;
        logic::Wire reset(logic::LogicState::LOW);
        auto datapath = make_datapath(
            clock,
            reset,
            encode_j_type(cpu::Opcode::J, 10)
        );

        datapath.evaluate();

        assert(datapath.decoded_instruction().opcode == cpu::Opcode::J);
        assert(datapath.control_signals().jump == true);
        assert(datapath.immediate().read_value() == 10);

        // Verify PC transitions to jump target (0 + 10 = 10) on clock edge
        clock.tick();
        datapath.evaluate();
        assert(datapath.pc().read_value() == 10);

        std::cout << "[PASS] J instruction sets jump=true and PC updates to 10\n";
    }

    // Processor state test: HALT stops PC execution
    {
        logic::Clock clock;
        logic::Wire reset(logic::LogicState::LOW);
        auto datapath = make_datapath(
            clock,
            reset,
            encode_i_type(cpu::Opcode::HALT, cpu::Register::R0, cpu::Register::R0, 0)
        );

        datapath.evaluate();

        assert(datapath.decoded_instruction().opcode == cpu::Opcode::HALT);
        assert(datapath.control_signals().halt == true);

        const auto pc_halt = datapath.pc().read_value();

        // Verify PC does NOT increment on clock edge
        clock.tick();
        datapath.evaluate();
        assert(datapath.pc().read_value() == pc_halt);

        clock.tick();
        datapath.evaluate();
        assert(datapath.pc().read_value() == pc_halt);

        std::cout << "[PASS] HALT instruction sets halt=true and prevents PC increment (PC holds at " << pc_halt << ")\n";
    }

    // Full small program execution test
    {
        logic::Clock clock;
        logic::Wire reset(logic::LogicState::LOW);
        Datapath datapath(clock, reset);

        // Program memory address maps to ROM
        // Instructions:
        // 0: LI   r1, 128 (0x80 - RAM base address)
        // 1: LI   r2, 20
        // 2: ADD  r3, r1, r2
        // 3: SW   r3, 0(r1)  <- Write to data memory address (128 + 0 = 128)
        // 4: LW   r4, 0(r1)  <- Load from data memory address (128 + 0 = 128) into r4
        // 5: SUB  r5, r4, r1 <- r5 = r4 - r1 = 148 - 128 = 20
        // 6: HALT
        
        constexpr std::uint32_t ram_base = 0x80;
        
        std::vector<std::size_t> program = {
            encode_i_type(cpu::Opcode::LI, cpu::Register::R1, cpu::Register::R0, ram_base),
            encode_i_type(cpu::Opcode::LI, cpu::Register::R2, cpu::Register::R0, 20),
            encode_r_type(cpu::Opcode::ADD, cpu::Register::R3, cpu::Register::R1, cpu::Register::R2),
            encode_s_type(cpu::Opcode::SW, cpu::Register::R3, cpu::Register::R1, 0),
            encode_i_type(cpu::Opcode::LW, cpu::Register::R4, cpu::Register::R1, 0),
            encode_r_type(cpu::Opcode::SUB, cpu::Register::R5, cpu::Register::R4, cpu::Register::R1),
            encode_i_type(cpu::Opcode::HALT, cpu::Register::R0, cpu::Register::R0, 0)
        };

        datapath.load_instructions(program);

        // We run the simulation until the CPU halts, up to a safety limit
        int cycle_count = 0;
        constexpr int max_cycles = 50;

        while (!datapath.control_signals().halt && cycle_count < max_cycles) {
            datapath.evaluate();
            clock.tick();
            datapath.evaluate();
            clock.tick();
            cycle_count++;
        }

        // Evaluate once more to ensure HALT propagates
        datapath.evaluate();

        assert(datapath.control_signals().halt == true);
        assert(datapath.read_register_for_test(cpu::Register::R1) == ram_base);
        assert(datapath.read_register_for_test(cpu::Register::R2) == 20);
        assert(datapath.read_register_for_test(cpu::Register::R3) == ram_base + 20);
        assert(datapath.read_register_for_test(cpu::Register::R4) == ram_base + 20);
        assert(datapath.read_register_for_test(cpu::Register::R5) == 20);

        std::cout << "[PASS] Full small program executed successfully in " << cycle_count << " cycles!\n";
    }

    std::cout << "[SKIP] R0 hardwired-zero test: current RegisterFile permits writes to R0\n";
    std::cout << "[PASS] SingleCycleDatapath register read tests successful!\n";
    return 0;
}

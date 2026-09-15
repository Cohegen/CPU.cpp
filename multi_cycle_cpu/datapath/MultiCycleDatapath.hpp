#pragma once

#include "../../components/ProgramCounter.hpp"
#include "../../components/ALUInterface.hpp"
#include "../../components/WriteBackMux.hpp"
#include "../../components/DataMemory.hpp"
#include "../core/registers/InstructionRegister.hpp"
#include "../core/registers/MemoryDataRegister.hpp"
#include "../core/registers/OperandA_reg.hpp"
#include "../core/registers/OperandB_reg.hpp"
#include "../core/registers/ALU_out_reg.hpp"
#include "../core/ControlSignals.hpp"
#include "../core/FSMControlUnit.hpp"
#include "../core/muxes/ALUSrcA_mux.hpp"
#include "../core/muxes/ALUSrcB_mux.hpp"
#include "../core/muxes/OutMux.hpp"
#include "../core/muxes/PCSource_mux.hpp"
#include "../core/muxes/RegDestMux.hpp"
#include "../../include/isa/Instruction.hpp"
#include "../../include/isa/InstructionDecoder.hpp"

#include <logic/signals/wire.hpp>
#include <logic/signals/bus.hpp>
#include <logic/signals/clock.hpp>
#include <logic/signals/logicState.hpp>
#include <logic/sequential/memory/RegisterFile.hpp>
#include <logic/combinational/shifters/LogicalLeftShifter.hpp>
#include <logic/simulator/Component.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace cpu {

template <
    std::size_t AddressWidth = 32,
    std::size_t DataWidth = 32,
    std::size_t InstructionWidth = 32,
    std::size_t RegisterAddressWidth = 4,
    std::size_t MemoryAddressWidth = 8
>
class MultiCycleDatapath : public logic::Component {
    static_assert(AddressWidth == DataWidth, "AddressWidth must equal DataWidth");
    static_assert(InstructionWidth == DataWidth, "InstructionWidth must equal DataWidth");
    static_assert(MemoryAddressWidth <= AddressWidth, "MemoryAddressWidth cannot exceed AddressWidth");

public:
    MultiCycleDatapath(
        logic::Clock& clock,
        logic::Wire& reset
    )
        : clock_(clock),
          reset_(reset),
          program_counter_(clock_signal_, reset_, pc_enable_, pc_next_, pc_),
          instruction_register_(memory_read_data_, clock_signal_, ir_write_, reset_),
          memory_data_register_(memory_read_data_, clock_signal_, mdr_write_, reset_),
          operand_a_(rs1_data_, clock_signal_, reset_, a_write_),
          operand_b_(rs2_data_, clock_signal_, b_write_, reset_),
          alu_out_(alu_result_, clock_signal_, alu_out_write_, reset_),
          register_file_(
              clock_,
              reset_,
              register_write_enable_,
              rs1_address_,
              rs2_address_,
              rd_address_,
              register_write_data_,
              rs1_data_,
              rs2_data_
          ),
          memory_(
              clock_,
              reset_,
              memory_read_enable_,
              memory_write_enable_,
              memory_address_,
              memory_write_data_,
              memory_read_data_
          ),
          alu_src_a_mux_(pc_, operand_a_.output(), alu_src_a_select_, src_a_),
          alu_src_b_mux_(
              operand_b_.output(),
              constant_one_,
              immediate_,
              shifted_immediate_,
              alu_src_b_select0_,
              alu_src_b_select1_,
              src_b_
          ),
          ior_d_mux_(pc_, alu_out_.output(), ior_d_, memory_address_wide_),
          pc_source_mux_(
              alu_result_,
              alu_out_.output(),
              jump_target_,
              pc_source_select0_,
              pc_source_select1_,
              pc_next_
          ),
          reg_dest_mux_(rt_address_, decoded_rd_address_, reg_dst_, rd_address_),
          writeback_mux_(
              alu_out_.output(),
              memory_data_register_.output(),
              memory_to_register_,
              register_write_data_
          ),
          alu_(
              src_a_,
              src_b_,
              ALUOperation::ADD,
              alu_result_,
              alu_zero_,
              alu_carry_
          ),
          immediate_shifter_(immediate_, shift_amount_, shifted_immediate_)
    {
        constant_one_.write(logic::LogicState::LOW);
        constant_one_[0].write(logic::LogicState::HIGH);
        shift_amount_.write(logic::LogicState::LOW);
    }

    void evaluate() noexcept override
    {
        evaluate_datapath();
    }

    void evaluate(const MultiCycleControlSignals& control) noexcept
    {
        control_ = control;
        evaluate_datapath();
    }

    void clock()
    {
        clock_.tick();
        evaluate_datapath();
    }

    void step() noexcept
    {
        evaluate_datapath();
        clock_.tick();
        evaluate_datapath();
        clock_.tick();
        evaluate_datapath();
    }

    void step(const MultiCycleControlSignals& control) noexcept
    {
        control_ = control;
        step();
    }

    /**
     Resets the CPU and FSM to initial state.
     */
    void reset() noexcept
    {
        fsm_.reset();
        reset_.write(logic::LogicState::HIGH);
        evaluate_datapath();
        clock_.tick();
        evaluate_datapath();
        clock_.tick();
        reset_.write(logic::LogicState::LOW);
        evaluate_datapath();
    }

    /*
     Executes a single clock cycle using the autonomous FSM control unit
     */
    void step_cycle() noexcept
    {
        control_ = fsm_.generate(decoded_instruction_);
        step();
        fsm_.step(decoded_instruction_);
    }

    /*
    Executes cycles autonomously until an instruction completes (returns to FETCH) or CPU halts
     */
    void step_instruction() noexcept
    {
        if (fsm_.is_halted()) {
            return;
        }
        do {
            step_cycle();
        } while (fsm_.current_state() != FSMControlUnit::State::FETCH && !fsm_.is_halted());
    }

    /*
     Runs instructions autonomously until the CPU halts or max_cycles is reached
     */
    void run(std::size_t max_cycles = 100000) noexcept
    {
        std::size_t cycles = 0;
        while (!fsm_.is_halted() && cycles < max_cycles) {
            step_cycle();
            ++cycles;
        }
    }

    [[nodiscard]]
    FSMControlUnit& fsm() noexcept
    {
        return fsm_;
    }

    [[nodiscard]]
    const FSMControlUnit& fsm() const noexcept
    {
        return fsm_;
    }

    void load_instructions(const std::vector<std::size_t>& instructions) noexcept
    {
        memory_.load_rom(instructions);
    }

    [[nodiscard]]
    logic::Bus<AddressWidth>& pc() noexcept
    {
        return pc_;
    }

    [[nodiscard]]
    const logic::Bus<AddressWidth>& pc() const noexcept
    {
        return pc_;
    }

    [[nodiscard]]
    const logic::Bus<AddressWidth>& pc_next() const noexcept
    {
        return pc_next_;
    }

    [[nodiscard]]
    const logic::Bus<InstructionWidth>& instruction() const noexcept
    {
        return instruction_register_.output();
    }

    [[nodiscard]]
    const DecodedInstruction& decoded_instruction() const noexcept
    {
        return decoded_instruction_;
    }

    [[nodiscard]]
    const MultiCycleControlSignals& control_signals() const noexcept
    {
        return control_;
    }

    [[nodiscard]]
    const logic::Bus<DataWidth>& alu_result() const noexcept
    {
        return alu_result_;
    }

    [[nodiscard]]
    const logic::Bus<DataWidth>& alu_out() const noexcept
    {
        return alu_out_.output();
    }

    [[nodiscard]]
    const logic::Wire& alu_zero() const noexcept
    {
        return alu_zero_;
    }

    [[nodiscard]]
    const logic::Bus<DataWidth>& src_a() const noexcept
    {
        return src_a_;
    }

    [[nodiscard]]
    const logic::Bus<DataWidth>& src_b() const noexcept
    {
        return src_b_;
    }

    [[nodiscard]]
    const logic::Bus<DataWidth>& operand_a() const noexcept
    {
        return operand_a_.output();
    }

    [[nodiscard]]
    const logic::Bus<DataWidth>& operand_b() const noexcept
    {
        return operand_b_.output();
    }

    [[nodiscard]]
    const logic::Bus<DataWidth>& immediate() const noexcept
    {
        return immediate_;
    }

    [[nodiscard]]
    const logic::Bus<DataWidth>& memory_read_data() const noexcept
    {
        return memory_read_data_;
    }

    [[nodiscard]]
    const logic::Bus<MemoryAddressWidth>& memory_address() const noexcept
    {
        return memory_address_;
    }

    [[nodiscard]]
    const logic::Bus<DataWidth>& register_write_data() const noexcept
    {
        return register_write_data_;
    }

    [[nodiscard]]
    DataMemory<MemoryAddressWidth, DataWidth>& memory() noexcept
    {
        return memory_;
    }

    [[nodiscard]]
    bool halted() const noexcept
    {
        return control_.halt || fsm_.is_halted();
    }

    void write_register_for_test(
        cpu::Register destination,
        std::uint32_t value
    ) noexcept
    {
        rd_address_.write_value(static_cast<std::size_t>(destination));
        register_write_data_.write_value(value);
        register_write_enable_.write(logic::LogicState::HIGH);

        register_file_.evaluate();
        clock_.tick();
        register_file_.evaluate();
        clock_.tick();
        register_file_.evaluate();

        register_write_enable_.write(logic::LogicState::LOW);
        register_file_.evaluate();
    }

    [[nodiscard]]
    std::uint32_t read_register_for_test(cpu::Register source) noexcept
    {
        rs1_address_.write_value(static_cast<std::size_t>(source));
        register_file_.evaluate();
        return static_cast<std::uint32_t>(rs1_data_.read_value());
    }

    std::uint32_t read_memory_for_test(std::size_t address) noexcept
    {
        memory_address_.write_value(address);
        memory_read_enable_.write(logic::LogicState::HIGH);
        memory_write_enable_.write(logic::LogicState::LOW);
        memory_.evaluate();
        return static_cast<std::uint32_t>(memory_read_data_.read_value());
    }

    void write_memory_for_test(std::size_t address, std::uint32_t value) noexcept
    {
        memory_address_.write_value(address);
        memory_write_data_.write_value(value);
        memory_write_enable_.write(logic::LogicState::HIGH);
        memory_read_enable_.write(logic::LogicState::LOW);
        memory_.evaluate();
        clock_.tick();
        memory_.evaluate();
        clock_.tick();
        memory_.evaluate();
        memory_write_enable_.write(logic::LogicState::LOW);
        memory_.evaluate();
    }

private:
    static logic::LogicState to_state(bool value) noexcept
    {
        return value ? logic::LogicState::HIGH : logic::LogicState::LOW;
    }

    void apply_control_wires() noexcept
    {
        ir_write_.write(to_state(control_.irWrite));
        mdr_write_.write(to_state(control_.mdrWrite));
        a_write_.write(to_state(control_.aWrite));
        b_write_.write(to_state(control_.bWrite));
        alu_out_write_.write(to_state(control_.aluOutWrite));
        register_write_enable_.write(to_state(control_.regWrite));
        memory_write_enable_.write(to_state(control_.memWrite));
        memory_read_enable_.write(to_state(control_.memRead));
        ior_d_.write(to_state(control_.iorD));
        reg_dst_.write(to_state(control_.regDst));
        alu_src_a_select_.write(to_state(control_.aluSrcA == ALUSrcA::RegA));
        memory_to_register_.write(to_state(control_.writebackSource == WriteBackSource::memoryData));

        const auto src_b = static_cast<std::uint8_t>(control_.aluSrcB);
        alu_src_b_select0_.write(to_state((src_b & 0x1U) != 0));
        alu_src_b_select1_.write(to_state((src_b & 0x2U) != 0));

        const auto pc_src = static_cast<std::uint8_t>(control_.pcSource);
        pc_source_select0_.write(to_state((pc_src & 0x1U) != 0));
        pc_source_select1_.write(to_state((pc_src & 0x2U) != 0));
    }

    void decode_instruction() noexcept
    {
        Instruction instruction_word(
            static_cast<std::uint32_t>(instruction_register_.output().read_value())
        );
        decoded_instruction_ = InstructionDecoder::decode(instruction_word);

        rs1_address_.write_value(static_cast<std::size_t>(decoded_instruction_.rs1));
        rs2_address_.write_value(static_cast<std::size_t>(decoded_instruction_.rs2));
        decoded_rd_address_.write_value(static_cast<std::size_t>(decoded_instruction_.rd));
        rt_address_.write_value(static_cast<std::size_t>(decoded_instruction_.rs2));

        immediate_.write_value(static_cast<std::uint32_t>(decoded_instruction_.immediate));
        jump_target_.write_value(static_cast<std::uint32_t>(decoded_instruction_.immediate));
    }

    void drive_pc_enable() noexcept
    {
        bool take_branch = false;
        if (control_.pcWriteCond) {
            const bool zero = alu_zero_.read() == logic::LogicState::HIGH;
            take_branch = control_.bne ? !zero : zero;
        }

        const bool enable = !control_.halt && (control_.pcWrite || take_branch);
        pc_enable_.write(to_state(enable));
    }

    void evaluate_datapath() noexcept
    {
        clock_signal_.write(clock_.state());
        apply_control_wires();
        decode_instruction();
        immediate_shifter_.evaluate();

        register_write_enable_.write(logic::LogicState::LOW);
        register_file_.evaluate();

        alu_src_a_mux_.evaluate();
        alu_src_b_mux_.evaluate();

        alu_.set_operation(control_.aluOperation);
        alu_.evaluate();

        ior_d_mux_.evaluate();
        for (std::size_t i = 0; i < MemoryAddressWidth; ++i) {
            memory_address_[i].write(memory_address_wide_[i].read());
        }

        for (std::size_t i = 0; i < DataWidth; ++i) {
            memory_write_data_[i].write(operand_b_.output()[i].read());
        }

        memory_.evaluate();
        writeback_mux_.evaluate();
        reg_dest_mux_.evaluate();
        pc_source_mux_.evaluate();
        drive_pc_enable();

        program_counter_.evaluate();
        instruction_register_.evaluate();
        memory_data_register_.evaluate();
        operand_a_.evaluate();
        operand_b_.evaluate();
        alu_out_.evaluate();

        register_write_enable_.write(to_state(control_.regWrite));
        register_file_.evaluate();
    }

    logic::Clock& clock_;
    logic::Wire& reset_;
    logic::Wire clock_signal_;
    logic::Wire pc_enable_;

    //control signals
    logic::Wire ir_write_;
    logic::Wire mdr_write_;
    logic::Wire a_write_;
    logic::Wire b_write_;
    logic::Wire alu_out_write_;
    logic::Wire register_write_enable_;
    logic::Wire memory_read_enable_;
    logic::Wire memory_write_enable_;
    logic::Wire ior_d_;
    logic::Wire reg_dst_;
    logic::Wire alu_src_a_select_;
    logic::Wire alu_src_b_select0_;
    logic::Wire alu_src_b_select1_;
    logic::Wire pc_source_select0_;
    logic::Wire pc_source_select1_;
    logic::Wire memory_to_register_;
    logic::Wire alu_zero_;
    logic::Wire alu_carry_;

    // internal component source and result buses
    logic::Bus<AddressWidth> pc_;
    logic::Bus<AddressWidth> pc_next_;
    logic::Bus<DataWidth> constant_one_;
    logic::Bus<DataWidth> immediate_;
    logic::Bus<DataWidth> shift_amount_;
    logic::Bus<DataWidth> shifted_immediate_;
    logic::Bus<DataWidth> jump_target_;
    logic::Bus<DataWidth> src_a_;
    logic::Bus<DataWidth> src_b_;
    logic::Bus<DataWidth> alu_result_;
    logic::Bus<AddressWidth> memory_address_wide_;
    logic::Bus<MemoryAddressWidth> memory_address_;
    logic::Bus<DataWidth> memory_write_data_;
    logic::Bus<DataWidth> memory_read_data_;
    logic::Bus<DataWidth> rs1_data_;
    logic::Bus<DataWidth> rs2_data_;
    logic::Bus<DataWidth> register_write_data_;
    logic::Bus<RegisterAddressWidth> rs1_address_;
    logic::Bus<RegisterAddressWidth> rs2_address_;
    logic::Bus<RegisterAddressWidth> rt_address_;
    logic::Bus<RegisterAddressWidth> decoded_rd_address_;
    logic::Bus<RegisterAddressWidth> rd_address_;

    MultiCycleControlSignals control_{};
    DecodedInstruction decoded_instruction_{};

    //architectural components of the datapath
    ProgramCounter<AddressWidth> program_counter_;
    InstructionRegister<InstructionWidth> instruction_register_;
    MemoryDataRegister<DataWidth> memory_data_register_;
    OperandA_reg<DataWidth> operand_a_;
    OperandB_reg<DataWidth> operand_b_;
    ALU_out_reg<DataWidth> alu_out_;
    logic::RegisterFile<RegisterAddressWidth, DataWidth> register_file_;
    DataMemory<MemoryAddressWidth, DataWidth> memory_;
    FSMControlUnit fsm_{};

    //Datapath muxes
    ALUSrcA_mux<AddressWidth, DataWidth, DataWidth> alu_src_a_mux_;
    ALUSrcB_mux<DataWidth, DataWidth> alu_src_b_mux_;
    PCSource_mux<AddressWidth, DataWidth, AddressWidth> ior_d_mux_;
    OutMux<DataWidth, DataWidth> pc_source_mux_;
    RegDestMux<RegisterAddressWidth, RegisterAddressWidth> reg_dest_mux_;
    WriteBackMux<DataWidth> writeback_mux_;
    ALUInterface<DataWidth> alu_;
    logic::LogicalLeftShifter<DataWidth> immediate_shifter_;
};

} 

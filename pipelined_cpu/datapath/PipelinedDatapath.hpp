/*
    

    An implementation of a 5-stage pipelined processor datapath (IF, ID, EX, MEM, WB)
    based on the ARM architecture pipeline schematic (Harris & Harris).
*/

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <logic/combinational/adders/RippleCarryAdder.hpp>
#include <logic/combinational/multiplexers/Mux.hpp>
#include <logic/sequential/memory/RegisterFile.hpp>
#include <logic/signals/bus.hpp>
#include <logic/signals/clock.hpp>
#include <logic/signals/logicState.hpp>
#include <logic/signals/wire.hpp>
#include <logic/simulator/Component.hpp>

#include "../../components/ALUInterface.hpp"
#include "../../components/ALUOperandMux.hpp"
#include "../../components/ControlSignals.hpp"
#include "../../components/DataMemory.hpp"
#include "../../components/InstructionMemory.hpp"
#include "../../components/ProgramCounter.hpp"
#include "../../components/WriteBackMux.hpp"
#include "../../include/isa/DecodedInstruction.hpp"
#include "../../include/isa/Instruction.hpp"
#include "../../include/isa/InstructionDecoder.hpp"
#include "../core/ControlSignals.hpp"
#include "../core/ControlUnit.hpp"
#include "../core/HazardUnit.hpp"
#include "../multiplexers/ForwardAE_mux.hpp"
#include "../multiplexers/ForwardBE_mux.hpp"
#include "../registers/EX_MEM.hpp"
#include "../registers/ID_EX.hpp"
#include "../registers/IF_ID.hpp"
#include "../registers/MEM_WB.hpp"

namespace cpu {

template <
    std::size_t AddressWidth = 32,
    std::size_t DataWidth = 32,
    std::size_t InstructionWidth = 32,
    std::size_t RegisterAddressWidth = 4,
    std::size_t InstructionMemoryAddressWidth = 8,
    std::size_t DataMemoryAddressWidth = 8
>
class PipelinedCycleDatapath : public logic::Component {
public:
    PipelinedCycleDatapath(
        logic::Clock& clock,
        logic::Wire& reset
    ) : clock_(clock),
        reset_(reset),
        clock_signal_(logic::LogicState::LOW),
        stall_f_(logic::LogicState::LOW),
        stall_d_(logic::LogicState::LOW),
        flush_d_(logic::LogicState::LOW),
        flush_e_(logic::LogicState::LOW),
        pc_enable_(logic::LogicState::HIGH),
        if_id_enable_(logic::LogicState::HIGH),
        if_id_reset_(logic::LogicState::LOW),
        id_ex_reset_(logic::LogicState::LOW),
        const_high_wire_(logic::LogicState::HIGH),
        const_low_wire_(logic::LogicState::LOW),
        forward_ae_{},
        forward_be_{},
        pc_prime_{},
        pc_{},
        pcplus4_f_{},
        pc_constant_one_{},
        pc_adder_carry_in_(logic::LogicState::LOW),
        pc_adder_carry_out_(logic::LogicState::LOW),
        instruction_f_{},
        branch_or_pcplus4_{},
        pc_src_w_(logic::LogicState::LOW),
        instruction_d_{},
        pcplus4_d_{},
        decoded_instruction_{},
        control_signals_d_{},
        rs1_address_d_{},
        rs2_address_d_{},
        rd_address_d_{},
        rd1_data_d_{},
        rd2_data_d_{},
        immediate_d_{},
        pcplus4_e_{},
        rd1_data_e_{},
        rd2_data_e_{},
        immediate_e_{},
        rs1_address_e_{},
        rs2_address_e_{},
        rd_address_e_{},
        src_a_e_{},
        write_data_e_{},
        src_b_e_{},
        alu_result_e_{},
        alu_zero_e_(logic::LogicState::LOW),
        alu_carry_e_(logic::LogicState::LOW),
        alu_src_e_wire_(logic::LogicState::LOW),
        branch_taken_e_(logic::LogicState::LOW),
        branch_target_e_{},
        branch_adder_carry_out_(logic::LogicState::LOW),
        mem_to_reg_e_wire_(logic::LogicState::LOW),
        alu_result_m_{},
        write_data_m_{},
        rd_address_m_{},
        data_mem_address_{},
        memory_read_data_m_{},
        mem_read_m_wire_(logic::LogicState::LOW),
        mem_write_m_wire_(logic::LogicState::LOW),
        reg_write_m_wire_(logic::LogicState::LOW),
        read_data_w_{},
        alu_result_w_{},
        rd_address_w_{},
        result_w_{},
        reg_write_w_wire_(logic::LogicState::LOW),
        mem_to_reg_w_wire_(logic::LogicState::LOW),
        wb_opcode_(Opcode::NOP),
        mem_opcode_(Opcode::NOP),
        ex_opcode_(Opcode::NOP),
        id_opcode_(Opcode::NOP),

        // Fetch components
        program_counter_(clock_signal_, reset_, pc_enable_, pc_prime_, pc_),
        instruction_mem_(pc_, instruction_f_),
        pc_adder_(pc_, pc_constant_one_, pc_adder_carry_in_, pcplus4_f_, pc_adder_carry_out_),
        pc_branch_mux_(pcplus4_f_, branch_target_e_, branch_taken_e_, branch_or_pcplus4_),
        pc_wb_mux_(branch_or_pcplus4_, result_w_, pc_src_w_, pc_prime_),

        // IF/ID register
        if_id_(instruction_f_, pcplus4_f_, clock_signal_, if_id_reset_, if_id_enable_),

        // Decode components
        register_file_(
            clock_,
            reset_,
            reg_write_w_wire_,
            rs1_address_d_,
            rs2_address_d_,
            rd_address_w_,
            result_w_,
            rd1_data_d_,
            rd2_data_d_
        ),

        // ID/EX register
        id_ex_(
            if_id_.pcplus4(),
            rd1_data_d_,
            rd2_data_d_,
            immediate_d_,
            rs1_address_d_,
            rs2_address_d_,
            rd_address_d_,
            clock_signal_,
            id_ex_reset_,
            const_high_wire_
        ),

        // Execute components
        forward_ae_mux_(id_ex_.read_data1(), result_w_, ex_mem_.alu_result(), forward_ae_, src_a_e_),
        forward_be_mux_(id_ex_.read_data2(), result_w_, ex_mem_.alu_result(), forward_be_, write_data_e_),
        alu_operand_mux_(write_data_e_, id_ex_.immediate(), alu_src_e_wire_, src_b_e_),
        alu_interface_(src_a_e_, src_b_e_, ALUOperation::ADD, alu_result_e_, alu_zero_e_, alu_carry_e_),
        branch_adder_(id_ex_.pcplus4(), id_ex_.immediate(), const_low_wire_, branch_target_e_, branch_adder_carry_out_),

        // EX/MEM register
        ex_mem_(
            alu_result_e_,
            write_data_e_,
            id_ex_.rd(),
            clock_signal_,
            reset_,
            const_high_wire_
        ),

        // Memory components
        data_mem_(
            clock_,
            reset_,
            mem_read_m_wire_,
            mem_write_m_wire_,
            data_mem_address_,
            ex_mem_.read_data2(),
            memory_read_data_m_
        ),

        // MEM/WB register
        mem_wb_(
            memory_read_data_m_,
            ex_mem_.alu_result(),
            ex_mem_.rd(),
            clock_signal_,
            reset_,
            const_high_wire_
        ),

        // Writeback components
        writeback_mux_(mem_wb_.alu_result(), mem_wb_.read_data(), mem_to_reg_w_wire_, result_w_),

        // Hazard Unit
        hazard_unit_(
            rs1_address_d_,
            rs2_address_d_,
            id_ex_.rs(),
            id_ex_.rt(),
            id_ex_.rd(),
            mem_to_reg_e_wire_,
            branch_taken_e_,
            ex_mem_.rd(),
            reg_write_m_wire_,
            mem_wb_.rd(),
            reg_write_w_wire_,
            forward_ae_,
            forward_be_,
            stall_f_,
            stall_d_,
            flush_d_,
            flush_e_
        )
    {
        // PC increment constant = 1 (word addressing in instruction memory)
        for (std::size_t i = 0; i < AddressWidth; ++i) {
            pc_constant_one_[i].write(i == 0 ? logic::LogicState::HIGH : logic::LogicState::LOW);
        }
        pc_adder_carry_in_.write(logic::LogicState::LOW);
        const_high_wire_.write(logic::LogicState::HIGH);
        const_low_wire_.write(logic::LogicState::LOW);
    }

    void evaluate() noexcept override {
        clock_signal_.write(clock_.state());

        //Writeback stage evaluation
        mem_to_reg_w_wire_.write(mem_wb_.mem_to_reg() ? logic::LogicState::HIGH : logic::LogicState::LOW);
        reg_write_w_wire_.write(mem_wb_.reg_write() ? logic::LogicState::HIGH : logic::LogicState::LOW);
        rd_address_w_.write_value(mem_wb_.rd().read_value());
        writeback_mux_.evaluate();

        // Checking if R15 (PC in ARM) was written in WB stage
        const bool r15_written = mem_wb_.reg_write() && (mem_wb_.rd().read_value() == 15);
        pc_src_w_.write(r15_written ? logic::LogicState::HIGH : logic::LogicState::LOW);

        register_file_.evaluate();

        //Memory stage evaluation
        mem_read_m_wire_.write(ex_mem_.mem_read() ? logic::LogicState::HIGH : logic::LogicState::LOW);
        mem_write_m_wire_.write(ex_mem_.mem_write() ? logic::LogicState::HIGH : logic::LogicState::LOW);
        reg_write_m_wire_.write(ex_mem_.reg_write() ? logic::LogicState::HIGH : logic::LogicState::LOW);

        for (std::size_t i = 0; i < DataMemoryAddressWidth; ++i) {
            data_mem_address_[i].write(ex_mem_.alu_result()[i].read());
        }
        data_mem_.evaluate();

        //Decode stage evaluation (decodinh current IF/ID instruction to drive RA1D, RA2D for HazardUnit)
        Instruction instruction_word(static_cast<std::uint32_t>(if_id_.instruction().read_value()));
        decoded_instruction_ = InstructionDecoder::decode(instruction_word);
        control_signals_d_ = PipelinedControlUnit::generate(decoded_instruction_);

        rs1_address_d_.write_value(static_cast<std::size_t>(decoded_instruction_.rs1));
        rs2_address_d_.write_value(static_cast<std::size_t>(decoded_instruction_.rs2));
        rd_address_d_.write_value(static_cast<std::size_t>(decoded_instruction_.rd));
        immediate_d_.write_value(static_cast<std::uint32_t>(decoded_instruction_.immediate));

        register_file_.evaluate();

        // Register File write-before-read bypass (WB-to-ID forwarding)
        // Resolves hazard when WB stage writes to a register that ID stage is reading in the same cycle
        const bool wb_writing = (reg_write_w_wire_.read() == logic::LogicState::HIGH);
        const auto wb_dest = rd_address_w_.read_value();
        if (wb_writing && (wb_dest == rs1_address_d_.read_value())) {
            rd1_data_d_.write_value(result_w_.read_value());
        }
        if (wb_writing && (wb_dest == rs2_address_d_.read_value())) {
            rd2_data_d_.write_value(result_w_.read_value());
        }

        // Execute stage & Hazard evaluation
        mem_to_reg_e_wire_.write(id_ex_.mem_to_reg() ? logic::LogicState::HIGH : logic::LogicState::LOW);

        // Run hazard detection for forwarding and load-use stalls
        hazard_unit_.evaluate();

        forward_ae_mux_.evaluate();
        forward_be_mux_.evaluate();

        alu_src_e_wire_.write(id_ex_.alu_src() ? logic::LogicState::HIGH : logic::LogicState::LOW);
        alu_operand_mux_.evaluate();

        alu_interface_.set_operation(id_ex_.alu_op());
        alu_interface_.evaluate();

        branch_adder_.evaluate();

        // Branch condition resolution
        bool branch_taken = false;
        if (id_ex_.branch()) {
            const bool zero = (alu_zero_e_.read() == logic::LogicState::HIGH);
            if (ex_opcode_ == Opcode::BEQ) {
                branch_taken = zero;
            } else if (ex_opcode_ == Opcode::BNE) {
                branch_taken = !zero;
            } else if (ex_opcode_ == Opcode::J) {
                branch_taken = true;
            } else {
                branch_taken = zero;
            }
        }
        branch_taken_e_.write(branch_taken ? logic::LogicState::HIGH : logic::LogicState::LOW);

        // Re-evaluate hazard unit with branch status
        hazard_unit_.evaluate();

        // Update pipeline control lines
        const bool halt_fetched = (decoded_instruction_.opcode == Opcode::HALT || id_opcode_ == Opcode::HALT || ex_opcode_ == Opcode::HALT || mem_opcode_ == Opcode::HALT || wb_opcode_ == Opcode::HALT);
        const bool pc_en = !halt_fetched && (stall_f_.read() == logic::LogicState::LOW);
        pc_enable_.write(pc_en ? logic::LogicState::HIGH : logic::LogicState::LOW);
        if_id_enable_.write(stall_d_.read() == logic::LogicState::LOW ? logic::LogicState::HIGH : logic::LogicState::LOW);
        if_id_reset_.write((reset_.read() == logic::LogicState::HIGH || flush_d_.read() == logic::LogicState::HIGH)
                               ? logic::LogicState::HIGH : logic::LogicState::LOW);
        id_ex_reset_.write((reset_.read() == logic::LogicState::HIGH || flush_e_.read() == logic::LogicState::HIGH)
                               ? logic::LogicState::HIGH : logic::LogicState::LOW);

        //Fetch stage evaluation
        pc_adder_.evaluate();
        pc_branch_mux_.evaluate();
        pc_wb_mux_.evaluate();
        instruction_mem_.evaluate();

        // Pipeline registers evaluation
        program_counter_.evaluate();
        if_id_.evaluate();

        id_ex_.set_controls(control_signals_d_);
        id_ex_.evaluate();

        PipelinedControlSignals ctrl_m = id_ex_.controls();
        if (branch_taken) {
            ctrl_m.regWrite = false;
            ctrl_m.memWrite = false;
            ctrl_m.memRead = false;
        }
        ex_mem_.set_controls(ctrl_m);
        ex_mem_.evaluate();

        mem_wb_.set_controls(ex_mem_.controls());
        mem_wb_.evaluate();
    }

    /// Advances the processor by one complete clock cycle.
    void step() noexcept {
        if (halted()) {
            return;
        }

        // Setup phase: Clock LOW
        clock_signal_.write(logic::LogicState::LOW);
        evaluate();
        clock_.tick();

        // Capture phase: Clock HIGH (rising edge updates sequential registers)
        clock_signal_.write(logic::LogicState::HIGH);
        evaluate();
        clock_.tick();

        // Settle back to Clock LOW
        clock_signal_.write(logic::LogicState::LOW);
        evaluate();

        // Update instruction opcodes across pipeline stages
        wb_opcode_ = mem_opcode_;
        mem_opcode_ = ex_opcode_;
        if (flush_e_.read() == logic::LogicState::HIGH) {
            ex_opcode_ = Opcode::NOP;
        } else {
            ex_opcode_ = id_opcode_;
        }

        if (flush_d_.read() == logic::LogicState::HIGH) {
            id_opcode_ = Opcode::NOP;
        } else if (stall_d_.read() == logic::LogicState::LOW) {
            id_opcode_ = decoded_instruction_.opcode;
        }
    }

    /// Resets the processor and all pipeline stage registers.
    void reset() noexcept {
        reset_.write(logic::LogicState::HIGH);
        clock_signal_.write(logic::LogicState::LOW);
        evaluate();
        clock_.tick();
        clock_signal_.write(logic::LogicState::HIGH);
        evaluate();
        clock_.tick();
        clock_signal_.write(logic::LogicState::LOW);
        reset_.write(logic::LogicState::LOW);
        evaluate();

        id_opcode_ = Opcode::NOP;
        ex_opcode_ = Opcode::NOP;
        mem_opcode_ = Opcode::NOP;
        wb_opcode_ = Opcode::NOP;
    }

    /// Runs autonomous execution until the pipeline halts or max_cycles is reached.
    void run(std::size_t max_cycles = 100000) noexcept {
        std::size_t c = 0;
        while (!halted() && c < max_cycles) {
            step();
            ++c;
        }
    }

    /// Program loading helpers
    void load_instructions(const std::vector<std::size_t>& instructions) noexcept {
        instruction_mem_.load(instructions);
    }

    void load_instructions(const std::vector<std::uint32_t>& instructions) noexcept {
        std::vector<std::size_t> widened(instructions.begin(), instructions.end());
        instruction_mem_.load(widened);
    }

    /// Testing and inspection methods
    void write_register_for_test(cpu::Register destination, std::uint32_t value) noexcept {
        rd_address_w_.write_value(static_cast<std::size_t>(destination));
        result_w_.write_value(value);
        reg_write_w_wire_.write(logic::LogicState::HIGH);

        register_file_.evaluate();
        clock_.tick();
        register_file_.evaluate();
        clock_.tick();
        register_file_.evaluate();

        reg_write_w_wire_.write(logic::LogicState::LOW);
        register_file_.evaluate();
    }

    [[nodiscard]]
    std::uint32_t read_register_for_test(cpu::Register source) noexcept {
        rs1_address_d_.write_value(static_cast<std::size_t>(source));
        register_file_.evaluate();
        return static_cast<std::uint32_t>(rd1_data_d_.read_value());
    }

    [[nodiscard]]
    std::uint32_t read_memory_for_test(std::size_t address) noexcept {
        data_mem_address_.write_value(address);
        mem_read_m_wire_.write(logic::LogicState::HIGH);
        mem_write_m_wire_.write(logic::LogicState::LOW);
        data_mem_.evaluate();
        return static_cast<std::uint32_t>(memory_read_data_m_.read_value());
    }

    void write_memory_for_test(std::size_t address, std::uint32_t value) noexcept {
        data_mem_address_.write_value(address);
        ex_mem_.read_data2().write_value(value);
        mem_write_m_wire_.write(logic::LogicState::HIGH);
        mem_read_m_wire_.write(logic::LogicState::LOW);
        data_mem_.evaluate();
        clock_.tick();
        data_mem_.evaluate();
        clock_.tick();
        data_mem_.evaluate();
        mem_write_m_wire_.write(logic::LogicState::LOW);
        data_mem_.evaluate();
    }

    // Accessors
    [[nodiscard]] logic::Bus<AddressWidth>& pc() noexcept { return pc_; }
    [[nodiscard]] const logic::Bus<AddressWidth>& pc() const noexcept { return pc_; }

    [[nodiscard]] const DecodedInstruction& decoded_instruction() const noexcept { return decoded_instruction_; }
    [[nodiscard]] const PipelinedControlSignals& control_signals() const noexcept { return control_signals_d_; }

    [[nodiscard]] IF_ID<InstructionWidth, AddressWidth>& if_id() noexcept { return if_id_; }
    [[nodiscard]] const IF_ID<InstructionWidth, AddressWidth>& if_id() const noexcept { return if_id_; }

    [[nodiscard]] ID_EX<AddressWidth, DataWidth, RegisterAddressWidth>& id_ex() noexcept { return id_ex_; }
    [[nodiscard]] const ID_EX<AddressWidth, DataWidth, RegisterAddressWidth>& id_ex() const noexcept { return id_ex_; }

    [[nodiscard]] EX_MEM<DataWidth, RegisterAddressWidth>& ex_mem() noexcept { return ex_mem_; }
    [[nodiscard]] const EX_MEM<DataWidth, RegisterAddressWidth>& ex_mem() const noexcept { return ex_mem_; }

    [[nodiscard]] MEM_WB<DataWidth, RegisterAddressWidth>& mem_wb() noexcept { return mem_wb_; }
    [[nodiscard]] const MEM_WB<DataWidth, RegisterAddressWidth>& mem_wb() const noexcept { return mem_wb_; }

    [[nodiscard]] HazardUnit<RegisterAddressWidth>& hazard_unit() noexcept { return hazard_unit_; }
    [[nodiscard]] const HazardUnit<RegisterAddressWidth>& hazard_unit() const noexcept { return hazard_unit_; }

    [[nodiscard]] const logic::Bus<2>& forward_ae() const noexcept { return forward_ae_; }
    [[nodiscard]] const logic::Bus<2>& forward_be() const noexcept { return forward_be_; }

    [[nodiscard]] const logic::Wire& stall_f() const noexcept { return stall_f_; }
    [[nodiscard]] const logic::Wire& stall_d() const noexcept { return stall_d_; }
    [[nodiscard]] const logic::Wire& flush_d() const noexcept { return flush_d_; }
    [[nodiscard]] const logic::Wire& flush_e() const noexcept { return flush_e_; }

    [[nodiscard]] DataMemory<DataMemoryAddressWidth, DataWidth>& data_memory() noexcept { return data_mem_; }
    [[nodiscard]] const DataMemory<DataMemoryAddressWidth, DataWidth>& data_memory() const noexcept { return data_mem_; }

    [[nodiscard]] InstructionMemory<AddressWidth, InstructionMemoryAddressWidth, InstructionWidth>& instruction_memory() noexcept {
        return instruction_mem_;
    }
    [[nodiscard]] const InstructionMemory<AddressWidth, InstructionMemoryAddressWidth, InstructionWidth>& instruction_memory() const noexcept {
        return instruction_mem_;
    }

    [[nodiscard]] Opcode id_opcode() const noexcept { return id_opcode_; }
    [[nodiscard]] Opcode ex_opcode() const noexcept { return ex_opcode_; }
    [[nodiscard]] Opcode mem_opcode() const noexcept { return mem_opcode_; }
    [[nodiscard]] Opcode wb_opcode() const noexcept { return wb_opcode_; }
    [[nodiscard]] bool halted() const noexcept { return wb_opcode_ == Opcode::HALT; }

private:
    // External signals
    logic::Clock& clock_;
    logic::Wire& reset_;
    logic::Wire clock_signal_;

    // Hazard control wires
    logic::Wire stall_f_;
    logic::Wire stall_d_;
    logic::Wire flush_d_;
    logic::Wire flush_e_;
    logic::Wire pc_enable_;
    logic::Wire if_id_enable_;
    logic::Wire if_id_reset_;
    logic::Wire id_ex_reset_;
    logic::Wire const_high_wire_;
    logic::Wire const_low_wire_;

    // Forwarding buses
    logic::Bus<2> forward_ae_;
    logic::Bus<2> forward_be_;

    // Fetch stage
    logic::Bus<AddressWidth> pc_prime_;
    logic::Bus<AddressWidth> pc_;
    logic::Bus<AddressWidth> pcplus4_f_;
    logic::Bus<AddressWidth> pc_constant_one_;
    logic::Wire pc_adder_carry_in_;
    logic::Wire pc_adder_carry_out_;
    logic::Bus<InstructionWidth> instruction_f_;
    logic::Bus<AddressWidth> branch_or_pcplus4_;
    logic::Wire pc_src_w_;

    // Decode stage
    logic::Bus<InstructionWidth> instruction_d_;
    logic::Bus<AddressWidth> pcplus4_d_;
    DecodedInstruction decoded_instruction_;
    PipelinedControlSignals control_signals_d_;
    logic::Bus<RegisterAddressWidth> rs1_address_d_;
    logic::Bus<RegisterAddressWidth> rs2_address_d_;
    logic::Bus<RegisterAddressWidth> rd_address_d_;
    logic::Bus<DataWidth> rd1_data_d_;
    logic::Bus<DataWidth> rd2_data_d_;
    logic::Bus<DataWidth> immediate_d_;

    // Execute stage
    logic::Bus<AddressWidth> pcplus4_e_;
    logic::Bus<DataWidth> rd1_data_e_;
    logic::Bus<DataWidth> rd2_data_e_;
    logic::Bus<DataWidth> immediate_e_;
    logic::Bus<RegisterAddressWidth> rs1_address_e_;
    logic::Bus<RegisterAddressWidth> rs2_address_e_;
    logic::Bus<RegisterAddressWidth> rd_address_e_;
    logic::Bus<DataWidth> src_a_e_;
    logic::Bus<DataWidth> write_data_e_;
    logic::Bus<DataWidth> src_b_e_;
    logic::Bus<DataWidth> alu_result_e_;
    logic::Wire alu_zero_e_;
    logic::Wire alu_carry_e_;
    logic::Wire alu_src_e_wire_;
    logic::Wire branch_taken_e_;
    logic::Bus<AddressWidth> branch_target_e_;
    logic::Wire branch_adder_carry_out_;
    logic::Wire mem_to_reg_e_wire_;

    // Memory stage
    logic::Bus<DataWidth> alu_result_m_;
    logic::Bus<DataWidth> write_data_m_;
    logic::Bus<RegisterAddressWidth> rd_address_m_;
    logic::Bus<DataMemoryAddressWidth> data_mem_address_;
    logic::Bus<DataWidth> memory_read_data_m_;
    logic::Wire mem_read_m_wire_;
    logic::Wire mem_write_m_wire_;
    logic::Wire reg_write_m_wire_;

    // Writeback stage
    logic::Bus<DataWidth> read_data_w_;
    logic::Bus<DataWidth> alu_result_w_;
    logic::Bus<RegisterAddressWidth> rd_address_w_;
    logic::Bus<DataWidth> result_w_;
    logic::Wire reg_write_w_wire_;
    logic::Wire mem_to_reg_w_wire_;

    // Internal tracking of opcodes
    Opcode wb_opcode_;
    Opcode mem_opcode_;
    Opcode ex_opcode_;
    Opcode id_opcode_;

    // Datapath hardware components
    ProgramCounter<AddressWidth> program_counter_;
    InstructionMemory<AddressWidth, InstructionMemoryAddressWidth, InstructionWidth> instruction_mem_;
    logic::RippleCarryAdder<AddressWidth> pc_adder_;
    logic::Mux<AddressWidth> pc_branch_mux_;
    logic::Mux<AddressWidth> pc_wb_mux_;

    IF_ID<InstructionWidth, AddressWidth> if_id_;
    logic::RegisterFile<RegisterAddressWidth, DataWidth> register_file_;
    ID_EX<AddressWidth, DataWidth, RegisterAddressWidth> id_ex_;

    ForwardAE_mux<DataWidth, DataWidth> forward_ae_mux_;
    ForwardBE_mux<DataWidth, DataWidth> forward_be_mux_;
    ALUOperandMux<DataWidth> alu_operand_mux_;
    ALUInterface<DataWidth> alu_interface_;
    logic::RippleCarryAdder<AddressWidth> branch_adder_;

    EX_MEM<DataWidth, RegisterAddressWidth> ex_mem_;
    DataMemory<DataMemoryAddressWidth, DataWidth> data_mem_;
    MEM_WB<DataWidth, RegisterAddressWidth> mem_wb_;
    WriteBackMux<DataWidth> writeback_mux_;

    HazardUnit<RegisterAddressWidth> hazard_unit_;
};

} 

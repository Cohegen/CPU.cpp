#pragma once

#include <cstddef>
#include <logic/sequential/registers/register.hpp>
#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/simulator/Component.hpp>
#include <logic/combinational/multiplexers/Mux.hpp>
#include "../core/ControlSignals.hpp"

namespace cpu {

    template <std::size_t DataWidth = 32, std::size_t RegAddrWidth = 5>
    class EX_MEM : public logic::Component {
    public:
        // Standard constructor (inputs first, then signals)
        EX_MEM(
            logic::Bus<DataWidth>& ALUResult,
            logic::Bus<DataWidth>& readData2,
            logic::Bus<RegAddrWidth>& rd,
            logic::Wire& clock,
            logic::Wire& reset,
            logic::Wire& enable,
            const PipelinedControlSignals& initial_controls = PipelinedControlSignals{}
        ) : ALUResult_(ALUResult),
            readData2_(readData2),
            rd_(rd),
            clock_(clock),
            reset_(reset),
            enable_(enable),

            // Data muxes & registers
            enable_mux_alu_result_(ALUResult_out_, ALUResult_, enable_, enable_out_alu_result_),
            reset_mux_alu_result_(enable_out_alu_result_, zero_alu_result_, reset_, alu_result_reg_in_),
            ALUResult_reg_(alu_result_reg_in_, clock_, ALUResult_out_),

            enable_mux_readData2_(readData2_out_, readData2_, enable_, enable_out_readData2_),
            reset_mux_readData2_(enable_out_readData2_, zero_readData2_, reset_, readData2_reg_in_),
            readData2_reg_(readData2_reg_in_, clock_, readData2_out_),

            enable_mux_rd_(rd_out_, rd_, enable_, enable_out_rd_),
            reset_mux_rd_(enable_out_rd_, zero_rd_, reset_, rd_reg_in_),
            rd_reg_(rd_reg_in_, clock_, rd_out_),

            // Control muxes & registers
            enable_mux_branch_(branch_out_, branch_in_, enable_, enable_out_branch_),
            reset_mux_branch_(enable_out_branch_, zero_1_, reset_, branch_reg_in_),
            branch_reg_(branch_reg_in_, clock_, branch_out_),

            enable_mux_mem_read_(mem_read_out_, mem_read_in_, enable_, enable_out_mem_read_),
            reset_mux_mem_read_(enable_out_mem_read_, zero_1_, reset_, mem_read_reg_in_),
            mem_read_reg_(mem_read_reg_in_, clock_, mem_read_out_),

            enable_mux_mem_write_(mem_write_out_, mem_write_in_, enable_, enable_out_mem_write_),
            reset_mux_mem_write_(enable_out_mem_write_, zero_1_, reset_, mem_write_reg_in_),
            mem_write_reg_(mem_write_reg_in_, clock_, mem_write_out_),

            enable_mux_reg_write_(reg_write_out_, reg_write_in_, enable_, enable_out_reg_write_),
            reset_mux_reg_write_(enable_out_reg_write_, zero_1_, reset_, reg_write_reg_in_),
            reg_write_reg_(reg_write_reg_in_, clock_, reg_write_out_),

            enable_mux_mem_to_reg_(mem_to_reg_out_, mem_to_reg_in_, enable_, enable_out_mem_to_reg_),
            reset_mux_mem_to_reg_(enable_out_mem_to_reg_, zero_1_, reset_, mem_to_reg_reg_in_),
            mem_to_reg_reg_(mem_to_reg_reg_in_, clock_, mem_to_reg_out_)
        {
            zero_alu_result_.write(logic::LogicState::LOW);
            zero_readData2_.write(logic::LogicState::LOW);
            zero_rd_.write(logic::LogicState::LOW);
            zero_1_.write(logic::LogicState::LOW);

            set_controls(initial_controls);
        }

        
        EX_MEM(
            logic::Wire& enable,
            logic::Wire& clock,
            logic::Wire& reset,
            logic::Bus<DataWidth>& ALUResult,
            logic::Bus<DataWidth>& readData2,
            logic::Bus<RegAddrWidth>& rd,
            const PipelinedControlSignals& initial_controls = PipelinedControlSignals{}
        ) : EX_MEM(ALUResult, readData2, rd, clock, reset, enable, initial_controls)
        {
        }

        void set_controls(const PipelinedControlSignals& ctrl) noexcept {
            branch_in_.write_value(ctrl.branch ? 1 : 0);
            mem_read_in_.write_value(ctrl.memRead ? 1 : 0);
            mem_write_in_.write_value(ctrl.memWrite ? 1 : 0);
            reg_write_in_.write_value(ctrl.regWrite ? 1 : 0);
            mem_to_reg_in_.write_value(ctrl.memToReg ? 1 : 0);
        }

        void set_branch(bool v) noexcept { branch_in_.write_value(v ? 1 : 0); }
        void set_mem_read(bool v) noexcept { mem_read_in_.write_value(v ? 1 : 0); }
        void set_mem_write(bool v) noexcept { mem_write_in_.write_value(v ? 1 : 0); }
        void set_reg_write(bool v) noexcept { reg_write_in_.write_value(v ? 1 : 0); }
        void set_mem_to_reg(bool v) noexcept { mem_to_reg_in_.write_value(v ? 1 : 0); }

        void evaluate() noexcept override {
            // Data path
            enable_mux_alu_result_.evaluate();
            reset_mux_alu_result_.evaluate();
            ALUResult_reg_.evaluate();

            enable_mux_readData2_.evaluate();
            reset_mux_readData2_.evaluate();
            readData2_reg_.evaluate();

            enable_mux_rd_.evaluate();
            reset_mux_rd_.evaluate();
            rd_reg_.evaluate();

            // Control path
            enable_mux_branch_.evaluate();
            reset_mux_branch_.evaluate();
            branch_reg_.evaluate();

            enable_mux_mem_read_.evaluate();
            reset_mux_mem_read_.evaluate();
            mem_read_reg_.evaluate();

            enable_mux_mem_write_.evaluate();
            reset_mux_mem_write_.evaluate();
            mem_write_reg_.evaluate();

            enable_mux_reg_write_.evaluate();
            reset_mux_reg_write_.evaluate();
            reg_write_reg_.evaluate();

            enable_mux_mem_to_reg_.evaluate();
            reset_mux_mem_to_reg_.evaluate();
            mem_to_reg_reg_.evaluate();
        }

        // Data output accessors
        [[nodiscard]] logic::Bus<DataWidth>& alu_result() noexcept { return ALUResult_out_; }
        [[nodiscard]] const logic::Bus<DataWidth>& alu_result() const noexcept { return ALUResult_out_; }
        [[nodiscard]] const logic::Bus<DataWidth>& read_alu_result() const noexcept { return ALUResult_out_; }

        [[nodiscard]] logic::Bus<DataWidth>& read_data2() noexcept { return readData2_out_; }
        [[nodiscard]] const logic::Bus<DataWidth>& read_data2() const noexcept { return readData2_out_; }

        [[nodiscard]] logic::Bus<RegAddrWidth>& rd() noexcept { return rd_out_; }
        [[nodiscard]] const logic::Bus<RegAddrWidth>& rd() const noexcept { return rd_out_; }

        // Control output accessors
        [[nodiscard]] bool branch() const noexcept { return branch_out_.read_value() != 0; }
        [[nodiscard]] bool mem_read() const noexcept { return mem_read_out_.read_value() != 0; }
        [[nodiscard]] bool mem_write() const noexcept { return mem_write_out_.read_value() != 0; }
        [[nodiscard]] bool reg_write() const noexcept { return reg_write_out_.read_value() != 0; }
        [[nodiscard]] bool mem_to_reg() const noexcept { return mem_to_reg_out_.read_value() != 0; }

        [[nodiscard]]
        PipelinedControlSignals controls() const noexcept {
            PipelinedControlSignals ctrl{};
            ctrl.branch = branch();
            ctrl.memRead = mem_read();
            ctrl.memWrite = mem_write();
            ctrl.regWrite = reg_write();
            ctrl.memToReg = mem_to_reg();
            return ctrl;
        }

    private:
        // Inputs
        logic::Bus<DataWidth>& ALUResult_;
        logic::Bus<DataWidth>& readData2_;
        logic::Bus<RegAddrWidth>& rd_;

        // Signals
        logic::Wire& clock_;
        logic::Wire& reset_;
        logic::Wire& enable_;

        // Control inputs (registered internally)
        logic::Bus<1> branch_in_;
        logic::Bus<1> mem_read_in_;
        logic::Bus<1> mem_write_in_;
        logic::Bus<1> reg_write_in_;
        logic::Bus<1> mem_to_reg_in_;

        // Outputs
        logic::Bus<DataWidth> ALUResult_out_;
        logic::Bus<DataWidth> readData2_out_;
        logic::Bus<RegAddrWidth> rd_out_;

        logic::Bus<1> branch_out_;
        logic::Bus<1> mem_read_out_;
        logic::Bus<1> mem_write_out_;
        logic::Bus<1> reg_write_out_;
        logic::Bus<1> mem_to_reg_out_;

        // Internal zero buses
        logic::Bus<DataWidth> zero_alu_result_;
        logic::Bus<DataWidth> zero_readData2_;
        logic::Bus<RegAddrWidth> zero_rd_;
        logic::Bus<1> zero_1_;

        // Internal intermediate buses
        logic::Bus<DataWidth> enable_out_alu_result_;
        logic::Bus<DataWidth> enable_out_readData2_;
        logic::Bus<RegAddrWidth> enable_out_rd_;

        logic::Bus<1> enable_out_branch_;
        logic::Bus<1> enable_out_mem_read_;
        logic::Bus<1> enable_out_mem_write_;
        logic::Bus<1> enable_out_reg_write_;
        logic::Bus<1> enable_out_mem_to_reg_;

        logic::Bus<DataWidth> alu_result_reg_in_;
        logic::Bus<DataWidth> readData2_reg_in_;
        logic::Bus<RegAddrWidth> rd_reg_in_;

        logic::Bus<1> branch_reg_in_;
        logic::Bus<1> mem_read_reg_in_;
        logic::Bus<1> mem_write_reg_in_;
        logic::Bus<1> reg_write_reg_in_;
        logic::Bus<1> mem_to_reg_reg_in_;

        // Data Multiplexers
        logic::Mux<DataWidth> enable_mux_alu_result_;
        logic::Mux<DataWidth> reset_mux_alu_result_;

        logic::Mux<DataWidth> enable_mux_readData2_;
        logic::Mux<DataWidth> reset_mux_readData2_;

        logic::Mux<RegAddrWidth> enable_mux_rd_;
        logic::Mux<RegAddrWidth> reset_mux_rd_;

        // Control Multiplexers
        logic::Mux<1> enable_mux_branch_;
        logic::Mux<1> reset_mux_branch_;

        logic::Mux<1> enable_mux_mem_read_;
        logic::Mux<1> reset_mux_mem_read_;

        logic::Mux<1> enable_mux_mem_write_;
        logic::Mux<1> reset_mux_mem_write_;

        logic::Mux<1> enable_mux_reg_write_;
        logic::Mux<1> reset_mux_reg_write_;

        logic::Mux<1> enable_mux_mem_to_reg_;
        logic::Mux<1> reset_mux_mem_to_reg_;

        // Pipeline registers
        logic::Register<DataWidth> ALUResult_reg_;
        logic::Register<DataWidth> readData2_reg_;
        logic::Register<RegAddrWidth> rd_reg_;

        logic::Register<1> branch_reg_;
        logic::Register<1> mem_read_reg_;
        logic::Register<1> mem_write_reg_;
        logic::Register<1> reg_write_reg_;
        logic::Register<1> mem_to_reg_reg_;
    };

} // namespace cpu

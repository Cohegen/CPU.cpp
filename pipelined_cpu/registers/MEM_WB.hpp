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
    class MEM_WB : public logic::Component {
    public:
        MEM_WB(
            logic::Bus<DataWidth>& readData,
            logic::Bus<DataWidth>& ALUResult,
            logic::Bus<RegAddrWidth>& rd,
            logic::Wire& clock,
            logic::Wire& reset,
            logic::Wire& enable,
            const PipelinedControlSignals& initial_controls = PipelinedControlSignals{}
        ) : readData_(readData),
            ALUResult_(ALUResult),
            rd_(rd),
            clock_(clock),
            reset_(reset),
            enable_(enable),

            // Data muxes & registers
            enable_mux_read_data_(readData_out_, readData_, enable_, enable_out_read_data_),
            reset_mux_read_data_(enable_out_read_data_, zero_read_data_, reset_, read_data_reg_in_),
            readData_reg_(read_data_reg_in_, clock_, readData_out_),

            enable_mux_alu_result_(ALUResult_out_, ALUResult_, enable_, enable_out_alu_result_),
            reset_mux_alu_result_(enable_out_alu_result_, zero_alu_result_, reset_, alu_result_reg_in_),
            ALUResult_reg_(alu_result_reg_in_, clock_, ALUResult_out_),

            enable_mux_rd_(rd_out_, rd_, enable_, enable_out_rd_),
            reset_mux_rd_(enable_out_rd_, zero_rd_, reset_, rd_reg_in_),
            rd_reg_(rd_reg_in_, clock_, rd_out_),

            // Control muxes & registers
            enable_mux_reg_write_(reg_write_out_, reg_write_in_, enable_, enable_out_reg_write_),
            reset_mux_reg_write_(enable_out_reg_write_, zero_1_, reset_, reg_write_reg_in_),
            reg_write_reg_(reg_write_reg_in_, clock_, reg_write_out_),

            enable_mux_mem_to_reg_(mem_to_reg_out_, mem_to_reg_in_, enable_, enable_out_mem_to_reg_),
            reset_mux_mem_to_reg_(enable_out_mem_to_reg_, zero_1_, reset_, mem_to_reg_reg_in_),
            mem_to_reg_reg_(mem_to_reg_reg_in_, clock_, mem_to_reg_out_)
        {
            zero_read_data_.write(logic::LogicState::LOW);
            zero_alu_result_.write(logic::LogicState::LOW);
            zero_rd_.write(logic::LogicState::LOW);
            zero_1_.write(logic::LogicState::LOW);

            set_controls(initial_controls);
        }

        // Support author's parameter order: (enable, clock, reset, ALUResult, readData, rd)
        MEM_WB(
            logic::Wire& enable,
            logic::Wire& clock,
            logic::Wire& reset,
            logic::Bus<DataWidth>& ALUResult,
            logic::Bus<DataWidth>& readData,
            logic::Bus<RegAddrWidth>& rd,
            const PipelinedControlSignals& initial_controls = PipelinedControlSignals{}
        ) : MEM_WB(readData, ALUResult, rd, clock, reset, enable, initial_controls)
        {
        }

        void set_controls(const PipelinedControlSignals& ctrl) noexcept {
            reg_write_in_.write_value(ctrl.regWrite ? 1 : 0);
            mem_to_reg_in_.write_value(ctrl.memToReg ? 1 : 0);
        }

        void set_reg_write(bool v) noexcept { reg_write_in_.write_value(v ? 1 : 0); }
        void set_mem_to_reg(bool v) noexcept { mem_to_reg_in_.write_value(v ? 1 : 0); }

        void evaluate() noexcept override {
            // Data path
            enable_mux_read_data_.evaluate();
            reset_mux_read_data_.evaluate();
            readData_reg_.evaluate();

            enable_mux_alu_result_.evaluate();
            reset_mux_alu_result_.evaluate();
            ALUResult_reg_.evaluate();

            enable_mux_rd_.evaluate();
            reset_mux_rd_.evaluate();
            rd_reg_.evaluate();

            // Control path
            enable_mux_reg_write_.evaluate();
            reset_mux_reg_write_.evaluate();
            reg_write_reg_.evaluate();

            enable_mux_mem_to_reg_.evaluate();
            reset_mux_mem_to_reg_.evaluate();
            mem_to_reg_reg_.evaluate();
        }

        // Data output accessors
        [[nodiscard]] logic::Bus<DataWidth>& read_data() noexcept { return readData_out_; }
        [[nodiscard]] const logic::Bus<DataWidth>& read_data() const noexcept { return readData_out_; }
        [[nodiscard]] const logic::Bus<DataWidth>& read_memory_data() const noexcept { return readData_out_; }

        [[nodiscard]] logic::Bus<DataWidth>& alu_result() noexcept { return ALUResult_out_; }
        [[nodiscard]] const logic::Bus<DataWidth>& alu_result() const noexcept { return ALUResult_out_; }
        [[nodiscard]] const logic::Bus<DataWidth>& read_alu_result() const noexcept { return ALUResult_out_; }

        [[nodiscard]] logic::Bus<RegAddrWidth>& rd() noexcept { return rd_out_; }
        [[nodiscard]] const logic::Bus<RegAddrWidth>& rd() const noexcept { return rd_out_; }

        // Control output accessors
        [[nodiscard]] bool reg_write() const noexcept { return reg_write_out_.read_value() != 0; }
        [[nodiscard]] bool mem_to_reg() const noexcept { return mem_to_reg_out_.read_value() != 0; }

        [[nodiscard]]
        PipelinedControlSignals controls() const noexcept {
            PipelinedControlSignals ctrl{};
            ctrl.regWrite = reg_write();
            ctrl.memToReg = mem_to_reg();
            return ctrl;
        }

    private:
        // Inputs
        logic::Bus<DataWidth>& readData_;
        logic::Bus<DataWidth>& ALUResult_;
        logic::Bus<RegAddrWidth>& rd_;

        // Signals
        logic::Wire& clock_;
        logic::Wire& reset_;
        logic::Wire& enable_;

        // Control inputs (registered internally)
        logic::Bus<1> reg_write_in_;
        logic::Bus<1> mem_to_reg_in_;

        // Outputs
        logic::Bus<DataWidth> readData_out_;
        logic::Bus<DataWidth> ALUResult_out_;
        logic::Bus<RegAddrWidth> rd_out_;

        logic::Bus<1> reg_write_out_;
        logic::Bus<1> mem_to_reg_out_;

        // Internal zero buses
        logic::Bus<DataWidth> zero_read_data_;
        logic::Bus<DataWidth> zero_alu_result_;
        logic::Bus<RegAddrWidth> zero_rd_;
        logic::Bus<1> zero_1_;

        // Internal intermediate buses
        logic::Bus<DataWidth> enable_out_read_data_;
        logic::Bus<DataWidth> enable_out_alu_result_;
        logic::Bus<RegAddrWidth> enable_out_rd_;

        logic::Bus<1> enable_out_reg_write_;
        logic::Bus<1> enable_out_mem_to_reg_;

        logic::Bus<DataWidth> read_data_reg_in_;
        logic::Bus<DataWidth> alu_result_reg_in_;
        logic::Bus<RegAddrWidth> rd_reg_in_;

        logic::Bus<1> reg_write_reg_in_;
        logic::Bus<1> mem_to_reg_reg_in_;

        // Data Multiplexers
        logic::Mux<DataWidth> enable_mux_read_data_;
        logic::Mux<DataWidth> reset_mux_read_data_;

        logic::Mux<DataWidth> enable_mux_alu_result_;
        logic::Mux<DataWidth> reset_mux_alu_result_;

        logic::Mux<RegAddrWidth> enable_mux_rd_;
        logic::Mux<RegAddrWidth> reset_mux_rd_;

        // Control Multiplexers
        logic::Mux<1> enable_mux_reg_write_;
        logic::Mux<1> reset_mux_reg_write_;

        logic::Mux<1> enable_mux_mem_to_reg_;
        logic::Mux<1> reset_mux_mem_to_reg_;

        // Pipeline registers
        logic::Register<DataWidth> readData_reg_;
        logic::Register<DataWidth> ALUResult_reg_;
        logic::Register<RegAddrWidth> rd_reg_;

        logic::Register<1> reg_write_reg_;
        logic::Register<1> mem_to_reg_reg_;
    };

} // namespace cpu
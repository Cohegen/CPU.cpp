#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/signals/clock.hpp>
#include <logic/signals/logicState.hpp>
#include <logic/simulator/Component.hpp>

#include "../core/memory/VectorRegisterFile.hpp"
#include "../core/execution/VectorExecutionUnit.hpp"
#include "../core/configuration/VectorLaneMaskUnit.hpp"
#include "../core/configuration/VectorMaskedWBUnit.hpp"

namespace cpu {

    class VectorDatapath : public logic::Component
    {
    public:
        // Constructor using logic::Wire for clock
        VectorDatapath(
            logic::Bus<5>& rs1,
            logic::Bus<5>& rs2,
            logic::Bus<5>& rd,
            logic::Bus<128>& write_data,
            logic::Bus<5>& write_addr,
            logic::Bus<3>& opcode,
            logic::Bus<8>& vl,
            logic::Wire& clock,
            logic::Wire& enable,
            logic::Wire& reset,
            logic::Bus<128>& result
        ) :
            rs1_(rs1),
            rs2_(rs2),
            rd_(rd),
            write_data_(write_data),
            write_addr_(write_addr),
            opcode_(opcode),
            vl_(vl),
            clock_wire_ref_(clock),
            enable_(enable),
            reset_(reset),
            result_(result),
            vector_reg_file_(rs1_, rs2_, rd_, rf_write_addr_, rf_write_data_, enable_, reset_, clock_wire_ref_),
            vec_execution_unit_(vector_reg_file_.read_data1(), vector_reg_file_.read_data2(), opcode_, execution_result_),
            vec_masked_unit_(vl_, lane_mask_),
            vec_masked_writeback_unit_(execution_result_, vector_reg_file_.read_data3(), lane_mask_, writeback_result_)
        {
        }

        // Overloaded constructor using logic::Clock
        VectorDatapath(
            logic::Bus<5>& rs1,
            logic::Bus<5>& rs2,
            logic::Bus<5>& rd,
            logic::Bus<128>& write_data,
            logic::Bus<5>& write_addr,
            logic::Bus<3>& opcode,
            logic::Bus<8>& vl,
            logic::Clock& clock,
            logic::Wire& enable,
            logic::Wire& reset,
            logic::Bus<128>& result
        ) :
            rs1_(rs1),
            rs2_(rs2),
            rd_(rd),
            write_data_(write_data),
            write_addr_(write_addr),
            opcode_(opcode),
            vl_(vl),
            clock_obj_ptr_(&clock),
            clock_wire_ref_(internal_clock_wire_),
            enable_(enable),
            reset_(reset),
            result_(result),
            vector_reg_file_(rs1_, rs2_, rd_, rf_write_addr_, rf_write_data_, enable_, reset_, clock_wire_ref_),
            vec_execution_unit_(vector_reg_file_.read_data1(), vector_reg_file_.read_data2(), opcode_, execution_result_),
            vec_masked_unit_(vl_, lane_mask_),
            vec_masked_writeback_unit_(execution_result_, vector_reg_file_.read_data3(), lane_mask_, writeback_result_)
        {
        }

        void evaluate() noexcept override
        {
            if (clock_obj_ptr_ != nullptr)
            {
                clock_wire_ref_.write(clock_obj_ptr_->state());
            }

            // 1. Evaluate vector register file reads
            vector_reg_file_.evaluate_read();

            // 2. Evaluate vector execution unit
            vec_execution_unit_.evaluate();

            // 3. Evaluate vector lane mask unit
            vec_masked_unit_.evaluate();

            // 4. Evaluate vector masked writeback unit
            vec_masked_writeback_unit_.evaluate();

            // 5. Propagate writeback result to datapath output
            for (std::size_t i = 0; i < 128; ++i)
            {
                result_[i].write(writeback_result_[i].read());
            }

            // 6. Drive register file write ports
            if (use_external_write_)
            {
                for (std::size_t i = 0; i < 5; ++i)
                {
                    rf_write_addr_[i].write(write_addr_[i].read());
                }
                for (std::size_t i = 0; i < 128; ++i)
                {
                    rf_write_data_[i].write(write_data_[i].read());
                }
            }
            else
            {
                // Internal instruction writeback path: destination is rd_, data is writeback_result_
                for (std::size_t i = 0; i < 5; ++i)
                {
                    rf_write_addr_[i].write(rd_[i].read());
                }
                for (std::size_t i = 0; i < 128; ++i)
                {
                    rf_write_data_[i].write(writeback_result_[i].read());
                }
            }

            // 7. Update register file with write inputs
            vector_reg_file_.evaluate_write();

            // 8. Re-evaluate reads to reflect any newly committed register state
            vector_reg_file_.evaluate_read();
        }

        // Control external write vs internal writeback mode
        void set_external_write(bool enable) noexcept
        {
            use_external_write_ = enable;
        }

        [[nodiscard]] bool is_external_write() const noexcept
        {
            return use_external_write_;
        }

        // Helper to initialize a vector register (useful for tests)
        void initialize_register(std::size_t reg_index, const std::array<std::uint32_t, 4>& lanes)
        {
            const bool prev_mode = use_external_write_;
            use_external_write_ = true;

            write_addr_.write_value(reg_index);
            for (std::size_t lane = 0; lane < 4; ++lane)
            {
                const std::size_t base = lane * 32;
                for (std::size_t bit = 0; bit < 32; ++bit)
                {
                    write_data_[base + bit].write(
                        ((lanes[lane] >> bit) & 1U) ? logic::LogicState::HIGH : logic::LogicState::LOW
                    );
                }
            }
            enable_.write(logic::LogicState::HIGH);
            clock_wire_ref_.write(logic::LogicState::LOW);
            evaluate();
            clock_wire_ref_.write(logic::LogicState::HIGH);
            evaluate();
            clock_wire_ref_.write(logic::LogicState::LOW);
            evaluate();
            enable_.write(logic::LogicState::LOW);

            use_external_write_ = prev_mode;
            evaluate();
        }

        // Inspection / verification accessors
        [[nodiscard]] const logic::Bus<128>& read_data1() const noexcept
        {
            return vector_reg_file_.read_data1();
        }

        [[nodiscard]] const logic::Bus<128>& read_data2() const noexcept
        {
            return vector_reg_file_.read_data2();
        }

        [[nodiscard]] const logic::Bus<128>& read_data3() const noexcept
        {
            return vector_reg_file_.read_data3();
        }

        [[nodiscard]] const logic::Bus<128>& execution_result() const noexcept
        {
            return execution_result_;
        }

        [[nodiscard]] const logic::Bus<4>& lane_mask() const noexcept
        {
            return lane_mask_;
        }

        [[nodiscard]] const logic::Bus<128>& writeback_result() const noexcept
        {
            return writeback_result_;
        }

        [[nodiscard]] const logic::Bus<128>& result() const noexcept
        {
            return result_;
        }

        [[nodiscard]] VectorRegisterFile<32, 128>& register_file() noexcept
        {
            return vector_reg_file_;
        }

        [[nodiscard]] const VectorRegisterFile<32, 128>& register_file() const noexcept
        {
            return vector_reg_file_;
        }

        // Static lane conversion helpers
        static std::uint32_t read_lane(const logic::Bus<128>& bus, std::size_t lane)
        {
            const std::size_t base = lane * 32;
            std::uint32_t val = 0;
            for (std::size_t bit = 0; bit < 32; ++bit)
            {
                if (bus[base + bit].read() == logic::LogicState::HIGH)
                {
                    val |= (static_cast<std::uint32_t>(1) << bit);
                }
            }
            return val;
        }

        static std::array<std::uint32_t, 4> read_vector(const logic::Bus<128>& bus)
        {
            return {read_lane(bus, 0), read_lane(bus, 1), read_lane(bus, 2), read_lane(bus, 3)};
        }

        static std::array<std::uint8_t, 4> read_mask(const logic::Bus<4>& mask_bus)
        {
            return {
                static_cast<std::uint8_t>(mask_bus[0].read() == logic::LogicState::HIGH ? 1 : 0),
                static_cast<std::uint8_t>(mask_bus[1].read() == logic::LogicState::HIGH ? 1 : 0),
                static_cast<std::uint8_t>(mask_bus[2].read() == logic::LogicState::HIGH ? 1 : 0),
                static_cast<std::uint8_t>(mask_bus[3].read() == logic::LogicState::HIGH ? 1 : 0)
            };
        }

    private:
        // External input references
        logic::Bus<5>& rs1_;
        logic::Bus<5>& rs2_;
        logic::Bus<5>& rd_;
        logic::Bus<128>& write_data_;
        logic::Bus<5>& write_addr_;
        logic::Bus<3>& opcode_;
        logic::Bus<8>& vl_;
        logic::Clock* clock_obj_ptr_{nullptr};
        logic::Wire internal_clock_wire_{logic::LogicState::LOW};
        logic::Wire& clock_wire_ref_;
        logic::Wire& enable_;
        logic::Wire& reset_;

        // Output reference
        logic::Bus<128>& result_;

        // Mode: false = internal instruction writeback (rd, writeback_result), true = external write
        bool use_external_write_{false};

        // Internal buses for register file write port
        logic::Bus<5> rf_write_addr_;
        logic::Bus<128> rf_write_data_;

        // Internal datapath buses
        logic::Bus<128> execution_result_;
        logic::Bus<4> lane_mask_;
        logic::Bus<128> writeback_result_;

        // Internal hardware components
        VectorRegisterFile<32, 128> vector_reg_file_;
        VectorExecutionUnit<128, 4, 32> vec_execution_unit_;
        VectorLaneMaskUnit vec_masked_unit_;
        VectorMaskedWriteBackUnit vec_masked_writeback_unit_;
    };

} // namespace cpu

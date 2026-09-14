#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <logic/signals/clock.hpp>
#include <logic/signals/wire.hpp>
#include "../single_cycle_cpu/datapath/SingleCycleDatapath.hpp"

namespace cpu {

template<
    std::size_t AddressWidth = 32,
    std::size_t DataWidth = 32,
    std::size_t InstructionWidth = 32,
    std::size_t RegisterAddressWidth = 4,
    std::size_t InstructionMemoryAddressWidth = 8,
    std::size_t DataMemoryAddressWidth = 8
>
class SingleCycleCPU {
public:
    SingleCycleCPU() : datapath_(clock_, reset_) {}

    void reset() noexcept {
        reset_.write(logic::LogicState::HIGH);
        datapath_.evaluate();
        reset_.write(logic::LogicState::LOW);
        datapath_.evaluate();
        halted_ = false;
    }

    void step() noexcept {
        if (halted_) {
            return;
        }
        datapath_.evaluate();

        // rising edge
        clock_.tick();
        datapath_.evaluate();

        // falling edge (complete 1 cycle)
        clock_.tick();
        datapath_.evaluate();

        // checking whether the instruction that has been executed was a halt
        if (datapath_.control_signals().halt) {
            halted_ = true;
        }
    }

    void run(std::size_t max_cycles = 100000) noexcept {
        std::size_t c = 0;
        while (!halted_ && c < max_cycles) {
            step();
            ++c;
        }
    }

    [[nodiscard]]
    bool halted() const noexcept {
        return halted_;
    }

    void load_program(const std::vector<std::size_t>& instructions) noexcept {
        datapath_.load_instructions(instructions);
    }

    void load_program(const std::vector<std::uint32_t>& instructions) noexcept {
        std::vector<std::size_t> widened(instructions.begin(), instructions.end());
        datapath_.load_instructions(widened);
    }

    [[nodiscard]]
    const SingleCycleDatapath<
        AddressWidth,
        DataWidth,
        InstructionWidth,
        RegisterAddressWidth,
        InstructionMemoryAddressWidth,
        DataMemoryAddressWidth
    >& datapath() const noexcept {
        return datapath_;
    }

    [[nodiscard]]
    SingleCycleDatapath<
        AddressWidth,
        DataWidth,
        InstructionWidth,
        RegisterAddressWidth,
        InstructionMemoryAddressWidth,
        DataMemoryAddressWidth
    >& datapath() noexcept {
        return datapath_;
    }

    [[nodiscard]]
    std::uint32_t read_register(Register reg) noexcept {
        return datapath_.read_register_for_test(reg);
    }

    void write_register(Register reg, std::uint32_t value) noexcept {
        datapath_.write_register_for_test(reg, value);
    }

    [[nodiscard]]
    std::uint32_t pc() const noexcept {
        return static_cast<std::uint32_t>(datapath_.pc().read_value());
    }

    [[nodiscard]]
    std::uint64_t cycles() const noexcept {
        return clock_.cycle() / 2;
    }

private:
    logic::Clock clock_;
    logic::Wire reset_;

    bool halted_{false};
    SingleCycleDatapath<
        AddressWidth,
        DataWidth,
        InstructionWidth,
        RegisterAddressWidth,
        InstructionMemoryAddressWidth,
        DataMemoryAddressWidth
    > datapath_;
};

} // namespace cpu

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <logic/signals/clock.hpp>
#include <logic/signals/wire.hpp>
#include "../multi_cycle_cpu/datapath/MultiCycleDatapath.hpp"

namespace cpu {

template<
    std::size_t AddressWidth = 32,
    std::size_t DataWidth = 32,
    std::size_t InstructionWidth = 32,
    std::size_t RegisterAddressWidth = 4,
    std::size_t MemoryAddressWidth = 8
>
class MultiCycleCPU {
public:
    MultiCycleCPU() : datapath_(clock_, reset_) {}

    void reset() noexcept {
        datapath_.reset();
    }

    /// Advances simulation by one clock cycle using autonomous FSM control.
    void step() noexcept {
        if (halted()) {
            return;
        }
        datapath_.step_cycle();
    }

    /// Executes cycles autonomously until an instruction finishes (or halts).
    void step_instruction() noexcept {
        if (halted()) {
            return;
        }
        datapath_.step_instruction();
    }

    void run(std::size_t max_cycles = 100000) noexcept {
        datapath_.run(max_cycles);
    }

    [[nodiscard]]
    bool halted() const noexcept {
        return datapath_.halted();
    }

    void load_program(const std::vector<std::size_t>& instructions) noexcept {
        datapath_.load_instructions(instructions);
    }

    void load_program(const std::vector<std::uint32_t>& instructions) noexcept {
        std::vector<std::size_t> widened(instructions.begin(), instructions.end());
        datapath_.load_instructions(widened);
    }

    [[nodiscard]]
    const MultiCycleDatapath<
        AddressWidth,
        DataWidth,
        InstructionWidth,
        RegisterAddressWidth,
        MemoryAddressWidth
    >& datapath() const noexcept {
        return datapath_;
    }

    [[nodiscard]]
    MultiCycleDatapath<
        AddressWidth,
        DataWidth,
        InstructionWidth,
        RegisterAddressWidth,
        MemoryAddressWidth
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
    std::uint32_t read_memory(std::size_t address) noexcept {
        return datapath_.read_memory_for_test(address);
    }

    void write_memory(std::size_t address, std::uint32_t value) noexcept {
        datapath_.write_memory_for_test(address, value);
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

    MultiCycleDatapath<
        AddressWidth,
        DataWidth,
        InstructionWidth,
        RegisterAddressWidth,
        MemoryAddressWidth
    > datapath_;
};

} // namespace cpu

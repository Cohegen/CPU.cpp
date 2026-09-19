#pragma once

#include <array>
#include <cstddef>

#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/simulator/Component.hpp>

namespace cpu {

template <
    std::size_t CPUAddressWidth = 32,
    std::size_t InstructionWidth = 32,
    std::size_t NumLines = 16,
    std::size_t InstructionsPerLine = 4
>
class InstructionCacheLine : public logic::Component {
    static_assert(CPUAddressWidth > 0, "CPUAddressWidth must be greater than zero");
    static_assert(InstructionWidth > 0, "InstructionWidth must be greater than zero");
    static_assert(InstructionWidth % 8 == 0, "InstructionWidth must be a multiple of 8");
    static_assert(NumLines > 0, "NumLines must be greater than zero");
    static_assert(InstructionsPerLine > 0, "InstructionsPerLine must be greater than zero");
    static_assert((NumLines & (NumLines - 1)) == 0, "NumLines must be a power of two");
    static_assert(
        (InstructionsPerLine & (InstructionsPerLine - 1)) == 0,
        "InstructionsPerLine must be a power of two"
    );

public:
    static constexpr std::size_t InstructionBytes = InstructionWidth / 8;

    static constexpr std::size_t OffsetBits = []() {
        std::size_t value = InstructionBytes * InstructionsPerLine;
        std::size_t bits = 0;
        while (value > 1) {
            value >>= 1;
            ++bits;
        }
        return bits;
    }();

    static constexpr std::size_t IndexBits = []() {
        std::size_t value = NumLines;
        std::size_t bits = 0;
        while (value > 1) {
            value >>= 1;
            ++bits;
        }
        return bits;
    }();

    static constexpr std::size_t TagBits = CPUAddressWidth - OffsetBits - IndexBits;

    struct CacheLine {
        bool valid = false;
        std::size_t tag = 0;
        std::array<std::size_t, InstructionsPerLine> instructions{};
    };

    InstructionCacheLine(
        logic::Wire& enable,
        logic::Bus<CPUAddressWidth>& address0,
        logic::Bus<CPUAddressWidth>& address1,
        logic::Bus<InstructionWidth>& instruction0,
        logic::Bus<InstructionWidth>& instruction1,
        logic::Wire& hit0,
        logic::Wire& hit1
    )
        : enable_(enable),
          address0_(address0),
          address1_(address1),
          instruction0_(instruction0),
          instruction1_(instruction1),
          hit0_(hit0),
          hit1_(hit1)
    {}

    void evaluate() noexcept override {
        if (enable_.read() != logic::LogicState::HIGH) {
            hit0_.write(logic::LogicState::LOW);
            hit1_.write(logic::LogicState::LOW);
            clear_output(instruction0_);
            clear_output(instruction1_);
            return;
        }

        evaluate_port(address0_, instruction0_, hit0_);
        evaluate_port(address1_, instruction1_, hit1_);
    }

    void install_line(
        std::size_t address,
        const std::array<std::size_t, InstructionsPerLine>& instructions
    ) noexcept {
        CacheLine& line = lines_[get_line_index(address)];
        line.valid = true;
        line.tag = get_tag(address);
        line.instructions = instructions;
    }

    void invalidate() noexcept {
        for (auto& line : lines_) {
            line.valid = false;
        }
    }

    [[nodiscard]] static std::size_t get_line_index(std::size_t address) noexcept {
        return (address >> OffsetBits) & (NumLines - 1);
    }

    [[nodiscard]] static std::size_t get_tag(std::size_t address) noexcept {
        return address >> (OffsetBits + IndexBits);
    }

    [[nodiscard]] static std::size_t get_instruction_offset(std::size_t address) noexcept {
        const std::size_t byte_offset = address & ((InstructionBytes * InstructionsPerLine) - 1);
        return byte_offset / InstructionBytes;
    }

private:
    logic::Wire& enable_;
    logic::Bus<CPUAddressWidth>& address0_;
    logic::Bus<CPUAddressWidth>& address1_;
    logic::Bus<InstructionWidth>& instruction0_;
    logic::Bus<InstructionWidth>& instruction1_;
    logic::Wire& hit0_;
    logic::Wire& hit1_;
    std::array<CacheLine, NumLines> lines_{};

    [[nodiscard]]
    static std::size_t get_address_value(const logic::Bus<CPUAddressWidth>& address) noexcept {
        std::size_t value = 0;
        for (std::size_t i = 0; i < CPUAddressWidth; ++i) {
            if (address[i].read() == logic::LogicState::HIGH) {
                value |= (1ULL << i);
            }
        }
        return value;
    }

    void evaluate_port(
        const logic::Bus<CPUAddressWidth>& address,
        logic::Bus<InstructionWidth>& instruction,
        logic::Wire& hit
    ) noexcept {
        const std::size_t address_value = get_address_value(address);
        const std::size_t line_index = get_line_index(address_value);
        const std::size_t tag = get_tag(address_value);
        const std::size_t instruction_offset = get_instruction_offset(address_value);

        const CacheLine& line = lines_[line_index];
        const bool cache_hit = line.valid && line.tag == tag;

        hit.write(cache_hit ? logic::LogicState::HIGH : logic::LogicState::LOW);

        if (cache_hit) {
            write_output(instruction, line.instructions[instruction_offset]);
        } else {
            clear_output(instruction);
        }
    }

    static void write_output(logic::Bus<InstructionWidth>& output, std::size_t value) noexcept {
        for (std::size_t i = 0; i < InstructionWidth; ++i) {
            output[i].write(
                (value & (1ULL << i)) ? logic::LogicState::HIGH : logic::LogicState::LOW
            );
        }
    }

    static void clear_output(logic::Bus<InstructionWidth>& output) noexcept {
        for (std::size_t i = 0; i < InstructionWidth; ++i) {
            output[i].write(logic::LogicState::LOW);
        }
    }
};

} // namespace cpu

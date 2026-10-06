#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace cpu
{

enum class BranchPredictionState : std::uint8_t
{
    STRONG_NOT_TAKEN = 0,
    WEAK_NOT_TAKEN   = 1,
    WEAK_TAKEN       = 2,
    STRONG_TAKEN     = 3
};

struct BranchPrediction
{
    bool taken{false};
    std::uint32_t target{0};
    bool btb_hit{false};
};

template <std::size_t TableSize = 256>
class BranchPredictor
{
public:
    static_assert(TableSize > 0, "TableSize must be greater than zero.");

    explicit BranchPredictor(BranchPredictionState initial_state = BranchPredictionState::WEAK_NOT_TAKEN) noexcept
    {
        reset(initial_state);
    }

    void reset(BranchPredictionState initial_state = BranchPredictionState::WEAK_NOT_TAKEN) noexcept
    {
        for (std::size_t i = 0; i < TableSize; ++i)
        {
            counters_[i] = static_cast<std::uint8_t>(initial_state);
            btb_[i] = BTBEntry{};
        }
    }

    [[nodiscard]]
    BranchPrediction predict(std::uint32_t pc) const noexcept
    {
        const std::size_t index = hash(pc);
        const bool taken = counters_[index] >= 2;
        const auto& btb_entry = btb_[index];
        const bool hit = btb_entry.valid && (btb_entry.tag == pc);

        return BranchPrediction{
            taken,
            hit ? btb_entry.target : 0U,
            hit
        };
    }

    [[nodiscard]]
    bool predict_taken(std::uint32_t pc) const noexcept
    {
        const std::size_t index = hash(pc);
        return counters_[index] >= 2;
    }

    void update(std::uint32_t pc, bool actual_taken, std::uint32_t actual_target) noexcept
    {
        const std::size_t index = hash(pc);
        std::uint8_t& counter = counters_[index];

        if (actual_taken)
        {
            if (counter < 3)
            {
                ++counter;
            }
            btb_[index].valid = true;
            btb_[index].tag = pc;
            btb_[index].target = actual_target;
        }
        else
        {
            if (counter > 0)
            {
                --counter;
            }
        }
    }

    [[nodiscard]]
    BranchPredictionState state(std::uint32_t pc) const noexcept
    {
        const std::size_t index = hash(pc);
        return static_cast<BranchPredictionState>(counters_[index]);
    }

    void set_state(std::uint32_t pc, BranchPredictionState new_state) noexcept
    {
        const std::size_t index = hash(pc);
        counters_[index] = static_cast<std::uint8_t>(new_state);
    }

private:
    struct BTBEntry
    {
        bool valid{false};
        std::uint32_t tag{0};
        std::uint32_t target{0};
    };

    [[nodiscard]]
    static constexpr std::size_t hash(std::uint32_t pc) noexcept
    {
        return (pc >> 2) % TableSize;
    }

    std::array<std::uint8_t, TableSize> counters_{};
    std::array<BTBEntry, TableSize> btb_{};
};

} // namespace cpu

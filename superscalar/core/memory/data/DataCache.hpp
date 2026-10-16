#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include "DataMemory.hpp"

namespace cpu
{

template <
    std::size_t NumLines = 16,
    std::size_t WordsPerLine = 4,
    std::size_t DataMemoryWords = 1024
>
class DataCache
{
    static_assert((NumLines & (NumLines - 1)) == 0, "NumLines must be a power of two.");
    static_assert((WordsPerLine & (WordsPerLine - 1)) == 0, "WordsPerLine must be a power of two.");

public:
    struct Line
    {
        bool valid{false};
        bool dirty{false};
        std::uint32_t tag{0};
        std::array<std::uint32_t, WordsPerLine> words{};
    };

    struct Stats
    {
        std::size_t reads{0};
        std::size_t read_hits{0};
        std::size_t read_misses{0};
        std::size_t writes{0};
        std::size_t write_hits{0};
        std::size_t write_misses{0};
        std::size_t evictions{0};
        std::size_t writebacks{0};

        [[nodiscard]]
        constexpr std::size_t total_accesses() const noexcept
        {
            return reads + writes;
        }

        [[nodiscard]]
        constexpr std::size_t total_hits() const noexcept
        {
            return read_hits + write_hits;
        }

        [[nodiscard]]
        constexpr std::size_t total_misses() const noexcept
        {
            return read_misses + write_misses;
        }

        [[nodiscard]]
        double hit_rate() const noexcept
        {
            const auto total = total_accesses();
            return total > 0 ? (100.0 * static_cast<double>(total_hits()) / static_cast<double>(total)) : 100.0;
        }

        [[nodiscard]]
        double miss_rate() const noexcept
        {
            return 100.0 - hit_rate();
        }
    };

    explicit DataCache(SuperScalarDataMemory<DataMemoryWords>& backing_memory) noexcept
        : memory_(backing_memory)
    {
        reset();
    }

    void reset() noexcept
    {
        for (auto& line : lines_)
        {
            line.valid = false;
            line.dirty = false;
            line.tag = 0;
            line.words.fill(0);
        }
        stats_ = Stats{};
    }

    // Read word from byte address
    [[nodiscard]]
    std::optional<std::uint32_t> read(std::uint32_t byte_address) noexcept
    {
        const std::size_t word_addr = byte_address / 4;
        if (word_addr >= DataMemoryWords)
        {
            return std::nullopt;
        }

        ++stats_.reads;
        const std::size_t line_offset = word_addr % WordsPerLine;
        const std::size_t line_index = (word_addr / WordsPerLine) % NumLines;
        const std::uint32_t tag = static_cast<std::uint32_t>(word_addr / (WordsPerLine * NumLines));

        auto& line = lines_[line_index];
        if (line.valid && line.tag == tag)
        {
            ++stats_.read_hits;
            return line.words[line_offset];
        }

        // Cache miss: refill line from backing memory
        ++stats_.read_misses;
        refill_line(line_index, word_addr, tag);
        return line.words[line_offset];
    }

    // Write word to byte address (Write-back, write-allocate)
    bool write(std::uint32_t byte_address, std::uint32_t data) noexcept
    {
        const std::size_t word_addr = byte_address / 4;
        if (word_addr >= DataMemoryWords)
        {
            return false;
        }

        ++stats_.writes;
        const std::size_t line_offset = word_addr % WordsPerLine;
        const std::size_t line_index = (word_addr / WordsPerLine) % NumLines;
        const std::uint32_t tag = static_cast<std::uint32_t>(word_addr / (WordsPerLine * NumLines));

        auto& line = lines_[line_index];
        if (line.valid && line.tag == tag)
        {
            ++stats_.write_hits;
            line.words[line_offset] = data;
            line.dirty = true;
            return true;
        }

        // Cache write miss: allocate line and mark dirty
        ++stats_.write_misses;
        refill_line(line_index, word_addr, tag);
        line.words[line_offset] = data;
        line.dirty = true;
        return true;
    }

    // Flush all dirty lines to backing memory
    void flush() noexcept
    {
        for (std::size_t i = 0; i < NumLines; ++i)
        {
            auto& line = lines_[i];
            if (line.valid && line.dirty)
            {
                writeback_line(i);
            }
        }
    }

    [[nodiscard]]
    const Stats& stats() const noexcept
    {
        return stats_;
    }

    [[nodiscard]]
    const Line& line(std::size_t index) const noexcept
    {
        return lines_[index];
    }

    [[nodiscard]]
    constexpr std::size_t capacity_bytes() const noexcept
    {
        return NumLines * WordsPerLine * 4;
    }

private:
    void writeback_line(std::size_t line_index) noexcept
    {
        auto& line = lines_[line_index];
        const std::size_t base_word = (static_cast<std::size_t>(line.tag) * NumLines + line_index) * WordsPerLine;
        for (std::size_t w = 0; w < WordsPerLine; ++w)
        {
            if (base_word + w < DataMemoryWords)
            {
                memory_.write_word(base_word + w, line.words[w]);
            }
        }
        line.dirty = false;
        ++stats_.writebacks;
    }

    void refill_line(std::size_t line_index, std::size_t word_addr, std::uint32_t new_tag) noexcept
    {
        auto& line = lines_[line_index];
        if (line.valid)
        {
            ++stats_.evictions;
            if (line.dirty)
            {
                writeback_line(line_index);
            }
        }

        const std::size_t base_word = (word_addr / WordsPerLine) * WordsPerLine;
        for (std::size_t w = 0; w < WordsPerLine; ++w)
        {
            line.words[w] = (base_word + w < DataMemoryWords) ? memory_.read_word(base_word + w) : 0U;
        }
        line.valid = true;
        line.dirty = false;
        line.tag = new_tag;
    }

    SuperScalarDataMemory<DataMemoryWords>& memory_;
    std::array<Line, NumLines> lines_{};
    Stats stats_{};
};

} // namespace cpu

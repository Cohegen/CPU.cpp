#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace cpu
{
    template<std::size_t SizeWords = 1024>
    class DataMemory
    {
    public:
        DataMemory() noexcept
        {
            reset();
        }

        // Read a word from memory at the given byte address
        // Byte address is divided by 4 to get word index
        [[nodiscard]]
        std::optional<std::uint32_t> read(std::uint32_t byte_address) const noexcept
        {
            const std::size_t word_index = byte_address / 4;
            if (word_index >= SizeWords)
            {
                return std::nullopt;
            }
            return memory_[word_index];
        }

        // Write a word to memory at the given byte address
        bool write(std::uint32_t byte_address, std::uint32_t data) noexcept
        {
            const std::size_t word_index = byte_address / 4;
            if (word_index >= SizeWords)
            {
                return false;
            }
            memory_[word_index] = data;
            return true;
        }

        // Direct word-index access for testing
        [[nodiscard]]
        std::uint32_t read_word(std::size_t word_index) const noexcept
        {
            if (word_index >= SizeWords)
            {
                return 0;
            }
            return memory_[word_index];
        }

        void write_word(std::size_t word_index, std::uint32_t data) noexcept
        {
            if (word_index < SizeWords)
            {
                memory_[word_index] = data;
            }
        }

        // Reset all memory to zero
        void reset() noexcept
        {
            memory_.fill(0);
        }

        [[nodiscard]]
        constexpr std::size_t size() const noexcept
        {
            return SizeWords;
        }

    private:
        std::array<std::uint32_t, SizeWords> memory_{};
    };
}

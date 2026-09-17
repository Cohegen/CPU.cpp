#pragma once

#include <cstdint>

namespace cpu
{
    class FetchBundle
    {
    public:

        FetchBundle();

        FetchBundle(
            uint32_t pc0,
            uint32_t instruction0,
            bool valid0,
            uint32_t pc1,
            uint32_t instruction1,
            bool valid1
        );

        uint32_t getPC0() const
        {
            return pc0;
        }

        uint32_t getPC1() const
        {
            return pc1;
        }

        uint32_t getInstruction0() const
        {
            return instruction0;
        }

        uint32_t getInstruction1() const
        {
            return instruction1;
        }

        bool isValid0() const
        {
            return valid0;
        }

        bool isValid1() const
        {
            return valid1;
        }

    private:

        uint32_t pc0;
        uint32_t instruction0;
        bool valid0;

        uint32_t pc1;
        uint32_t instruction1;
        bool valid1;
    };
}

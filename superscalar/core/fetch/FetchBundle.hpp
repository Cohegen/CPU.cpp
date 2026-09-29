#pragma once

#include <cstdint>

namespace cpu
{
    class FetchBundle
    {
    public:

        FetchBundle()
            : pc0(0),
              instruction0(0),
              valid0(false),
              pc1(0),
              instruction1(0),
              valid1(false)
        {
        }

        FetchBundle(
            uint32_t pc0,
            uint32_t instruction0,
            bool valid0,
            uint32_t pc1,
            uint32_t instruction1,
            bool valid1
        )
            : pc0(pc0),
              instruction0(instruction0),
              valid0(valid0),
              pc1(pc1),
              instruction1(instruction1),
              valid1(valid1)
        {
        }

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

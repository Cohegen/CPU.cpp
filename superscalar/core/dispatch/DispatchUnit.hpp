#pragma once
#include <cstddef>
#include "../decode/RenameBundle.hpp"
#include "ReOrderBuffer.hpp"

namespace cpu
{
    class DispatchUnit
    {
        public:
         using ROBIndex = std::size_t;

         explicit DispatchUnit(ReOrderBuffer<>&rob):rob_(rob)
         {

         }

         //2-wide dispatch
         [[nodiscard]]
         bool dispatch(
            const RenameBundle& input0,
            const RenameBundle& input1,
            ROBIndex& rob_index0,
            ROBIndex& rob_index1
         )noexcept
         {
            const std::size_t  valid_count = static_cast<std::size_t>(input0.valid) + static_cast<std::size_t>(input1.valid);

            if(valid_count==0)
            {
                return 0;
            }

            //checking if there's enough room in ROB
            if(rob_.capacity()- rob_size() <valid_count)
            {
                return false;
            }

            //dispatch lane 0 first
            if(input0.valid)
            {
                if(!rob_.allocate(input0,rob_index0))
                {
                    return false;
                }
            }

            //dispatch lane 1
            if(input1.valid)
            {
                if(!rob_.allocate(input1,rob_index1))
                {
                    return false;
                }
            }
            return true;
         }

         //single instruction dispatch
         [[nodiscard]]
         bool dispatch(const RenameBundle& input,ROBIndex& rob_index)noexcept
         {
            if(!input.valid)
            {
                return true;
            }
            return rob_.allocate(input,rob_index);
         }

        private:
          ReOrderBuffer<>& rob_;
    };
}
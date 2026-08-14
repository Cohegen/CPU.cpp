/*
Defining the operations which our CPU will be doing
*/

#pragma once
#include <cstdint>
 
namespace cpu{
    enum class Opcode : std::uint8_t{
        //No operation opcode
        NOP  = 0x00,

       //Arithmetic opcodes 
       ADD  = 0x01,
       SUB  = 0x02,

       //Logical opcodes
       AND  = 0x03,
       OR   = 0x04,
       XOR  = 0x05,
       NOT  = 0x06,

       //Immediate opcodes
       LI   = 0x07,
       ADDI = 0x08,

       //memory opcodes
       LW   = 0x09,
       SW   = 0x0A,

      //control flow
      BEQ  = 0x0B,
      BNE  = 0x0C,
      J    = 0x0D,
      
      //stops the CPU from executing instructions
      HALT = 0x0E
    };
}
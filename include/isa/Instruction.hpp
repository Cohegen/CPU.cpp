/*
Defining an Instruction class which represents a raw 32 bit instruction
*/

#pragma once
#include <cstdint>

#include "Opcode.hpp"
#include "Registers.hpp"
#include "logic/signals/bus.hpp"

namespace cpu {
    class Instruction {
        public:
          using Word = std::uint32_t;

          explicit Instruction(Word raw) : raw_(raw) {}

          [[nodiscard]]
          Word raw() const noexcept {
            return raw_;
          }

          /*
          Determines opcode using hardware Bus slicing
          */
          [[nodiscard]]
          Opcode opcode() const noexcept {
            logic::Bus<32> inst_bus;
            inst_bus.write_value(raw_);

            logic::Bus<6> opcode_bus;
            for (std::size_t i = 0; i < 6; ++i) {
                opcode_bus[i].write(inst_bus[26 + i].read());
            }

            return static_cast<Opcode>(opcode_bus.read_value());
          }

          /*
          Determines destination register rd using hardware Bus slicing
          */
          [[nodiscard]]
          Register rd() const noexcept {
            logic::Bus<32> inst_bus;
            inst_bus.write_value(raw_);

            logic::Bus<4> rd_bus;
            for (std::size_t i = 0; i < 4; ++i) {
                rd_bus[i].write(inst_bus[22 + i].read());
            }

            return static_cast<Register>(rd_bus.read_value());
          }

          /*
          Determines source register 1 rs1 using hardware Bus slicing
          */
          [[nodiscard]]
          Register rs1() const noexcept {
            logic::Bus<32> inst_bus;
            inst_bus.write_value(raw_);

            logic::Bus<4> rs1_bus;
            for (std::size_t i = 0; i < 4; ++i) {
                rs1_bus[i].write(inst_bus[18 + i].read());
            }

            return static_cast<Register>(rs1_bus.read_value());
          }

          /*
          Determines source register 2 rs2 using hardware Bus slicing
          */
          [[nodiscard]]
          Register rs2() const noexcept {
            logic::Bus<32> inst_bus;
            inst_bus.write_value(raw_);

            logic::Bus<4> rs2_bus;
            for (std::size_t i = 0; i < 4; ++i) {
                rs2_bus[i].write(inst_bus[14 + i].read());
            }

            return static_cast<Register>(rs2_bus.read_value());
          }

          /*
          Determines signed immediate using hardware Bus fan-out sign extension
          */
          [[nodiscard]]
          std::int32_t immediate_signed() const noexcept {
              logic::Bus<32> inst_bus;
              inst_bus.write_value(raw_);

              logic::Bus<32> imm_bus;
              // Pass lower 18 bits directly
              for (std::size_t i = 0; i < 18; ++i) {
                  imm_bus[i].write(inst_bus[i].read());
              }

              // Fan-out bit 17 (sign bit) to remaining upper 14 bits (bits 18..31)
              const auto sign_state = inst_bus[17].read();
              for (std::size_t i = 18; i < 32; ++i) {
                  imm_bus[i].write(sign_state);
              }

              return static_cast<std::int32_t>(imm_bus.read_value());
          }
        private:
          Word raw_;
    };
}
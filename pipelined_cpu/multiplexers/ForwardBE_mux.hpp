/*
    ForwardBE_mux.hpp

    3:1 Forwarding Multiplexer for ALU Operand B / Store Data in the Execute (EX) stage.
    Selects between:
      - 00: RD2 from ID/EX register (unforwarded register operand 2)
      - 01: ResultW from MEM/WB stage (forwarded from writeback)
      - 10: ALUOutM from EX/MEM stage (forwarded from memory stage)
*/

#pragma once

#include <cstddef>
#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/simulator/Component.hpp>
#include <logic/combinational/multiplexers/Mux3.hpp>

namespace cpu {

    template <std::size_t DataWidth = 32, std::size_t N = 32>
    class ForwardBE_mux : public logic::Component {
    public:
        static_assert(DataWidth == N, "DataWidth must be equal to N");

        /// Constructor using two separate control wires (select0 = LSB, select1 = MSB)
        ForwardBE_mux(
            logic::Bus<DataWidth>& rd2,
            logic::Bus<DataWidth>& resultW,
            logic::Bus<DataWidth>& aluOutM,
            logic::Wire& select0,
            logic::Wire& select1,
            logic::Bus<N>& writeDataE
        ) : rd2_(rd2),
            resultW_(resultW),
            aluOutM_(aluOutM),
            select0_(select0),
            select1_(select1),
            writeDataE_(writeDataE),
            mux_(rd2_, resultW_, aluOutM_, select0_, select1_, writeDataE_)
        {}

        /// Constructor using a 2-bit control bus (forwardBE[0] = LSB, forwardBE[1] = MSB)
        ForwardBE_mux(
            logic::Bus<DataWidth>& rd2,
            logic::Bus<DataWidth>& resultW,
            logic::Bus<DataWidth>& aluOutM,
            logic::Bus<2>& forwardBE,
            logic::Bus<N>& writeDataE
        ) : ForwardBE_mux(rd2, resultW, aluOutM, forwardBE[0], forwardBE[1], writeDataE)
        {}

        void evaluate() noexcept override {
            mux_.evaluate();
        }

        [[nodiscard]]
        logic::Bus<N>& output() noexcept {
            return writeDataE_;
        }

        [[nodiscard]]
        const logic::Bus<N>& output() const noexcept {
            return writeDataE_;
        }

        [[nodiscard]]
        logic::Bus<N>& write_data_e() noexcept {
            return writeDataE_;
        }

        [[nodiscard]]
        const logic::Bus<N>& write_data_e() const noexcept {
            return writeDataE_;
        }

        [[nodiscard]]
        const logic::Bus<DataWidth>& rd2() const noexcept {
            return rd2_;
        }

        [[nodiscard]]
        const logic::Bus<DataWidth>& result_w() const noexcept {
            return resultW_;
        }

        [[nodiscard]]
        const logic::Bus<DataWidth>& alu_out_m() const noexcept {
            return aluOutM_;
        }

    private:
        // Inputs
        logic::Bus<DataWidth>& rd2_;
        logic::Bus<DataWidth>& resultW_;
        logic::Bus<DataWidth>& aluOutM_;
        logic::Wire& select0_;
        logic::Wire& select1_;

        // Output
        logic::Bus<N>& writeDataE_;

        // Internal 3:1 Mux
        logic::Mux3<N> mux_;
    };

    // Alias for alternative naming convention
    template <std::size_t DataWidth = 32, std::size_t N = 32>
    using ForwardB_mux = ForwardBE_mux<DataWidth, N>;

} // namespace cpu

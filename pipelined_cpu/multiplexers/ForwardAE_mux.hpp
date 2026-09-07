/*
    ForwardAE_mux.hpp

    3:1 Forwarding Multiplexer for ALU Operand A in the Execute (EX) stage.
    Selects between:
      - 00: RD1 from ID/EX register (unforwarded register operand 1)
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
    class ForwardAE_mux : public logic::Component {
    public:
        static_assert(DataWidth == N, "DataWidth must be equal to N");

        /// Constructor using two separate control wires (select0 = LSB, select1 = MSB)
        ForwardAE_mux(
            logic::Bus<DataWidth>& rd1,
            logic::Bus<DataWidth>& resultW,
            logic::Bus<DataWidth>& aluOutM,
            logic::Wire& select0,
            logic::Wire& select1,
            logic::Bus<N>& srcAE
        ) : rd1_(rd1),
            resultW_(resultW),
            aluOutM_(aluOutM),
            select0_(select0),
            select1_(select1),
            srcAE_(srcAE),
            mux_(rd1_, resultW_, aluOutM_, select0_, select1_, srcAE_)
        {}

        /// Constructor using a 2-bit control bus (forwardAE[0] = LSB, forwardAE[1] = MSB)
        ForwardAE_mux(
            logic::Bus<DataWidth>& rd1,
            logic::Bus<DataWidth>& resultW,
            logic::Bus<DataWidth>& aluOutM,
            logic::Bus<2>& forwardAE,
            logic::Bus<N>& srcAE
        ) : ForwardAE_mux(rd1, resultW, aluOutM, forwardAE[0], forwardAE[1], srcAE)
        {}

        void evaluate() noexcept override {
            mux_.evaluate();
        }

        [[nodiscard]]
        logic::Bus<N>& output() noexcept {
            return srcAE_;
        }

        [[nodiscard]]
        const logic::Bus<N>& output() const noexcept {
            return srcAE_;
        }

        [[nodiscard]]
        logic::Bus<N>& src_a_e() noexcept {
            return srcAE_;
        }

        [[nodiscard]]
        const logic::Bus<N>& src_a_e() const noexcept {
            return srcAE_;
        }

        [[nodiscard]]
        const logic::Bus<DataWidth>& rd1() const noexcept {
            return rd1_;
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
        logic::Bus<DataWidth>& rd1_;
        logic::Bus<DataWidth>& resultW_;
        logic::Bus<DataWidth>& aluOutM_;
        logic::Wire& select0_;
        logic::Wire& select1_;

        // Output
        logic::Bus<N>& srcAE_;

        // Internal 3:1 Mux
        logic::Mux3<N> mux_;
    };

    // Alias for alternative naming convention
    template <std::size_t DataWidth = 32, std::size_t N = 32>
    using ForwardA_mux = ForwardAE_mux<DataWidth, N>;

} // namespace cpu

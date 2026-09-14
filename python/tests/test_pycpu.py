import os
import sys
import unittest

current_dir = os.path.dirname(os.path.abspath(__file__))
parent_dir = os.path.dirname(current_dir)
sys.path.insert(0, parent_dir)

from pycpu import CPU, assemble, Opcode, Register


class TestPyCPU(unittest.TestCase):
    def test_assembler_encoding(self):
        words = assemble("""
            addi r1, r0, 10
            add  r2, r1, r1
            halt
        """)
        self.assertEqual(len(words), 3)
        self.assertTrue(all(isinstance(w, int) for w in words))

    def test_cpu_arithmetic_program(self):
        cpu = CPU()
        cpu.reset()
        self.assertEqual(cpu.pc, 0)
        self.assertFalse(cpu.is_halted)

        # Run a program that computes:
        # r1 = 15
        # r2 = 25
        # r3 = r1 + r2 = 40
        # r4 = r3 - r1 = 25
        # halt
        cpu.load_assembly("""
            addi r1, r0, 15
            addi r2, r0, 25
            add  r3, r1, r2
            sub  r4, r3, r1
            halt
        """)

        cycles = cpu.run()
        self.assertTrue(cpu.is_halted)
        self.assertGreater(cycles, 0)

        # Check register values
        self.assertEqual(cpu.registers["r1"], 15)
        self.assertEqual(cpu.registers["r2"], 25)
        self.assertEqual(cpu.registers["r3"], 40)
        self.assertEqual(cpu.registers["r4"], 25)

    def test_multi_cycle_architecture(self):
        cpu = CPU(architecture="multi_cycle")
        cpu.reset()
        self.assertEqual(cpu.architecture, "multi_cycle")

        cpu.load_assembly("""
            addi r1, r0, 15
            addi r2, r0, 25
            add  r3, r1, r2
            sub  r4, r3, r1
            halt
        """)

        cycles = cpu.run()
        self.assertTrue(cpu.is_halted)
        self.assertEqual(cpu.registers["r1"], 15)
        self.assertEqual(cpu.registers["r2"], 25)
        self.assertEqual(cpu.registers["r3"], 40)
        self.assertEqual(cpu.registers["r4"], 25)
        self.assertGreater(cycles, 4)

    def test_pipelined_architecture(self):
        cpu = CPU(architecture="pipelined")
        cpu.reset()
        self.assertEqual(cpu.architecture, "pipelined")

        cpu.load_assembly("""
            addi r1, r0, 15
            addi r2, r0, 25
            add  r3, r1, r2
            sub  r4, r3, r1
            halt
        """)

        cycles = cpu.run()
        self.assertTrue(cpu.is_halted)
        self.assertEqual(cpu.registers["r1"], 15)
        self.assertEqual(cpu.registers["r2"], 25)
        self.assertEqual(cpu.registers["r3"], 40)
        self.assertEqual(cpu.registers["r4"], 25)
        self.assertGreater(cycles, 0)

    def test_cross_architecture_comparison(self):
        program_asm = """
            addi r1, r0, 10
            addi r2, r0, 20
            add  r3, r1, r2
            sub  r4, r3, r1
            halt
        """
        results = {}
        for arch in ("single_cycle", "multi_cycle", "pipelined"):
            cpu = CPU(architecture=arch)
            cpu.reset()
            cpu.load_assembly(program_asm)
            cycles = cpu.run()
            self.assertTrue(cpu.is_halted)
            results[arch] = {
                "cycles": cycles,
                "r1": cpu.registers["r1"],
                "r2": cpu.registers["r2"],
                "r3": cpu.registers["r3"],
                "r4": cpu.registers["r4"],
            }

        # Verify exact functional equivalence across all architectures
        for arch in ("multi_cycle", "pipelined"):
            self.assertEqual(results[arch]["r1"], results["single_cycle"]["r1"])
            self.assertEqual(results[arch]["r2"], results["single_cycle"]["r2"])
            self.assertEqual(results[arch]["r3"], results["single_cycle"]["r3"])
            self.assertEqual(results[arch]["r4"], results["single_cycle"]["r4"])

        # Microarchitectural cycle count comparison:
        # Single-Cycle (1 cycle per instr) < Pipelined (latency + drain) < Multi-Cycle (3-5 cycles per instr)
        self.assertLess(results["single_cycle"]["cycles"], results["pipelined"]["cycles"])
        self.assertLess(results["pipelined"]["cycles"], results["multi_cycle"]["cycles"])

    def test_cpi_calculation(self):
        cpu = CPU(architecture="multi_cycle")
        cpu.reset()
        words = cpu.load_assembly("""
            addi r1, r0, 5
            addi r2, r0, 10
            add  r3, r1, r2
            halt
        """)
        cpu.run()
        cpi = cpu.cpi(len(words))
        self.assertGreater(cpi, 1.0)

    def test_register_dict_access(self):
        cpu = CPU()
        cpu.reset()
        # Direct write and read
        cpu.registers["r5"] = 1234
        self.assertEqual(cpu.registers[5], 1234)
        self.assertEqual(cpu.registers["x5"], 1234)

    def test_inspect(self):
        cpu = CPU(architecture="pipelined")
        cpu.reset()
        cpu.registers["r1"] = 99
        info = cpu.inspect()
        self.assertIn("pc", info)
        self.assertIn("architecture", info)
        self.assertEqual(info["architecture"], "pipelined")
        self.assertIn("registers", info)
        self.assertEqual(info["registers"]["r1"], 99)


if __name__ == "__main__":
    print("=" * 60)
    print("       RUNNING PYCPU TEST SUITE")
    print("=" * 60)
    unittest.main(verbosity=2)

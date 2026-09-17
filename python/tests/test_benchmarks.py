import os
import sys
import unittest

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from pycpu.benchmarks import BenchmarkRunner, BENCHMARKS


class TestMicroarchitectureBenchmarks(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.runner = BenchmarkRunner()

    def test_fibonacci_benchmark(self):
        report = self.runner.run_benchmark(BENCHMARKS["fibonacci"])
        for arch, res in report.results.items():
            self.assertTrue(res.passed, f"Fibonacci failed on {arch}: {res.message}")
        self.assertEqual(report.results["single_cycle"].cycles, 39)
        self.assertEqual(report.results["multi_cycle"].cycles, 151)
        self.assertEqual(report.results["pipelined"].cycles, 55)
        self.assertGreater(report.speedup_pipe_vs_single, 2.0)

    def test_array_sum_benchmark(self):
        report = self.runner.run_benchmark(BENCHMARKS["array_sum"])
        for arch, res in report.results.items():
            self.assertTrue(res.passed, f"Array sum failed on {arch}: {res.message}")
        self.assertEqual(report.results["single_cycle"].cycles, 45)
        self.assertEqual(report.results["multi_cycle"].cycles, 182)
        self.assertEqual(report.results["pipelined"].cycles, 71)

    def test_raw_hazard_benchmark(self):
        report = self.runner.run_benchmark(BENCHMARKS["raw_hazard"])
        for arch, res in report.results.items():
            self.assertTrue(res.passed, f"RAW hazard failed on {arch}: {res.message}")
        self.assertEqual(report.results["pipelined"].cycles, 14)

    def test_load_use_scheduling_comparison(self):
        unscheduled = self.runner.run_benchmark(BENCHMARKS["load_use_unscheduled"])
        scheduled = self.runner.run_benchmark(BENCHMARKS["load_use_scheduled"])
        # Both complete in 14 pipelined cycles, but scheduled performed 3 more useful instructions!
        self.assertEqual(unscheduled.results["pipelined"].cycles, 14)
        self.assertEqual(scheduled.results["pipelined"].cycles, 14)
        self.assertEqual(unscheduled.results["single_cycle"].instructions, 7)
        self.assertEqual(scheduled.results["single_cycle"].instructions, 10)

    def test_factorial_benchmark(self):
        report = self.runner.run_benchmark(BENCHMARKS["factorial"])
        for arch, res in report.results.items():
            self.assertTrue(res.passed, f"Factorial failed on {arch}: {res.message}")
        self.assertEqual(report.results["single_cycle"].cycles, 104)
        self.assertEqual(report.results["pipelined"].cycles, 160)


if __name__ == "__main__":
    unittest.main()

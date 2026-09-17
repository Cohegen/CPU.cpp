"""Benchmarking engine and comparative performance analyzer across CPU microarchitectures."""

from dataclasses import dataclass, asdict
from typing import Dict, List, Optional, Any
from pycpu import CPU
from .suite import Benchmark, BENCHMARKS


@dataclass
class ClockModel:
    """Clock frequency configuration (in MHz) based on Harris & Harris critical path timing."""
    single_cycle_mhz: float = 100.0   # 10.0 ns cycle (long combinational path: PC->IMEM->RF->ALU->DMEM->WB)
    multi_cycle_mhz: float = 400.0    # 2.5 ns cycle (balanced shorter stages)
    pipelined_mhz: float = 400.0      # 2.5 ns cycle (5 pipelined stages)

    def period_ns(self, architecture: str) -> float:
        freq = getattr(self, f"{architecture}_mhz", 400.0)
        return 1000.0 / freq


@dataclass
class ArchResult:
    architecture: str
    cycles: int
    instructions: int
    cpi: float
    ipc: float
    clock_freq_mhz: float
    exec_time_ns: float
    passed: bool
    message: str


@dataclass
class BenchmarkReport:
    benchmark: Benchmark
    results: Dict[str, ArchResult]
    speedup_multi_vs_single: float
    speedup_pipe_vs_single: float
    speedup_pipe_vs_multi: float

    def to_dict(self) -> Dict[str, Any]:
        return {
            "benchmark_name": self.benchmark.name,
            "category": self.benchmark.category,
            "description": self.benchmark.description,
            "expected": self.benchmark.expected_summary,
            "results": {arch: asdict(res) for arch, res in self.results.items()},
            "speedup_multi_vs_single": self.speedup_multi_vs_single,
            "speedup_pipe_vs_single": self.speedup_pipe_vs_single,
            "speedup_pipe_vs_multi": self.speedup_pipe_vs_multi,
        }


class BenchmarkRunner:
    """Executes benchmarks across single-cycle, multi-cycle, and pipelined microarchitectures."""

    ARCHITECTURES = ["single_cycle", "multi_cycle", "pipelined"]

    def __init__(self, clock_model: Optional[ClockModel] = None):
        self.clock_model = clock_model or ClockModel()

    def run_benchmark(self, benchmark: Benchmark, max_cycles: int = 50000) -> BenchmarkReport:
        results: Dict[str, ArchResult] = {}
        inst_count = 0

        # Step 1: Run Single-Cycle to determine instruction count (CPI = 1.0)
        sc_cpu = CPU("single_cycle")
        if benchmark.init_memory:
            for addr, val in benchmark.init_memory.items():
                sc_cpu.write_memory(addr, val)
        sc_cpu.load_assembly(benchmark.asm)
        sc_cycles = sc_cpu.run(max_cycles=max_cycles)
        inst_count = sc_cycles  # In single-cycle, cycles == committed instructions
        sc_passed = True
        sc_msg = "OK"
        if benchmark.validate:
            sc_passed, sc_msg = benchmark.validate(sc_cpu)

        period_sc = self.clock_model.period_ns("single_cycle")
        results["single_cycle"] = ArchResult(
            architecture="single_cycle",
            cycles=sc_cycles,
            instructions=inst_count,
            cpi=1.0,
            ipc=1.0,
            clock_freq_mhz=self.clock_model.single_cycle_mhz,
            exec_time_ns=sc_cycles * period_sc,
            passed=sc_passed,
            message=sc_msg,
        )

        # Step 2: Run Multi-Cycle and Pipelined
        for arch in ["multi_cycle", "pipelined"]:
            cpu = CPU(arch)
            if benchmark.init_memory:
                for addr, val in benchmark.init_memory.items():
                    cpu.write_memory(addr, val)
            cpu.load_assembly(benchmark.asm)
            cycles = cpu.run(max_cycles=max_cycles)
            passed = True
            msg = "OK"
            if benchmark.validate:
                passed, msg = benchmark.validate(cpu)

            cpi = cycles / inst_count if inst_count > 0 else 0.0
            ipc = inst_count / cycles if cycles > 0 else 0.0
            period = self.clock_model.period_ns(arch)
            exec_time_ns = cycles * period

            results[arch] = ArchResult(
                architecture=arch,
                cycles=cycles,
                instructions=inst_count,
                cpi=round(cpi, 3),
                ipc=round(ipc, 3),
                clock_freq_mhz=getattr(self.clock_model, f"{arch}_mhz"),
                exec_time_ns=round(exec_time_ns, 2),
                passed=passed,
                message=msg,
            )

        t_single = results["single_cycle"].exec_time_ns
        t_multi = results["multi_cycle"].exec_time_ns
        t_pipe = results["pipelined"].exec_time_ns

        sp_multi_vs_single = round(t_single / t_multi, 2) if t_multi > 0 else 0.0
        sp_pipe_vs_single = round(t_single / t_pipe, 2) if t_pipe > 0 else 0.0
        sp_pipe_vs_multi = round(t_multi / t_pipe, 2) if t_pipe > 0 else 0.0

        return BenchmarkReport(
            benchmark=benchmark,
            results=results,
            speedup_multi_vs_single=sp_multi_vs_single,
            speedup_pipe_vs_single=sp_pipe_vs_single,
            speedup_pipe_vs_multi=sp_pipe_vs_multi,
        )

    def run_all(self) -> List[BenchmarkReport]:
        reports = []
        for name, bench in BENCHMARKS.items():
            reports.append(self.run_benchmark(bench))
        return reports

    def format_cli_table(self, reports: List[BenchmarkReport]) -> str:
        lines = [
            "=" * 105,
            f"{'BENCHMARK':<22} | {'CATEGORY':<18} | {'METRIC':<14} | {'SINGLE-CYCLE':<14} | {'MULTI-CYCLE':<14} | {'PIPELINED':<14}",
            "=" * 105,
        ]
        for r in reports:
            bname = r.benchmark.name
            cat = r.benchmark.category
            sc = r.results["single_cycle"]
            mc = r.results["multi_cycle"]
            pipe = r.results["pipelined"]

            lines.append(f"{bname:<22} | {cat:<18} | {'Instructions':<14} | {sc.instructions:<14} | {mc.instructions:<14} | {pipe.instructions:<14}")
            lines.append(f"{'':<22} | {'':<18} | {'Clock Cycles':<14} | {sc.cycles:<14} | {mc.cycles:<14} | {pipe.cycles:<14}")
            lines.append(f"{'':<22} | {'':<18} | {'CPI':<14} | {sc.cpi:<14.2f} | {mc.cpi:<14.2f} | {pipe.cpi:<14.2f}")
            lines.append(f"{'':<22} | {'':<18} | {'IPC':<14} | {sc.ipc:<14.2f} | {mc.ipc:<14.2f} | {pipe.ipc:<14.2f}")
            lines.append(f"{'':<22} | {'':<18} | {'Exec Time (ns)':<14} | {sc.exec_time_ns:<14.1f} | {mc.exec_time_ns:<14.1f} | {pipe.exec_time_ns:<14.1f}")
            lines.append(f"{'':<22} | {'':<18} | {'Speedup vs SC':<14} | {'1.00x (ref)':<14} | {f'{r.speedup_multi_vs_single:.2f}x':<14} | {f'{r.speedup_pipe_vs_single:.2f}x':<14}")
            lines.append(f"{'':<22} | {'':<18} | {'Speedup vs MC':<14} | {'-':<14} | {'1.00x (ref)':<14} | {f'{r.speedup_pipe_vs_multi:.2f}x':<14}")
            lines.append(f"{'':<22} | {'':<18} | {'Status':<14} | {'PASS' if sc.passed else 'FAIL':<14} | {'PASS' if mc.passed else 'FAIL':<14} | {'PASS' if pipe.passed else 'FAIL':<14}")
            lines.append("-" * 105)
        return "\n".join(lines)

    def generate_markdown(self, reports: List[BenchmarkReport]) -> str:
        lines = [
            "## Microarchitecture Benchmark Results",
            "",
            "> **Timing Model** (Harris & Harris Ch. 7):",
            f"> - **Single-Cycle**: {self.clock_model.single_cycle_mhz:.0f} MHz (Period: {self.clock_model.period_ns('single_cycle'):.1f} ns)",
            f"> - **Multi-Cycle**: {self.clock_model.multi_cycle_mhz:.0f} MHz (Period: {self.clock_model.period_ns('multi_cycle'):.1f} ns)",
            f"> - **Pipelined**: {self.clock_model.pipelined_mhz:.0f} MHz (Period: {self.clock_model.period_ns('pipelined'):.1f} ns)",
            "",
            "| Benchmark | Category | Insts | Cycles (SC / MC / Pipe) | CPI (SC / MC / Pipe) | Exec Time (SC / MC / Pipe) | Pipelined Speedup | Status |",
            "|:---|:---|:---:|:---:|:---:|:---:|:---:|:---:|",
        ]
        for r in reports:
            sc = r.results["single_cycle"]
            mc = r.results["multi_cycle"]
            pipe = r.results["pipelined"]
            status = "PASS" if (sc.passed and mc.passed and pipe.passed) else "FAIL"
            lines.append(
                f"| **{r.benchmark.name}** | {r.benchmark.category} | {sc.instructions} | "
                f"{sc.cycles} / {mc.cycles} / {pipe.cycles} | "
                f"{sc.cpi:.2f} / {mc.cpi:.2f} / {pipe.cpi:.2f} | "
                f"{sc.exec_time_ns:.0f}ns / {mc.exec_time_ns:.0f}ns / {pipe.exec_time_ns:.0f}ns | "
                f"**{r.speedup_pipe_vs_single:.2f}x** vs SC<br>({r.speedup_pipe_vs_multi:.2f}x vs MC) | "
                f"`{status}` |"
            )
        return "\n".join(lines)

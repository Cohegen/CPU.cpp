"""Standard Microarchitecture Benchmark Definitions for CPU.cpp."""

from dataclasses import dataclass
from typing import Callable, Dict, Optional, Tuple
from pycpu import CPU


@dataclass
class Benchmark:
    name: str
    category: str
    description: str
    asm: str
    init_memory: Optional[Dict[int, int]] = None
    validate: Optional[Callable[[CPU], Tuple[bool, str]]] = None
    expected_summary: str = ""


# 1. Fibonacci: Computes 8th Fibonacci number (0, 1, 1, 2, 3, 5, 8, 13, 21)
FIBONACCI_ASM = """
    li r1, 0
    li r2, 1
    li r3, 7
    li r4, 1
loop:
    add r5, r1, r2
    mv r1, r2
    mv r2, r5
    sub r3, r3, r4
    bnez r3, loop
    halt
"""


def _validate_fib(cpu: CPU) -> Tuple[bool, str]:
    res = cpu.registers[2]
    return (res == 21, f"r2 = {res} (expected 21)")


# 2. Array Sum: Accumulates 8 words from RAM[0x80..0x87] into r3 and stores to RAM[0x98]
ARRAY_SUM_ASM = """
    li r1, 0x80
    li r2, 8
    li r3, 0
    li r4, 1
arr_loop:
    lw r5, 0(r1)
    add r3, r3, r5
    add r1, r1, r4
    sub r2, r2, r4
    bnez r2, arr_loop
    sw r3, 16(r1)
    halt
"""


def _validate_array_sum(cpu: CPU) -> Tuple[bool, str]:
    r3 = cpu.registers[3]
    mem_val = cpu.read_memory(0x98)
    ok = (r3 == 360) and (mem_val == 360)
    return (ok, f"r3 = {r3}, RAM[0x98] = {mem_val} (expected 360)")


# 3. RAW Hazard Chain: Deep back-to-back dependency chain testing ALU-to-ALU forwarding
RAW_HAZARD_ASM = """
    li r1, 2
    add r2, r1, r1
    add r3, r2, r1
    add r4, r3, r2
    add r5, r4, r3
    add r6, r5, r4
    add r7, r6, r5
    add r8, r7, r6
    add r9, r8, r7
    add r10, r9, r8
    halt
"""


def _validate_raw(cpu: CPU) -> Tuple[bool, str]:
    r10 = cpu.registers[10]
    return (r10 == 178, f"r10 = {r10} (expected 178)")


# 4. Unscheduled Load-Use: Dependent adds immediately follow memory loads (triggers 1-cycle stalls in pipeline)
LOAD_USE_UNSCHEDULED_ASM = """
    li r1, 0x80
    lw r2, 0(r1)
    add r3, r2, r2
    lw r4, 1(r1)
    add r5, r4, r3
    lw r6, 2(r1)
    add r7, r6, r5
    halt
"""


def _validate_load_use_unscheduled(cpu: CPU) -> Tuple[bool, str]:
    r7 = cpu.registers[7]
    return (r7 == 50, f"r7 = {r7} (expected 50)")


# 5. Scheduled Load-Use: Independent instructions fill load delay slots to eliminate stalls
LOAD_USE_SCHEDULED_ASM = """
    li r1, 0x80
    lw r2, 0(r1)
    li r8, 100
    add r3, r2, r2
    lw r4, 1(r1)
    add r8, r8, r8
    add r5, r4, r3
    lw r6, 2(r1)
    add r8, r8, r8
    add r7, r6, r5
    halt
"""


def _validate_load_use_scheduled(cpu: CPU) -> Tuple[bool, str]:
    r7 = cpu.registers[7]
    r8 = cpu.registers[8]
    ok = (r7 == 50) and (r8 == 400)
    return (ok, f"r7 = {r7}, r8 = {r8} (expected r7=50, r8=400)")


# 6. Factorial: Computes 5! = 120 using software multiplication loops
FACTORIAL_ASM = """
    li r1, 5
    li r2, 1
    li r5, 1
fact_loop:
    beqz r1, done
    mv r3, r2
    li r4, 0
    mv r6, r1
mult_loop:
    beqz r6, mult_done
    add r4, r4, r3
    sub r6, r6, r5
    b mult_loop
mult_done:
    mv r2, r4
    sub r1, r1, r5
    b fact_loop
done:
    halt
"""


def _validate_factorial(cpu: CPU) -> Tuple[bool, str]:
    r2 = cpu.registers[2]
    return (r2 == 120, f"r2 = {r2} (expected 120)")


BENCHMARKS: Dict[str, Benchmark] = {
    "fibonacci": Benchmark(
        name="fibonacci",
        category="Arithmetic & Loops",
        description="Iterative Fibonacci sequence (computes F(8) = 21)",
        asm=FIBONACCI_ASM,
        validate=_validate_fib,
        expected_summary="R2 = 21",
    ),
    "array_sum": Benchmark(
        name="array_sum",
        category="Memory I/O",
        description="Accumulate 8 memory words (RAM[0x80..0x87]) and store sum to RAM[0x98]",
        asm=ARRAY_SUM_ASM,
        init_memory={0x80 + i: (i + 1) * 10 for i in range(8)},
        validate=_validate_array_sum,
        expected_summary="Sum = 360",
    ),
    "raw_hazard": Benchmark(
        name="raw_hazard",
        category="ALU Forwarding",
        description="10 straight-line dependent instructions testing ALU-to-ALU forwarding",
        asm=RAW_HAZARD_ASM,
        validate=_validate_raw,
        expected_summary="R10 = 178",
    ),
    "load_use_unscheduled": Benchmark(
        name="load_use_unscheduled",
        category="Data Hazards (Stalls)",
        description="Back-to-back load-use instructions causing 1-cycle pipeline stalls",
        asm=LOAD_USE_UNSCHEDULED_ASM,
        init_memory={0x80: 5, 0x81: 15, 0x82: 25},
        validate=_validate_load_use_unscheduled,
        expected_summary="R7 = 50",
    ),
    "load_use_scheduled": Benchmark(
        name="load_use_scheduled",
        category="Instruction Scheduling",
        description="Compiler-scheduled load-use filling delay slots with independent work",
        asm=LOAD_USE_SCHEDULED_ASM,
        init_memory={0x80: 5, 0x81: 15, 0x82: 25},
        validate=_validate_load_use_scheduled,
        expected_summary="R7 = 50, R8 = 400",
    ),
    "factorial": Benchmark(
        name="factorial",
        category="Nested Control Flow",
        description="Iterative factorial (5! = 120) with nested multiplication loop",
        asm=FACTORIAL_ASM,
        validate=_validate_factorial,
        expected_summary="R2 = 120",
    ),
}

# pycpu

A high-level, Pythonic CPU simulation frontend powered by the C++20 `CPU.cpp` architecture engine.

## Features
- **Human-Readable Assembly**: Write and load assembly programs as multi-line strings (`addi`, `add`, `sub`, `lw`, `sw`, `beq`, `halt`).
- **Dictionary-Style Register Access**: Read and write registers using `cpu.registers['r1']`, `cpu.registers['x1']`, or `cpu.registers[1]`.
- **Clock-Cycle Stepping & Inspection**: Step individual clock cycles, inspect program counter, and dump register files.
- **Fast C++ Execution**: Direct execution via pre-compiled `_pycpu_core` bindings.

## Quick Start
```python
import pycpu
from pycpu import CPU

# 1. Initialize CPU
cpu = CPU()
cpu.reset()

# 2. Write and load assembly program
cpu.load_assembly("""
    addi r1, r0, 50
    addi r2, r0, 75
    add  r3, r1, r2
    halt
""")

# 3. Run to completion
cycles = cpu.run()
print(f"Program halted in {cycles} cycles.")
print(f"Register r3 = {cpu.registers['r3']}")  # 125
print(cpu.inspect())
```

## Installation & Build
```bash
# Build using CMake:
cmake -B build -S . -G "MinGW Makefiles" -DPYTHON_EXECUTABLE=python
cmake --build build

# Or install via pip:
pip install .
```

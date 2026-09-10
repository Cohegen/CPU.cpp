from typing import Union, List, Dict
from ._pycpu_core import NativeCPU32, Register, Opcode
from .assembler import assemble, _REG_MAP

class RegisterAccessor:
    """Dictionary-like accessor for CPU general purpose registers (r0-r15)."""

    def __init__(self, native_cpu: NativeCPU32):
        self._cpu = native_cpu

    def _resolve_reg(self, key: Union[int, str, Register]) -> Register:
        if isinstance(key, Register):
            return key
        if isinstance(key, int):
            if 0 <= key <= 15:
                return getattr(Register, f"R{key}")
            raise IndexError(f"Register index {key} out of range [0, 15]")
        key_str = str(key).strip().lower()
        if key_str in _REG_MAP:
            return _REG_MAP[key_str]
        raise KeyError(f"Unknown register name: '{key}'. Valid names: r0-r15, x0-x15")

    def __getitem__(self, key: Union[int, str, Register]) -> int:
        reg = self._resolve_reg(key)
        return self._cpu.read_register(reg)

    def __setitem__(self, key: Union[int, str, Register], value: int):
        reg = self._resolve_reg(key)
        self._cpu.write_register(reg, int(value) & 0xFFFFFFFF)

    def to_dict(self) -> Dict[str, int]:
        return {f"r{i}": self[i] for i in range(16)}

    def __repr__(self) -> str:
        # Show all registers with non-zero values or first 8
        items = [f"r{i}={self[i]}" for i in range(16) if self[i] != 0 or i < 4]
        return f"<Registers {', '.join(items)}>"


class CPU:
    """High-level Pythonic CPU simulation frontend wrapping C++ CPU.cpp."""

    def __init__(self):
        self._native = NativeCPU32()
        self.registers = RegisterAccessor(self._native)

    @property
    def native(self) -> NativeCPU32:
        return self._native

    def reset(self):
        """Resets the CPU state and PC to 0."""
        self._native.reset()

    def step(self):
        """Advances simulation by one clock cycle."""
        self._native.step()

    def run(self, max_cycles: int = 100000) -> int:
        """Runs until a HALT instruction is encountered or max_cycles is reached."""
        start_cycles = self.cycles
        self._native.run(max_cycles)
        return self.cycles - start_cycles

    @property
    def pc(self) -> int:
        """Current program counter."""
        return self._native.pc()

    @property
    def is_halted(self) -> bool:
        """Whether execution stopped at a HALT instruction."""
        return self._native.halted()

    @property
    def cycles(self) -> int:
        """Number of elapsed clock cycles."""
        return self._native.cycles()

    def load_program(self, instructions: List[int]):
        """Loads raw 32-bit machine instructions into instruction memory."""
        self._native.load_program(instructions)

    def load_assembly(self, asm_code: str) -> List[int]:
        """Assembles and loads human-readable assembly instructions."""
        words = assemble(asm_code)
        self.load_program(words)
        return words

    def inspect(self) -> Dict[str, Union[int, bool, Dict[str, int]]]:
        """Returns a snapshot of the CPU state."""
        return {
            "pc": self.pc,
            "cycles": self.cycles,
            "halted": self.is_halted,
            "registers": self.registers.to_dict(),
        }

    def __repr__(self) -> str:
        return f"<CPU pc={self.pc} cycles={self.cycles} halted={self.is_halted}>"

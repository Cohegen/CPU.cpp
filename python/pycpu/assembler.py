import re
from typing import List
from ._pycpu_core import (
    Opcode,
    Register,
    encode_r_type,
    encode_i_type,
    encode_s_type,
    encode_b_type,
    encode_j_type,
)

_REG_MAP = {
    "r0": Register.R0, "x0": Register.R0, "zero": Register.R0,
    "r1": Register.R1, "x1": Register.R1,
    "r2": Register.R2, "x2": Register.R2,
    "r3": Register.R3, "x3": Register.R3,
    "r4": Register.R4, "x4": Register.R4,
    "r5": Register.R5, "x5": Register.R5,
    "r6": Register.R6, "x6": Register.R6,
    "r7": Register.R7, "x7": Register.R7,
    "r8": Register.R8, "x8": Register.R8,
    "r9": Register.R9, "x9": Register.R9,
    "r10": Register.R10, "x10": Register.R10,
    "r11": Register.R11, "x11": Register.R11,
    "r12": Register.R12, "x12": Register.R12,
    "r13": Register.R13, "x13": Register.R13,
    "r14": Register.R14, "x14": Register.R14,
    "r15": Register.R15, "x15": Register.R15,
}

def _parse_reg(token: str) -> Register:
    token = token.strip().lower()
    if token in _REG_MAP:
        return _REG_MAP[token]
    # Allow raw integer index like 1 -> Register.R1
    if token.isdigit():
        idx = int(token)
        if 0 <= idx <= 15:
            return _REG_MAP[f"r{idx}"]
    raise ValueError(f"Unknown register name: '{token}'. Valid registers: r0-r15 / x0-x15")

def _parse_int(token: str) -> int:
    token = token.strip()
    if token.startswith(("0x", "0X")):
        return int(token, 16)
    if token.startswith(("0b", "0B")):
        return int(token, 2)
    return int(token)


def assemble_line(line: str) -> int:
    """Assembles a single assembly instruction into a 32-bit machine word."""
    # Strip comments
    line = re.sub(r"[;#].*$", "", line).strip()
    if not line:
        return None

    # Split opcode and operands
    parts = re.split(r"[\s,]+", line)
    op = parts[0].upper()
    args = parts[1:]

    if op == "NOP":
        return encode_r_type(Opcode.NOP, Register.R0, Register.R0, Register.R0)

    elif op == "HALT":
        return encode_r_type(Opcode.HALT, Register.R0, Register.R0, Register.R0)

    elif op in ("ADD", "SUB", "AND", "OR", "XOR"):
        opcode = getattr(Opcode, op)
        rd = _parse_reg(args[0])
        rs1 = _parse_reg(args[1])
        rs2 = _parse_reg(args[2])
        return encode_r_type(opcode, rd, rs1, rs2)

    elif op == "NOT":
        rd = _parse_reg(args[0])
        rs1 = _parse_reg(args[1])
        return encode_r_type(Opcode.NOT, rd, rs1, Register.R0)

    elif op == "ADDI":
        rd = _parse_reg(args[0])
        rs1 = _parse_reg(args[1])
        imm = _parse_int(args[2])
        return encode_i_type(Opcode.ADDI, rd, rs1, imm)

    elif op == "LI":
        rd = _parse_reg(args[0])
        imm = _parse_int(args[1])
        return encode_i_type(Opcode.LI, rd, Register.R0, imm)

    elif op == "LW":
        rd = _parse_reg(args[0])
        # Format can be lw rd, imm(rs1) or lw rd, rs1, imm
        if "(" in args[1]:
            match = re.match(r"(-?\w+)\((\w+)\)", args[1])
            if match:
                imm = _parse_int(match.group(1))
                rs1 = _parse_reg(match.group(2))
            else:
                raise ValueError(f"Malformed memory operand: {args[1]}")
        else:
            rs1 = _parse_reg(args[1])
            imm = _parse_int(args[2]) if len(args) > 2 else 0
        return encode_i_type(Opcode.LW, rd, rs1, imm)

    elif op == "SW":
        rs2 = _parse_reg(args[0])
        if "(" in args[1]:
            match = re.match(r"(-?\w+)\((\w+)\)", args[1])
            if match:
                imm = _parse_int(match.group(1))
                rs1 = _parse_reg(match.group(2))
            else:
                raise ValueError(f"Malformed memory operand: {args[1]}")
        else:
            rs1 = _parse_reg(args[1])
            imm = _parse_int(args[2]) if len(args) > 2 else 0
        return encode_s_type(Opcode.SW, rs2, rs1, imm)

    elif op in ("BEQ", "BNE"):
        opcode = getattr(Opcode, op)
        rs1 = _parse_reg(args[0])
        rs2 = _parse_reg(args[1])
        imm = _parse_int(args[2])
        return encode_b_type(opcode, rs1, rs2, imm)

    elif op == "J":
        imm = _parse_int(args[0])
        return encode_j_type(Opcode.J, imm)

    else:
        raise ValueError(f"Unknown instruction opcode: '{op}'")


def assemble(asm_text: str) -> List[int]:
    """Assembles a multi-line string or list of assembly instructions into machine words."""
    machine_code = []
    lines = asm_text.strip().splitlines() if isinstance(asm_text, str) else asm_text
    for line in lines:
        word = assemble_line(line)
        if word is not None:
            machine_code.append(word)
    return machine_code

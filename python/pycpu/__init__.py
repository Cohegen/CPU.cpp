import os
import sys

if sys.platform == "win32":
    current_dir = os.path.dirname(os.path.abspath(__file__))
    try:
        os.add_dll_directory(current_dir)
    except (AttributeError, OSError):
        pass

    msys_bin = r"C:\msys64\ucrt64\bin"
    if os.path.isdir(msys_bin):
        try:
            os.add_dll_directory(msys_bin)
        except (AttributeError, OSError):
            pass

from ._pycpu_core import (
    Opcode,
    Register,
    NativeCPU32,
    encode_r_type,
    encode_i_type,
    encode_s_type,
    encode_b_type,
    encode_j_type,
)

from .cpu import CPU, RegisterAccessor
from .assembler import assemble, assemble_line

__all__ = [
    "CPU",
    "RegisterAccessor",
    "assemble",
    "assemble_line",
    "Opcode",
    "Register",
    "NativeCPU32",
    "encode_r_type",
    "encode_i_type",
    "encode_s_type",
    "encode_b_type",
    "encode_j_type",
]

import os
from setuptools import setup, find_packages
from pybind11.setup_helpers import Pybind11Extension, build_ext

cpu_root = os.path.abspath("..")
logic_include = os.path.abspath("../../Logic.cpp/include")

ext_modules = [
    Pybind11Extension(
        "pycpu._pycpu_core",
        [
            "src/bindings.cpp",
            os.path.join(cpu_root, "src", "InstructionDecoder.cpp"),
        ],
        include_dirs=[
            cpu_root,
            os.path.join(cpu_root, "include"),
            os.path.join(cpu_root, "components"),
            os.path.join(cpu_root, "single_cycle_cpu"),
            os.path.join(cpu_root, "single_cycle_cpu", "datapath"),
            os.path.join(cpu_root, "multi_cycle_cpu"),
            os.path.join(cpu_root, "multi_cycle_cpu", "datapath"),
            os.path.join(cpu_root, "pipelined_cpu"),
            os.path.join(cpu_root, "pipelined_cpu", "datapath"),
            logic_include,
            os.path.join(logic_include, "logic"),
        ],
        cxx_std=20,
    ),
]

setup(
    name="pycpu",
    version="0.1.0",
    packages=find_packages(),
    install_requires=["pylogic>=0.1.0"],
    ext_modules=ext_modules,
    cmdclass={"build_ext": build_ext},
)

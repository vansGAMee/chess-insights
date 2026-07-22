from setuptools import setup
from pybind11.setup_helpers import Pybind11Extension, build_ext

ext_modules = [
    Pybind11Extension(
        "stats_cpp",
        ["stats_cpp.cpp"],
        include_dirs=["/usr/include/nlohmann"],
        extra_compile_args=["-std=c++17", "-O3"],
        define_macros=[("PYBIND11_DETAILED_ERROR_MESSAGES", None)],
    ),
]

setup(
    name="stats_cpp",
    ext_modules=ext_modules,
    cmdclass={"build_ext": build_ext},
    zip_safe=False,
)
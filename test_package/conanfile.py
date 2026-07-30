"""Conan test_package for drogon-pay.

Consumer-side verification: builds a minimal Drogon host that
find_package(DrogonPay)-links the static library, loads PayPlugin from a
config block and asserts the plugin routes are actually reachable
(catches the classic static-library symbol-drop / route-404 failure mode).
"""

import os

from conan import ConanFile
from conan.tools.build import can_run
from conan.tools.cmake import CMake, cmake_layout


class DrogonPayTestPackage(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"
    test_type = "explicit"

    def requirements(self):
        self.requires(self.tested_reference_str)

    def layout(self):
        cmake_layout(self)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def test(self):
        if can_run(self):
            cmd = os.path.join(self.cpp.build.bindir, "test_package")
            self.run(cmd, env="conanrun")

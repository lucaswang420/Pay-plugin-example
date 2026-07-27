"""PayBackend Conan dependency descriptor.

Replaces the legacy conanfile.txt. Single source of truth for PayBackend's
C++ dependencies across Linux/Windows/macOS.

Dependency set and drogon version are aligned with the authforge project
(drogon/1.9.13 from Conan instead of a locally built Drogon; OpenSSL 3.x).
"""

from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps


class PayBackendConan(ConanFile):
    name = "pay-plugin"
    version = "1.0.0"
    settings = "os", "compiler", "build_type", "arch"

    default_options = {
        # --- drogon options (aligned with authforge) ---
        "drogon/*:with_orm": True,
        "drogon/*:with_postgres": True,
        "drogon/*:with_redis": True,
        "drogon/*:with_ctl": True,
        "drogon/*:with_sqlite": True,
        "drogon/*:with_brotli": True,

        # --- libcurl: keep a single TLS stack (OpenSSL) across the project ---
        "libcurl/*:with_ssl": "openssl",
    }

    def requirements(self):
        self.requires("drogon/1.9.13")
        self.requires("openssl/3.5.7", override=True)
        self.requires("jsoncpp/1.9.5")
        self.requires("hiredis/1.2.0")
        self.requires("libcurl/8.6.0")
        self.requires("brotli/1.1.0")
        self.requires("zlib/1.3.1", override=True)

    def build_requirements(self):
        self.test_requires("gtest/1.14.0")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

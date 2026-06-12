import os
from conan import ConanFile
from conan.tools.cmake import cmake_layout

class rin(ConanFile):
    settings = ['os', 'build_type', 'arch', 'compiler']
    generators = ['CMakeToolchain', 'CMakeDeps']

    def requirements(self):
        self.requires("sqlite3/3.50.4")
        self.requires("botan/3.9.0")
        self.requires("catch2/3.11.0")
        self.requires("qt/6.8.3")

    def build_requirements(self):
        self.tool_requires("cmake/[>=3.24]")
        if os.name == "posix":
            self.tool_requires("gcc/15.1.0")

    # def configure(self):
    #     self.options["qt/*"].shared = False
    #     self.options["qt/*"].qtmultimedia = False

    def layout(self):
        self.folders.generators = ""
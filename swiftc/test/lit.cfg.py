# -*- Python -*-

import os
import platform
import re
import subprocess
import sys

import lit.formats
import lit.util

from lit.llvm import llvm_config
from lit.llvm.subst import ToolSubst
from lit.llvm.subst import FindTool

# Configuration file for the 'lit' test runner.

# name: The name of this test suite.
config.name = "Swiftc"

# We prefer the lit internal shell which provides a better user experience on failures.
use_lit_shell = True
lit_shell_env = os.environ.get("LIT_USE_INTERNAL_SHELL")
if lit_shell_env:
    use_lit_shell = lit.util.pythonize_bool(lit_shell_env)

# testFormat: The test format to use to interpret tests.
config.test_format = lit.formats.ShTest(execute_external=not use_lit_shell)

# suffixes: A list of file extensions to treat as test files.
config.suffixes = [
    ".swift",
    ".ll",
    ".s",
    ".c",
    ".cpp",
]

config.substitutions.append(("%PATH%", config.environment["PATH"]))
config.substitutions.append(("%llvmshlibdir", config.llvm_shlib_dir))
config.substitutions.append(("%pluginext", config.llvm_plugin_ext))

llvm_config.use_default_substitutions()

# ask llvm-config about asserts
llvm_config.feature_config([("--assertion-mode", {"ON": "asserts"})])

# Targets
config.targets = frozenset(config.targets_to_build.split())
for arch in config.targets_to_build.split():
    config.available_features.add(arch.lower() + "-registered-target")

# excludes: A list of directories to exclude from the testsuite.
config.excludes = ["Inputs", "CMakeLists.txt", "README.txt", "LICENSE.txt"]

# Plugins (loadable modules)
if config.has_plugins:
    config.available_features.add("plugins")

# test_source_root: The root path where tests are located.
config.test_source_root = os.path.dirname(__file__)

# test_exec_root: The root path where tests should be run.
config.test_exec_root = os.path.join(config.swiftc_obj_root, "test")

# Tweak the PATH to include the tools dir.
llvm_config.with_environment("PATH", config.swiftc_tools_dir, append_path=True)
llvm_config.with_environment("PATH", config.llvm_tools_dir, append_path=True)

if config.swiftc_standalone_build:
    if config.swiftc_llvm_tools_dir != "":
        if config.llvm_tools_dir != config.swiftc_llvm_tools_dir:
            llvm_config.with_environment(
                "PATH", config.swiftc_llvm_tools_dir, append_path=True
            )

# For each occurrence of a swiftc tool name, replace it with the full path to
# the build directory holding that tool.
tools = [
    ToolSubst(
        "%swiftc",
        command=FindTool("swiftc"),
        unresolved="fatal",
    ),
    ToolSubst(
        "%swiftc_sc1",
        command=FindTool("swiftc"),
        extra_args=["-sc1"],
        unresolved="fatal",
    ),
]

# Add all the tools and their substitutions.
if config.swiftc_standalone_build:
    llvm_config.add_tool_substitutions(
        tools, [config.swiftc_llvm_tools_dir, config.llvm_tools_dir]
    )
else:
    llvm_config.add_tool_substitutions(tools, config.llvm_tools_dir)

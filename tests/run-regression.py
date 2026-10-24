#!/usr/bin/env python3
"""Run a focused C++ regression against an already built Makefile build (Linux).

Usage: python3 tests/run-regression.py tests/notebook-loading.cpp [build]
Reuses the application's compilation flags and objects without a second Qt build.
"""
import json
import pathlib
import shlex
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parent.parent
source = pathlib.Path(sys.argv[1]).resolve()
build = pathlib.Path(sys.argv[2] if len(sys.argv) > 2 else root / "build").resolve()
output = build / "regression" / source.stem
output.parent.mkdir(exist_ok=True)
entry = next(item for item in json.loads((build / "compile_commands.json").read_text())
             if pathlib.Path(item["file"]) == root / "src/main.cpp")
command = shlex.split(entry["command"])
command[command.index("-o") + 1] = str(output.with_suffix(".o"))
command[command.index("-c") + 1] = str(source)
subprocess.run(command, cwd=entry["directory"], check=True)
command = shlex.split((build / "src/CMakeFiles/qsnote.dir/link.txt").read_text())
command[command.index("CMakeFiles/qsnote.dir/main.cpp.o")] = str(output.with_suffix(".o"))
command[command.index("-o") + 1] = str(output)
subprocess.run(command, cwd=build / "src", check=True)
subprocess.run([str(output)], cwd=root, check=True, timeout=120)

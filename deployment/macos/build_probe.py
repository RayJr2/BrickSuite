#!/usr/bin/env python3
"""Relink a disposable package acceptance executable from actual Release objects."""
import argparse
import shlex
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('build', type=Path)
parser.add_argument('output', type=Path)
parser.add_argument('--source', type=Path, default=Path(__file__).with_name('PackageProbe.cpp'))
args = parser.parse_args()
build = args.build.resolve()
repo = Path(__file__).resolve().parents[2]
flags = (build / 'CMakeFiles/BrickSuite.dir/flags.make').read_text()
values = dict(line.split(' = ', 1) for line in flags.splitlines() if ' = ' in line)
obj = args.output.resolve().with_suffix('.o')
subprocess.run(['/usr/bin/c++', *shlex.split(values['CXX_FLAGS']), *shlex.split(values['CXX_DEFINES']), *shlex.split(values['CXX_INCLUDES']),
                '-I'+str(repo), '-c', str(args.source.resolve()), '-o', str(obj)], check=True)
command = shlex.split((build/'CMakeFiles/BrickSuite.dir/link.txt').read_text())
command = [str(obj) if value == 'CMakeFiles/BrickSuite.dir/src/main.cpp.o' else value for value in command]
command[command.index('-o')+1] = str(args.output.resolve())
subprocess.run(command, cwd=build, check=True)

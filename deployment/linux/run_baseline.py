#!/usr/bin/env python3
"""Run a command in the isolated Jammy filesystem; never use host build tools."""
import argparse
import os
from pathlib import Path
import subprocess


def command(root, work, repo, arguments, gui=False):
    cmd = ['bwrap', '--unshare-user', '--uid', '0', '--gid', '0', '--unshare-pid',
           '--unshare-uts', '--unshare-ipc', '--die-with-parent', '--new-session',
           '--bind', str(root), '/', '--proc', '/proc', '--dev', '/dev',
           '--ro-bind', str(repo), '/src', '--bind', str(work), '/work',
           '--ro-bind', '/etc/resolv.conf', '/etc/resolv.conf', '--chdir', '/work',
           '--clearenv', '--setenv', 'PATH', '/opt/qt/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin',
           '--setenv', 'HOME', '/root', '--setenv', 'LANG', 'C.UTF-8',
           '--setenv', 'DEBIAN_FRONTEND', 'noninteractive']
    if gui:
        cmd += ['--dev-bind', '/dev/dri', '/dev/dri'] if Path('/dev/dri').exists() else []
        if Path('/tmp/.X11-unix').exists(): cmd += ['--ro-bind', '/tmp/.X11-unix', '/tmp/.X11-unix']
        for key in ['DISPLAY', 'XAUTHORITY']:
            if os.environ.get(key):
                if key == 'XAUTHORITY': cmd += ['--ro-bind', os.environ[key], '/tmp/xauthority']
                cmd += ['--setenv', key, '/tmp/xauthority' if key == 'XAUTHORITY' else os.environ[key]]
        wayland = os.environ.get('WAYLAND_DISPLAY')
        runtime = os.environ.get('XDG_RUNTIME_DIR')
        if wayland and runtime:
            socket = Path(runtime) / wayland
            cmd += ['--ro-bind', str(socket), '/tmp/wayland-host', '--setenv', 'WAYLAND_DISPLAY', '/tmp/wayland-host']
    return cmd + ['--', *arguments]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--gui', action='store_true')
    parser.add_argument('arguments', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    arguments = args.arguments[1:] if args.arguments[:1] == ['--'] else args.arguments
    repo = Path(__file__).resolve().parents[2]
    return subprocess.call(command(args.root.resolve(), args.work.resolve(), repo, arguments, args.gui))

if __name__ == '__main__':
    raise SystemExit(main())

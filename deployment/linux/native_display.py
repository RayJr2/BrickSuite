#!/usr/bin/env python3
"""Start a disposable baseline X11 or native Wayland display for GUI acceptance."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
import time


def main():
    p=argparse.ArgumentParser();p.add_argument('platform',choices=['xcb','wayland']);p.add_argument('command',nargs=argparse.REMAINDER);args=p.parse_args()
    base=Path(tempfile.mkdtemp(prefix='bricksuite-display-'));base.chmod(0o700)
    env=os.environ.copy();env['XDG_RUNTIME_DIR']=str(base);env['LIBGL_ALWAYS_SOFTWARE']='1'
    with (base/'display.log').open('w') as log:
        if args.platform=='xcb':
            env['DISPLAY']=':77';cmd=['Xvfb',':77','-screen','0','1600x1000x24','-nolisten','tcp','-ac']
            ready=Path('/tmp/.X11-unix/X77')
        else:
            env['WAYLAND_DISPLAY']='wayland-acceptance';cmd=['weston','--backend=headless-backend.so','--use-gl','--width=1600','--height=1000','--socket=wayland-acceptance','--idle-time=0']
            ready=base/'wayland-acceptance'
        process=subprocess.Popen(cmd,env=env,stdout=log,stderr=log)
        try:
            for _ in range(100):
                if ready.exists():break
                if process.poll() is not None:raise RuntimeError('Display failed: '+(base/'display.log').read_text())
                time.sleep(.1)
            else:raise RuntimeError('Display did not become ready')
            command=args.command[1:] if args.command[:1]==['--'] else args.command
            return subprocess.call(command,env=env)
        finally:
            if process.poll() is None:process.terminate();process.wait(timeout=10)
if __name__=='__main__':raise SystemExit(main())

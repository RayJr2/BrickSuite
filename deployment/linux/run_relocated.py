#!/usr/bin/env python3
"""Run an extracted bundle with source/build/SDK paths hidden in a mount namespace."""
import argparse
import os
from pathlib import Path
import subprocess


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,help='Jammy rootfs; omit for newer-host smoke')
    p.add_argument('--bundle',type=Path,required=True);p.add_argument('--evidence',type=Path,required=True);p.add_argument('--ldraw',type=Path,required=True)
    p.add_argument('--installed',action='store_true',help='Mount the synthetic installed bundle at /opt/BrickSuite')
    p.add_argument('--host-display',action='store_true');p.add_argument('arguments',nargs=argparse.REMAINDER);args=p.parse_args()
    args.evidence.mkdir(parents=True,exist_ok=True)
    cmd=['bwrap','--unshare-user','--uid','0','--gid','0','--unshare-pid','--unshare-uts','--unshare-ipc','--new-session','--die-with-parent']
    if args.root:cmd+=['--ro-bind',str(args.root.resolve()),'/']
    else:cmd+=['--ro-bind','/','/','--tmpfs','/home']
    root=args.root.resolve() if args.root else Path('/')
    for name in ['root','work','src','opt']:
        if (root/name).exists():cmd+=['--tmpfs','/'+name]
    destination='/opt/BrickSuite' if args.installed else '/mnt/relocated bundle/BrickSuite'
    cmd+=['--proc','/proc','--dev','/dev','--tmpfs','/tmp','--tmpfs','/mnt',
          '--ro-bind',str(args.bundle.resolve()),destination,
          '--ro-bind',str(Path(__file__).resolve().parent),'/mnt/acceptance-tools',
          '--ro-bind',str(args.ldraw.resolve()),'/mnt/ldraw','--bind',str(args.evidence.resolve()),'/mnt/evidence',
          '--chdir',destination,'--clearenv',
          '--setenv','PATH','/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin',
          '--setenv','HOME','/root','--setenv','LANG','C.UTF-8','--setenv','BRICKSUITE_TEST_LDRAW','/mnt/ldraw']
    if not args.root:
        # AppArmor's world-accessible query node accepts write/read requests;
        # DBus needs it to evaluate policy. This exposes no policy mutation nodes.
        query=Path('/sys/kernel/security/apparmor/.access')
        if query.exists():cmd+=['--bind',str(query),str(query)]
    if args.host_display:
        if Path('/dev/dri').exists():cmd+=['--dev-bind','/dev/dri','/dev/dri']
        if Path('/tmp/.X11-unix').exists():cmd+=['--ro-bind','/tmp/.X11-unix','/tmp/.X11-unix']
        if os.environ.get('DISPLAY'):cmd+=['--setenv','DISPLAY',os.environ['DISPLAY']]
        if os.environ.get('XAUTHORITY'):cmd+=['--ro-bind',os.environ['XAUTHORITY'],'/tmp/xauthority','--setenv','XAUTHORITY','/tmp/xauthority']
        if os.environ.get('WAYLAND_DISPLAY'):
            socket=Path(os.environ.get('XDG_RUNTIME_DIR','/run/user/1000'))/os.environ['WAYLAND_DISPLAY']
            cmd+=['--ro-bind',str(socket),'/tmp/host-wayland','--setenv','WAYLAND_DISPLAY','/tmp/host-wayland']
    arguments=args.arguments[1:] if args.arguments[:1]==['--'] else args.arguments
    return subprocess.call([*cmd,'--',*arguments])
if __name__=='__main__':raise SystemExit(main())

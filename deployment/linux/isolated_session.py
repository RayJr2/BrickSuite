#!/usr/bin/env python3
"""Run acceptance/tests with disposable settings and a verified private Secret Service."""
import argparse
import os
from pathlib import Path
import re
import secrets
import subprocess
import sys
import tempfile
import time


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--platform',default='offscreen')
    p.add_argument('--credentials',type=Path,help='probe to write/restart/read synthetic credentials')
    p.add_argument('--inside-private-bus',action='store_true',help=argparse.SUPPRESS)
    p.add_argument('command',nargs=argparse.REMAINDER)
    args=p.parse_args()
    if not args.inside_private_bus:
        return subprocess.call(['dbus-run-session','--',sys.executable,str(Path(__file__).resolve()),'--inside-private-bus',*sys.argv[1:]])
    args.output.mkdir(parents=True,exist_ok=True)
    base=Path(tempfile.mkdtemp(prefix='bricksuite-acceptance-'))
    env=os.environ.copy()
    wayland=env.get('WAYLAND_DISPLAY')
    if wayland and not os.path.isabs(wayland):env['WAYLAND_DISPLAY']=str(Path(env['XDG_RUNTIME_DIR'])/wayland)
    for key in ['GNOME_KEYRING_CONTROL','GNOME_KEYRING_PID','DBUS_STARTER_ADDRESS','DBUS_STARTER_BUS_TYPE','LD_LIBRARY_PATH','QT_PLUGIN_PATH','QT_QPA_PLATFORM_PLUGIN_PATH']:env.pop(key,None)
    for key,name in [('XDG_DATA_HOME','data'),('XDG_CONFIG_HOME','config'),('XDG_CACHE_HOME','cache'),('XDG_RUNTIME_DIR','runtime'),('TMPDIR','tmp')]:
        (base/name).mkdir(mode=0o700);env[key]=str(base/name)
    env['M39_AUDIT_OUTPUT']=str(args.output.resolve());env['QT_QPA_PLATFORM']=args.platform
    password=secrets.token_hex(24).encode();daemon=None
    log=(args.output/'keyring.log').open('w')
    def start():
        nonlocal daemon
        daemon=subprocess.Popen(['gnome-keyring-daemon','--foreground','--unlock','--components=secrets','--control-directory='+str(base/'runtime/keyring')],env=env,stdin=subprocess.PIPE,stdout=log,stderr=log)
        daemon.stdin.write(password);daemon.stdin.close()
        for _ in range(60):
            r=subprocess.run(['gdbus','call','--session','--dest','org.freedesktop.DBus','--object-path','/org/freedesktop/DBus','--method','org.freedesktop.DBus.GetConnectionUnixProcessID','org.freedesktop.secrets'],env=env,text=True,capture_output=True)
            if r.returncode==0:
                pid=int(re.search(r'uint32 (\d+)',r.stdout)[1])
                if pid!=daemon.pid:raise RuntimeError('Unexpected Secret Service owner; refusing credential access')
                print('Verified private Secret Service owner',flush=True);return
            time.sleep(.1)
        raise RuntimeError('Private keyring startup failed')
    def stop():
        if daemon and daemon.poll() is None:daemon.terminate();daemon.wait(timeout=10)
    def probe(mode,extra=None):
        with (args.output/(mode+'.log')).open('w') as stream:
            subprocess.run([str(args.credentials),mode],env=extra or env,stdout=stream,stderr=subprocess.STDOUT,check=True,timeout=90)
        print(mode+' passed',flush=True)
    try:
        start()
        if args.credentials:
            probe('cred-write');stop();start();probe('cred-read');probe('cred-clean')
            failed_env=env.copy();failed_env['DBUS_SESSION_BUS_ADDRESS']='unix:path='+str(base/'nonexistent-bus')
            probe('cred-unavailable',failed_env)
        command=args.command[1:] if args.command[:1]==['--'] else args.command
        if command:return subprocess.call(command,env=env)
        return 0
    finally:
        stop();log.close()
        # The temporary keyring and synthetic settings are retained for diagnosis;
        # no credential values are written to the evidence directory or output.
if __name__=='__main__':raise SystemExit(main())

#!/usr/bin/env python3
"""Compile the locked OpenSSL 3.5 LTS runtime with Jammy GCC 11."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess


def main():
    p=argparse.ArgumentParser();p.add_argument('--jobs',default='6');args=p.parse_args()
    version=json.loads(Path(__file__).with_name('source-lock.json').read_text())['openssl']['version']
    source=Path('/work/sources')/('openssl-'+version)
    env={**os.environ,'CC':'gcc-11'}
    subprocess.run(['./Configure','linux-x86_64','shared','no-tests','no-module','--prefix=/usr','--openssldir=/etc/ssl','--libdir=lib','-ffile-prefix-map=/work=.'],cwd=source,env=env,check=True)
    subprocess.run(['make','-j'+args.jobs],cwd=source,env=env,check=True)
    subprocess.run(['make','DESTDIR=/work/openssl-prefix','install_sw'],cwd=source,env=env,check=True)
    for pattern in ['libssl.so*','libcrypto.so*']:
        for src in Path('/work/openssl-prefix/usr/lib').glob(pattern):
            dest=Path('/opt/qt/lib')/src.name
            if dest.exists() or dest.is_symlink():dest.unlink()
            if src.is_symlink():dest.symlink_to(src.readlink())
            else:shutil.copy2(src,dest)
if __name__=='__main__':main()

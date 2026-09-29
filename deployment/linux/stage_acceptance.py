#!/usr/bin/env python3
"""Extract the actual release archive and add clearly disposable acceptance tools."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tarfile
from audit_bundle import audit


def main():
    p=argparse.ArgumentParser();p.add_argument('archive',type=Path);p.add_argument('destination',type=Path);p.add_argument('--build',type=Path,default=Path('/work/build'));p.add_argument('--probes',type=Path,default=Path('/work/probes'));p.add_argument('--source-sha',required=True);args=p.parse_args()
    args.destination.mkdir(parents=True,exist_ok=False)
    with tarfile.open(args.archive) as tf:
        for member in tf.getmembers():
            if not (args.destination/member.name).resolve().is_relative_to(args.destination.resolve()):raise ValueError('Unsafe archive member')
            if member.issym() and (Path(member.linkname).is_absolute() or not (args.destination/member.name).parent.joinpath(member.linkname).resolve().is_relative_to(args.destination.resolve())):raise ValueError('Unsafe archive symlink')
            if not (member.isfile() or member.isdir() or member.issym()):raise ValueError('Unexpected archive entry')
        tf.extractall(args.destination)
    bundle=args.destination/'BrickSuite';audit(bundle,args.source_sha)
    # Acceptance executables are never included in the distributable archive.
    for src in [args.probes/'PackageProbe',args.probes/'NativeProbe',*[args.build/name for name in ['LDrawPrintPreparationServiceTest','FitCalibrationGenerationServiceTest','SecureHostFoundationTest','McutQueueWakeupTest']]]:
        dest=bundle/'bin'/src.name;shutil.copy2(src,dest)
        subprocess.run(['strip','--strip-unneeded',str(dest)],check=True)
        subprocess.run(['patchelf','--set-rpath','$ORIGIN/../lib',str(dest)],check=True)
    audit(bundle,args.source_sha)
if __name__=='__main__':main()

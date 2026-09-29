#!/usr/bin/env python3
"""Install/reinstall/uninstall a real archive in a retained synthetic root only."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tarfile


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('archive',type=Path);p.add_argument('destination',type=Path);args=p.parse_args()
    args.destination=args.destination.resolve();args.destination.mkdir(parents=True,exist_ok=False)
    extracted=args.destination/'extracted';extracted.mkdir()
    with tarfile.open(args.archive) as archive:
        for member in archive.getmembers():
            target=extracted/member.name
            if not target.resolve().is_relative_to(extracted):raise RuntimeError('Unsafe archive path')
            if member.issym() and (Path(member.linkname).is_absolute() or not (target.parent/member.linkname).resolve().is_relative_to(extracted)):raise RuntimeError('Unsafe archive symlink')
            if not (member.isdir() or member.isfile() or member.issym()):raise RuntimeError('Unsupported archive member')
        archive.extractall(extracted)
    bundle=extracted/'BrickSuite';root=args.destination/'system';root.mkdir()
    sentinel=root/'home/test/.local/share/BrickSuite/BrickSuite.db';sentinel.parent.mkdir(parents=True);sentinel.write_text('synthetic user data: preserve')
    with (args.destination/'lifecycle.log').open('w') as log:
        for script in ['install.sh','install.sh','uninstall.sh','install.sh']:
            subprocess.run([str(bundle/script),'--root',str(root)],input='y\n',text=True,stdout=log,stderr=subprocess.STDOUT,check=True)
            assert sentinel.read_text()=='synthetic user data: preserve'
            if script=='uninstall.sh':assert not (root/'opt/BrickSuite').exists()
    installed=root/'opt/BrickSuite'
    report=json.loads((installed/'share/abi-audit.json').read_text())
    for row in report['elf']:assert hashlib.sha256((installed/row['path']).read_bytes()).hexdigest()==row['sha256']
    assert (root/'usr/share/applications/bricksuite.desktop').is_file()
    assert (root/'usr/share/icons/hicolor/256x256/apps/bricksuite.png').is_file()
    assert (root/'usr/local/bin/bricksuite').is_symlink()
    result={'archive_sha256':hashlib.sha256(args.archive.read_bytes()).hexdigest(),'elf_count':len(report['elf']),'install':True,'reinstall':True,'uninstall':True,'user_data_preserved':True,'installed_bundle':str(installed)}
    (args.destination/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))


if __name__=='__main__':main()

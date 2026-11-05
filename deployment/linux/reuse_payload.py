"""Explicit private installer candidate; qualified archive bytes remain authoritative."""
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import tarfile
import tempfile

from audit_bundle import audit, digest, POLICY
from package_linux import archive_tree, write_json

ALLOWED_DELTA={'deployment/linux/installer.py','deployment/linux/test_installer.py'}
REPLACED={'BrickSuite/installer.py','BrickSuite/share/build-metadata.json'}


def git(source,*args):
    return subprocess.check_output(['git','-C',str(source),*args])


def clean_head(source):
    if git(source,'status','--porcelain','--untracked-files=all').strip():
        raise ValueError('Installer reuse requires a clean source checkout')
    return git(source,'rev-parse','HEAD').decode().strip()


def source_delta(source,payload,packaging):
    if not re.fullmatch('[0-9a-f]{40}',payload):raise ValueError('Invalid payload commit')
    subprocess.run(['git','-C',str(source),'merge-base','--is-ancestor',payload,packaging],check=True)
    changed=set(filter(None,git(source,'diff','--name-only','--no-renames','-z',payload,packaging).decode().split('\0')))
    if not changed <= ALLOWED_DELTA:raise ValueError('Disallowed source delta: '+', '.join(sorted(changed-ALLOWED_DELTA)))
    return sorted(changed)


def inventory(archive):
    """Validate archive structure before extraction; include every byte, mode and link."""
    rows={}
    with tarfile.open(archive) as stream:
        for member in stream:
            path=PurePosixPath(member.name)
            if path.is_absolute() or '..' in path.parts or not path.parts or path.parts[0]!='BrickSuite' or str(path)!=member.name.rstrip('/'):
                raise ValueError('Unsafe archive path: '+member.name)
            if member.name in rows or not (member.isfile() or member.isdir() or member.issym()) or member.mode & 0o7000:
                raise ValueError('Duplicate/unsupported archive member: '+member.name)
            if member.issym():
                target=PurePosixPath(member.linkname)
                if target.is_absolute():raise ValueError('Absolute archive symlink')
                parts=list(path.parent.parts)
                for part in target.parts:
                    if part=='..':
                        if len(parts)<=1:raise ValueError('Escaping archive symlink')
                        parts.pop()
                    elif part!='.':parts.append(part)
            rows[member.name]={'kind':'file' if member.isfile() else 'dir' if member.isdir() else 'link','mode':member.mode,
                               'value':hashlib.sha256(stream.extractfile(member).read()).hexdigest() if member.isfile() else member.linkname if member.issym() else ''}
    for name in rows:
        for parent in PurePosixPath(name).parents:
            if str(parent)=='.':break
            if str(parent) not in rows or rows[str(parent)]['kind']!='dir':raise ValueError('Non-directory archive ancestor: '+name)
    return rows


def extract(archive,destination):
    # inventory() has rejected special files, traversal and symlink ancestors.
    inventory(archive)
    with tarfile.open(archive) as stream:
        members=stream.getmembers()
        for member in sorted(members,key=lambda m:len(PurePosixPath(m.name).parts)):
            path=destination/member.name
            if member.isdir():path.mkdir(exist_ok=True)
            elif member.isfile():
                with stream.extractfile(member) as src,path.open('wb') as dest:shutil.copyfileobj(src,dest)
            else:path.symlink_to(member.linkname)
            if not member.issym():path.chmod(member.mode)


def compare_payload(before,after,installer):
    if before.keys()!=after.keys():raise ValueError('Candidate archive inventory changed')
    for name,row in before.items():
        other=after[name]
        if name not in REPLACED and row!=other:raise ValueError('Reused payload hash/type/mode mismatch: '+name)
        if name in REPLACED and (other['kind']!='file' or other['mode']!=row['mode']):raise ValueError('Replacement type/mode mismatch')
    if after['BrickSuite/installer.py']['value']!=hashlib.sha256(installer).hexdigest():raise ValueError('Packaged installer byte mismatch')


def reuse(args):
    archive=args.reuse_qualified_payload
    if not archive:raise ValueError('Payload reuse requires explicit opt-in')
    expected=args.qualified_archive_sha256
    if not expected or not re.fullmatch('[0-9a-f]{64}',expected) or digest(archive)!=expected:
        raise ValueError('Qualified archive hash mismatch')
    packaging=clean_head(args.source)
    if packaging[:7] not in str(args.output):raise ValueError('Use a distinct candidate output path containing the packaging commit')
    if args.output.exists():raise ValueError('Candidate output already exists')
    before=inventory(archive)
    record=json.loads((args.build/'source-record.json').read_text())
    args.output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.installer-reuse-',dir=args.output.parent) as temporary:
        work=Path(temporary);extract(archive,work);bundle=work/'BrickSuite'
        metadata=json.loads((bundle/'share/build-metadata.json').read_text())
        payload=metadata['source_sha']
        if not re.fullmatch(r'\d+\.\d+\.\d+',metadata.get('version','')):raise ValueError('Invalid application version')
        if record.get('source_sha')!=payload or any(record.get(k)!=metadata.get(k) for k in ['schema','protocol']):raise ValueError('Payload build record mismatch')
        if metadata.get('package_kind')!='ubuntu-22.04-baseline-release' or metadata.get('payload_reused') or metadata.get('test_gate')!='passed':raise ValueError('Not a qualified single-source baseline payload')
        if metadata.get('architecture')!='x86_64' or metadata.get('glibc_floor')!='2.35':raise ValueError('Wrong baseline payload')
        delta=source_delta(args.source,payload,packaging)
        tests=json.loads((bundle/'share/test-summary.json').read_text())
        retained=json.loads((args.build.parent/'evidence/tests/summary.json').read_text())
        configured=json.loads((args.build/'configured-tests.json').read_text())
        if tests!=retained or tests['full']!={'passed':configured['count'],'failed':0,'skipped':0} or any(tests[k]['passed']<=0 or tests[k]['failed'] or tests[k]['skipped'] for k in ['full','focused']):raise ValueError('Qualified test evidence mismatch')
        dependencies=json.loads((bundle/'share/dependencies.json').read_text())
        if dependencies['plugins']!=POLICY['plugins'] or set(dependencies['system_allowed_sonames'])!=set(POLICY['system_sonames']):raise ValueError('Dependency contract mismatch')
        previous=json.loads((bundle/'share/abi-audit.json').read_text())
        if audit(bundle,payload,resolve=False)!=previous:raise ValueError('Qualified ELF audit/hash mismatch')
        installer=git(args.source,'show',packaging+':deployment/linux/installer.py')
        (bundle/'installer.py').write_bytes(installer)
        metadata.update(package_kind='installer-reuse-candidate',payload_reused=True,
                        payload_source_commit=payload,packaging_source_commit=packaging,installer_source_commit=packaging,
                        qualified_archive_sha256=expected,allowed_source_delta=delta,
                        test_gate='reused qualified payload; not a new application test run',
                        reuse_tool_hashes={name:digest(Path(__file__).with_name(name)) for name in ['reuse_payload.py','package_linux.py','audit_bundle.py']})
        # source_sha retains its original meaning for binaries and the ABI report.
        metadata['packaging_tool_hashes']['installer.py']=hashlib.sha256(installer).hexdigest()
        write_json(bundle/'share/build-metadata.json',metadata)
        if audit(bundle,payload,resolve=False)!=previous:raise ValueError('Candidate ELF payload changed')
        candidate=work/('BrickSuite-v'+metadata['version']+'-Linux-x86_64.tar.gz')
        epoch=int(git(args.source,'show','-s','--format=%ct',packaging))
        archive_tree(bundle,candidate,epoch)
        compare_payload(before,inventory(candidate),installer)
        if digest(archive)!=expected or clean_head(args.source)!=packaging:raise ValueError('Inputs changed during reuse')
        result={'archive':str(args.output/candidate.name),'bytes':candidate.stat().st_size,'sha256':digest(candidate),
                'payload_source_commit':payload,'packaging_source_commit':packaging,'package_kind':metadata['package_kind'],
                'payload_reused':True,'unchanged_entries':len(before)-len(REPLACED),'elf_count':previous['elf_count'],
                'required_license_files':len(metadata['required_licenses']),'installer_matches_commit':True}
        # Publish only a complete private candidate directory, never the normal release pointer.
        completed=work/'completed';completed.mkdir()
        candidate.rename(completed/candidate.name)
        (completed/(candidate.name+'.sha256')).write_text(result['sha256']+'  '+candidate.name+'\n')
        for name in ['build-metadata.json','dependencies.json','abi-audit.json']:shutil.copy2(bundle/'share'/name,completed/name)
        write_json(completed/'reuse-verification.json',result)
        completed.rename(args.output)
        return result

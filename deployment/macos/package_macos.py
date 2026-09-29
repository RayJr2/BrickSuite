#!/usr/bin/env python3
"""Stage, audit, and ad-hoc sign a disposable single-architecture macOS app."""
import argparse
import json
import os
import plistlib
import shutil
import subprocess
import tempfile
from pathlib import Path
from audit_bundle import audit, binaries
from sign_bundle import sign_bundle


PLUGINS = (
    'platforms/libqcocoa.dylib', 'sqldrivers/libqsqlite.dylib',
    'tls/libqopensslbackend.dylib', 'networkinformation/libqapplenetworkinformation.dylib',
    'styles/libqmacstyle.dylib', 'iconengines/libqsvgicon.dylib',
    'imageformats/libqgif.dylib', 'imageformats/libqico.dylib',
    'imageformats/libqjpeg.dylib', 'imageformats/libqsvg.dylib',
    'imageformats/libqwebp.dylib',
)


def run(*args):
    subprocess.run([str(a) for a in args], check=True)


def copy(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)


def notices(source, destination):
    # Keep relative paths so identical license names never overwrite one another.
    for path in source.rglob('*'):
        if path.is_file() and not path.is_symlink() and any(
            path.name.upper().startswith(n) for n in ('LICENSE', 'LICENCE', 'COPYING', 'COPYRIGHT', 'NOTICE', 'AUTHORS')
        ):
            copy(path, destination / path.relative_to(source))
    for path in source.rglob('qt_attribution.json'):
        copy(path, destination / path.relative_to(source))
        entries = json.loads(path.read_text(), strict=False)
        for entry in entries if isinstance(entries, list) else [entries]:
            for name in entry.get('LicenseFile', '').split(','):
                license_file = path.parent / name.strip()
                if name.strip() and license_file.is_file():
                    copy(license_file, destination / license_file.relative_to(source))


def normalize_binary(binary, frameworks):
    """Make one Mach-O self-contained using the bundle framework directory."""
    load = subprocess.check_output(['otool', '-l', str(binary)], text=True)
    import re
    for rpath in re.findall(r'cmd LC_RPATH\s+cmdsize \d+\s+path (.*?) \(offset', load):
        run('install_name_tool', '-delete_rpath', rpath, binary)
    relative = os.path.relpath(frameworks, binary.parent)
    run('install_name_tool', '-add_rpath', '@loader_path/' + relative, binary)
    deps = subprocess.check_output(['otool', '-L', str(binary)], text=True).splitlines()[1:]
    for line in deps:
        dep = line.strip().split(' (compatibility')[0]
        if dep.startswith(('/System/Library/', '/usr/lib/')):
            continue
        if '.framework/' in dep:
            name = dep.split('/')
            index = next(i for i, part in enumerate(name) if part.endswith('.framework'))
            target = '/'.join(name[index:])
        else:
            target = Path(dep).name
        if not (frameworks / target).exists():
            raise RuntimeError(f'Missing dependency {dep} for {binary}')
        run('install_name_tool', '-change', dep, '@rpath/' + target, binary)
    if binary.suffix == '.dylib':
        run('install_name_tool', '-id', '@rpath/' + binary.name, binary)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('build', 'qt', 'qt-source', 'openssl', 'openssl-source', 'lib3mf-source', 'output'):
        parser.add_argument('--' + name, required=True, type=Path)
    parser.add_argument('--source-sha', help='Immutable source commit recorded in bundle metadata')
    parser.add_argument('--binary-dir', type=Path, help='Configuration-specific binary directory (defaults to build)')
    parser.add_argument('--probe', type=Path, help='Optional disposable acceptance executable; omit for release staging')
    parser.add_argument('--arch', required=True, choices=('arm64', 'x86_64'))
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    bundle = args.output.resolve()
    if bundle.exists() or bundle.suffix != '.app':
        raise SystemExit('Output must be a new .app path; existing output is never removed.')
    binary_dir = args.binary_dir or args.build
    shutil.copytree(binary_dir / 'BrickSuite.app', bundle, symlinks=True)
    contents = bundle / 'Contents'
    frameworks = contents / 'Frameworks'
    frameworks.mkdir(exist_ok=True)
    helper = contents / 'MacOS/BrickSuiteMeshBooleanWorker'
    helper_source = binary_dir / 'BrickSuiteMeshBooleanWorker'
    if not helper.exists():
        copy(helper_source, helper)
    deployed = []
    for plugin in PLUGINS:
        target = contents / 'PlugIns' / plugin
        copy(args.qt / 'plugins' / plugin, target)
        deployed.append(target)
    # Include runtime-loaded SSL explicitly; macdeployqt cannot infer it from Qt.
    for name in ('libssl.3.dylib', 'libcrypto.3.dylib'):
        copy(args.openssl / 'lib' / name, frameworks / name)
    extras = [helper, *deployed, frameworks / 'libssl.3.dylib', frameworks / 'libcrypto.3.dylib']
    if args.probe:
        probe = contents / 'MacOS/BrickSuitePackageProbe'
        copy(args.probe, probe)
        extras.append(probe)
    run(args.qt / 'bin/macdeployqt', bundle, '-no-plugins', '-no-strip', '-always-overwrite',
        *('-executable=' + str(p) for p in extras))
    for binary in binaries(bundle):
        normalize_binary(binary, frameworks)
    resources = contents / 'Resources'
    resources.mkdir(exist_ok=True)
    (resources / 'qt.conf').write_text('[Paths]\nPlugins = PlugIns\n')
    # Convert the existing application artwork; do not invent a new release icon.
    with tempfile.TemporaryDirectory(prefix='bricksuite-icon-') as temp:
        iconset = Path(temp) / 'BrickSuite.iconset'
        iconset.mkdir()
        for size in (16, 32, 128, 256, 512):
            run('sips', '-s', 'format', 'png', '-z', size, size,
                repo / 'resources/icons/bricksuite_multi.ico', '--out', iconset / f'icon_{size}x{size}.png')
            run('sips', '-s', 'format', 'png', '-z', size*2, size*2,
                repo / 'resources/icons/bricksuite_multi.ico', '--out', iconset / f'icon_{size}x{size}@2x.png')
        run('iconutil', '-c', 'icns', iconset, '-o', resources / 'BrickSuite.icns')
    plist_path = contents / 'Info.plist'
    with plist_path.open('rb') as stream:
        plist = plistlib.load(stream)
    if args.source_sha:
        import re
        if not re.fullmatch(r'[0-9a-f]{40}', args.source_sha):
            raise ValueError('Expected a full Git commit SHA')
        plist['BrickSuiteSourceCommit'] = args.source_sha
        plist['BrickSuiteArchitecture'] = args.arch
    plist['CFBundleIconFile'] = 'BrickSuite.icns'
    # Qt 6.10 widgets retain the established appearance when built using SDK 26.
    plist['UIDesignRequiresCompatibility'] = True
    with plist_path.open('wb') as stream:
        plistlib.dump(plist, stream)
    licenses = resources / 'Licenses'
    copy(repo / 'LICENSE', licenses / 'BrickSuite/LICENSE')
    copy(repo / 'THIRD_PARTY_NOTICES.md', licenses / 'THIRD_PARTY_NOTICES.md')
    shutil.copytree(args.build / 'deployment/licenses/MCUT', licenses / 'MCUT')
    notices(args.lib3mf_source, licenses / 'lib3mf')
    notices(args.openssl_source, licenses / 'OpenSSL')
    for module in ('qtbase', 'qtsvg', 'qtimageformats', 'qtwebsockets'):
        notices(args.qt_source / module, licenses / 'Qt' / module)
    shutil.copytree(args.qt_source / 'LICENSES', licenses / 'Qt/LICENSES')
    result = audit(bundle, args.arch, '13.0')
    report = bundle.with_suffix('.audit.json')
    report.write_text(json.dumps(result, indent=2) + '\n')
    if result['errors']:
        raise RuntimeError('\n'.join(result['errors']))
    sign_bundle(bundle)
    print(f'Packaged {len(result["binaries"])} {args.arch} Mach-O files; audit: {report}')


if __name__ == '__main__':
    main()

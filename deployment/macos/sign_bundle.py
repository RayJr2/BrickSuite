"""Deterministic inside-out ad-hoc signing, independent of architecture/signature state."""
import plistlib
import subprocess
from pathlib import Path

from audit_bundle import binaries


def signing_order(bundle):
    bundle = Path(bundle).resolve()
    info = plistlib.loads((bundle/'Contents/Info.plist').read_bytes())
    name = info['CFBundleExecutable']
    if Path(name).name != name:
        raise ValueError('Invalid bundle executable name')
    main = bundle/'Contents/MacOS'/name
    helper = bundle/'Contents/MacOS/BrickSuiteMeshBooleanWorker'
    leaves = list(binaries(bundle))
    for required in (main, helper):
        if required not in leaves:
            raise ValueError(f'Missing required Mach-O: {required}')
    # Framework binaries first, then their enclosing framework bundles. Never
    # depend on linker-generated signatures being present on any architecture.
    framework_leaves = [p for p in leaves if any(part.endswith('.framework') for part in p.relative_to(bundle).parts)]
    frameworks = sorted((p for p in bundle.rglob('*.framework') if p.is_dir() and not p.is_symlink()),
                        key=lambda p: (-len(p.parts), str(p)))
    remaining = [p for p in leaves if p != main and p not in framework_leaves]
    # Libraries/plugins precede helper executables; all precede the main binary.
    remaining.sort(key=lambda p: (p.parent == bundle/'Contents/MacOS', str(p)))
    return [*framework_leaves, *frameworks, *remaining, main, bundle]


def sign_bundle(bundle):
    order = signing_order(bundle)
    for path in order:
        print(f'Signing {path}', flush=True)
        subprocess.run(['codesign', '--force', '--sign', '-', '--timestamp=none', str(path)], check=True)
    subprocess.run(['codesign', '--verify', '--deep', '--strict', '--verbose=2', str(order[-1])], check=True)

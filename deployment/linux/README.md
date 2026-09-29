# M39.L2 Linux baseline and relocatable tarball

The release floor is **Ubuntu 22.04 x86_64, glibc 2.35, GCC 11**. A build made on
the Ubuntu 26.04 development host is not a Linux release artifact. These scripts
use a rootless Bubblewrap namespace with Ubuntu 22.04 userspace; they do not
install packages into or change the host OS. The host kernel is shared.

## Install the release on Ubuntu

Extract `BrickSuite-v0.4.0-Linux-x86_64.tar.gz`, enter `BrickSuite`, and run
`./install.sh`. Approve missing runtime packages if requested, then installation.
Launch **Applications → BrickSuite** or `bricksuite`. No SDK or build tools are
required. The complete user instructions are in [RUNTIME.md](RUNTIME.md).

Ubuntu 22.04+ x86_64 is the automatic installation scope. The installer checks
actual system libraries/commands/services/fonts using `runtime-requirements.json`,
whose SONAME set is regression-checked against `policy.json`. Private Qt, MCUT,
OpenSSL and ICU are not installed through APT. Newer Ubuntu's GLib t64 package
name is handled only when its runtime is missing. A working unlocked keyring is
required at runtime, not during installation.

Application files go to `/opt/BrickSuite`; desktop entry to
`/usr/share/applications/bricksuite.desktop`; icon to
`/usr/share/icons/hicolor/256x256/apps/bricksuite.png`; command symlink to
`/usr/local/bin/bricksuite`. None of these locations holds mutable user state.
Run `/opt/BrickSuite/uninstall.sh` to remove only these managed files.

The installer verifies all audited ELF hashes and relative RPATH records, then
runs the system loader against each installed ELF. It uses Python's standard
library and `ldd`, not binutils or a Qt SDK. These integrity checks detect damaged
payloads; the adjacent checksum/manifest is not a cryptographic publisher signature.
Installation serializes on a lock, stages on the target filesystem, and rolls
back caught copy/replacement/verification failures. It is not a power-loss-proof
filesystem transaction; an interrupted rename can leave a `.previous-*` backup
for manual recovery. Close a running BrickSuite before upgrade/uninstall.
Conflicting unmanaged files are refused rather than overwritten.

Installer-only regressions (all filesystem writes use temporary roots):

```sh
python3 deployment/linux/run_baseline.py --root build/linux-l2/rootfs \
  --work build/linux-l2/work -- python3 /src/deployment/linux/test_installer.py
```

For an actual extracted bundle, `./install.sh --root /tmp/bricksuite-install-test`
exercises staging under a synthetic root; it never invokes host sudo/APT.
Requirements must already be present. Its absolute command symlink is intended
for mounting/chrooting that root, not launching from the developer's real `/usr`.
`./uninstall.sh --root /tmp/bricksuite-install-test` removes the synthetic install.
Run the real archive lifecycle check in the baseline image:

```sh
python3 deployment/linux/run_baseline.py --root build/linux-l2/rootfs \
  --work build/linux-l2/work -- python3 /src/deployment/linux/test_install_archive.py \
  /work/release/BrickSuite-v0.4.0-Linux-x86_64.tar.gz /work/installer-acceptance
```

This installs, reinstalls, uninstalls and reinstalls the archive under a fresh
synthetic root, checking user-data preservation and ELF identity. For installed
runtime tests, `run_relocated.py --installed --bundle <synthetic-root>/opt/BrickSuite`
mounts the installed files at `/opt/BrickSuite` while hiding the development SDK;
pass `--bundle /opt/BrickSuite` to the acceptance driver (with disposable probes
staged as described below). Never use the developer machine’s real system paths
for these tests.

A separate clean Ubuntu laptop still needs the manual installation/menu/launch
acceptance described in RUNTIME.md; these tests do not replace it.

## Locked inputs

`source-lock.json` records URLs, SHA-256 hashes and Git revisions. Inputs are:

- Ubuntu Base 22.04.5; signed Ubuntu APT snapshot `20260928T000000Z`.
- Official Qt 6.10.3 Linux archives for qtbase, SVG, WebSockets, image formats,
  Wayland and ICU 73.2. The archive labels are not an ABI guarantee: every shipped
  ELF is audited. The Qt libraries inspected here require at most GLIBC 2.34,
  GLIBCXX 3.4.29 and CXXABI 1.3.13, compatible with Jammy's runtime.
- OpenSSL 3.5.8 LTS, built from the hash-verified source with GCC 11, shared,
  `no-module`, prefix `/usr`, config `/etc/ssl`. Its built-in default provider is
  used; no development-host SSL libraries are copied. The SDK's TLS runtime and
  the packaged TLS runtime use this same build.
- MCUT `047d75ffe6e33ede572cb25217047a4756188401`, with the committed
  `cmake/BrickSuiteMcutLinuxQueue.cmake` generated-header overlay. It prevents a
  lost queue wakeup on Linux. Downloaded upstream sources stay unmodified.
  `McutQueueWakeup` and `McutMeshBoolean` cover the correction and worker behavior.
- lib3mf `64bb454d1fcb53effa57d3cef752a10d740d41a2`, statically linked, including its
  pinned bundled zlib/libzip/crypto dependencies. Final executable ABI checks
  cover the statically linked code. Its submodule revisions are recorded.
- A SHA-256-pinned LDraw complete archive, enabling all eight source-backed CTest
  registrations. Preserve this archive in a content-addressed cache: upstream
  `complete.zip` is mutable, and a changed download must fail hash verification.

See [Ubuntu snapshots](https://snapshot.ubuntu.com/),
[Qt platform documentation](https://doc.qt.io/qt-6.10/supported-platforms.html), and
[OpenSSL sources and maintenance](https://www.openssl-library.org/source/).

## Build and test

Host requirements: Linux x86_64, Python 3, tar/xz, Bubblewrap with unprivileged user
namespaces enabled, network access, and sufficient disk/RAM for all test targets.
The default build uses six compiler jobs. On a native Ubuntu 22.04 CI runner,
these same inside-image commands can run directly after installing the locked
prerequisites; do not silently substitute a newer runner image.

On a fresh Ubuntu 22.04 runner, install the host namespace tools with
`sudo apt-get update` and `sudo apt-get install -y bubblewrap python3 xz-utils ca-certificates`.
These host tools do not supply the release compiler/runtime.

From the repository root:

```sh
git status --short
git log -5 --oneline
python3 deployment/linux/bootstrap.py --directory build/linux-l2
python3 deployment/linux/run_baseline.py --root build/linux-l2/rootfs \
  --work build/linux-l2/work -- python3 /src/deployment/linux/build_release.py
python3 deployment/linux/run_baseline.py --root build/linux-l2/rootfs \
  --work build/linux-l2/work -- python3 /src/deployment/linux/isolated_session.py \
  --output /work/evidence/tests-session -- \
  python3 /src/deployment/linux/run_tests.py
python3 deployment/linux/run_baseline.py --root build/linux-l2/rootfs \
  --work build/linux-l2/work -- python3 /src/deployment/linux/test_release_tools.py
python3 deployment/linux/run_baseline.py --root build/linux-l2/rootfs \
  --work build/linux-l2/work -- python3 /src/deployment/linux/build_probe.py
python3 deployment/linux/run_baseline.py --root build/linux-l2/rootfs \
  --work build/linux-l2/work -- python3 /src/deployment/linux/package_linux.py
```

Bootstrap refuses to overwrite an existing rootfs. Use a fresh directory for a
new reproduction. Retain the download cache; each cached input is rehashed. The
rootless image records package ownership through fakeroot and disables the
unused privileged system-DBus launcher. Session DBus and a private Secret Service
run normally. APT signature verification remains enabled.

CMake configures Release, C++17, GCC 11, `BUILD_TESTING=ON`, and the installed
LDraw source. The build script derives all test executable targets from CMake,
explicitly builds them, and verifies the eight installed-LDraw registrations.
The `BaselineTests.cmake` hook runs the existing source-inspection Help test
from the checkout, preserving its assertions after release `__FILE__` prefix
mapping. The serial test runner rejects failed, skipped, or missing cases. It then runs a
focused MCUT/queue/Local Override/credential/source/Host/Remote subset. Tests do
not access the desktop user's credentials: the runner starts a fresh DBus and
verifies ownership of its isolated keyring before any credential access.

The first build environment was prepared with these same pinned inputs before
the bootstrap wrapper was added. Its package inventory is preserved alongside
the artifact. The wrapper was then exercised from a fresh base through package installation,
Qt extraction and OpenSSL compilation; `rcc 6.10.3`, the rebuilt OpenSSL 3.5.8
and an empty `dpkg --audit` were verified. Re-run this fresh-image check in the
L3 rehearsal; no byte-identical compiler-output claim is made.

## Package policy

`policy.json` is the explicit plugin and system-library contract. The tree is:

```text
BrickSuite/
  install.sh / uninstall.sh / installer.py / runtime-requirements.json
  BrickSuite                         # simple exec launcher, no LD_LIBRARY_PATH
  bin/BrickSuite
  bin/BrickSuiteMeshBooleanWorker
  bin/qt.conf
  lib/                               # private ELF closure
  plugins/                           # explicit allowlist, SQLite only
  share/applications/BrickSuite.desktop
  share/icons/hicolor/256x256/apps/BrickSuite.png
  share/licenses/
  share/build-metadata.json
  share/dependencies.json
  share/abi-audit.json
  share/source-lock.json
  share/ubuntu-build-packages.tsv
  README.md
```

Applications use `$ORIGIN/../lib`, libraries `$ORIGIN`, and plugins
`$ORIGIN/../../lib`. `qt.conf` controls Qt plugin discovery. The recursive auditor
checks architecture, GLIBC/GLIBCXX/CXXABI versions, dependencies, RPATHs, embedded
development prefixes, symlinks, required licenses, helpers and source SHA.
Generic legitimate runtime strings such as `/tmp/` are not development paths;
actual source/build/SDK prefixes are rejected. Two upstream Qt Widgets warning
strings contain `/home/qt/work/qt/`; `normalize_qt_diagnostics.py` replaces only
these known prefixes with the equal-length `./qt6-source-dir/`. Code, offsets,
filenames and NUL terminators remain unchanged. Unexpected inventories fail.
The recipe and exact remapping record accompany Qt under `share/licenses/Qt`.
Relative source filenames are allowed; absolute development prefixes are not.

Bundled: required Qt libraries, ICU, MCUT, OpenSSL, zlib/zstd/Brotli as needed.
The complete release gate allows GLIBC 2.35, GLIBCXX 3.4.30 and CXXABI 1.3.13.
Jammy ships GCC 12 libstdc++ even with its GCC 11 compiler: the pristine
Ubuntu Base 22.04.5 archive includes `libstdc++.so.6.0.30`, exporting the
versioned condition-variable wait used by MCUT. This remains the Ubuntu 22.04
floor; the Qt-only closure has the lower GLIBCXX 3.4.29 requirement.

System: glibc, libstdc++, libgcc, DBus, Secret Service, fonts, X11/xcb, Wayland,
GL/EGL/GLX/GLVND and graphics drivers. Do not bundle libstdc++ or libgcc: their
host versions must remain available to newer-host graphics libraries.
The exact runtime package command and manual desktop integration instructions
are in [RUNTIME.md](RUNTIME.md), copied into the archive.

Plugins include XCB and Wayland platforms, XCB EGL/GLX, Wayland EGL/xdg-shell and
decorations, Compose/IBus input, the desktop portal, SQLite, OpenSSL TLS,
GIF/ICO/JPEG/SVG/WebP image decoding and SVG icons. No unrelated SQL drivers or
entire SDK plugin directory is copied. Qt licenses are collected from the
shipped components' SBOM dependency graph and corresponding source attributions.

The output is `work/release/BrickSuite-v0.4.0-Linux-x86_64.tar.gz` plus a `.sha256`
file and metadata/manifest/audit sidecars. Archive ordering, timestamps and gzip
headers are normalized; executable modes and relative symlinks are preserved.
This is deterministic archiving of identical inputs, not a claim that arbitrary
rebuilds produce byte-identical compiler output.

## Relocated package acceptance

Use the actual archive. `stage_acceptance.py` first audits its unmodified
extraction, then adds disposable probes and selected test executables, never to
the release tarball. These tools link the actual Release production objects.

```sh
SOURCE_SHA=$(git rev-parse HEAD)
python3 deployment/linux/run_baseline.py --root build/linux-l2/rootfs \
  --work build/linux-l2/work -- python3 /src/deployment/linux/stage_acceptance.py \
  /work/release/BrickSuite-v0.4.0-Linux-x86_64.tar.gz /work/acceptance \
  --source-sha "$SOURCE_SHA"
# Move this extracted tree outside the source/build/SDK trees.
cp -a build/linux-l2/work/acceptance /tmp/BrickSuite-L2-acceptance
python3 deployment/linux/run_relocated.py --root build/linux-l2/rootfs \
  --bundle /tmp/BrickSuite-L2-acceptance/BrickSuite \
  --evidence /tmp/BrickSuite-L2-evidence --ldraw build/linux-l2/work/ldraw -- \
  python3 /mnt/acceptance-tools/native_display.py xcb -- \
  python3 /mnt/acceptance-tools/isolated_session.py --platform xcb --output /mnt/evidence/core \
  --credentials '/mnt/relocated bundle/BrickSuite/bin/PackageProbe' -- \
  python3 /mnt/acceptance-tools/accept_bundle.py --source-workflows --native --prepare
```

For baseline X11 GUI acceptance, replace the command after `--` with:

```sh
python3 /mnt/acceptance-tools/native_display.py xcb -- \
  python3 /mnt/acceptance-tools/isolated_session.py --platform xcb \
  --output /mnt/evidence/xcb -- python3 /mnt/acceptance-tools/accept_bundle.py --native --prepare
```

Repeat with `wayland` in both positions for native Wayland. The baseline harness
uses Xvfb or Weston headless with Mesa software OpenGL; this is GUI runtime
acceptance in baseline userspace, not a physical Ubuntu 22.04 GPU-driver test.
The probe asserts real OpenGL contexts/framebuffers, F1 Help, file dialogs, LDraw,
Host TLS and the frozen preparation outcomes for 2456/3037/3021. 3021's bounded
failure is the expected frozen behavior, not a successful printable mesh.
The disposable probe sets Qt's logical active window before sending F1 because
headless Weston has no physical input seat. It still exercises the application's
real shortcut and Help window; no production shortcut behavior is changed.

Run the relocated `FitCalibrationGenerationServiceTest` for actual calibration
3MF generation, `LDrawPrintPreparationServiceTest --routing-only /mnt/ldraw` and
`--3021-only /mnt/ldraw` for exact geometry outcomes, and `SecureHostFoundationTest`
for secure Host/Remote protocol acceptance. Use the same isolated-session wrapper.
The acceptance driver first launches the actual packaged shell launcher and
application, waits for its production startup-completed log marker, and checks
its loaded-library mappings before terminating that disposable instance.
The core package probe checks SQL/schema 35/backup, resources, structural API
construction, P-256 TLS and pin rejection, actual worker self-launch, timeout,
and the real child processes' 512 MiB address-space limit. Credential values are
synthetic; only hashes are recorded across the keyring restart.

For Ubuntu 26.04 forward smoke, omit `--root`, add `--host-display`, and run the
same core and native probes against the extracted baseline tarball. Both SDK and
source trees are hidden by mount namespaces. Runtime mappings must resolve
private Qt/OpenSSL/MCUT within the relocated bundle. Host-display tests use the
logged-in desktop compositor; the private keyring remains separate.
On AppArmor-enabled hosts, the wrapper exposes only the kernel's existing
world-accessible `.access` query interface as writable so session DBus can query
policy. Policy-loading interfaces remain read-only; host security policy is
unchanged. The test home and application data are disposable.

A clean-machine release check should additionally extract the untouched tarball,
install only RUNTIME.md prerequisites, configure an LDraw library, and have a
user confirm the visible workflows. Technical probes do not replace user
acceptance or testing physical GPU/driver combinations.

## M39.L3 handoff (no workflow added here)

1. Select Ubuntu **22.04 x86_64** explicitly and record the source commit.
2. Verify/cache every source-lock hash; retain the pinned LDraw archive by hash.
3. Bootstrap the pinned Jammy environment (or an equivalent pinned image).
4. Run `build_release.py`, which builds the app, helper and all configured tests.
5. Run isolated `run_tests.py`; retain full/focused JUnit and require zero skips.
6. Run `test_release_tools.py` and `test_installer.py`, then `build_probe.py` and `package_linux.py`.
7. Extract the tarball outside all development prefixes and run the core,
   credential-restart, source-geometry, calibration, secure-network and X11/
   Wayland acceptance commands above with SDK/build paths unavailable.
8. Require ABI/RPATH/closure audits and `git diff --check`; record schema 35 and
   protocol 1.5. Publish no AppImage, Debian package, or macOS/Windows changes.
9. Upload the tarball, checksum, build metadata, dependency manifest, ABI report,
   JUnit reports and acceptance evidence. Verify the checksum after transfer.
10. Treat an actual user runtime-acceptance decision and any limitations in the
    L2 results report as separate release gates before enabling publication.

No GitHub Actions workflow is implemented by M39.L2. No commit/push is performed.

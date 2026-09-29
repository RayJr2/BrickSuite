# M39.L2B Qt Creator Deploy validation

Implementation and command-line target validation passed. Final visible Qt Creator
acceptance remains Ray's step. No application C++ source, schema, protocol,
Windows packaging or macOS packaging was changed. No commit or push was made.

## 1. Existing Windows convention

`deployment/windows/package_windows.bat <Release-build-dir>` is the checked-in
Windows packaging entry point. It discovers Qt/OpenSSL from CMakeCache, invokes
`windeployqt --release` and Inno Setup, stages under `<build-dir>/deploy/BrickSuite`
and reports installer output under `<build-dir>/deploy/installer`. It checks
required binaries and metadata; its Release option is supplied to windeployqt.
There is no checked-in Windows CMake Deploy target or Windows Qt Creator custom
step configuration to copy. That script and Windows metadata generation remain
unchanged. The Linux target adopts the Deploy name and build-tree output convention.

## 2. Linux target

**Deploy**, a Linux-only CMake custom target, visible to Qt Creator's build-target
selector after CMake refresh. It depends on BrickSuite and
BrickSuiteMeshBooleanWorker for Release builds. Tests are not target prerequisites.

## 3. Files changed

- `CMakeLists.txt`: Linux-only deployment include.
- `cmake/BrickSuiteLinuxDeploy.cmake`: target, production dependencies, optional
  notice-source cache and discovery of configured Qt/compiler/OpenSSL inputs.
- `cmake/RequireLinuxDeployRelease.cmake`: actionable configuration/tool failures.
- `deployment/linux/local_deploy.py`: native-input adapter, source notice cache,
  host ABI policy, safe build-tree publication and Compile Output summary.
- `deployment/linux/package_linux.py`: explicit local profile of the authoritative
  packager; common staging/plugins/RPATH/archive/checksum remain shared.
- `deployment/linux/audit_bundle.py`: explicit local ceiling option and correct
  `ldd` path parsing when directories contain spaces; default release ceiling is unchanged.
- `deployment/linux/notices.py`: license attribution for the actual native OpenSSL.
- `deployment/linux/installer.py`: accepts the explicitly labeled local ABI policy;
  baseline policy and installer UX/actions are unchanged.
- `deployment/linux/test_local_deploy.py`, `test_release_tools.py`,
  `test_installer.py`: local/baseline separation and path/installer regressions.
- `deployment/linux/README.md` and this record: workflow and validation.

## 4. Release and Debug behavior

Release builds production dependencies and packages them. Debug fails immediately
with “Linux Deploy requires a Release configuration” and instructions to select
Release and run CMake. The single-config Debug rejection was verified without
building Debug application dependencies. Multi-config generators check the selected
configuration at execution time. Missing Python/packaging tools produce a nonzero,
actionable failure. No environment variables need manual editing.

## 5. Exact Qt Creator steps

1. Select **Desktop Qt 6.10.3 Release**.
2. Run CMake to refresh the target list.
3. Open **Projects → Build Settings → Build Steps → Details**.
4. Select the CMake build target **Deploy** instead of `all`, then build it.
   Qt Creator's build-target selector may also expose the same target directly.
5. Read **BrickSuite Linux deployment package created** in Compile Output for
   the archive path, byte size, checksum, architecture, commit, OS, ABI and Qt.

This is the CMake build target; no IDE-specific parallel packaging workflow or
edited `.user` file is required. The project was initially on Debug, so the
Release selection matters.

## 6. Exact artifact location

`/home/ray/Programming/Qt/BrickSuite/build/Desktop_Qt_6_10_3_Release/deploy/BrickSuite-v0.4.0-Linux-x86_64.tar.gz`

Checksum, build metadata, dependency manifest and ABI report are adjacent.
Implementation paths derive from the selected build directory and configured kit;
no developer home path is hard-coded in implementation. A failed package run
preserves the preceding managed local artifact. Official archives elsewhere are
not overwritten.

## 7. Size and SHA-256

**46,383,445 bytes**.

```text
2417059c6348d9adedc97437ebdd33779956b503653e412f511cb21cdbf8d054
```

Checksum was verified after creation. Installer/uninstaller executable modes,
main/helper, all 19 plugins, private libraries and notices are included.

## 8. Source commit and architecture

Source commit **81348fa29ef3dbd1cd0bee784073640fcf3e01ae**, **x86_64**.
The package records `source_dirty: true` for this uncommitted deployment work,
plus packaging-tool and CMake-integration hashes. Qt **6.10.3**, GCC **15.2.0**,
native OpenSSL **3.5.5 (27 January 2026)**, Ubuntu **26.04.1 LTS**.
Matching Qt/ICU notice sources were reused through the optional CMake cache entry;
no baseline application or runtime binaries were substituted for native inputs.

## 9. Local ABI floor

All **43 ELF files** were recursively audited. Observed maximum requirements:
**GLIBC 2.43 / GLIBCXX 3.4.32 / CXXABI 1.3.15**.
Build-host runtime policy: GLIBC 2.43 / GLIBCXX 3.4.35 / CXXABI 1.3.17.
The observed requirements, rather than a guessed compiler/distro label, are reported.
This is explicitly a **local development package**, not an Ubuntu 22.04 release.
The unmodified default release gate correctly rejects this artifact as exceeding
GLIBC 2.35. The official Ubuntu 22.04 policy remains 2.35 / 3.4.30 / 1.3.13.

## 10. One authoritative packaging implementation

`Deploy → local_deploy.py → package_linux.stage`.
The adapter supplies native build inputs and a declared local policy; it does not
implement a second runtime collector, plugin installer, RPATH editor, ELF auditor,
installer generator or archive/checksum writer. Baseline packaging retains its
Ubuntu 22.04 environment requirement, source checks and exhaustive test gate.
Local Deploy skips only that full-suite gate and records that fact in metadata.
Installer behavior remains `/opt/BrickSuite`, desktop/icon/command integration,
consent before sudo, safe replacement and preserved user data/credentials.

## 11. Regression and package validation

- **19/19 packaging regressions passed**, including actual private-library
  resolution from a path containing spaces.
- **30/30 installer regressions passed**, including explicit local-policy handling
  while a non-local archive cannot silently widen the baseline ceiling.
- **10/10 local-deploy regressions passed**: Debug/tools failures, strict baseline
  default, explicit local policy, architecture/RPATH rejection, notice-cache
  errors, repeat publication and failure recovery.
- The 19 packaging and 30 installer tests also passed under Ubuntu 22.04.
- Real final-archive install/reinstall/uninstall/reinstall passed in a temporary
  synthetic root with spaces; user data survived. The laptop's real `/opt` was
  not changed.
- Recursive local ABI/closure/RPATH audit passed after relocation outside the
  checkout, including a path with spaces. The source/build/SDK prefixes are
  rejected during packaging. Actual packaged startup and loaded-library mappings
  passed in a namespace with development SDK paths hidden and private data/keyring.
- The final package has the same 43 ELF hashes as the runtime-tested candidate;
  the repeated target run refreshed only the shared audit/tool provenance.
- The full 125-test application suite was not rerun, as requested: no production
  C++ or runtime behavior changed.

Evidence is retained under
`build/Desktop_Qt_6_10_3_Release/deploy-validation/`.

## 12. Command-line target validation

Using the CMake executable selected by the existing Qt Creator kit:

```sh
/home/ray/Qt/Tools/CMake/bin/cmake --build   /home/ray/Programming/Qt/BrickSuite/build/Desktop_Qt_6_10_3_Release   --target Deploy --parallel 6
```

**Passed twice**, including replacing an existing local deployment. Compile Output
contains the requested summary. Release CMake configure passed. Debug CMake
configure passed, and its Deploy invocation failed with the expected Release-only
message. No separate Qt Creator-only code path is used.

## 13. Diff checks

`git diff --check`, Python syntax, shell installer regressions, artifact checksum,
packaging-tool provenance and CMake integration hash checks passed. Initial tree
was clean at 81348fa; changes are limited to CMake/Linux deployment integration.

## 14. Preserved invariants

Schema **35**, protocol **1.5**, M37/M38, MCUT **512 MiB** production limit,
user-data locations, credential design, Windows and macOS packaging are unchanged.
No production C++ edits, commit, push or CI workflow were introduced.

## 15. Remaining Qt Creator acceptance

No command-line Deploy or packaging blocker remains. Ray still needs to select
Release, refresh CMake and build **Deploy** visibly in the open Qt Creator project.
The optional notice-source cache is configured for this build; fresh builds can
automatically download matching pinned notice sources once. Local Deploy artifacts
must not replace the separately validated Ubuntu 22.04 official release artifact.

---

# M39.L2A installer validation record

Technical checks passed. This installer-enabled archive supersedes the earlier
M39.L2 archive for end-user acceptance. The original M39.L2 record is retained
below. Application source remains `4e02d11867a67a706a30e2d280c1b6977acaed24`;
all changes remain uncommitted under `deployment/linux/`.

## 1. Installer workflow

The existing archive root remains `BrickSuite/`, containing executable
`install.sh` and `uninstall.sh`, the standard-library Python installer, the runtime
requirements manifest, and the original application bundle. No duplicate binaries,
AppImage or Debian package are introduced. The installer reports the platform,
checks prerequisites, offers to install missing runtime packages, asks to install
the application, stages/verifies the bundle and installs desktop integration.
It prints success and launch options without opening the application as root.

## 2. Supported OS and architecture

`/etc/os-release` supplies distribution ID/version; `platform.machine()` supplies
architecture. Automatic installation accepts **Ubuntu 22.04 or newer, x86_64**.
Other distributions, older Ubuntu and other architectures stop before any package
manager action. Portability of the extracted application remains separate.
Python 3 is required by the installer and is normally included with Ubuntu.

## 3. Runtime prerequisite list and detection

The following Ubuntu 22.04 package mapping covers the entire M39.L2 system SONAME
allowlist, verified by a regression test. These are candidates only; packages
whose functionality is already present are not requested:

```text
libc6
libstdc++6
libgcc-s1
libgl1
libegl1
libopengl0
libglx0
libglvnd0
libglib2.0-0
libdbus-1-3
libgssapi-krb5-2
libfontconfig1
libfreetype6
libx11-6
libx11-xcb1
libxcb1
libxcb-cursor0
libxcb-icccm4
libxcb-image0
libxcb-keysyms1
libxcb-glx0
libxcb-randr0
libxcb-render0
libxcb-render-util0
libxcb-shape0
libxcb-shm0
libxcb-sync1
libxcb-xfixes0
libxcb-xkb1
libxcb-util1
libxcb-xinerama0
libxkbcommon0
libxkbcommon-x11-0
libwayland-client0
libwayland-cursor0
libwayland-egl1
libsecret-tools
dbus
fontconfig
dbus-user-session
gnome-keyring
fonts-dejavu-core
```

On Ubuntu 24.04+, missing GLib maps to `libglib2.0-0t64`. Shared libraries are
checked in the x86_64 system loader cache, including actual file existence.
Commands use a system PATH; session DBus uses its user socket unit; a Secret
Service provider uses GNOME Keyring or KWallet daemon availability; `fc-list`
checks for installed fonts. No keyring unlock or credential read is needed to
install. System Qt/OpenSSL/MCUT/ICU, SDKs, compilers and development packages are
not requested. APT may install dependencies of the missing runtime packages.

Read-only detection passed on the Ubuntu 26.04 host. The isolated Ubuntu 22.04
image initially lacked `dbus-user-session`; adding it satisfied the check. This
changed only the isolated image, and the bootstrap package list now includes it.
No packages were installed into the developer laptop's operating system.

## 4. Consent and sudo

No sudo occurs before affirmative user consent. Missing prerequisites are listed;
acceptance runs the displayed `apt-get install --no-remove <missing packages>`
through normal sudo behavior. Declining prints the manual command and exits
without installing. APT failure or still-missing prerequisites stops the install.
A separate confirmation precedes application installation/removal and its sudo
request. EOF declines. Synthetic-root mode never invokes sudo or APT.

## 5. Installed layout and verification

```text
/opt/BrickSuite/
  BrickSuite
  install.sh / uninstall.sh / installer.py / runtime-requirements.json
  bin/BrickSuite
  bin/BrickSuiteMeshBooleanWorker
  bin/qt.conf
  lib/
  plugins/
  share/licenses/
  share/icons/hicolor/256x256/apps/BrickSuite.png
  share/{build-metadata,dependencies,abi-audit,source-lock,installation}.json
  README.md
```

No mutable user data is stored there. ELF identity, all audited ABI/RPATH records,
development-path rejection, executable modes, safe symlinks, plugins, licenses,
Qt configuration and loader resolution are verified before staging, after staging
and after replacement. No build tools are required for end-user verification.
Private direct dependencies and all Qt/MCUT/TLS/ICU resolutions must remain in the
bundle; system font/graphics libraries retain their own transitive dependencies.
The installer does not use `LD_LIBRARY_PATH`.

## 6. Desktop integration

`/usr/share/applications/bricksuite.desktop` uses Name=BrickSuite,
Exec/TryExec=`/opt/BrickSuite/BrickSuite`, Icon=bricksuite, Terminal=false,
Categories=Utility. No file associations. Icon:
`/usr/share/icons/hicolor/256x256/apps/bricksuite.png`. Desktop/icon caches are
refreshed when their existing utilities are available. `desktop-file-validate`
passed on the generated entry. Menu visibility on the separate acceptance laptop
remains a user check.

## 7. Command launcher

`/usr/local/bin/bricksuite` is a symlink to `/opt/BrickSuite/BrickSuite`.
The existing small launcher locates its bundle and execs the binary, without
changing global library paths or the system `secret-tool` environment.

## 8. Reinstall and upgrade

Close BrickSuite and run `install.sh` from a separately extracted archive.
The installed version is reported. A lock serializes changes; complete staged
application and integration files replace the previous files through renames.
Caught copy/replacement/verification errors restore the previous installation.
Rollback is tested, including failure after replacement. This is not a power-loss
transaction; a killed process may leave a `.previous-*` backup for recovery.
Unmanaged installations and unrelated/modified integration files are refused
with a clear message rather than overwritten. There is no network auto-updater.

## 9. Uninstall and data preservation

`/opt/BrickSuite/uninstall.sh` or the archive's `./uninstall.sh` asks before
removing the managed application, desktop entry, icon and command symlink.
Databases, backups, settings, logs, LDraw, Secret Service entries, Host identity
and paired-device credentials are not accessed or removed. Shared packages remain.
Synthetic fixtures covering all these user-data categories survived upgrade and
uninstall. The actual archive lifecycle also preserved a synthetic user database.

## 10. Validation results

- **29/29 installer regressions passed** on Ubuntu 22.04, including real ELF
  verification, prerequisites, consent, rejection/failure cases, synthetic-root
  lifecycle, rollback, desktop/icon/launcher, symlinks and user-data preservation.
- **18/18 existing packaging regressions passed**. Total: **47/47**.
- Focused Linux CTest rerun: **27/27 passed, zero failures/skips**, 55.25 seconds.
- M39.L2 full **125/125**, including all eight installed-LDraw cases, remains the
  full-suite evidence. It was not rerun for installer-only changes: all **43 ELF
  files are byte-identical** to that validated release; application source and
  build configuration are unchanged.
- Recursive final ABI, RPATH, plugin and relocation audits passed: maximum
  **GLIBC 2.35 / GLIBCXX 3.4.30 / CXXABI 1.3.13**.
- Real archive install/reinstall/uninstall/reinstall passed in synthetic roots
  on **Ubuntu 22.04 and Ubuntu 26.04**, using the distributed shell entry points.
- Installed files were mounted at **`/opt/BrickSuite`**, with SDK/build paths
  hidden. Actual application startup/loaded-library closure, workers/containment,
  schema 35/backup, Host TLS, Help/F1, file dialogs, 3001 3D Viewer and calibration
  workspace passed on Ubuntu 22.04 X11 and Ubuntu 26.04 native Wayland.
- Installed Ubuntu 22.04 credential write/restart/read/cleanup/unavailable-backend
  checks passed in a verified private keyring. No real user state was used.
- Baseline rendering: llvmpipe; newer-host rendering: Intel HD Graphics 630.
  Qt's previously observed Wayland text-input focus diagnostics remain during
  synthetic window switching; no application/database critical failure occurred.
- Desktop metadata validation, Python/shell syntax, archive executable modes,
  tool-provenance hashes, checksum and whitespace checks passed.

Evidence: `build/linux-l2/l2a-*.log`,
`build/linux-l2/work/l2a-install-verified/{result.json,lifecycle.log}`,
`build/linux-l2/work/l2a-host-install/{result.json,lifecycle.log}`, and
`build/linux-l2/work/evidence/l2a-runtime/`.
Acceptance probes were added only to the disposable installed copies after the
installer lifecycle checks; they are absent from the distributed tarball.

## 11. Resulting artifact

`build/linux-l2/work/l2a-installer-release/BrickSuite-v0.4.0-Linux-x86_64.tar.gz`

**46,020,506 bytes**. SHA-256:

```text
37d955d9254adb71604a7ab13578f2cb36257494596230b6c6ce8a3b8c9fa4ef
```

Adjacent checksum, build metadata, dependency manifest and recursive ABI audit
are retained. Source/schema/protocol and packaging-tool hashes are recorded.

## 12. Exact clean-laptop manual acceptance

Copy the tarball and its `.sha256` file to the other Ubuntu 22.04+ x86_64 laptop,
which should have no Qt SDK or BrickSuite development environment. In a terminal:

```sh
sha256sum -c BrickSuite-v0.4.0-Linux-x86_64.tar.gz.sha256
tar -xzf BrickSuite-v0.4.0-Linux-x86_64.tar.gz
cd BrickSuite
./install.sh
```

Approve missing prerequisites if requested, then installation to `/opt/BrickSuite`.
Launch **Applications → BrickSuite**, or `bricksuite`. Confirm Help/F1, credential
storage in the logged-in desktop's unlocked keyring, configured LDraw/3D Viewer,
printing/calibration, backup and the user's normal workflows. Close BrickSuite
before reinstalling/upgrading. Optional uninstall: `/opt/BrickSuite/uninstall.sh`;
confirm existing user data remains available.

## 13. Remaining M39.L2 acceptance blocker

No technical installer blocker was found. **The separate clean Ubuntu laptop's
manual install/menu/launch and normal user workflow acceptance is still pending**.
Automated synthetic-root and isolated display checks do not replace it, nor do
they certify all Ubuntu 22.04 physical graphics drivers. Schema 35, protocol 1.5,
M37/M38, the MCUT 512 MiB limit, credential design, user-data locations, Windows
and macOS remain unchanged. No commit, push or Linux CI workflow was made.

---

# M39.L2 validation record

Technical release gates passed; user runtime acceptance remains separate.
Application source: `4e02d11867a67a706a30e2d280c1b6977acaed24`.
All changes are new, uncommitted files under `deployment/linux/`. Schema 35,
protocol 1.5, M37/M38, application source, Windows installer and macOS workflows
remain unchanged. No commit or push was made.

## 1. Exact build environment

Ubuntu Base 22.04.5 x86_64 in a rootless Bubblewrap namespace, using signed Ubuntu
APT snapshot `20260928T000000Z`. Runtime glibc: **2.35**, package
`libc6 2.35-0ubuntu3.15`; compiler **GCC 11.4.0**, C++17; CMake **3.22.1**;
Ninja **1.10.1**. It shares the Ubuntu 26.04 laptop kernel, not its compiler or
userspace libraries. No host packages or security policy were changed.

The independent fresh bootstrap completed through Qt extraction and OpenSSL
compilation. `rcc 6.10.3`, OpenSSL 3.5.8 and empty `dpkg --audit` were verified.
Build package inventory is included as `share/ubuntu-build-packages.tsv`.

## 2. Qt strategy

Fresh, hash-verified official **Qt 6.10.3** archives: QtBase, SVG, WebSockets,
Wayland, image formats and ICU 73.2. The selected 36-ELF preflight closure required
at most **GLIBC 2.34 / GLIBCXX 3.4.29 / CXXABI 1.3.13**, so a Qt source rebuild
was unnecessary. Matching source was retained for notices and relinking.
No binaries were copied from the development laptop's Qt SDK.

## 3. OpenSSL, MCUT and lib3mf

- **OpenSSL 3.5.8 LTS (25 August 2026)**, built with Jammy GCC 11, shared,
  built-in default provider, `no-module`. Source SHA-256:
  `a8f84a39918ec6415ce765d9b429d313ba97b8143169c172e734b9514464f5b2`.
  Bundled `libssl.so.3` and `libcrypto.so.3`; runtime version was asserted.
- **MCUT** revision `047d75ffe6e33ede572cb25217047a4756188401`, shared
  `libmcut.so.1.2.0`, built on Jammy. The existing generated-header Linux queue
  overlay prevents a lost wakeup; upstream downloaded sources remain unchanged.
  Overlay and generated-header hashes accompany the dependency manifest.
  `McutQueueWakeup`, `McutMeshBoolean` and relocated worker checks passed.
- **lib3mf 2.5.0**, revision `64bb454d1fcb53effa57d3cef752a10d740d41a2`,
  x86_64, static with pinned bundled dependencies. It adds no external lib3mf
  runtime dependency. The executable ABI audit covers its linked code; source
  and submodule revisions and notices are included.

All source URLs/hashes are in [source-lock.json](source-lock.json). The installed
LDraw archive is hash-pinned (`d2a695868ed2b3957c45b022a6451908edab22cc043179dd61d18dd382b35e11`),
with LDConfig update 2026-05-29. The archive is a test input, not bundled user data.

## 4. Final recursive ABI ceilings

| Closure | GLIBC | GLIBCXX | CXXABI |
| --- | --- | --- | --- |
| BrickSuite | 2.35 | 3.4.29 | 1.3.13 |
| Mesh Boolean Worker | 2.34 | 3.4.29 | 1.3.9 |
| MCUT | 2.34 | 3.4.30 | 1.3.13 |
| Complete shipped closure, 43 ELF files | **2.35** | **3.4.30** | **1.3.13** |

All are ELF64 x86_64. Jammy's stock `libstdc++.so.6.0.30` exports GLIBCXX 3.4.30
and CXXABI 1.3.13, including the condition-variable symbol used by MCUT. The
GCC 11 compiler and Ubuntu 22.04 floor are preserved. System libstdc++/libgcc are
used to avoid overriding newer-host graphics-driver runtime requirements.

## 5. Release build and tests

Built BrickSuite, its helper and all **117 configured test executables**, with
`BUILD_TESTING=ON`. CMake registered **125 tests**. Final serial CTest:
**125/125 passed, 0 failed, 0 skipped**, 1001.31 seconds. Focused MCUT, queue,
Local Override, credentials, 2456/3037/3021, Host/Remote/pairing:
**27/27 passed, 0 failed, 0 skipped**, 61.43 seconds.

All eight installed-LDraw cases passed: six source calibration families plus
PrintCompositionRouting and PrintPreparation3021. No geometry, resource,
credential, timeout or calibration assertions were weakened.

An initial run found that release prefix mapping made the Help source-inspection
test's `__FILE__` relative. The Linux-only `BaselineTests.cmake` sets that test's
working directory to the checkout; its existing assertions then passed.
Final logs/JUnit: `build/linux-l2/work/evidence/tests/{full,focused}.{log,xml}`.

## 6. Bundle layout

```text
BrickSuite/
  BrickSuite                         # portable exec launcher
  bin/BrickSuite
  bin/BrickSuiteMeshBooleanWorker
  bin/qt.conf
  lib/                               # private runtime
  plugins/                           # 19 allowed plugins
  share/applications/BrickSuite.desktop
  share/icons/hicolor/256x256/apps/BrickSuite.png
  share/licenses/
  share/{build-metadata,dependencies,abi-audit,source-lock}.json
  share/{test-summary.json,ubuntu-build-packages.tsv}
  README.md
```

Manual desktop integration is documented; no file associations are introduced.
Acceptance probes are added only to disposable extracted copies, never the tarball.

## 7. Exact Qt plugin inventory

```text
platforms/libqxcb.so
platforms/libqwayland.so
xcbglintegrations/libqxcb-egl-integration.so
xcbglintegrations/libqxcb-glx-integration.so
wayland-graphics-integration-client/libqt-plugin-wayland-egl.so
wayland-shell-integration/libxdg-shell.so
wayland-decoration-client/libadwaita.so
wayland-decoration-client/libbradient.so
platforminputcontexts/libcomposeplatforminputcontextplugin.so
platforminputcontexts/libibusplatforminputcontextplugin.so
platformthemes/libqxdgdesktopportal.so
sqldrivers/libqsqlite.so
tls/libqopensslbackend.so
imageformats/libqgif.so
imageformats/libqico.so
imageformats/libqjpeg.so
imageformats/libqsvg.so
imageformats/libqwebp.so
iconengines/libqsvgicon.so
```

SQLite is the only shipped SQL driver. QtTest and unrelated SDK plugins are absent.

## 8. Bundled versus system dependencies

Private: 12 Qt libraries (Core, Gui, Widgets, Network, SQL, WebSockets, OpenGL,
OpenGLWidgets, SVG, DBus, WaylandClient, XcbQpa), ICU 73.2 data/uc/i18n, MCUT,
OpenSSL 3.5.8, zlib 1.2.11, zstd 1.4.8 and Brotli 1.0.9. The definitive inventory
and input hashes are in `share/dependencies.json`; Qt library count is derived
from that manifest rather than the module archive count.

System: glibc, libstdc++, libgcc, GLib, DBus, Kerberos/GSSAPI, fontconfig/freetype,
fonts, X11/xcb, Wayland, GL/EGL/GLX/GLVND and graphics drivers. The exact Ubuntu
22.04 package command is in [RUNTIME.md](RUNTIME.md). No global library override
is used. Licenses/notices cover the actual closure and static lib3mf dependencies;
source and Qt/MCUT replacement/relinking requirements are documented.

## 9. Secret Service contract

Requires **system `libsecret-tools`**, session DBus and an unlocked Secret Service
provider such as GNOME Keyring. `secret-tool` is not bundled; there is no plaintext
fallback. Baseline and Ubuntu 26 checks passed synthetic credential write,
keyring restart/read, cleanup and clear unavailable-backend failure. Each test
verified private DBus/keyring ownership before accessing credentials. Real user
credentials and application data were not used.

## 10. RPATH and relocation

Every final ELF passed architecture, ABI, dependency and development-path audits.
RPATH/RUNPATH: executables `$ORIGIN/../lib`, private libraries `$ORIGIN`, plugins
`$ORIGIN/../../lib`. `qt.conf` resolves bundle plugins; the launcher simply execs
the binary without setting `LD_LIBRARY_PATH`.

The archive was extracted outside checkout/build/SDK directories and mounted at
`/mnt/relocated bundle/BrickSuite`, with source/build/SDK paths hidden and a clean
environment. Actual launched application and probe memory maps resolved private
libraries only inside that bundle, with allowed host system libraries.

Two upstream Qt Widgets diagnostic strings retained `/home/qt/work/qt/` after
stripping. A narrowly checked, equal-length replacement changes only those two
read-only `.rodata` prefixes to `./qt6-source-dir/`; instructions, offsets and NUL
terminators stay intact. Recipe and exact mapping are included in Qt notices.
Unknown strings/sections fail the gate. All 43 final ELF hashes match the accepted
candidate; the final archive refresh changed packaging provenance metadata only.

## 11. Package probe

Passed actual launcher/startup and loaded-library closure; QSQLITE; schema 35
in a disposable database; verified backup; Help/resources/icon decoding;
structural provider construction without real API keys; OpenSSL backend;
ephemeral P-256 TLS and incorrect-fingerprint rejection; protocol 1.5;
worker union (12 mm³) and difference (4 mm³); Local Override self-launch/union;
zero-ms test deadline termination; both real worker processes' 512 MiB address
space limit; rejection of a 600 MiB allocation; queue regression.

The untouched product launcher was started and terminated by the harness after
its production startup marker. Workflow probes link the actual Release objects;
they do not substitute a development executable for package acceptance.

## 12. Ubuntu 22.04 bundle acceptance

**X11/Xvfb and native Wayland/Weston passed**: startup, pinned Host listener TLS,
F1 Help, file dialogs, LDraw, valid OpenGL framebuffer, printing preparation,
and calibration workspace. Both rendered using llvmpipe (LLVM 15.0.7).
2456 and 3037 reached Ready for Printing; 3021 retained its expected bounded
Preparation failed outcome. Relocated source geometry tests, actual calibration
3MF generation and secure Host/Remote suites also passed under X11.

Screenshots were inspected for visible geometry and preparation labels. The
Wayland probe sets logical Qt focus before synthetic F1 because headless Weston
has no input seat. This is test harness behavior, not a production shortcut change.
No unexpected warning/critical application log entries were found in successful
startup/native runs. Physical Ubuntu 22.04 GPU/driver coverage remains unverified.

## 13. Ubuntu 26.04 forward compatibility

XWayland passed startup/core/workers, private credential restart/failure checks,
F1 Help, file dialogs and 3001 3D Viewer on **Mesa Intel HD Graphics 630 (KBL GT2)**.
Native Wayland also passed the same core/native checks from the final tarball,
using the same Intel hardware renderer. Qt emitted text-input leave/focus
diagnostics during synthetic window switching; all assertions passed and no
application/database critical error occurred. This does not certify IME input.
The isolated DBus needs the existing AppArmor `.access` query node writable;
policy mutation nodes remain read-only and host policy is unchanged.

## 14. Final tarball

`build/linux-l2/work/final-release/BrickSuite-v0.4.0-Linux-x86_64.tar.gz`

Size: **46,018,164 bytes**. SHA-256:

```text
22c77a54a25d57b5492d3c2c0b72acaf4e01e77d798157a771866bfbe2f129a4
```

Adjacent `.sha256`, `build-metadata.json`, `dependencies.json` and `abi-audit.json`
are produced. Executable modes, relative symlinks and layout survive extraction.
The metadata identifies source SHA, architecture, distro, glibc floor, compiler,
Qt, OpenSSL, schema/protocol, successful test gate and packaging-tool hashes.

## 15. Packaging regressions

**18/18 passed** in Ubuntu 22.04. Covers ABI ceilings (GLIBC/GLIBCXX/CXXABI),
absolute RUNPATH, unresolved dependencies, architecture, missing helper/plugin/
license, source SHA, unintended SQL plugins, non-ELF payload, escaping symlinks,
absolute versus relative source paths, exact diagnostic remapping with unchanged
instructions/offsets, atomic rejection of unexpected strings, relocation with
spaces, archive permissions/symlinks and deterministic archive creation.
Final log: `build/linux-l2/packaging-tests-final.log`. Python syntax and final
whitespace checks passed, including all new untracked deployment files.

## 16. Exact M39.L3 handoff

[README.md](README.md) contains the complete commands for fresh bootstrap,
Release/all-test build, serial/focused gates, probe build, packaging, extraction
and isolated X11/Wayland acceptance. Its ten-step M39.L3 sequence specifies an
explicit Ubuntu 22.04 runner, pinned inputs/content-addressed LDraw cache, closure
audits, zero skips, evidence retention and uploading archive/checksum/metadata.
No GitHub Actions workflow, AppImage or Debian package is created in L2.

## 17. Remaining acceptance and scope

No technical blocker remains for preparing Linux CI artifact work.
Automated technical gates establish the baseline tarball and reproducible L3
handoff. User confirmation of visible workflows on an independently provisioned
machine remains a release acceptance step. Ubuntu 22.04 hardware GPU/driver
combinations were not tested; baseline display checks used software rendering.
API construction is covered, not live provider calls with personal credentials.
L3 CI implementation/publication remains a separate requested phase after L2
acceptance. No schema/protocol, frozen behavior or resource-limit change occurred.

## Evidence index

- Build: `build/linux-l2/release-build.log`.
- Test logs/JUnit: `build/linux-l2/work/evidence/tests/`.
- Final recursive audit and dependency manifest: `build/linux-l2/work/final-release/`.
- Retained successful runtime logs, acceptance JSON and screenshots:
  `build/linux-l2/work/evidence/runtime/`.
- Final archive was extracted and audited again; final baseline XCB startup/core
  and Ubuntu 26 native Wayland smoke passed. All 43 binaries are byte-identical
  to those used in the full baseline X11/Wayland and newer-host XWayland checks.
- Earlier failed harness attempts remain in local logs for diagnosis. These
  included missing logical focus on headless Wayland, the read-only AppArmor
  query node, and an attempted offscreen launch (that plugin is intentionally
  absent from the release). Successful reruns used the documented platforms.

# M39 Linux Phase 1 — runtime and deployment readiness audit

Audit date: 2026-09-29. Starting commit: `94cd98e` (Ubuntu Qt 6.10.3/GCC 15 compatibility). The working tree was clean at the start. Schema **35** and Protocol **1.5** remain unchanged. No commit, push, packaging implementation, container image, or CI workflow was created. Windows installation, macOS packaging, M37 geometry/fit, M38 tooltip behavior, and M31/M33 dispositions were not redesigned.

This is a development-machine technical audit, not user acceptance or clean-machine release certification. Synthetic runtime checks used isolated application data/settings and a private session bus and GNOME Keyring. No personal database or provider credential was used. Native widget probes linked the production Release objects and exercised the application's initialization and widgets; they are not a packaged executable test.

## 1. Development environment

| Component | Observed value |
| --- | --- |
| Distribution | Ubuntu 26.04.1 LTS, x86_64 |
| Kernel | 7.0.0-34-generic |
| glibc | 2.43 |
| Compiler | GCC 15.2.0, C++17 |
| Qt | 6.10.3, Linux gcc_64 |
| Qt Creator | 20.0.2 |
| CMake / Ninja | 3.30.5 / 1.12.1 |
| OpenSSL runtime | 3.5.5, 27 January 2026 |
| Graphics | Mesa 26.0.8, Intel HD Graphics 630 (KBL GT2); OpenGL 4.6 context verified |
| Secret Service | GNOME Keyring 50.0-1; libsecret-tools 0.21.7 |
| Repository | `94cd98e` plus the narrow audit fixes below |

The existing Qt Creator Debug configuration uses the installed Qt 6.10.3 kit, GCC, and Unix Makefiles. The audited Release configuration uses that same Qt/compiler with Ninja. A Creator Release entry exists; the clean Release build reported here is the independently configured audit directory, not a claim that every Creator configuration was exercised.

The laptop is suitable for native development and runtime acceptance. It must not define the distributed binary's compatibility floor.

## 2. Build and test baseline

Clean Release configuration/application build, all **117 configured test executables**, and the final Debug application build succeeded. The final suite registers **125 tests**, including two new Linux regressions and eight installed-LDraw cases. **Full serial CTest: 125 passed, 0 failed, 0 skipped, 0 disabled**, in 824.66 seconds (13 minutes 45 seconds).

The clean Release object directory is `build/m39-linux-release`, configured with `BUILD_TESTING=ON` and an installed LDraw root. Configuration reused already downloaded, pinned MCUT/lib3mf **source** trees through CMake FetchContent overrides; compiled outputs were fresh. All configured test executables are explicitly built because they are excluded from the default build. Full CTest runs serially, with isolated XDG directories, a private session bus/keyring, and the offscreen Qt platform. Native OpenGL/UI evidence is separate from offscreen tests. Windows/macOS builds were not rerun on this Linux host; the shared UI edits use portable Qt layouts, and both backend corrections are Linux-only.

Eight source-backed LDraw cases are registered: BallSocket, PinBarrelHinge, InterleavedFingerHinge, ClickHinge, RetainedRotatingWheel, PlainRoundBoreWheel, PrintCompositionRouting, and PrintPreparation3021. All eight passed. They are not absent or counted as skips. Platform-conditional assertions such as Windows native access/lock-error checks in PartReferenceAudit and the macOS-only worker memory case are not whole-test CTest skips; the separate Linux RLIMIT probe supplies Linux containment evidence.

The initial clean Release application build emitted 497 warning lines: 448 vendored libzip configuration macro redefinitions, 33 MCUT old-style C function diagnostics, and remaining vendored uninitialized/reordering/string-copy diagnostics. These are not new application compile failures. The initial explicit test build emitted no warnings. Rebuilding MCUT for the queue overlay repeated 42 existing vendored warning lines in Release and 34 in Debug; no application warning or compile error was introduced. The optional Vulkan header probe is unavailable; this application uses the observed OpenGL path. Private-session portal/GVFS warnings arose in the isolated native harness and do not establish a production desktop defect. Six CTest cases emitted offscreen `propagateSizeHints` warnings; ExternalLDrawViewer also reported unsupported offscreen OpenGL context creation, which is why the successful native xcb and Wayland viewer checks are separate evidence. SecureHostFoundation emitted a closed-socket write warning while exercising connection failure paths; the test passed.

## 3. Reproduced defects and narrow fixes

1. **Inventory/Collection forced excessive window width.** Native Qt minimum-size measurements were 1316 logical pixels for My Inventory and 1672 for My Collection, forcing the entire main window to 1676. Split each dense filter strip into two Qt layout rows. No platform pixel constants, query changes, or workflow changes were added. The resulting minimum widths are 788 and 894; the main window minimum is 898.
2. **Secret Service failure reported as credential absence.** With an unavailable session bus, `secret-tool lookup` failed with stderr and empty stdout; the application returned a successful missing-key result. Linux lookup now recognizes only exit 1 with empty stdout **and stderr** as a normal miss. Backend failures remain errors. A Linux-only regression executable covers normal absence, unavailable service, unexpected silent failure, and successful synthetic lookup.

3. **MCUT queue lost wakeup on Linux.** The initial serial run stalled in FitCalibrationArtifact. A debugger-launched reproduction showed the caller waiting in MCUT `wait_for_events_impl` / `mcDispatch`, while its API thread slept on the task queue condition variable. The stack came from the trusted in-process friction-pin calibration path. The queue producer changed the tail and notified without synchronizing with the head mutex used by the wait predicate, allowing a lost notification. A Linux-only CMake-generated header overlay reuses MCUT's existing synchronized `disrupt_wait_for_data()` notification after releasing the tail mutex. Downloaded sources remain untouched. A 100,000-transition queue regression fails on the original header and passes with the correction; the previously stalled calibration executable also passes with the corrected library. The original suite was interrupted and superseded by the final full run, not counted as a completed baseline.

The fixes affect `MyInventoryWidget.cpp`, `MyCollectionWidget.cpp`, the Linux branch of `CredentialStore.cpp`, and Linux MCUT build configuration. CMake registers credential and queue regression executables. No credential fallback, geometry algorithm, fit definition, execution-mode change, or increased resource limit was added. The MCUT dependency manifest must record the local synchronization correction alongside its pinned revision.

## 4. Native runtime and layout

The native probe ran separately with Qt xcb/XWayland and native Wayland in the laptop's GNOME session, using the production theme and tooltip initialization and fresh synthetic workspaces. It opened all major main tabs, created a workspace and storage location, and created a synthetic MOC Build. Parts Catalog loaded the application's bundled catalog. Part Reference loaded 2985 entries across 38 catalogs. Settings tabs, built-in Help through F1, procurement preview, LEGO Fit Calibration, and catalog/external 3D viewers were exercised.

| Surface | Observed logical size / limitation |
| --- | --- |
| Main window, including Builds | 1200×800 accepted; requested 1000×750 expands to 1000×775 |
| Inventory / Collection | Filter rows fit after correction; horizontal table scrolling remains available |
| Settings API / Server and other tabs | Server increases minimum width to 913–929 depending on displayed fingerprint/backend; all three Server sub-tabs accepted 1000×750 |
| Part Reference | 1000×750 accepted; minimum width 974 |
| Procurement preview, synthetic row | 1000×750 and 1200×800 accepted |
| LEGO Fit Calibration | Requested 1000×750 expands to 1000×789 |
| 3D viewer | 1200×800 accepted; minimum 1058×725 |
| 3D export chooser | 800×650 accepted |
| Pulling, empty synthetic Build | 1000×750 accepted; minimum 659×196 |

These remaining height/minimum constraints are acceptance boundaries, not a claim that every screen fits a 750-pixel-high work area. Native Wayland repeated startup, dialogs, F1 Help, and both OpenGL viewers successfully. One Qt Wayland text-input leave-event warning appeared during dialog closure; no corresponding failure was observed. This is a smoke check, not exhaustive input-method/compositor acceptance. Populated Inventory editing/moving, allocation/pulling, real procurement mappings, actual provider requests, and two-machine Host/Remote operation remain user workflow acceptance. No speculative changes were made to those systems.

## 5. Credential storage

Production Linux code invokes **`secret-tool`**, not the libsecret C API. Consequently `libsecret-tools` is a runtime prerequisite for this implementation. Its libsecret/GLib/DBus dependencies, a running session bus, and an unlocked Secret Service provider are also required. Installing only `libsecret-1` is insufficient.

Using production credential names against a verified private Secret Service process, the audit wrote and read synthetic Rebrickable/Brickset keys, a Remote client paired credential, a paired-device token, and a Host P-256 identity. The keyring daemon was stopped and restarted between write and read. All four values matched via hashes and the Host public fingerprint remained stable. Synthetic entries were removed. Values were never printed; the temporary manifest contains hashes/public fingerprint only.

No plaintext fallback was introduced. Existing legacy QSettings API-key migration remains unchanged; failed migration can retain an already existing legacy entry, so this audit does not assert that every historical installation has no plaintext legacy setting. New credential writes use Secret Service. Unavailable-backend behavior is now explicit rather than silently appearing unconfigured.

## 6. TLS and Host/Remote

The active Qt TLS backend is **openssl**, `supportsSsl()` is true, and the native process loads `libssl.so.3` and `libcrypto.so.3`. BrickSuite directly needs libcrypto for P-256 identity/certificate operations. Secure WebSockets and the Remote TLS client require the Qt Network OpenSSL backend plugin and both OpenSSL libraries, even though the plugin dynamically loads them and a simple DT_NEEDED scan alone does not reveal the full requirement. Fingerprint pinning uses this same connection/identity infrastructure; it is not a separate TLS package.

The native Settings check enabled Host mode with a synthetic Secret Service token and an ephemeral loopback port. Both the server log and reopened Settings confirmed a secure listener on 127.0.0.1. SecureHostFoundation, PairingFoundation, Host authentication/admission/read/write coverage, Remote reads/session/mutations, and the remaining configured networking suites all passed. SecureHostFoundation includes first-use fingerprint presentation, pinned WSS challenge-response, pairing, and rejected credentials/pins. These localhost results are not a substitute for firewall, discovery, LAN, cross-platform, or two-machine acceptance. Protocol 1.5 is unchanged.

## 7. MCUT and Local Printable Override

Production helper resolution is beside the executable: `BrickSuiteMeshBooleanWorker`. Its shared MCUT library is also required. Local Printable Override uses the application's early worker dispatch before QApplication initialization.

The Linux child sets `RLIMIT_AS` to at most **512 MiB**, respecting a lower inherited limit. A probe of the production containment function confirmed a 536870912-byte ceiling and rejection of a 600 MiB mapping. Parent-side bounded waits, termination, malformed-result validation, and temporary-directory cleanup remain intact. No limit was increased or bypassed.

The relevant test suites exercise actual union/subtraction results, volume/determinism, timeout, malformed/missing output, failed/missing executables, cleanup, and Local Override provenance/source preservation. McutMeshBoolean, LocalPrintableOverrideService, and McutQueueWakeup all passed (0.26, 10.54, and 0.56 seconds respectively). Trusted calibration generators intentionally use the existing in-process MCUT path, which is why the queue race could stall the calibration test rather than return a worker timeout. That execution policy remains unchanged. This distinguishes the kernel limit probe from a claim that every possible out-of-memory geometry workload was tested.

## 8. Installed LDraw

A normal installed library was selected through the real Settings → 3D Models UI in the isolated application profile. A second process confirmed persistence. No personal settings were changed.

Catalog part 3001 and external `3001.dat` both loaded with 700 triangles, 472 hard edges, 224 conditional edges, and zero omitted degenerate faces. Their native OpenGL viewers reported geometry loaded successfully.

Installed-source results:

| Case | Result |
| --- | --- |
| 2456 | Ready through local composition; 18 semantic operands, zero sequential Booleans, complete coverage and one closed component |
| 3037 | Ready after seven bounded Booleans; source-faithful single manifold, 1720 triangles, retained stud identities and deterministic repeat |
| 3021 | Expected bounded Not Ready; operation 5 rejects MCUT face-triangulation size-query failure (-1), with deterministic repeat and no parent crash |
| Six calibration families | All registered source-backed family cases passed |

PrintCompositionRouting passed in 14.27 seconds; PrintPreparation3021 passed in 2.40 seconds. The 3021 result is the expected safe rejection, not an export-readiness claim. No compatible local Verified Fit Profile was present; therefore a genuine verified-profile ManufacturingMesh export was **not** performed. Synthetic manufacturing tests do not constitute physical fit verification.

## 9. Actual ELF floor and proposed support baseline

`readelf`, `objdump`, and `ldd` were run for the application, worker, Qt libraries, and relevant plugins. No unresolved shared-library dependency was found on this development machine.

| ELF | Highest observed requirements |
| --- | --- |
| Native Release BrickSuite | GLIBC_2.43; GLIBCXX_3.4.32; CXXABI_1.3.15 |
| Native Release worker | GLIBC_2.34; GLIBCXX_3.4.21; CXXABI_1.3.9, before transitive dependency requirements |
| Installed Qt Core / Gui x86_64 | GLIBC_2.34; GLIBCXX_3.4.29 |

The application's GLIBC_2.43 references include `sqrtf` and `acosf`; this is an actual binary requirement, not merely the host's installed libc version. Transitive dependencies must be checked too. The current artifact cannot target Ubuntu 22.04/24.04.

Recommend **Ubuntu 22.04 x86_64, GCC 11, Qt 6.10.3**, with a release ABI ceiling of **glibc 2.35** and GCC 11's libstdc++ requirements. Qt 6.10 lists Ubuntu 22.04/GCC 11 as supported; the inspected x86_64 Qt binaries are consistent with that target. This is the oldest practical Ubuntu baseline recommended for this release, not a claim that older Linux is impossible. [Qt supported platforms](https://doc.qt.io/qt-6.10/supported-platforms.html), [Ubuntu 22.04 libc6](https://packages.ubuntu.com/jammy/libc6).

Validate the recommendation by building and auditing the complete closure on 22.04 before advertising support. Pin Qt downloads and hashes; inspect every bundled library/plugin. If a selected Qt archive exceeds the floor, build the required Qt modules from pinned sources on the baseline rather than copying newer host libraries. Use baseline-compatible maintained OpenSSL 3.x, with an explicit security update policy. This audit did not create that environment or certify its artifact.

## 10. Artifact-format comparison

| Format | Advantages | Costs and boundaries |
| --- | --- | --- |
| AppImage | One downloadable executable; familiar portable distribution; can contain Qt/plugins/MCUT/OpenSSL | Still needs an old build floor, host graphics and Secret Service; FUSE availability/extract-and-run behavior; desktop integration and update mechanism need separate decisions |
| Portable tar.gz | Transparent dependency closure; easy relocation/inspection and clean-machine debugging; no FUSE requirement | Manual extraction and launcher/menu setup; explicitly documented system prerequisites; updates replace extracted versions |
| Native .deb | Declared dependencies, package-manager install/removal, desktop/icon integration | Narrower distro ABI/package-name scope; multiple distributions may need separate builds; cannot solve newer glibc by declaring dependencies |

Recommend **one x86_64 portable tar.gz first** for v0.4.0. Establish its relocatable closure and acceptance before considering an AppImage wrapper. Add .deb only if native package installation becomes a release requirement; do not maintain three formats by default. [AppImage build guidance](https://docs.appimage.org/reference/best-practices.html), [FUSE considerations](https://docs.appimage.org/user-guide/troubleshooting/fuse.html), [Debian shared-library policy](https://www.debian.org/doc/debian-policy/ch-sharedlibs.html).

## 11. Qt deployment inventory

Bundle the required Qt 6.10.3 runtime libraries: Core, Gui, Widgets, Sql, Network, WebSockets, OpenGL, OpenGLWidgets, and transitive DBus. Concurrent is linked by CMake but is absent from the inspected application's DT_NEEDED list; derive final inclusion from the release artifact rather than module names alone. xcb needs Qt XcbQpa; Wayland needs Qt WaylandClient and selected shell/graphics integration plugins.

Required/selected plugins:

- `sqldrivers/libqsqlite.so` only. This Qt SQLite plugin has no external SQLite SONAME dependency. Do not copy unrelated SQL drivers.
- `platforms/libqxcb.so`; include the selected Wayland platform and its integration plugins only with native Wayland acceptance.
- `tls/libqopensslbackend.so`; deploy compatible libssl.so.3 and libcrypto.so.3.
- Image support for actual application inputs: JPEG, ICO, WebP/GIF as supported, and SVG plus QtSvg if selected. PNG is built into this Qt Gui. Avoid indiscriminately shipping TIFF/TGA/ICNS/WBMP and their extra dependencies.

Record plugin hashes and recursively audit dependencies. A development Qt installation contains many irrelevant plugins. [Qt Linux deployment](https://doc.qt.io/qt-6.10/linux-deployment.html).

## 12. Native dependency contract

| Dependency | Proposed treatment |
| --- | --- |
| glibc, ELF loader, libm/resolver | System; minimum glibc 2.35; never copy the laptop's libc |
| libstdc++, libgcc | Baseline-compatible system runtime; verify GLIBCXX/CXXABI ceilings. Avoid an older bundled C++ runtime overriding a newer host Mesa requirement |
| Qt and MCUT | Bundle private runtime libraries; release MCUT SONAME observed as libmcut.so.1.2.0 |
| OpenSSL | Bundle baseline-compatible libssl/libcrypto 3.x, with tracked security updates; use host CA trust appropriately |
| ICU 73 | Bundle matching Qt's icui18n/icuuc/icudata requirements |
| zlib, zstd, Brotli | Audit baseline versions; include private copies where chosen for deterministic closure, preserving licenses |
| lib3mf and bundled ZIP support | Statically compiled; no separate lib3mf/libzip SONAME in the inspected executable. Include license notices for compiled third-party code |
| libsecret / secret-tool | Distro runtime package dependency, plus Secret Service provider; production calls system secret-tool |
| DBus, GLib/GThread, Kerberos support | Distro dependencies for the initial Ubuntu scope; Qt Network references libgssapi_krb5 |
| X11/xcb/xkbcommon | Distro dependencies: xcb cursor, icccm, util, image, keysyms, randr, render/render-util, shape, shm, sync, xfixes, xkb, xkbcommon-x11, X11-xcb as required by selected plugins |
| Wayland client/cursor/xkbcommon | Distro dependencies when shipping/accepting Wayland |
| fontconfig/freetype/fonts | Distro dependencies; test actual rendering on a clean desktop |
| OpenGL/EGL/GLX/GLVND, DRI and GPU driver stack | Host GPU/driver dependencies; do not copy developer GPU drivers into the bundle |

This is a proposed Ubuntu 22.04+ dependency contract, not a promise of a fully self-contained binary on arbitrary distributions. The exact package manifest must be resolved and tested on the chosen baseline. `libsecret-tools` is available as a separate Ubuntu runtime package. [Package record](https://packages.ubuntu.com/jammy/libsecret-tools).

The lib3mf dependency configuration uses included ZIP/zlib code; inspection did not establish a separate LibreSSL runtime requirement. Do not infer one solely from an option name in dependency CMake files. Preserve notices/source obligations for Qt, MCUT, lib3mf, and statically included dependencies.

## 13. RPATH and relocatability

The application and worker currently have absolute RUNPATH entries pointing to the developer Qt installation and build-tree `bin` directory, including a trailing empty entry. They are **not release-relocatable**. Qt's own libraries use `$ORIGIN`; plugins use a relative path to Qt libraries.

Proposed layout:

```text
BrickSuite/
  bin/BrickSuite
  bin/BrickSuiteMeshBooleanWorker
  bin/qt.conf
  lib/...
  plugins/platforms/...
  plugins/sqldrivers/libqsqlite.so
  plugins/tls/libqopensslbackend.so
  plugins/imageformats/...
  share/applications/...
  share/icons/...
  share/licenses/...
```

Use executable/worker RUNPATH `$ORIGIN/../lib`, private library RUNPATH `$ORIGIN`, appropriate plugin-relative paths, and `qt.conf` to locate bundle plugins. Audit all ELF entries, including transitive libraries. Do not use a global LD_LIBRARY_PATH wrapper that could contaminate the system `secret-tool` process or host driver loading. Packaging must reject checkout/home/build paths. The current Linux CMake install rule copies BrickSuite and its worker to `bin` but does not collect the Qt/MCUT/plugin closure. No install or RPATH changes were made during this audit.

## 14. Desktop integration

Create a desktop entry and suitable PNG/SVG icon assets in the packaging phase; the existing application icon resources alone do not install a Linux menu entry. Use Name=BrickSuite, a stable executable/launcher location, Terminal=false, and an appropriate registered category such as Utility. Tarball integration should be explicit and removable. No file association is needed now: the normal startup path does not offer a verified open-file CLI contract. Verify X11 and native Wayland separately. [Desktop entry specification](https://specifications.freedesktop.org/desktop-entry/latest-single/), [category registry](https://specifications.freedesktop.org/menu/latest/category-registry.html).

## 15. Clean-machine acceptance

Use fresh Ubuntu 22.04 and 24.04 desktop VMs plus this newer laptop. Install only the documented runtime prerequisites. No checkout, Qt SDK, compiler, CMake, Codex, or developer QT/LD environment variables may be required. Move the extracted artifact to a different path and make the development Qt prefix unavailable for the test.

Acceptance must cover startup/new database and migration of a representative synthetic older database, Help, real provider API access, Secret Service persistence across logout/restart and clear failure when unavailable, Host/Remote pairing and fingerprint rejection, two-machine cross-platform connections, installed LDraw persistence, native viewer rendering, 2456/3037/3021 preparation, worker timeout/containment/cleanup, calibration artifacts, an actually compatible verified-profile export where available, and backup/restore using disposable data. Exercise X11 and Wayland, scaling, populated Inventory/Build/pulling/procurement, and file dialogs.

Record exact OS/library/GPU versions, test evidence, expected limitations, and user acceptance separately from automated pass counts.

## 16. CI recommendation

Use GitHub Actions for the eventual Linux artifact, parallel in purpose to the existing macOS pipeline without changing it. Start with an explicit Ubuntu 22.04 runner, not `ubuntu-latest`; use a digest-pinned 22.04 container/sysroot when stronger environment reproducibility is needed. Pin GCC/toolchain package revisions or snapshot, CMake/Ninja, Qt 6.10.3 archive hashes, and third-party sources. Runner availability and image updates must be rechecked at implementation time. [GitHub-hosted runners](https://docs.github.com/en/actions/reference/runners/github-hosted-runners).

Gates, in order:

1. Configure Release and explicitly build every configured test target.
2. Run serial CTest with private DBus/Secret Service and synthetic data; retain JUnit/logs. Include a pinned, licensed LDraw fixture/library snapshot so source-backed cases are registered.
3. Run native-widget/OpenGL checks under a suitable Xvfb/Mesa environment; do not interpret offscreen-only results as graphics acceptance.
4. Assemble the single tarball; recursively verify ABI ceilings, ELF closure, plugin selection, licenses, no developer paths, and preserved 512 MiB worker policy.
5. Run relocated-bundle smoke checks without the SDK and with a clean environment.
6. Upload only the gated versioned x86_64 artifact, SHA-256 checksum, dependency/license manifest and provenance. Clean-machine desktop/two-machine acceptance remains a release gate.

Existing workflows cover macOS artifacts and published Help; no Linux artifact workflow was found. No CI was implemented in this audit.

## 17. Dependency-ordered phases and blockers

- **M39.L1 — Linux runtime/test closure:** technical build/test baseline and narrow fixes complete; obtain remaining populated workflow/native desktop acceptance.
- **M39.L2 — baseline and relocatable bundle:** validate Ubuntu 22.04 ABI floor, freeze runtime dependency/security contract, implement one tarball, desktop integration, license manifest and relocation checks.
- **M39.L3 — reproducible GitHub artifact:** pin the build environment/dependencies, enforce test/package gates, upload artifact and checksums.
- **M39.L4 — clean-machine and cross-platform Host/Remote acceptance:** baseline/newer desktops, X11/Wayland, real provider credentials, pairing/pinning, graphics/printing/calibration and backup.

No failing automated Linux tests remain. The exact outstanding decisions/prerequisites before committing to a package implementation are:

1. Establish the proposed Ubuntu 22.04/GCC 11/Qt 6.10.3 build environment and verify the **whole** dependency closure meets glibc 2.35; the laptop's glibc 2.43 binary cannot be reused as the release artifact.
2. Settle the one-tarball artifact contract, runtime package prerequisites, and selected xcb/Wayland plugins. Native Wayland smoke passed here, but baseline-machine Wayland acceptance is still required before advertising it.
3. Define maintenance ownership and pinned inputs for bundled OpenSSL/Qt/ICU/MCUT, including the Linux queue correction, licenses, and security updates.

Packaging must then resolve the known absolute RUNPATHs and missing runtime/plugin installation. Real provider/LAN, populated workflow, clean-machine, and cross-platform acceptance remain explicit **release** gates. A genuine compatible Verified Fit Profile is required for the conditional manufacturing export check; no profile was fabricated. These are distinct from the now-green technical baseline.

Final repository validation: `git diff --check` passed. Schema 35 and Protocol 1.5 remain unchanged. No commit or push.

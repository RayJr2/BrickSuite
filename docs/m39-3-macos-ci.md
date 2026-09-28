# M39.3 — separate macOS architecture artifacts

## Implementation

One manual workflow, `.github/workflows/macos-artifacts.yml`, uses a two-entry
matrix and the same `deployment/macos/ci_release.py` orchestration and
`package_macos.py` packaging implementation for both architectures:

| Architecture | Runner | Xcode | Deployment target |
| --- | --- | --- | --- |
| arm64 | `macos-15` | 16.4 | 13.0 |
| x86_64 | `macos-15-intel` | 16.4 | 13.0 |

GitHub's [runner reference](https://docs.github.com/en/actions/reference/runners/github-hosted-runners)
documents these labels. Xcode 16.4 is listed in both the
[ARM64 image](https://github.com/actions/runner-images/blob/main/images/macos/macos-15-arm64-Readme.md)
and [Intel image](https://github.com/actions/runner-images/blob/main/images/macos/macos-15-Readme.md).
[Qt 6.10's supported macOS configurations](https://doc.qt.io/qt-6.10/macos.html)
include Xcode 15 or newer and macOS 13 or newer on both architectures.

Each job verifies its native architecture and exact checkout SHA. The SHA appears
in job output, the app's Info.plist, build metadata and job summary. There is no
Universal2 artifact, Linux packaging, tag trigger, GitHub Release publication or
repository write permission. Matrix failures remain independent and visible.

Dependencies are built from source: hash-locked official Qt 6.10.3 and OpenSSL
3.6.4 archives, plus the unchanged MCUT/mio/lib3mf CMake pins and lib3mf bundled
dependencies. Qt uses its established configure/build/install process and the
M39.2 module/configuration choices. Source-building Qt also avoids inheriting a
precompiled kit's minimum OS or accidental Universal2 libraries. The Qt archive
digest is published in its [official download metadata](https://download.qt.io/official_releases/qt/6.10/6.10.3/single/qt-everywhere-src-6.10.3.tar.xz.mirrorlist).

CTest targets are discovered, built explicitly and run serially. All configured
tests must pass with no skips. The metadata lists the eight currently conditional
installed-LDraw integrations as unregistered, not skipped. Existing worker
containment fixtures must demonstrate success, bounded timeout, the sampled-memory
limit and cleanup. No geometry deadlines, assertions or production behavior change.

The production bundle excludes tests/probes. A disposable copy runs the expanded
packaged probe without personal Keychain access or live API keys. The exact plugin
allowlist, metadata, recursive architecture/dependency/minimum-OS audit, TLS,
worker, resource/database probes and deep strict codesign check all gate uploads.
Archives are extracted and compared for bytes, modes and symlinks before upload.
Signatures are ad-hoc, not Developer ID; these artifacts are non-notarized candidates.

## Local validation (2026-09-28)

Baseline was clean at `96b5ec3ad9bb03ba59a2f340d79b4f0427862dbd`, with the viewer
fix and M39.2 packaging committed. Production app sources, Schema 35, Protocol 1.5,
M37/M38 geometry and Windows/Linux behavior are unchanged by M39.3.

- Local host: arm64 macOS 26.7, Xcode 26.1.1, SDK 26.1. This does **not** substitute
  for the specified Xcode 16.4/macOS 15 GitHub builds.
- Reused the previously source-built Qt 6.10.3/OpenSSL 3.6.4 macOS 13 dependencies
  for local packaging checks; did not claim a fresh GitHub dependency bootstrap.
- Rebuilt BrickSuite/helper and all 115 configured test executables.
- First serial CTest: 114 passed, one `FitCalibrationArtifact` timeout at 180 s,
  zero skipped; eight installed-LDraw integrations unregistered. Its isolated
  unchanged rerun passed in 5.70 s. The workflow contains no automatic retry or
  weakened deadline; that first run would correctly block artifact upload.
- Final full serial suite: **115 configured, 115 passed, zero failed, zero skipped**
  in 60.79 seconds; eight installed-LDraw integrations remain unregistered.
- Nine portable release-gate tests pass, including rejection of wrong/mixed
  architectures, runner-OS minimum, external dependencies/rpaths/symlinks,
  unresolved run paths, missing results and inaccurate pass/skip accounting.
- Python syntax and shell syntax checks pass. Actionlint 1.7.7 passes with a local
  known-label configuration for `macos-15-intel`, which postdates that linter's
  bundled labels; the label was independently verified against GitHub's reference.
  Pinned checkout/setup-python/upload-artifact SHAs match their official tags.
- Shared local ARM64 packaging: 27 Mach-O files, arm64 only, minos 13.0; zero
  audit errors; deep strict ad-hoc signature verification passes.
- Clean-environment packaged probe passes: OpenSSL 3.6.4, P-256 identity/TLS,
  matching and wrong fingerprint cases, bundled MCUT union/subtraction, bounded
  worker rejection, actual Local Override self-launch with validated result,
  SQLite/Schema 35/backup, embedded Help/F1/icon, provider construction and runtime
  image closure. No real credentials or provider requests were used.
- Controlled worker test: success and memory/timeout termination passed; cleanup
  and child exit confirmed. Allocation is 64 MiB against a 32 MiB test limit;
  macOS enforcement remains sampled, not kernel-hard instantaneous containment.
- ZIP extraction preserves file bytes, executable bits and symlinks, and the
  extracted bundle passes the dependency audit and signature verification.

Local validation ZIP only (not a GitHub or M39.4 acceptance artifact):

| Field | Value |
| --- | --- |
| File | `/private/tmp/bricksuite-m393-local/BrickSuite-v0.4.0-macOS-arm64.zip` |
| Size | 30,019,265 bytes |
| SHA-256 | `0c0e88c3c03e17c5a98a47d62c799edb6ef5c93602a95324f9a96b819eef1ccc` |
| Production app source SHA | `96b5ec3ad9bb03ba59a2f340d79b4f0427862dbd` |
| Packaging scripts | Local M39.3 working changes validated before commit |

## GitHub status and M39.4 handoff

| Required result | GitHub ARM64 | GitHub x86_64 |
| --- | --- | --- |
| Build/Qt/toolchain/test counts | Pending push and dispatch | Pending push and dispatch |
| Package/architecture/dependency audit | Not run | Not run |
| TLS/MCUT/Local Override/containment | Not run | Not run |
| Codesign/archive size/SHA-256 | Not generated | Not generated |
| Upload/artifact link | Not uploaded | Not uploaded |

The user authorized a local implementation commit after validation, **no push**.
Ray will review and push it. A newly created local-only workflow cannot be
dispatched on GitHub. No GitHub run or Intel runtime result is claimed.

After pushing, dispatch **macOS architecture artifacts** on the reviewed branch.
Confirm both matrix jobs are green and both `build-metadata.json` files name the
same source SHA. Download `bricksuite-macos-arm64` and `bricksuite-macos-x86_64`;
verify each inner ZIP against its `.zip.sha256` before extracting with Archive
Utility or `ditto -x -k`. Use those exact archives for M39.4, not the local check ZIP.

On Ray's ARM Mac, quit the previous BrickSuite instance and test the downloaded
ARM64 app: launch, Help/F1, Keychain/API, LDraw/3D Viewer, Prepare for Printing,
Host/Remote, calibration and backup. Record macOS version, architecture, archive
hash, source SHA and individual outcomes. Preserve live data and credentials.

In the Intel macOS VM, record VM macOS version and `uname -m`, verify the x86_64
archive hash, then run the same acceptance checklist on that exact extracted app.
GitHub execution does not establish Intel UI/Keychain acceptance. If the VM runs
macOS 13, record that explicitly as Ventura evidence; macOS 15 CI does not prove
macOS 13 runtime compatibility. Actual macOS 13 acceptance and public signing/
notarization remain release gates, alongside investigation of any repeated
calibration timeout on clean GitHub runners.

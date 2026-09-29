# M39 macOS final closure: validation blocked

Historical initial attempt. See the [portability follow-up](m39-macos-portability-closure.md)
for the corrections, final 123/123 result and subsequent Deploy acceptance.

Validation date: 2026-09-29. Starting commit:
`c57a9b0d5682e0e00f19bb91d053f0c45067c83c`; starting working tree clean.
Host: native ARM64, macOS 26.7, Xcode 26.1.1. Release configuration uses
Qt 6.10.3 and OpenSSL 3.6.4 rebuilt for macOS 13.0, with application target
`CMAKE_OSX_DEPLOYMENT_TARGET=13.0`. No commit or push was made.

## Deploy implementation

The Apple-only `Deploy` target builds the application, Boolean worker and
disposable package probe. It invokes the existing packager, bundle validator,
ZIP round-trip validator, ad-hoc signer and extracted-package runtime probe via
`local_deploy.py`. No full test-suite dependency was added. The existing CI
architecture matrix and release gates are unchanged.

The probe is an excluded-from-ALL CMake executable using application sources
with the probe entry point. `package_macos.py` gained an optional binary-directory
argument for configuration-specific outputs; existing callers retain their
default behavior. The adapter records source commit/dirty state, host, Qt,
architecture, minimum OS, archive size, path and SHA-256. It publishes only after
the existing validation functions succeed.

Expected output:
`<release-build-dir>/deploy/BrickSuite-v0.4.0-macOS-arm64.zip`.
**No ZIP was produced in this closure attempt:** the requested prerequisite,
a green full installed-LDraw suite, was not met. ZIP size/hash, final dependency
and minimum-OS audits, deep/strict signing verification, archive round trip and
extracted-package runtime acceptance are therefore pending, not passed.

Release application, worker, all configured test executables and the new CMake
probe target built successfully. A separate Debug configuration rejected Deploy
with the required Release-only message before building its binaries. All 16
macOS packaging Python regressions passed (13 existing plus 3 adapter tests).
The latter cover early configuration rejection, dependency-source version checks,
and preservation/rollback/replacement of managed output directories.

## Installed-LDraw coverage

The installed library was discovered from the existing `LDraw.LibraryPath`
preference and supplied only to the local CMake cache using
`BRICKSUITE_CALIBRATION_LDRAW_ROOT`. No personal path or library was added to
repository defaults. The otherwise equivalent no-library configuration registered
115 tests. Enabling the existing option registered eight more, for **123**.
Previously these cases were unregistered, not skipped.

| Newly registered CTest case | Result | Seconds |
| --- | --- | ---: |
| FitCalibrationSourceBallSocket | Passed | 44.02 |
| FitCalibrationSourcePinBarrelHinge | Passed | 31.52 |
| FitCalibrationSourceInterleavedFingerHinge | Passed | 30.52 |
| FitCalibrationSourceClickHinge | Passed | 17.09 |
| FitCalibrationSourceRetainedRotatingWheel | Passed | 54.54 |
| FitCalibrationSourcePlainRoundBoreWheel | Passed | 55.06 |
| PrintCompositionRouting | Failed | 6.50 |
| PrintPreparation3021 | Failed | 0.57 |

All six calibration integrations exercised the actual installed source library.
The full serial Release CTest run completed in **326.07 seconds: 121 passed,
2 failed, 0 skipped, 0 disabled**. Existing per-test timeouts and assertions were
retained. Independent reproductions of the two failures were diagnostic runs;
they do not replace or hide the original full-suite result. Platform-conditional
assertions within executables remain distinct from whole-test skips.

## Geometry blockers and investigation

`PrintCompositionRouting` reaches part 3037 but preparation fails at Boolean
operation 4, exceeding the unchanged 512 MiB worker memory budget. The required
seven-operation Ready outcome is not achieved. The failure reproduced in an
isolated run and through native viewer preparation.

A separate instrumented build of the same pinned MCUT dependency located the
allocation growth in `triangulate_face` → CDT `add_super_triangle` → locator
initialization → KD-tree insertion/expansion. A diagnostic bounds capture showed
the inserted point and both bounds as `(nan, nan)`. The containment test fails
forever for these values and root expansion continues allocating. Memory sampling
confirmed actual RSS/physical-footprint growth, not merely a stale high-water
measurement. These observations identify the allocation mechanism; the upstream
origin of the non-finite triangulation coordinates has not been established.

Part 3021 remains safely Not Ready, but the strict test fails because its
triangulation query rejects at operation 2 with result `-2`, rather than the
established operation-5/result-`-1` expectation. Independent runs reproduced the
same observed rejection. The parent remains alive. This is an extraction/
triangulation divergence, not a library-path or missing-test configuration issue.

An isolated MCUT experiment disabling floating-point contraction did not solve
either case: 3037 still exceeded the memory budget and 3021 moved to an
operation-5 memory failure. No such compiler change was applied to production.
No dependency source, production geometry, assertion, tolerance, timeout or
resource limit was changed. A NaN guard alone would reject the invalid result;
it would not establish 3037's required Ready geometry. Further correction needs
geometry/numerical investigation, so work stopped before modifying frozen M37
semantics, as requested.

## Native acceptance

The disposable native harness uses isolated preferences and an in-memory
database; these checks are build-tree native acceptance, **not ZIP acceptance**.

- Part 3001 and an external `.ldr` referencing 3001 both rendered visible geometry
  with a shared native OpenGL context. Wireframe and axes changed the framebuffer;
  the black-viewport regression was not observed.
- Part 2456 rendered and reached Ready for Printing through the existing local
  preparation route. The installed-source routing test passed its 2456 checks
  before failing on 3037.
- Part 3037 rendered but preparation reported **Safe workload limit exceeded**.
- Part 3021 rendered and reported **Preparation failed**, without a parent crash.
  This safe UI outcome does not satisfy the stricter operation/code regression.
- The isolated LDraw setting was written and read back successfully.
- No existing compatible Verified Fit Profile was found; ManufacturingMesh export
  with such a profile was unavailable. No profile was fabricated.

## Qt Creator

Importing the clean Release build created a matching source-built Qt 6.10.3 kit
and `Release (imported)` configuration, preserving the original development kit.
After CMake refresh, `Deploy` was visible under **Projects → Build Settings →
Build Steps → Details → Targets**. The intended action is to clear `all`, select
`Deploy`, then build and read Compile Output. The identical CLI entry is
`cmake --build <release-build-dir> --target Deploy`.

Selecting a target checkbox through the native UI repeatedly crashed Qt Creator.
The crash report records `EXC_BAD_ACCESS` in `QAccessibleCache::deleteInterface`,
through accessibility update and tree-model `setData`/delegate handling. No
BrickSuite packaging command was running. Import was subsequently saved using a
normal quit, but a successful Qt Creator Deploy invocation has **not** been
validated. The IDE crash is a separate remaining workflow blocker.

## Preserved boundaries and remaining work

Exact changed files (all uncommitted):

- `CMakeLists.txt`
- `cmake/BrickSuiteMacDeploy.cmake` (new)
- `cmake/RequireMacDeployRelease.cmake` (new)
- `deployment/macos/package_macos.py`
- `deployment/macos/local_deploy.py` (new)
- `deployment/macos/test_local_deploy.py` (new)
- `deployment/macos/ViewerAcceptanceMain.cpp`
- `deployment/macos/README.md`
- `docs/m39-final-macos-validation.md` (new)

Schema 35 and Protocol 1.5 remain unchanged. No CredentialStore/Keychain,
Remote security, M37 production geometry/fit, M38 help, Windows installer,
Linux deployment or GitHub architecture-matrix changes were made.
`git diff --check` passed.

Closure requires resolving the installed-source MCUT divergence without weakening
the assertions or limits, rerunning all 123 tests serially, and then exercising
Deploy from CLI and Qt Creator. Only then can the new ZIP's architecture,
minimum-OS, signing, dependency, archive and runtime claims be validated. User
workflow acceptance remains separate from technical test results.

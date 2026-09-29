# M39 macOS portability follow-up

This continues the uncommitted work from
`c57a9b0d5682e0e00f19bb91d053f0c45067c83c`. The prior 121/123 result remains
recorded in [the initial closure report](m39-final-macos-validation.md).
No commit or push is authorized or performed.

## Preserved failure evidence and platform comparison

The installed library is selected through `BRICKSUITE_CALIBRATION_LDRAW_ROOT`
in the local cache, not a repository default. Original captured operands are
finite, index-valid and pass the existing closed-manifold input validation.

| Case | Original macOS | Previously recorded Linux |
| --- | --- | --- |
| 3037 | Boolean 4, `p/4-4cyli.dat\|p/4-4disc.dat`; actual memory growth and worker termination | Ready after seven Booleans; 1,720 triangles, one valid source-faithful manifold, deterministic repeat |
| 3021 | Boolean 2, face-triangulation size query `-2`; Failed/BooleanFailed, deterministic and contained | Boolean 5, same query `-1`; expected deterministic Not Ready, no parent crash |

Linux evidence is the checked-in [Linux readiness report](m39-linux-readiness-audit.md)
and the unchanged source-backed assertions it passed. No Linux machine was
executed during this Mac task. Raw Linux operand dumps were unavailable here;
byte-for-byte cross-platform operand equivalence is not claimed.

Both platforms use the same source identities, semantic builder, coverage
requirements, role sorting and sequential MCUT route. Part 3037 has 792 source
triangles and eight covered groups/operands; 3021 has 508 source triangles and
nine operands. MCUT options remain double vertices, enforced general position,
and the existing union fragment filters. The 512 MiB production budget, face/
vertex limits and deadlines are unchanged. The first 3037 operands contain
20 vertices/36 triangles and 50 vertices/96 triangles. Original macOS accumulated
face counts entering operations 1–4 were 36, 142, 260, 402; the additive stud
has 96 triangles each time. The failing call contains 203 accumulated vertices
and 50 additive vertices. Thus divergence is already visible in *earlier MCUT
results*, not in source identity or semantic recognition.

### First non-finite value

Instrumented copies of the pinned dependency, isolated from production, located
the first NaN in `source/frontend.cpp`, `triangulate_face`, at
`normalize(perturbation_vector)` during duplicate projected-vertex handling.
The affected face is 55; current vertex 8, recorded counterpart 6, predecessor
7, successor 9. The predecessor vector is `(0,0)`, successor vector is
`(0.77855987548828054,-0.52031993865966775)`, and the selected perturbation
vector is `(0,-0)`. Normalizing it creates the first non-finite coordinates.
Source coordinates, both Boolean inputs, the extracted 3D face and its initial
2D projection are finite. This happens inside MCUT, after dispatch, during the
face-triangulation size query; it is not a BrickSuite output conversion defect.

The upstream duplicate map contains *compacted* indices, but this caller treats
them as original indices. After earlier duplicates, it misidentifies an adjacent
duplicate and selects the zero edge for normalization. NaN then reaches CDT
super-triangle bounds and KD-tree insertion. `isInsideBox` remains false, and
root expansion keeps allocating. Measured RSS/physical footprint grew to over
512 MiB before the unchanged watchdog killed the worker; the parent survived.

Correcting that index alone experimentally removed runaway allocation but yielded
degenerate output faces, correctly rejected by BrickSuite. That geometry-changing
experiment was **not** adopted. No arbitrary coordinate replacement, duplicate
deletion, new Boolean route or retry loop was introduced.

## Narrow production corrections

The pinned revision remains `047d75ffe6e33ede572cb25217047a4756188401`.
`BrickSuiteMcutPortability.cmake` generates a small source/header overlay without
editing downloaded sources:

1. Name `std::minstd_rand0` for general-position seeding, retaining seed 1.
   `default_random_engine` selected a different engine under libc++ than under
   the validated Linux/libstdc++ configuration.
2. Disable floating-point contraction for MCUT only, including its predicates:
   GCC/Clang `-ffp-contract=off`, MSVC `/fp:strict`. No application-wide arithmetic
   flags, tolerances, geometry ownership or fit-family definitions change.
3. Reject non-finite CDT coordinates before bounds computation, state mutation
   or KD-tree insertion. The existing MCUT exception path returns a bounded
   error. Invalid geometry is never repaired or accepted by this guard.

Controlled experiments showed that neither the explicit engine nor disabling
contraction alone restored 3037. Together they pass its original seven-operation,
1,720-triangle, manifold, dimensional-fidelity, retained-stud and exact-repeat
assertions. The common implementation is used on all platforms.

The first general-position perturbation has the same scale
`0.00069978568147683619`. The old Mac vector was
`(-0.00022844733598835151,0.00058629103542240726,-0.00068780128551285873)`;
the explicit-engine/non-contracted vector is
`(-0.000040057536549056574,0.000065260955752635124,0.00046201166514151475)`.
Final Mac accumulated face counts entering 3037's seven operations are
36, 174, 324, 498, 640, 960, 1324; additive counts are
96, 96, 96, 96, 288, 288, 288. The final result is 1,720 triangles.

BrickSuite additionally checks returned vertex coordinates immediately after
MCUT extraction, before requesting triangulation, and clears partially decoded
meshes when rejecting malformed worker output. The latter was exposed by a new
non-finite worker-output test: the error was already correct, but invalid partial
mesh storage remained attached to it. Integration identity
`1.2.0-047d75f-portability1` prevents preparation-cache reuse under the old backend
identity. No schema or protocol version change is involved.

## 3021 portable contract

The guarded production path now rejects operation 5 with triangulation-size-query
error `-4` (`MC_INVALID_VALUE`, MCUT's mapping of the guard's `invalid_argument`).
This is a bounded extraction rejection, with no timeout, memory exhaustion,
parent crash or PreparedMesh. Three independent process runs, each also repeating
preparation internally, produced the same diagnostic.

The test accepts only demonstrated query-rejection pairs: operation 5/code -1
(Linux), operation 2/code -2 (original Mac), or operation 5/code -4 (finite guard).
It requires the exact stud operand identity, Failed/BooleanFailed, complete
source coverage, ordered operation progress, successful preceding operations,
failed final operation, no Ready mesh, and identical repeated rejection. Arbitrary
backend errors, changed routes, resource failures and invalid-result failures
do not satisfy it. No 3021 geometry correction or Ready conversion was made.

## Regression and native evidence

`McutMeshBoolean` now includes a bounded child exercising NaN, positive infinity
and negative infinity in both CDT coordinates, for initial and subsequent
insertion. It requires rejection before state mutation. Compiled against the
original unguarded header, the same test fails under containment; with the overlay,
it passes. Existing timeout, memory, failure and cleanup fixtures remain active.
An added malformed worker-output fixture proves no non-finite partial mesh escapes
in the failure result. Worker input decoding also rejects all three non-finite
values. The configured CTest count remains 123.

All six source calibration families passed with the corrected MCUT. Final focused
MCUT/3037/3021 tests passed, as did all 16 packaging Python regressions. Intermediate
red results were retained and investigated: the finite guard's newly demonstrated
-4 diagnostic needed explicit classification, and the partial-output-mesh fixture
required clearing failure results. Neither was hidden by retries.

Native build-tree acceptance passed for 3001, an external `.ldr`, 2456 Ready,
3037 Ready and 3021 safely Not Ready. Visible geometry, native shared OpenGL
context, edges/axes and isolated LDraw preference persistence were verified.
These checks alone are not packaged acceptance. No compatible existing Verified
Fit Profile was available; none was fabricated.

## Qt Creator investigation and workaround

Qt Creator 20.0.2 on macOS 26.7 reproduces the crash when toggling the standard
`all` checkbox in the large Release target list, as well as Deploy. The Debug
configuration's smaller list permits both selecting Deploy and clearing all.
The crash is `EXC_BAD_ACCESS`/SIGSEGV in `QAccessibleCache::deleteInterface`,
through `QAccessible::updateAccessibility`, `QAbstractItemModel::layoutChanged`,
`Utils::BaseTreeModel::setData` and `QStyledItemDelegate::editorEvent`. No package
command or BrickSuite application code is running. No duplicate/recursive target
dependency or invalid CMake metadata was found. The observations point to the
IDE's accessibility/model handling; they do not establish the exact upstream
heap-lifetime defect.

Safe normal IDE workflow: select the existing Release kit, refresh CMake, open
**Projects → Build Settings → Build Steps → Details**, retain `all` and enter
`--target Deploy` in **CMake arguments**. The preview is
`cmake --build <release-build-dir> --target all --target Deploy`. This avoids
the checkbox interaction and invokes the same target and packager. No shell
step, `.user` file editing/commit, global accessibility change, new packager or
Release-gate bypass is required. The setting was saved through Qt Creator.

## Final release gate and package

The first follow-up full run stalled in `FitCalibrationSourceRetainedRotatingWheel`
after the focused cases had passed. Sampling showed the main thread blocked in
`mcDispatch`/`wait_for_events_impl` and the API thread asleep in
`thread_safe_queue::wait_pop_head`, with no computing thread. This is the same
lost-wakeup pattern as the existing Linux correction. The original 100,000-
transition queue test also failed on this Mac after its 10-second deadline.
The partial full run was stopped and retained as interrupted evidence, not counted
as a completed or green suite.

The existing synchronized-notification overlay now applies on every OS; its
implementation and historical path are retained. `McutMeshBoolean` runs the
existing queue regression in a contained child, keeping the Mac CTest count at
123. Linux's standalone test remains registered. This is a synchronization
correction, not a geometry change, and no deadline was increased.

After explicitly building all configured executables, the final serial Release
CTest run passed **123/123**, with **0 failed, 0 skipped, 0 disabled**, in
315.26 seconds. Focused validation passed 9/9 in 140.27 seconds, including MCUT
containment/finite/queue tests and all eight installed-source cases. Packaging
Python regressions passed 16/16. The eight source cases in the final full run:

| Test | Result | Seconds |
| --- | --- | ---: |
| FitCalibrationSourceBallSocket | Passed | 43.81 |
| FitCalibrationSourcePinBarrelHinge | Passed | 32.49 |
| FitCalibrationSourceInterleavedFingerHinge | Passed | 31.49 |
| FitCalibrationSourceClickHinge | Passed | 17.58 |
| FitCalibrationSourceRetainedRotatingWheel | Passed | 56.25 |
| FitCalibrationSourcePlainRoundBoreWheel | Passed | 56.59 |
| PrintCompositionRouting (2456 and 3037) | Passed | 6.63 |
| PrintPreparation3021 | Passed | 0.67 |

Only after the full suite passed, command-line `Deploy` completed all gates.
Qt Creator's visible Release (imported), Qt 6.10.3 configuration then completed
`cmake --build <release-build-dir> --target all --target Deploy` successfully in
25 seconds. Its normal CMake refresh also succeeded. The checkbox crash itself
remains an IDE issue; the saved CMake-arguments workflow avoids it without changing
the target or packaging implementation.

The final artifact is `build/m39-final-release/deploy/BrickSuite-v0.4.0-macOS-arm64.zip`,
30,016,245 bytes, SHA-256
`2448d846c5b9e54dba1e06dc7e302b5ea5d4401b3dab3aed6aa8280a6993cd6d`.
It identifies the starting SHA and dirty source state, ARM64, Qt 6.10.3,
minimum macOS 13.0 and ad-hoc/non-notarized signing. The adjacent metadata,
bundle audit, SHA-256 sidecar and probe log describe this exact artifact.

Both Deploy invocations passed architecture/minimum-OS and dependency closure,
plugin allowlist, inside-out signing and deep/strict verification, ZIP round-trip
bytes/permissions/symlinks, and the extracted-package probe. The probe confirms
OpenSSL 3.6.4 without environment overrides, ephemeral P-256 TLS and fingerprint
rejection, Protocol 1.5, QSQLITE/Schema 35/verified backup, MCUT union/difference
and watchdogs, actual packaged Local Override self-launch, Help resources/F1
mapping, icon and loaded-image closure. GUI acceptance is recorded below separately.

The production ZIP contains 27 audited Mach-O files, all ARM64 with minimum OS
at or below 13.0, and zero audit errors. A fresh extraction passed deep/strict
verification again. Native acceptance used a disposable copy of that extraction,
with the existing test-only startup/viewer entry points linked from Release
objects, normalized to the extracted libraries, audited and re-signed. These
entry points isolate INI settings and synthetic database state; they are absent
from the deliverable. The actual packaged production executable and worker are
also exercised by the package probe. No build-tree library search environment
was used for extracted-package checks.

The untouched extraction's own `Contents/MacOS/BrickSuite` was then launched
directly with a separate temporary HOME/TMPDIR. It created its database under
that home, displayed the 0.4.0 main window, opened fully rendered Help via F1,
and exited with code 0. This direct launch used the production entry point and
native settings backend; no wrapper or bundle modification was involved.

| Extracted-package native check | Outcome |
| --- | --- |
| Main-window startup | Visible BrickSuite 0.4.0 workspace; fresh isolated Schema 35 database; clean exit |
| F1 Help | Visible Help dialog and rendered Getting Started text/images |
| 3001 | Native shared OpenGL context; 34,417 visible pixels; edges and axes change framebuffer |
| External `.ldr` | Native rendering, 34,417 visible pixels; edges/axes pass |
| 2456 | Ready for Printing via established native/local preparation |
| 3037 | Ready for Printing; source test additionally proves seven Booleans, 1,720 finite triangles and exact deterministic repeat |
| 3021 | Preparation failed safely; strict source test proves deterministic bounded triangulation rejection |
| Installed LDraw preference | Selected local library written/read through isolated UserSettings/INI; source loaded successfully |

The isolated startup log contains no critical or operational warning. Opening
Help produces Qt's missing `Sans-serif` font-alias performance warning (114–115 ms);
Help remains visible and functional. No credentials or real databases were used;
viewer preference writes were confined to test INI settings. Existing Verified
Fit profile acceptance still needs a compatible
user-owned profile; none was fabricated. Execution was on macOS 26.7, with
13.0 compatibility audited in every bundled binary, not tested on a macOS 13 host.

## Exact working-tree files

Modified:

- `CMakeLists.txt`
- `cmake/BrickSuiteMcut.cmake`
- `cmake/BrickSuiteMcutLinuxQueue.cmake`
- `deployment/macos/README.md`
- `deployment/macos/ViewerAcceptanceMain.cpp`
- `deployment/macos/package_macos.py`
- `src/services/geometry/print/McutMeshBooleanService.cpp`
- `tests/LDrawPrintPreparationServiceTest.cpp`
- `tests/McutMeshBooleanTest.cpp`
- `third_party/mcut/README.md`

Added (untracked):

- `cmake/BrickSuiteMacDeploy.cmake`
- `cmake/BrickSuiteMcutPortability.cmake`
- `cmake/RequireMacDeployRelease.cmake`
- `deployment/macos/local_deploy.py`
- `deployment/macos/test_local_deploy.py`
- `docs/m39-final-macos-validation.md`
- `docs/m39-macos-portability-closure.md`
- `tests/McutFiniteGeometryChecks.cpp`
- `tests/McutQueueRegression.cpp`

This list includes the preserved Deploy work from the initial attempt.
`git diff --check` passes. No IDE state, local LDraw default, downloaded source,
credential, application database, log or generated package is tracked. HEAD is
unchanged; no commit or push was performed.

## Cross-platform follow-up

Linux and Windows must rerun `McutMeshBoolean`, `PrintCompositionRouting`,
`PrintPreparation3021`, all six `FitCalibrationSource*` cases, calibration/
ManufacturingMesh tests and their full suites. Linux also retains `McutQueueWakeup`.
Confirm native 2456/3037/3021 workflows and dependency packaging on each platform.
Mac results do not substitute for these runs. Schema 35, Protocol 1.5, Keychain,
M37 fit semantics, M38 help, installers and the GitHub architecture split remain
unchanged. No limits or test registrations were removed.

There is no remaining blocker in the requested local ARM64 test/Deploy gates.
The Qt Creator checkbox defect remains, with a verified normal IDE workaround.
Windows/Linux validation of shared changes and GitHub's two architecture jobs
remain follow-up release work; neither Intel nor those operating systems was
validated by this Mac run. User workflow acceptance remains separate from the
automated and disposable native checks above.

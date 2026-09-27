# Unified Calibration: installed-user acceptance (v0.4.0)

This is the release acceptance record and checklist, not a second user manual.
Current user instructions live in `resources/help/fit_calibration.html` and
`resources/help/calibration_packages.html`, embedded by
`resources/help/help.qrc`. Package contracts are documented in
[calibration-package-generation.md](calibration-package-generation.md).

## Engineering audit

| Area | Implementation and acceptance evidence |
| --- | --- |
| Release entry point | `MainWindow` exposes Tools → LEGO Fit Calibration outside the Debug-only menu. All generation modes call `FitCalibrationGenerationService`; no developer executable or manual artifact identity is required. |
| Workspace lifecycle | `FitCalibrationDialog` restores the saved workspace, activates combo selection and New immediately, and selects a valid fallback after deletion. Dirty Save/Cancel and failed-save handling retain the prior context. Empty workspaces disable feature evidence controls. `FitCalibrationWorkspaceUiTest` exercises these paths, including a separate process restart. |
| Single package | A unique folder contains matching `.3mf` / `-session.json` and `publication.json`. Geometry/session agreement, 3MF reopen, staged rename, collision handling, registration interruption and idempotent recovery are covered by `FitCalibrationGenerationServiceTest`. Historical single-session JSON remains supported. |
| Recommended / Custom | The recommended perpendicular core contains Stud OD, Tube Wall Cell, Post Wall Cell and Technic Axle Hole arm width v2. Custom uses capabilities and a plan preview; only the full core combines, with other selections in separate fixtures. `FitCalibrationUnifiedGenerationTest` covers independent identities, companions, sibling isolation, publication and recovery. UI tests open both planning paths. |
| Evidence / lineage | Continuations retain coarse/extension history and begin fresh later observations. Service/library tests cover lineage and orientation. The UI regression reopens an explicitly Verified child and checks seven original Coarse candidates and their observation separately from three verification candidates and their repeats. It also switches tabs to guard against displaying the latest artifact twice. |
| Installed sources | `unavailableReason` checks required Parts beneath the configured LDraw root; source generators load through `LDrawLibraryService` before publication. Missing dependencies fail loading, without substitute geometry. Six installed-source tests cover Ball Socket, Pin / Barrel Hinge, Interleaved-Finger Hinge, Click Hinge, Retained Rotating Wheel and Plain Round-Bore Wheel, including continuation and Verified-session reload. The test-only CMake LDraw root is not a runtime application dependency. |
| Corrections | Existing evidence policy, profile matching and ManufacturingMesh tests remain authoritative. Supported Standard Stud OD/height and Tube/Post Wall corrections are production behavior; recognition, verified contract and printed orientation still gate application. No functional geometry, correction values, profile rules or override ownership behavior changed in this audit. |

The audited runtime calibration sources contain no repository paths, test-fixture
paths or command-line generation requirement. Generators, immutable definitions
and labels are compiled into the application; Help is a Qt resource. User-selected
LDraw geometry remains an external installed-library prerequisite for the relevant
families, not a repository prerequisite.

The audit exposed one publication defect: the compact three-candidate Standard Bar
verification fixture could not fit the ordinary single-line underside label.
Its existing `BAR-OD-PERP` abbreviation now occupies two underside lines, retaining
the fixed letter size, fixture bounds, candidate values and protected bar surfaces.
The seven-candidate coarse layout is unchanged. The workspace regression generates,
verifies and reopens this child through the real service and dialog.

`FitCalibrationArtifactLocation` uses `QStandardPaths::DocumentsLocation`, with
the existing Home fallback, plus `BrickSuite/Calibration Artifacts`. Managed
sessions, workspace records, profiles and history use `AppDataLocation` under
`LegoCalibration`; the last-workspace preference uses `QSettings`. Publication
uses Qt filesystem operations and sibling staging. Multi-fixture assembly uses a
temporary workspace and publishes only the complete checked package. These paths
are platform-aware; no Windows-only calibration path was introduced.

## Ray's installed Release acceptance checklist

Use an isolated acceptance workspace and actual physical observations. Do not
invent evidence in a real calibration or delete an existing production workspace.
Record app version, OS, printer/material/nozzle/process, generated package paths
and pass/fail results. Build/CTest success does not complete this checklist.

1. Launch the **installed Release** application as a normal user with the
   repository/build folders unavailable. Open built-in Help and confirm it loads.
2. Open **Tools → LEGO Fit Calibration...**. Configure and Validate the installed
   library in **Edit → Settings → 3D Models** for source-dependent families.
3. Confirm the last-used workspace and manufacturing context restore. On a clean
   installation, an empty state is expected until the first workspace is created.
4. Use **Workspace... → New Workspace...** to create a disposable context; switch
   the combo between contexts without restarting. Exercise pending-edit Cancel
   and Save. Delete only a disposable workspace using **Delete Selected
   Workspace...**; confirm fallback. An empty workspace must not accept feature
   orientation/notes. Leave the intended acceptance context selected.
5. Select **Single Calibration → Standard Stud**, **Generate Calibration
   Fixture...**, outside diameter. Confirm busy feedback stays responsive and
   **Calibration Package Ready** lists a same-base 3MF/session pair in a new
   Documents package folder. A second generation must not overwrite it.
6. Select **Recommended Calibration Package**, review the plan and generate.
   Expect one perpendicular fixture containing four independently identified
   calibrations, with a package companion.
7. Select **Custom Calibration Package**. Choose the four recommended core
   members plus Standard Bar. Expect five calibrations in two fixtures in the
   preview and output. Check unavailable sources/prerequisites are explained;
   do not remove installed library files to simulate failure.
8. Open every generated 3MF in the preferred slicer at 100% scale. Inspect
   dimensions, orientation, readable family labels and candidate identification.
   Print the fixture(s) with the recorded process before recording real evidence.
9. Use **Import Session / Package...** for the Single `-session.json`, then a
   multi-family `package-session.json`. Select each member in **Feature
   calibration**; reimport must preserve newer managed evidence. Check a retained
   historical single-session companion if available.
10. Record actual printed orientation and a genuine observation using **Add
    Observation**, then **Save Managed**. Switching away/back must retain it;
    sibling calibrations must not inherit it.
11. When physical evidence has a boundary Preferred result (or uniformly Too
    Tight/Too Loose), use **Continue Calibration...**. Confirm a new child/package
    within the supported range, unchanged parent evidence, and fresh child rows.
    If no genuine boundary result is available, leave this manual check pending;
    automated tests exercise synthetic boundary cases.
12. Continue an interior Preferred result into Fine Search or direct verification.
    After printing, record new repeats and neighbors. **Coarse Search** must show
    the original/extension evidence; **Fine Search / Verification** must show the
    later artifact. Review earlier coarse stages in the history selector. Include
    Standard Bar direct verification to inspect the corrected two-line underside
    label on its compact three-candidate fixture.
13. Use **Mark Verified** only when physical evidence satisfies the enabled gate.
    Save, close and reopen the dialog; select the Verified feature and confirm
    the original Coarse and later verification rows remain separate.
14. Choose **Create / Update Fit Profile...**. Confirm the manufacturing context,
    verified features and actual orientations. Existing compatible corrections
    must remain; unverified or mismatched features must not be promoted.
15. Restart BrickSuite. Confirm workspace restoration, observations, lineage,
    separate Coarse/Fine review, Verified status and profile persistence. Check
    the application log for unexpected warnings/errors from these operations.
16. Use **Recover Package...** on an intact package's `publication.json` or
    companion. Repeating recovery must preserve observations and avoid duplicates.
    Actual interrupted-registration recovery is fault-injected in automated
    temporary-storage tests; do not kill the installed application or edit real
    managed records to manufacture this condition.

For difficult Parts, separately follow the tool-agnostic external-repair Help:
native preparation → external repair export → preferred repair tool → import →
strict validation or eligible explicit user-reviewed nominal acceptance → nominal
Prepared Mesh export. External repair does not establish fit ownership. The current
Experimental Auto Fit gate rejects unproven repaired-surface mapping with zero
applied corrections and keeps the nominal override available.

## Visual Help and packaging follow-up

There are no existing calibration screenshots in this Help topic to replace.
The existing PNG/QRC infrastructure is sufficient; no documentation tooling change
is needed. Defer actual installed-app captures to M39 after runtime acceptance:

- Restored workspace with generation mode selector and feature context.
- Custom core-plus-Bar plan showing five calibrations / two fixtures.
- A genuinely Verified feature's distinct Coarse and Fine views, plus the
  Create / Update Fit Profile action (redact personal manufacturing information).

The automated offscreen UI test checks behavior, not visual layout at user DPI.
No synthetic screenshot should be presented as an accepted physical calibration.

M38 must verify the Windows installed Release without Qt/developer PATH or repo
access, required runtime libraries/plugins and any override worker deployment,
normal-user Documents/application-data permissions, Unicode/long/redirected paths,
Help loading and configured LDraw access. macOS bundle resources/runtime signing
and Linux installed executable/library/plugin resolution need equivalent installed
checks. Also verify OS-specific Documents/AppData locations and permission failure
diagnostics. No macOS/Linux packaging or installed-machine acceptance is claimed by
the Windows engineering audit.

## Completion gate

Finalization validation on Windows: Qt 6.10.3 MinGW Release configure and build
passed, including explicit builds of all 113 configured test executables. The
focused calibration/ManufacturingMesh selection passed 8/8; full configured CTest
passed 119/119 in 156.36 seconds with four workers, including all six installed
LDraw source integrations. Both working-tree and staged `git diff --check` passed.
No Debug build, installed-app acceptance, physical printing or macOS/Linux run was
performed. Changes remain uncommitted; no push was performed.

The architecture may remain frozen while acceptance runs. Final v0.4.0 sign-off
requires green Release/focused/full/installed-source tests and Ray's checklist
results, with M38 installed-platform packaging acceptance recorded separately.
No physical-print, slicer or installed-user result is inferred from automated tests.
Schema remains 35 and Protocol remains 1.5.

# Calibration package generation

`FitCalibrationGenerationService` is the UI-independent Phase 1 boundary for
the generation paths previously orchestrated by `FitCalibrationDialog`.
`Request` selects family/variant, coarse/fine/verification stage, supported
orientation, manufacturing workspace, optional parent session, optional later
stage candidate spacing/count, and the installed LDraw library when needed.
Boundary extension is determined by the existing evidence policy. Coarse values
come from the existing family definitions, not a second table of defaults.

Geometry and observation templates are produced together by the existing family
generator. The service adds labels using the existing naming/label infrastructure,
assigns fresh print and session identities, and retains the generator definition
identity in the publication record. Continuations reuse `continuationSession` to
retain historical evidence and begin with empty child observations. No generator,
verification policy, profile merge rule, or ManufacturingMesh algorithm changes.

## Publication and registration

The default root is `FitCalibrationArtifactLocation::directory()`, using the OS
Documents location (with its existing Home fallback). Each print gets a new
`<family-stage>-<UUID>` folder containing:

- `<family-stage>.3mf`;
- `<family-stage>-session.json`, the existing single-session JSON format;
- `publication.json`, a versioned publication record with file hashes, definition,
  print, and session identities. This is not a new session format.

Publication happens in this order:

1. Generate geometry and its session from the same definition.
2. Write in a temporary `.pending-*` sibling directory.
3. Parse the companion and compare the complete session with the generated one.
4. Reopen the millimeter 3MF with lib3mf and compare object counts, vertex
   coordinates (including placements), and triangle indices with generated data.
5. Write the publication record and rename the complete staging directory to its
   unique final name. An existing destination is never overwritten.
6. Register the session through the existing managed calibration library.

Before the rename, failures discard staging; a process crash can leave an
unpublished `.pending-*` directory, which recovery refuses. After the rename,
registration failure leaves a complete, hash-bound package and an explicit
published-but-unregistered result. The UI never reports this as fully completed.
This is process-interruption recovery, not a filesystem power-loss guarantee.

`recover(directory)` validates the publication record, both hashes, the session
identity, and readable 3MF before retrying registration. A library-root lock
serializes package registrations. Repeated recovery accepts an existing matching
definition while preserving later observations, verification, and process edits;
it never replaces that session with the original draft. Conflicting definitions
or unreadable existing records are rejected. Legacy session import remains with
the unchanged library importer; it does not require a publication record.

## Single-calibration UI

**Generate Calibration Fixture...** and **Continue Calibration...** call the same
service on a background worker with modal busy feedback. The dialog retains
selection, observations, stage review, Verify, and Create / Update Fit Profile.
It no longer writes generation geometry or companion files, chooses artifact IDs,
or registers generated sessions itself. The completion summary identifies both
files and retains print instructions. **Recover Package...** retries a complete
published package after interruption, including after application restart.

Definitions and Help remain application resources; generated packages belong in
Documents; managed sessions/profiles remain in application data. No repository
or developer executable is required for supported generation paths.

Single-family generation retains its existing companion and recovery contract.
Multi-family generation now builds on this pipeline and the existing unified
pilot as described in Phase 3 below.

`FitCalibrationGenerationServiceTest` uses temporary artifact/library roots and
publication checkpoints to test paired output, definition agreement, context,
lineage, all currently exposed coarse paths, orientations, destination isolation,
partial writes, reopen rejection, interrupted registration, evidence-preserving
retry, unchanged single-session import, and invalid output destinations. Existing
calibration library/artifact/package tests continue to cover evidence, profiles,
historical formats, and generator geometry independently.

## Phase 2 family and continuation coverage

`FitCalibrationCapabilities` is a UI-independent registry consumed by the selector
and generation preflight. It records variants, physical fixture orientations,
stages, candidate count/spacing controls, established correction ranges, installed
source requirements, prerequisites, and candidate marker strategy. The dialog
contains no mesh generation or publication logic. The existing evidence policy
still determines whether observations qualify for continuation or verification.

The additional supported source fixtures are:

| Family | Installed source | Fixture orientation | New continuation range |
| --- | --- | --- | --- |
| Ball Socket | 14418 | Parallel | Existing +/-0.40 mm contact/throat correction |
| Pin / Barrel Hinge | 3938 | Parallel | Existing +/-0.25 mm pin OD correction |
| Interleaved-Finger Hinge | 4275a | Parallel | Within existing +/-0.15 mm bump-height range |
| Click Hinge | 30345 | Parallel | Within existing +/-0.15 mm arrestor-height range |
| Retained Rotating Wheel | 30027b | Perpendicular | Within existing +/-0.15 mm bearing correction |
| Plain Round-Bore Wheel | 30027a | Perpendicular | Existing source generator's blind-bore validation |

The three previously fixed generators accept a stage definition while retaining
their original coarse defaults and nominal source geometry. Pin/Barrel now accepts
3, 5, or 7 candidates. Frictionless Pin continuation reuses the retained source
profile and existing operand regenerator; its bore, slot and axial profile remain
protected. Boundary extensions outside established bounded source ranges remain
unavailable: wider ranges require a separately validated geometry contract.

New workflow requests use odd counts from 3 through 7. Seven still brackets the
coarse search; three/five provide the existing repeat/fine-search stages and
neighbor evidence. No historical candidate array is reduced or reindexed.
Source-backed pieces retain existing direct identification: Ball Socket embossed
numerals and hinge/wheel relief dots. The unchanged dot helper uses 1.04 mm dots,
0.60 mm relief, and at least 0.56 mm pad spacing (0.40 mm nozzle / 0.20 mm layer).
No marker extension is introduced. Ball Socket now rejects counts above seven
before accessing its seven-numeral lookup table.

Generation preflight checks the installed source and matching Verified Stud OD
workspace prerequisite. Source continuations additionally require the established
physical orientation and matching retained source identity/contract. The geometry
orientation does not itself constitute physical evidence: observations and an
explicit Verify action remain necessary. New children retain parent observations
in Coarse/history and start with empty Fine/Verification observations. Resuming a
Verified child continues to expose earlier coarse evidence independently.

Configure `BRICKSUITE_CALIBRATION_LDRAW_ROOT` to an installed library to enable
the six `FitCalibrationSource<Family>` tests in CTest. They generate/reopen/register packages
for all six source families and checks direct continuation, candidate mapping,
parent evidence, and resumed Verified history using temporary managed storage.
No user sessions are written. The regular generation test also checks frictionless
boundary continuation. Existing family tests retain historical geometry checks.

Before commit, inspect newly exposed fixtures in BrickSuite/Bambu Studio for bed
placement, candidate identification, and intended orientation. This is wiring
acceptance, not invalidation of prior physical calibration. The single-session
contract remains supported alongside the Phase 3 package envelope.

Calibration collections request nine decimal places from the existing 3MF writer
so translated source vertices survive the unchanged strict reopen comparison.
Other callers retain the writer default. Detached candidates use a compact
three-column layout; this translates pieces without rotating or changing their
geometry, candidate mapping, or physical orientation.

## Phase 3 unified calibration packages

`PackageRequest` contains existing `Request` selections; `planPackage` validates
all selections through generation preflight before expensive work. Selections
must share a complete manufacturing context and identify supported variants and
orientations. Duplicate selections, missing prerequisites/sources, and unsupported
candidate requests are rejected. Packages are bounded to 32 independent sessions.
The plan lists physical fixtures and their member selections for UI review.

The Recommended package promotes the unchanged four-zone perpendicular pilot:
Standard Stud OD, Tube Wall Cell, Post Wall Cell, and Technic Axle Hole Arm Width
v2. It reuses existing candidate geometry, physical family labels, candidate
markers, translations, and zone validation. These four new coarse selections
share `perpendicular-core.3mf`. All other selections, including continuations,
retain their own physical fixtures. For example, adding Standard Bar creates
`02-standard-bar-perpendicular.3mf`; five calibrations require two fixtures.
This conservative grouping does not add a general plate-packing algorithm or
combine mechanisms whose physical grouping has not been established.

### Companion and identities

A unique `calibration-package-<UUID>` folder under the existing Documents artifact
root contains:

- one or more descriptively named 3MF files;
- `package-session.json`, format `BrickSuiteUnifiedCalibrationPackage`, version 1;
- `calibration-NN-session.json` for each member, using the unchanged single-session
  format (including retained parent/history evidence for continuations);
- `publication.json`, version 2, binding the package identity and companion hash.

The envelope records the manufacturing context/fingerprint, fixture identities
and file hashes, each session and companion hash, family/variant, intended
orientation, candidate correction/functional-value mapping, and zone membership.
Each session is embedded as well as provided in its compatible member file.
Combined fixtures additionally retain the existing zone manifest with geometry
hashes, bounds, translations, physical labels, and marker witnesses.

Package, fixture, session, and zone are distinct identity roles. Every live package,
fixture, session, and print artifact receives a fresh identity; deterministic pilot
identities are rebased, including zone/candidate references. The original pilot
geometry stays unchanged. Package membership and intended orientation do not
create physical observations or verification evidence.

### Publication, import, and recovery

`generatePackage` writes into a private `.pending-*` directory. Separate fixtures
use the existing single-family service inside disposable staging and private
managed storage; they cannot register partially generated packages in the user's
library. Combined generation validates the existing zone/session contract and
reopens the written 3MF against generated named meshes. Before publication, all
3MFs, file hashes, session definitions, candidate/variant/orientation mappings,
and complete fixture/zone membership are checked. Only then is the entire folder
renamed to its unique final destination.

`recoverPackage` performs the same complete file/companion checks before managed
registration. Under the existing package-registration lock, it preflights every
existing session for definition conflicts, then imports missing sessions only.
Matching sessions retain later observations, Preferred/Verified state, process
edits, and history. Interruption can leave a detectable registered subset; retry
loads existing identities and registers only missing members. The result reports
registered count and does not claim completion until every member is registered.
This is recoverable registration, not a filesystem-wide atomic database transaction
or a power-loss durability guarantee. Unpublished staging is never recoverable as
a completed package.

**Import Session / Package...** recognizes the new companion and the historical
single-session formats. Keep the complete folder together when moving a package:
package import requires its bound fixtures, member companions, and publication
record. **Recover Package...** accepts either record or companion. Successful
import displays all independent members through the existing manufacturing
workspace's feature selector. It preserves previously managed evidence, rather
than restoring the original package's draft over newer work.

Continuation remains session-centric: select one member and use **Continue
Calibration...**. Its new child may be a normal single-family package, with the
existing parent/child lineage. The service API also accepts continuation requests
in packages; the Custom UI intentionally starts coarse selections. Siblings are
not regenerated or mutated. Verified package members use the unchanged profile
eligibility/merge rules. Package membership grants no additional authority.

### UI and validation

The dialog offers **Single Calibration**, **Recommended Calibration Package**,
and **Custom Calibration Package**. Custom selection shows family, variant,
orientation, and capability-derived availability. A plan summary shows selection
and physical-fixture counts before generation. The existing background/busy model
reports real fixture `1 of N`, companion-writing, verification, publication, and
registration stages, with the several-minute explanation. No synthetic percentage
or unsafe cancellation is added. Failures distinguish unpublished work from a
complete published package needing registration recovery.

`FitCalibrationUnifiedGenerationTest` covers planning rejection, the four-zone
fixture plus a separate Standard Bar fixture, distinct identities, serialization,
legacy session decoding, geometry/hash/label bindings, fault injection before
publication and during registration, repeated recovery, semantic mismatch
rejection, Verified sibling preservation, independent continuation, and normal
profile eligibility. Existing single-family, pilot/zone, capability, library,
profile, and installed-source integration tests remain part of Release CTest.

For runtime acceptance, select the four Recommended families plus perpendicular
Standard Bar in Custom mode. Confirm five calibrations/two fixtures, generate,
open both 3MFs in the slicer at 100% scale, and inspect placement, family labels,
and candidate identification. Reopen `package-session.json`, select one feature,
and record a clearly identified software-test observation only if intended; check
that sibling observations are unchanged. Do not mark software-test evidence
Verified or use it to create a production profile. Actual physical calibration
and slicer visual acceptance remain user checks; packaging tests do not replace
them. No full-package printing is needed to check software wiring.

### Workspace activation and empty-feature state

Creation and resume use `showWorkspace` to populate the manufacturing context,
select the current workspace, and refresh capability/generation state. An empty
workspace disables feature orientation and notes: there is no feature evidence
to edit yet. Process-change signals during hydration or without a feature session
must not mark a session dirty. Otherwise the generation action can silently stop
at `saveSession()` because no feature exists to save. The managed workspace is
already persisted independently of feature sessions.

`FitCalibrationWorkspaceUiTest` drives workspace creation and all three generation
choices through real Qt widgets using temporary managed/settings storage. It
checks the empty-feature control failure, A-to-new-B activation, switching back,
reopening the dialog within the same application, and the five-calibration/two-
fixture Custom plan. It cancels choosers before generating geometry or recording
physical evidence.

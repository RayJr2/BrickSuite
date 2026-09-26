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

## UI and future scope

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

The generation result already accommodates collections of named meshes (the
existing Plain Round Bore Wheel continuation). A later version can add multiple
zone/session members to publication and a versioned companion envelope, while
retaining this single-session import path. Phase 1 does not expose the unified
pilot, add families to the selector, or supply missing continuation generators.
Source-backed family exposure and continuation now use the capability registry
described below. Unsupported contracts fail explicitly rather than substituting
generic geometry.

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
acceptance, not invalidation of prior physical calibration. Phase 3 remains a
multi-family package/zone design and unified UI, with independently identified
sessions and a versioned multi-session envelope; the single-session contract
must remain supported.

Calibration collections request nine decimal places from the existing 3MF writer
so translated source vertices survive the unchanged strict reopen comparison.
Other callers retain the writer default. Detached candidates use a compact
three-column layout; this translates pieces without rotating or changing their
geometry, candidate mapping, or physical orientation.

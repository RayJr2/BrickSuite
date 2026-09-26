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
Ball Socket/Frictionless Pin continuation remains unavailable; unsupported
contracts fail explicitly rather than using generic replacement geometry.

`FitCalibrationGenerationServiceTest` uses temporary artifact/library roots and
publication checkpoints to test paired output, definition agreement, context,
lineage, all currently exposed coarse paths, orientations, destination isolation,
partial writes, reopen rejection, interrupted registration, evidence-preserving
retry, unchanged single-session import, and invalid output destinations. Existing
calibration library/artifact/package tests continue to cover evidence, profiles,
historical formats, and generator geometry independently.

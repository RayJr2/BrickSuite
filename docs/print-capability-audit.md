# Print Capability Audit run persistence

## Part Reference acceptance corpus

Debug builds offer **Test → Print Capability Audit → Part Reference**. This reads
`PartReferenceManifest` and the existing effective local customization service;
there is no copied Part list and no geometry-policy override. Discovery runs in
background and shows positions, unique canonical Parts, duplicate memberships,
and installed-model availability before Start. The bundled definitions currently
contain 2,985 positions in 38 catalogs; local customizations can increase these.

Canonical database Part ID is the deduplication key. Unresolved catalog entries
remain visible under their normalized Part number, with a `no_catalog_part`
result; they are not silently discarded. Empty identities are separately listed
as structurally invalid positions. The first occurrence defines processing order.
Every catalog/section, original entry number, manifest position, and display order
is retained, including multiple memberships of the same canonical Part.

Random Sample exclusions do not filter this corpus. No-Color, Sticker and
nonstandard-ID properties are recorded in `eligibility_notes`; supported models
are still tested through normal validation. Missing installed source is recorded
as `model_unavailable` / `no_ldraw_model`, separately from native geometry failure.
Availability means a readable, nonempty root model resolves through the normal
candidate list; dependency/load failures remain explicit `load_failed` results.

Each run writes the complete `part-reference-plan.json` before Part work. The
version-1 plan contains canonical identities, candidates, memberships and order;
its SHA-256 fingerprint covers that ordered definition and its schema/policy.
The current shared policy is `ordinary-brick-bounded-v2`. This fingerprint does
not certify that installed LDraw files or Verified profiles remain unchanged.
Metadata records application/Qt version, start time, library path, Auto Fit setting,
corpus counts/fingerprint, selected range, elapsed time and completion status.
Run-state schema 2 also records the active global `currentReferenceSequence`.

CSV schema 6 appends `canonical_part_id`, `part_reference_sequence`,
`part_reference_memberships` (a JSON array), `model_state`, and `eligibility_notes`.
`sample_sequence` remains 1..N within the run. Existing native and override result
semantics and export filenames remain unchanged.

On completion or clean Stop, `summary.json` and `run-metadata.json` contain global
and per-catalog counts/rates. Native and practical percentages use model-bearing
Parts in the selected range, excluding no-model Parts. Incomplete ranges are
explicitly partial measurements. Catalogs are overlapping membership views:
**do not sum them to reconstruct the global denominator**. Native coverage/resource
failures remain counted even when an override recovers practical printability.
`failure-review.json` lists source-coverage, resource-limit, no-model and unexpected
failures, with their sequence and diagnostics. These lists do not imply that every
failure needs new geometry engineering.

### Complete run and bounded chunks

1. Choose **Part Reference**, inspect the discovered counts, and choose an output folder.
2. Leave **First unique Part = 1**, **Parts in this run = All remaining** for the full corpus.
3. Start; individual failures do not open modal dialogs. Stop and Close retain existing safe behavior.
4. Review CSV, metadata, run-state, stage timings, summary and failure-review files.

In-place resume is deliberately not implemented. Existing runs do not persist a
complete execution environment/profile snapshot or an append-recovery journal;
adding trustworthy reconciliation of partial quoted CSV records and successful
exports would expand this task beyond corpus measurement. Use saved-plan chunks:

- For planned chunks, choose a first index and count (for example 1/100, 101/100).
- After the first run, use **Load saved Part Reference plan...** with its
  `part-reference-plan.json`, then select the next range. This preserves order and
  fingerprint even if the current manifest changes. Incompatible/damaged plans refuse loading.
- After Stop/interruption, inspect the last complete CSV row and `run-state.json`.
  Start a new chunk at the first uncompleted global reference sequence. Rerun a
  cancelled row or ambiguous active Part; never skip it solely because it was active.
- Preserve both run folders. If a boundary Part was rerun, count its chosen completed
  result once by `(corpusFingerprint, canonical_part_id)` (or unresolved Part number).
  Do not add percentages or count overlapping chunks twice. Do not mix measurements
  across changed LDraw installations, profiles or settings without identifying them.
- Output directories are always new. This is explicit chunking, not automatic resume;
  the tool does not append to, repair, or overwrite earlier evidence.

The historical 100-Part random baseline below remains a separate measurement.

## Native and local-override results

Every model-bearing Part first attempts the existing native preparation and
applicable Verified Fit/export/reopen workflow. A native success is
`native_success`; it does not consult the override store. Only a native
preparation failure triggers a read-only `LocalPrintableOverrideService::load`
using the actual catalog Part ID and freshly loaded authoritative Source.
Native loading, ManufacturingMesh, export, and reopen failures are not hidden
by an override fallback. Stop skips optional override work.

The existing loader checks identity, source/repaired fingerprints, acceptance
versions, and geometry. The audit never imports, repairs, approves, removes, or
updates overrides. A strict or already user-accepted override must complete
nominal PreparedMesh 3MF export and reopen to count as `strict_override_success`
or `user_override_success`. Experimental Auto Fit is never invoked. A candidate
awaiting review is not a stored accepted override and cannot count as success.

CSV schema 6 retains the schema-5 native/override fields:

- `native_result_category`, `native_diagnostic`: the underlying native outcome;
- `local_override_state`: `not_checked`, `none`, `strictly_validated`,
  `user_accepted`, `stale`, or `invalid_unavailable`;
- `local_override_used`, `local_override_stale`, `local_override_diagnostic`;
- `local_override_route`, `local_override_identity`;
- `export_result`, `reopen_result`: `not_attempted`, `success`, or `failed`.

`local_override_used` means selected for export, not necessarily successful.
Only the final success category plus successful reopen is a recovery. Stale or
unavailable overrides retain the native failure. Override export/reopen failures
use `export_failed`/`reopen_failed`, while retaining the native category separately.
`preparation_route` and coverage still describe native preparation; the separate
override route describes its rescue. Prepared vertex/face counts describe the
mesh selected for accepted export. Diagnostic failure exports remain separate;
their presence never counts as success. Final filenames retain the
`<part_number>-<result_category>.3mf` convention.

The final dialog reports native successes, override-assisted recoveries, and
practical successes separately. Native print-service percentage is native
successes / sampled model-bearing Parts. Practical percentage is (native +
override successes) / the same denominator. Native preparation failures remain
counted even when rescued. Catalog-to-printable includes both success kinds over
eligible sampled Parts; model availability retains its separate denominator.

`local_override_load`, `local_override_export`, and `local_override_reopen` join
the durable phase/timing lifecycle below. The active native failure category is
also checkpointed before override work. Model filters, seeded sampling, Stop,
and CSV-before-completed-state ordering are unchanged.

### Replaying a historical sample

Preserve the original run folder. Use its `run-state.json` `orderedParts` array,
not a new random draw against a possibly changed catalog. In PowerShell:

```powershell
$baseline = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'BrickSuite\Print Capability Audits\20260926T075310Z-b3b05671'
$plan = Get-Content -LiteralPath (Join-Path $baseline 'run-state.json') -Raw | ConvertFrom-Json
$plan.seed
$plan.orderedParts.Count
$plan.orderedParts -join "`r`n" | Set-Clipboard
```

In the Debug application's **Test > Print Capability Audit**, set seed
**855063087** while Random Sample is selected, then switch to **Part List** and
paste. Retain the standard-ID exclusion, installed library, and normal Auto Fit
setting. Part List uses all 100 IDs in recorded order and ignores the random
model filter; seed is retained as provenance and does not reorder this list.
Choose an output root for new runs and Start; never replace the old CSV.

The completed baseline above has 100 model-bearing Parts: 11 native successes,
86 source-coverage failures, and 3 resource-limit failures. Compare old `success`
rows with new `native_result_category == native_success`; then separately count
`strict_override_success`/`user_override_success` in final `result_category`.
Check Part IDs, order, model identity, and any library/profile changes before
attributing differences to implementation changes. This is a replay with current
production inputs, not a restoration of old geometry or profile data.

`BatchPrintableOverrideTest` covers strict/user recoveries, stale source and mesh
fingerprints, invalid storage, unaccepted candidates, nominal export/reopen,
reopen failure, store immutability, metrics, and Stop using isolated fixtures.
Its optional five arguments are catalog database, LDraw library, override store,
new output root, and comma-separated Part IDs; this mode only reads the catalog
and existing overrides. Native 3001, rescued 23422, and no-override 6553 can be
checked without creating or approving an override.

## Sample persistence

The audit uses the existing batch print pipeline. Random Sample applies Sticker,
no-Color, optional nonstandard-ID, then optional installed-model exclusions before
shuffling. Model availability means a readable, nonempty installed Part file,
resolved through the Part number or catalog LDraw aliases; it does not certify
geometry. Part List ignores the model exclusion. The checkbox defaults off.

Each unique output run directory contains `run-metadata.json`, `results.csv`,
`run-state.json`, `stage-timings.jsonl`, and `exports/`. State schema version 1 contains:

- `stateSchemaVersion`, `runId`, `sampleMode`, `seed`, `libraryRoot`;
- `excludeNoColor`, `excludeStickerCategory`, `excludeNonstandardIds`,
  `excludeNoModel`, and `partIdEligibilityRule`;
- catalog/exclusion/eligibility totals, including `excludedNoModel`;
- `requestedEligibleCount`, `requestedSampleCount`, `actualSampledCount`, and the full `orderedParts`
  array, whose one-based position is the CSV `sample_sequence`;
- `ldrawCandidates`, recording each selected Part's known model aliases;
- `currentPhase`, `phaseTimingsMs`, `stopRequested`;
- `currentSampleSequence` (zero before processing), `currentPartNumber`,
  `currentPartStatus`, `completedCount`, and `status`.

The complete plan is saved before model work. Before each Part, state records its
sequence and identity with phase `active`. After the result row is written and
synced, the completed count advances and the phase becomes `finished` (including
failed or safely cancelled results). CSV rows are never added for pending Parts.
Stop before loading can leave phase `not_started`; an untouched plan is `pending`.

State and related audit JSON updates share a same-directory temporary-file helper.
It performs Qt flush and native `_commit` on Windows or `fsync` on POSIX, releases
the temporary file's native handle, then atomically replaces the destination
(`MoveFileExW` with replace/write-through on Windows; `rename` on POSIX).
It never removes the old checkpoint first or falls back to direct overwrite.
CSV rows use the same flush/sync.
Persistence failure aborts the run before the next Part. A crash leaves the last
committed state intact. CSV and JSON are separate files: interruption between a
row flush and its state update can leave one additional completed CSV row; inspect
that row alongside the active checkpoint. No later Part can start in that window.
This supports process-crash attribution, not automatic resume or a filesystem
power-loss guarantee.

Windows replacement errors 5 (access denied), 32 (sharing violation), and 33
(lock violation) receive at most six attempts, with 25/50/100/200/400 ms backoffs
(775 ms total sleep) and a 1500 ms elapsed-time retry cutoff. POSIX retries only
`EBUSY`/`EINTR`. Temporary-file open/write/sync failures and other replacement
errors fail immediately. Persistent access denial can mean permissions or a
longer-lived sharing restriction; the diagnostic does not attribute it to AV.
Failures/retries identify operation, temporary/destination paths, native and Qt
errors, attempt and elapsed time. The worker preserves the first exhausted
failure and starts no later Part. A failed run remains incomplete, not a clean
Stop or successful completion. Fault hooks are explicitly supplied by tests only.

For saved-plan continuation, use `firstReferenceSequence + completedCount` after
checking the CSV's contiguous completed prefix against the checkpoint. A row
ahead of that count is ambiguous and must be rerun; never skip it. Keep the old
folder unchanged and publish the continuation to a new run folder.

The 2026-09-27 persistence incident at local sequence 83 / reference sequence 252
(30144) had 83 flushed rows and `completedCount=83`, `currentPartStatus=finished`,
`currentPhase=run_state`. The next `completed` phase checkpoint failed, before its
timing row or any next Part. Continue that saved plan at **253 (2453b)**. The old
combined QSaveFile error and application log did not identify open/write/sync vs
commit or the native error/handle owner; its exact filesystem cause is unproven.
No temporary remained, and the destination checkpoint remained valid. Existing
UI readers close on scope exit and do not poll during their own active run;
worker and Stop writes share a mutex. A Windows reader without delete-sharing
can still prevent atomic replacement, which the bounded retry now tolerates.
The saved corpus accounts for 2985 built-in plus five user memberships, 2990
unique Parts, and zero duplicate memberships; it is not a 2985-Part corpus.

`PartReferenceAuditTest` injects replacement failures (no antivirus/timing race),
checks old-checkpoint preservation, bounded recovery/exhaustion, next-Part gating,
and a CSV-ahead continuation that reruns only the ambiguous and remaining Parts.

Normal completion sets `status` to `completed`. Safe Stop sets `stopped` after
preserving the last/current checkpoint. An interrupted run remains `running`.
The dialog remembers the output root in `printAudit/outputRoot` and displays
incomplete state from that folder and its immediate run subdirectories. Selecting
an individual run folder is also supported. Completed runs are omitted; stopped runs remain identifiable. Stop is persisted as `stopping` immediately, without waiting for geometry to return. A mutex serializes this UI request with worker checkpoints, so neither write can erase the other. The worker alone finalizes results and marks `stopped`.

ManufacturingMeshServiceTest uses synthetic model files and the optional
`beforePart` checkpoint observer to throw a simulated interruption before model
loading. It verifies persisted order, active identity, completed rows/counts,
model eligibility and refill, aliases, and distinct Stop/completion states.

## Bounded work and dialog lifecycle

The shared interactive and audit preparation policy permits at most eight
sequential Boolean operations. Larger workloads first use the existing exact
planar local composer; if it cannot establish a valid result, preparation returns
a resource-limit failure before MCUT. The ManufacturingMesh fallback uses the
same eight-operation ceiling; larger supported composed meshes retain their
existing localized-correction route. Source coverage, strict mesh validation, and dimensional
fidelity checks remain mandatory. The seed-8125 stall was actually sequence 21
(2456), not sequence 20 (23422): the old UI displayed only the last completed row.
2456 entered sequential Boolean composition and its isolated probe consumed
roughly 194 GB of private committed memory. The shared policy routes it through
validated local composition.

Catalog preparation MCUT calls now run in `BrickSuiteMeshBooleanWorker`, shared
by interactive preparation, the audit, and ManufacturingMesh. Each operation has a 30-second
parent-enforced deadline, a 512 MiB child memory limit, at most 50,000 input
faces / 150,000 input vertices, and at most 100,000 output faces / 300,000
output vertices. Worker startup failure, abnormal exit, timeout, missing or
malformed output fail closed. Returned meshes are independently validated in
the parent. The existing Local Printable Override union retains its separate,
stricter 10-second / 2,000-input-face worker; it does not spawn nested workers.
Ship the Boolean worker beside the application executable (inside `MacOS` for
an application bundle). A missing helper produces a diagnostic, never an
in-process fallback. Trusted calibration generators retain their established
backend execution path. Non-MCUT preparation work retains its existing bounds.

New run metadata records `booleanExecutionPolicy`. Before each sequential
Boolean, the durable state records `currentBooleanOperation` (one-based) as
well as `booleanOperations` (planned total). Both reset for the next Part.
The `ordinary-brick-bounded-v2` routing decision and corpus identity are
unchanged, so existing saved plans remain valid for a new continuation chunk.

The 3021 crash investigation found 144 durable rows, active reference sequence
145, and phase `preparing/boolean_composition`. Windows identified a Debug
MCUT breakpoint exception (`0x80000003`), not Auto Fit or export. The exact DLL
offset resolves to `get_connected_component_data_impl_detail`, MCUT
`source/frontend.cpp:3314`, asserting `cc_uptr->cdt_index_cache.empty()` while
`cdt_index_cache_initialized` is false. The first face-triangulation size query
returns error `-1` after leaving a partial cache. BrickSuite previously ignored
that status and issued the copy query, triggering the Debug assertion (Release
continued to an invalid mesh). All MCUT extraction statuses are now checked;
the service immediately returns failure and releases the context without a
second query. Native assertions elsewhere remain contained by the worker.

The historical checkpoint recorded eight planned operations, not which one
faulted, and no retained minidump was available. Bounded reproduction isolated
operation five: 608 accumulated triangles plus a 96-triangle stud operand,
identical serialized inputs in Debug and Release. Both now return the same
triangulation-query failure without emitting Ready or crashing either process.
Installed `parts/3021.dat` has 508 source/stitched triangles, nine groups and
nine operands, ten source boundary loops (160 edges), and 988 closed-operand
faces. Recognition finds six stud and two receiving-clutch features. No
ManufacturingMesh correction or Auto Fit selection is reached. The planar
composer can represent this source (eight regions, 1,084 faces, 544 vertices),
but the unchanged routing policy selects the eight-operation sequential route.
Source analysis is bounded by the existing 20-million candidate-check limit;
the 508-triangle source has at most 128,778 distinct triangle pairs. The
operation-count ceiling could not detect this data-dependent triangulation
failure or contain an assertion inside an individual call.

To continue an interrupted Part Reference audit, load its saved
`part-reference-plan.json`, set **First unique Part** to the active global
reference sequence (145 for this crash), and choose a short **Parts in this
run** count first. A new run folder preserves the original evidence and full
corpus fingerprint. Chunk CSV `sample_sequence` starts at one;
`part_reference_sequence` retains the global position. Never append to or edit
the crashed run's CSV, JSON, or exports. An unavailable or rejected Part still
gets its own completed failure row in the new chunk.

Diagnostic export is optional. Meshes above 50,000 faces or 150,000 vertices are
rejected before orientation/copying or lib3mf generation. Stop skips optional
export/reopen at safe boundaries. Existing bounded diagnostic reconstruction
keeps its elapsed/output limits and checks cancellation between attempts. None
of these decisions promote a failed preparation to success. These are workload
bounds; the separate MCUT subprocess deadline described above applies only to
MCUT operations.

Live phases include loading, preparation subphases, source-coverage classification,
failure classification, diagnostic candidate/extraction, diagnostic export,
3MF geometry generation, 3MF write, reopen, persistence, and completed. Each
phase is checkpointed before the UI is notified. `prepare_ms` is measured around
the entire preparation call even on failures. Per-Part stage timings are synced
to `stage-timings.jsonl`; absent stages were not attempted.

Close and window X record a pending-close request, request Stop, and show
Stopping / closing. The dialog remains alive until QFutureWatcher reports that
the worker has finished, then closes automatically. Late queued UI updates are
ignored. Parent/application teardown cancels and waits for safe worker completion;
it never destroys callback targets while the worker is active. No geometry
thread is force-terminated.

Release validation includes `PrintCapabilityAuditDialog` (offscreen widgets with
a semaphore-controlled worker) and `ManufacturingMeshService` (phase durability,
Stop, diagnostic limits, CSV/state ordering). The latter also supports
`--audit-trace <library> <output> <comma-separated Parts or plan.json> [prefix count]`
for focused investigation. Plan replay reads optional `ldrawCandidates`; for
older plans use recorded CSV model identities when reconstructing aliases.

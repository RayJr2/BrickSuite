# Print Capability Audit run persistence

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

CSV schema 5 preserves the existing fields and adds:

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

State updates use QSaveFile atomic replacement, with Qt flush and native `_commit`
on Windows or `fsync` on POSIX before commit. CSV rows use the same flush/sync.
Persistence failure aborts the run before the next Part. A crash leaves the last
committed state intact. CSV and JSON are separate files: interruption between a
row flush and its state update can leave one additional completed CSV row; inspect
that row alongside the active checkpoint. No later Part can start in that window.
This supports process-crash attribution, not automatic resume or a filesystem
power-loss guarantee.

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

The audit-specific `audit-bounded-v1` preparation profile permits at most eight
sequential Boolean operations. Larger workloads first use the existing exact
planar local composer; if it cannot establish a valid result, preparation returns
a resource-limit failure before MCUT. Normal interactive printing retains its
existing profile. Source coverage, strict mesh validation, and dimensional
fidelity checks remain mandatory. The seed-8125 stall was actually sequence 21
(2456), not sequence 20 (23422): the old UI displayed only the last completed row.
2456 entered sequential Boolean composition and its isolated probe consumed
roughly 194 GB of private committed memory. The audit profile routes it through
validated local composition.

Diagnostic export is optional. Meshes above 50,000 faces or 150,000 vertices are
rejected before orientation/copying or lib3mf generation. Stop skips optional
export/reopen at safe boundaries. Existing bounded diagnostic reconstruction
keeps its elapsed/output limits and checks cancellation between attempts. None
of these decisions promote a failed preparation to success. These are workload
bounds, not a promise to interrupt a native call at a wall-clock deadline.

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

# Print Capability Audit run persistence

The audit uses the existing batch print pipeline. Random Sample applies Sticker,
no-Color, optional nonstandard-ID, then optional installed-model exclusions before
shuffling. Model availability means a readable, nonempty installed Part file,
resolved through the Part number or catalog LDraw aliases; it does not certify
geometry. Part List ignores the model exclusion. The checkbox defaults off.

Each unique output run directory contains `run-metadata.json`, `results.csv`,
`run-state.json`, and `exports/`. State schema version 1 contains:

- `stateSchemaVersion`, `runId`, `sampleMode`, `seed`, `libraryRoot`;
- `excludeNoColor`, `excludeStickerCategory`, `excludeNonstandardIds`,
  `excludeNoModel`, and `partIdEligibilityRule`;
- catalog/exclusion/eligibility totals, including `excludedNoModel`;
- `requestedEligibleCount`, `requestedSampleCount`, `actualSampledCount`, and the full `orderedParts`
  array, whose one-based position is the CSV `sample_sequence`;
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
an individual run folder is also supported. Clean terminal states are omitted.

ManufacturingMeshServiceTest uses synthetic model files and the optional
`beforePart` checkpoint observer to throw a simulated interruption before model
loading. It verifies persisted order, active identity, completed rows/counts,
model eligibility and refill, aliases, and distinct Stop/completion states.

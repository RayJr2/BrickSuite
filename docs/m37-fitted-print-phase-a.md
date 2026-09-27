# M37 fitted-print coverage: Phase A

## Printing intent and evidence

Print Capability Audit offers **Fit Profile: Automatic / No explicit selection**
or an explicitly selected managed compatible Verified profile. **Print Orientation**
offers Nominal and X/Y/Z +90-degree rotations in print coordinates. Automatic keeps
the existing ambiguity rule. Explicit selection chooses manufacturing intent; it
never substitutes for family, role, orientation, prerequisite or ownership evidence.
The normal viewer already makes this choice available in ManufacturingMesh export,
with a nominal-not-applicable profile message for PreparedMesh. It is unchanged.

Profiles are loaded once per run. Metadata, summary and run state retain complete
profile/process snapshots, format version, selection identity/name, orientation and
a SHA-256 context fingerprint. Invalid selected profiles stop before output or
geometry. Saved plans bind this context independently of the unchanged corpus
fingerprint. Changing profile evidence (even under the same identity), selection,
Auto Fit setting or orientation refuses continuation. **Refresh current Part
Reference** explicitly creates a new comparison context. Historical plans without
context allow only the legacy Automatic/nominal default; old profile contents
cannot retrospectively be certified. Historical files are never rewritten.

## Measurement contract

CSV/run metadata schema **7** retains existing columns and adds:

- `geometry_result`, `nominal_prepared_ready`;
- `fit_status`, `fit_diagnostic`, `fitted_export_succeeded`;
- source and total recognized/applicable Verified feature counts;
- corrected, nonzero and Verified-zero feature counts, and `partial_fit_coverage`;
- selected profile, print orientation and context fingerprint;
- `source_fit_features`: identities, families, roles, contracts, recipes, source and
  transformed axes, eligibility and reference ancestry.

The source inspection checkpoint runs existing source-only recognizers before
preparation. It performs no operand construction/closure, MCUT, repair or PreparedMesh
work. This is a **conservative source opportunity count**: receiving-tube/body
contracts currently require preparation context. Those additional interfaces enter
the total count only after preparation establishes them; they are not retrospectively
called pre-preparation certification. Equivalent source/operand representations are
counted once using identity or the existing family/role/contract/frame equivalence.
Reporting equivalence grants no geometric ownership.

`ManufacturingMesh.appliedFitFeatures` records actual successful application sites
and is returned only with validated geometry. Nominal/unverified hinge features are
absent. Availability does not populate this ledger. A feature with both zero and
nonzero corrections counts once as nonzero; a fixed axle-hole tip-to-tip correction
also contributes to this distinction.

Fit statuses: `not_inspected`, `auto_fit_disabled`,
`no_supported_features_recognized`, `recognized_no_applicable_verified_evidence`,
`recognized_multiple_profiles`, `selected_profile_no_applicable_evidence`,
`applicable_evidence_not_applied`, `verified_zero_applied`,
`verified_nonzero_applied`, `partial_verified_fit`,
`nominal_no_verified_application`, `fitted_manufacturing_failed`,
`local_override_nominal`. Applied statuses describe validated ManufacturingMesh;
only `fitted_export_succeeded=1` also certifies final export/reopen. Partial means
some recognized interfaces remain nominal. Geometry/export failures are independent.

Every mode writes `summary.json`. Its fitted-print funnel gives explicit numerator,
denominator and percentage (null for zero denominator): model-bearing/completed,
native final exports/model-bearing, source opportunities/model-bearing, applicable
source evidence/source opportunities, fitted final exports/all applicable
opportunities. The last denominator includes disclosed preparation-dependent
opportunities. Counts include nominal Ready, nonzero, Verified-zero, partial,
nominal fallback and ManufacturingMesh failure. Partial overlaps zero/nonzero;
it is not a disjoint additional success bucket. Incomplete runs describe completed
rows; the Part Reference summary also retains selected-range totals.

Files are written/reopened under `exports/diagnostic/` and published into
`exports/success/` only after successful reopen. Publication failure remains a
failure and leaves the file diagnostic. Filenames retain
`<part>-<result_category>.3mf`; CSV provides nominal/zero/nonzero/partial metadata.
Old run layouts are unchanged. Nominal overrides, the eight-operation ceiling,
isolated MCUT execution and durable checkpoint replacement are unchanged.

## Installed-source measurements, 2026-09-27

Original full run: `20260927T145620Z-ae4747dc`, unchanged. Deterministic lists retain
its CSV order: 140 multiple-profile rows; 28 no-profile rows excluding the genuine
role/evidence cases 2881, 251, 2923 and 3937. No new full-corpus scan was performed.
New local outputs and lists are under ignored `build/m37-fit-phase-a/`.

| Explicit H2D, nominal orientation | Observed result |
| --- | --- |
| 15573, 65509, 98138 | Nonzero fitted export/reopen |
| 30374 | Verified-zero export/reopen |
| 3705 | Nominal; orientation evidence does not match |
| 3937 | Nominal; female barrels receive no male-pin correction |
| 32278 | Nominal Ready; 15 operations exceed eight; no Boolean started |
| 3021 | Bounded preparation failure; six source studs measured first |

**140 Parts:** all nominal Ready and applicable; **131 nonzero fitted successes**
(120 without recognized uncorrected interfaces, **11 partial**), zero exclusively
Verified-zero successes, **eight ManufacturingMesh failures**, one nominal fallback
(4275b). 132 final exports reopened. Elapsed 256.470 seconds.

Partial: 3666, 3460, 3008, 22885, 32952, 7729, 2452, 3938, 6134, 33122, 60476.
Failures: 6141 (existing route applies no correction; sparse service diagnostic),
3710/3010 (10 operations exceed eight), 41855/3045/3675 (strict source-surface
validation), 4276b (no supported closed semantic body), 30293 (no MCUT fragments).

**28 orientation controls:** nominal gives 28 nominal exports, zero applicable.
Explicit X +90: **18396, 30027b, 2596**. Explicit Y +90: the other 25 Parts.
These choices follow recorded source axes for this proof; production never searches
orientations automatically. All 28 become applicable; **21 fitted successes**:
**18 nonzero**, **three Verified-zero** (32002, 3673, 4274), no partial successes.
Failures: 18396 (existing route applies no correction),
15462/24316/32209/55013/87083 (degenerate MCUT result), 6587 (no fragments).
All seven retain nominal readiness. **3705 succeeds at Y +90** with existing evidence.
Nominal run: 40.426 seconds; alternate groups: 7.194 + 38.554 seconds.
Timings depend on machine/load; these runs preceded the final publication/UI polish,
which did not change geometry, fit selection, application counts or safety policy.

These measurements support freezing the bounded policy after runtime acceptance.
A separate narrow follow-up could improve route diagnostics and examine applicable
features left nominal (notably 4275b and partial source-surface routes). This phase
measures those gaps without redesigning routes or assigning repaired ownership.

## Ray's runtime acceptance

1. In Debug, open **Test > Print Capability Audit**, choose **Part List**, load
   `build/m37-fit-phase-a/acceptance-8.txt`, select the H2D / PETG / 0.40 mm / LEGO Fit
   profile and Nominal. Start a new run. Check the table above and metadata/context,
   CSV fit counts, and success/diagnostic folders.
2. Run 3705 alone at **Y +90 degrees**. Expect a nonzero fitted export.
3. Load `ambiguous-140.txt` from the same folder with H2D and Nominal. Compare with
   the measured counts above, including partial and bounded failures.
4. Run `orientation-x.txt` at X +90 and `orientation-y.txt` at Y +90, both with H2D.
   `orientation-28.txt` at Nominal is the negative control. Keep run folders separate.
   Do not rerun the full 2,990-Part corpus yet.

The opt-in `PartReferenceAuditTest --fit-acceptance` harness accepts a saved corpus,
installed library, read-only managed profile library/identity, list, output root and
nominal/x/y orientation. It refuses lists over 140 Parts or identities outside the
saved corpus. It does not mutate catalog, calibration, overrides or historical output.

## Validation

- Qt 6.10.3 / MinGW 13.1 Release configure, application and all 115 configured test
  executable targets built successfully.
- Debug application (including audit UI) built successfully; no Debug tests built.
- Focused ManufacturingMesh, Part Reference audit, override, composition-routing
  and 3021 tests passed. Full configured CTest: **123/123**, 172.52 seconds.
- The final eight-Part run repeated all expected outcomes after publication polish.
  Assertions checked counts, source recognition before failure, context equality,
  success/diagnostic paths, and opportunity-denominator funnel values.
- Schema **35**, Protocol **1.5**, geometry policy and recognizer contracts unchanged.
- GUI workflow acceptance remains Ray's separate step. No commit or push.

## Ledger-authoritative completion reporting

Service, audit and native viewer completion messages use the shared
`ManufacturingFitSummary`. Selected profile identity does not establish an applied
correction. Empty application ledgers report nominal output; Verified-zero entries
explicitly report zero dimensional adjustment; nonzero and partial application
report applied/recognized counts. Source/operand reporting equivalence and CSV
schema 7 classification are unchanged. The viewer reports export success only;
the audit additionally reports its existing successful reopen check. Failed fitted
manufacturing explicitly retains nominal PreparedMesh availability.

The bounded `PartReferenceAuditTest --fit-reporting` acceptance harness uses the
same arguments as `--fit-acceptance` with exactly four controls: 4275b, 30374,
98138 and 3666. It checks nominal/no application, genuine Verified-zero, nonzero
and partial reporting respectively, including successful export/reopen.

Reporting correction validation: Qt 6.10.3 MinGW Release application and all 115
configured test targets built; four focused tests passed; full CTest passed
123/123 (161.25 seconds). The four installed-source controls exported/reopened
with counts 0/3, 1/1 Verified-zero, 1/1 nonzero and 6/11 partial respectively.
Their exported vertex/triangle arrays exactly matched the prior Phase A outputs.
Schema 35 and Protocol 1.5 remain unchanged. Ray's final display spot-check is
limited to those four Parts; no physical print or full-corpus rerun is required.

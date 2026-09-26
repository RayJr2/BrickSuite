# M37 source-constrained residual repair proof — 2026-09-26

**Recommendation: option 3 for v0.4.0 — do not add this repair fallback.** The
constrained stage can make justified seam joins, but it cannot meet the supplied
23422 authority/fidelity requirements. No tested repair candidate is accepted.
This is a release-scoping decision, not a claim that every future repair method
is impossible. Existing preparation and the known-good 3001 route stay unchanged.

The baseline was clean. Work is isolated to `experiments/source-constrained-repair`,
an optional provenance diagnostic in the preceding Geogram experiment, and this
report. No production source, root CMake, schema, catalog, profile or user data
was changed. No commit or push.

## What was actually tested

Started from the saved **seven-component / 90-boundary-edge** Studio 23422
Geogram result. No broad repair restart, new hole-filling pass, smoothing or
largest-component selection was applied to that residual. The unchanged
Geogram pass was replayed only to trace which faces were generated; its vertex
and face arrays matched the saved residual **exactly**. Every corpus replay was
also checked this way before any lineage was attached.

Exact matched files remained read-only:

- `D:\Lego Studio\Exported_obj\23422.obj`, SHA-256
  `6d299861076f69d6581e1a32afee849386c1e7e193660cb539d654be4a1163ed`.
- `D:\Bambu Studio\Exported_3mf\23422.3mf`, SHA-256
  `d7531ae2cb871a8850cc3da606419e3dff459428fdf6a049840c8d05d72aaad7`.

Both stay at **0.4 mm per Studio coordinate unit**. There is no fitted scale or
2.5× ambiguity. Installed `D:\LDraw\parts\23422.dat` and its expanded primitive/
subpart/reference ancestry remain the authoritative comparison.

## Boundary classification and authored seam method

All 90 primary boundary edges form **ten degree-two closed paths**, with lengths
2.092735–4.185482 mm. They are near the body's outer rims at Z ≈ ±4 mm, not across
the center bore. Some paths coincide spatially or contain tiny slivers; graph
closure alone does not prove an intended missing face.

Every edge is linked to its incident residual face, traced original input face,
supporting source triangles, nearest authoritative triangles and full available
LDraw file/line/reference ancestry. Nearest triangles are explicitly **nearby
evidence**, not owners. Nearby reversed/coincident edges are recorded even when
they fail the seam proof.

| Loop | Edges | Rim / side | Max boundary-probe distance to LDraw mm | Classification |
|---|---:|---|---:|---|
| 0 | 10 | Bottom, left | 0.068152 | Ambiguous/reject |
| 1 | 5 | Top, left | 0.068523 | Ambiguous/reject |
| 2 | 6 | Top, left | 0.068522 | Ambiguous/reject |
| 3 | 10 | Bottom, right | 0.068152 | Ambiguous/reject |
| 4 | 14 | Top, right | 0.068103 | Ambiguous/reject |
| 5 | 6 | Top, right | 0.068522 | Ambiguous/reject |
| 6 | 5 | Top, right | 0.068523 | Ambiguous/reject |
| 7 | 14 | Top, left | 0.068103 | Ambiguous/reject |
| 8 | 10 | Bottom, left | 0.068152 | Ambiguous/reject |
| 9 | 10 | Bottom, right | 0.068152 | Ambiguous/reject |

**None has an authoritatively supported complete boundary.** Original Studio
face IDs are recoverable, but the nearby installed LDraw facets differ. The
observed body mismatch is not numerical equality and exceeds the unchanged
0.0004 mm source-support seam tolerance. Assigning the nearest primitive or
welding based on distance would manufacture ownership.

The implemented seam proof requires two non-generated, traced faces with unique
authoritative support; distinct source triangles sharing the same exact authored
edge with exactly two incident source faces; reversed coincident residual
intervals on that edge; unique pairing; and no protected opening. Numerical
endpoint equality is limited to 1e-9 mm. Joining rewrites vertex indices, preserves
face lineage and creates no spanning patch. New collapsed/near-degenerate faces
or nonmanifold edges cause rollback.

This rule is deliberately incomplete: an unmatched segmentation/intersection
interval stays rejected rather than receiving a speculative split. It succeeds
on a synthetic authored tetrahedron soup and on eight real 80910 edge pairs.
It authorizes **zero** primary 23422 seams.

The primary loop inventory and ancestry evidence are in
`build/source-constrained-proof/final/23422/residual-analysis/boundaries.json`.
The companion `23422-boundary-rims.png` shows the source/residual outlines and
the rejected paths. A loop's location does not itself certify that it is or is
not an intentional opening; primary loops retain `unproven; no capping permitted`.

## Generated patches and provenance

No new patch is created. A patch would require all five independent proofs:
complete authoritative boundary; exclusion of intentional openings; unambiguous
material side; strict local source/reference fidelity; and preserved wall/feature
topology. None of these residuals supplied a complete certificate. There is no
blind fill operation followed by an optimistic acceptance test.

The preceding Geogram pass did create geometry. An input-face-plus-one attribute
now traces faces through cleanup, subdivision and reordering; new hole faces have
zero. A focused missing-tetrahedron-face test verifies one generated face and
three retained input faces. 6553's second-pass lineage is composed back through
the first pass. Output faces explicitly say **`generated repair geometry`** or
`surviving input geometry`.

This distinction corrects an important limitation of the previous overlap-only
estimate: a generated face can happen to coincide with authoritative geometry
without becoming authored geometry. Generated faces receive **no retained fit
ownership**. Surviving source attribution additionally requires matching support
for both the traced original face and its output fragment.

| Candidate | Inherited generated faces | Generated area mm² | Retained authoritative area mm² / % |
|---|---:|---:|---:|
| 23422 Studio | 150 | 19.546599 | 1.388768 / 0.232762% |
| 6553 | 74 | 23.130554 | 752.654019 / 97.018431% |
| 44375a | 4,401 | 233.382746 | 4,468.171195 / 95.036051% |
| 80910 | 778 | 176.617628 | 1,739.731412 / 90.783640% |
| 3001 outer-only control | 0 | 0 | 3,642.036572 / 100% |

New generated count/area is **0 / 0 mm² for every constrained candidate**.
Studio's low authoritative percentage reflects the already different original
geometry/tessellation; it does not mean the repair invented 99.8% of the object.
Unmatched surviving faces are retained as unresolved, not relabeled generated.

## Quantitative 23422 comparison

The constrained primary mesh is unchanged from the saved residual because no
operation met the proof conditions. That is an explicit rejected result, not a
claim that merely running the stage repaired it.

| Measure | Original Studio OBJ | Saved Geogram | Constrained result | Bambu reference |
|---|---:|---:|---:|---:|
| Vertices | 932 | 1,040 | 1,040 | 931 |
| Faces | 1,130 | 2,022 | 2,022 | 1,870 |
| Components | 11 | 7 | 7 | 1 |
| Boundary edges | 732 | 90 | 90 | 0 |
| Nonmanifold edges | 0 | 0 | 0 | 0 |
| Near-degenerate faces, area < 1e-12 mm² | 0 | 18 | 18 | 0 |
| Exact contact/intersection pairs | 1,856 | 942 | 942 | 1,407 |
| Point / segment / area pairs | 1,204 / 652 / 0 | 836 / 106 / 0 | 836 / 106 / 0 | 1,202 / 204 / 1 |
| Width × depth × height mm | 25.107264 × 8 × 8 | Same | Same | 25.108000 × 7.999200 × 7.999200 |
| Bore diameter near Z=2.5 mm | ≈4.800 | ≈4.800 | ≈4.800 | ≈4.799–4.801 |
| Body OD | ≈8.000 | ≈8.000 | ≈8.000 | ≈8.000 |
| New constrained patches | n/a | n/a | 0 | Unknown Bambu creation history |

Native topology counts use the original vertex indexing. The CGAL inspector
splits four original-OBJ vertices to form its measurement mesh; this does not
change the reported native 932-vertex input. Contact totals use the same exact
classifier as the preceding proof. They are **not penetration depths**, and the
Bambu counts do not negate its known-good visual/edge-topology benchmark. They
do mean that strict zero-contact certification is not established by a slicer's
successful repair label alone.

For primary original/residual/constrained bounds, X is ±12.553632 mm, Y is
approximately ±4.0000002 and Z approximately ±3.9999998. No coordinate changes
are made by the constrained stage; bounds and triangle geometry are identical.

### Deviations

All distances are mm; sampled P95/RMS use the same fixed-seed 20,000 area samples
and barycentric grid as before. Max below is the independent global Hausdorff
estimate with fixed **0.001 mm algorithm error**, not a relaxed tolerance. A
reported approximately ≤0.001 is limited by that measurement precision.

| Direction | Max estimate | P95 | RMS |
|---|---:|---:|---:|
| Constrained ↔ saved Geogram | **0 exactly** | **0** | **0** |
| Constrained → authoritative LDraw | 0.392319 | 0.374201 | 0.164852 |
| LDraw → constrained | 1.596518 | 0.387452 | 0.206157 |
| Constrained → original Studio | 0.269372 | 0.000024 | 0.025051 |
| Original Studio → constrained | 1.596518 | 0.000023 | 0.106385 |
| Constrained → Bambu | 0.540400 | 0.000480 | 0.038014 |
| Bambu → constrained | approximately ≤0.001 | 0.000432 | 0.000262 |

For context, the unchanged prior original→Bambu max/P95/RMS is
1.596139 / 0.000536 / 0.110719; Bambu→original is
0.269361 / 0.000480 / 0.022257. Original→LDraw is
0.392319 / 0.373576 / 0.159524 and reverse is
0.400166 / 0.381754 / 0.175529. The original source discrepancy is already present
before repair.

The worst constrained→Bambu difference remains near **(0,2.4,0) mm**, around the
bore/groove, with nearest reference near (0,1.859599,0). The worst LDraw→candidate
sample is near (4,0,0), the arm/body interface. Deleted internal sheets contribute
to reverse distances; they cannot automatically be counted as exterior damage,
nor excused without an explicit material classification.

### Feature checks and the authority mismatch

- Five axial bore probes remain open in original, residual, constrained and
  Bambu meshes. Both arms occupy the same principal face-adjacency component.
- At Z=2.5 mm, constrained bore radius is 2.399945–2.400000 mm and radial wall
  1.599961–1.600000 mm. Bambu wall samples span 1.599200–1.600681 mm. These are
  section probes, **not** a certified global minimum-wall analysis.
- Both curved arms, body diameter and major dimensions are unchanged. No new
  intersection or near-degenerate geometry was introduced, but the existing
  942 pairs and 18 near-degenerate faces remain.
- The groove/recess is still visibly present, but the 0.5404 mm localized
  reference discrepancy prevents certifying its complete shape. Other intentional
  openings are not globally certified merely because five bore rays pass.

The installed LDraw has a **5.60 mm vertex-diameter polygonal bore**, with
2.752131–2.8 mm sampled radial extent, while the matched Studio original already
has the **4.80 mm bore**. Its outer-body tessellation is also finer. Preserving
the supplied Studio geometry and simultaneously claiming strict identity to
this installed authoritative surface is not possible within the existing
source-support tolerance. This is not a scaling error or evidence that Bambu
changed the bore. The proof does not change authority, guess an alias, or attach
an installed primitive's fit semantics to the Studio file.

## Secondary corpus and ownership consequences

| Part | Constrained V / F | Components / boundaries | Exact pairs | Result |
|---|---:|---:|---:|---|
| 6553 | 511 / 1,030 | 1 / 0 | 0 | Topology passes; source/material/feature certification incomplete |
| 44375a | 4,778 / 9,350 | 2 / 204 | 908 | No justified join; fails topology and fidelity |
| 80910 | 1,622 / 3,152 | 17 / 96 | 527 | Eight seams joined; still fails |
| 3001 | 484 / 964 | 1 / 0 | 0 | Unchanged geometric control; normal route already succeeds |

All have zero nonmanifold edges. 44375a retains 96 near-degenerate faces; the
other secondary candidates have zero. 44375a has three boundary paths; 80910
starts with 24. The eight 80910 joins remove 16 boundary edges and eight redundant
vertices **without changing any triangle coordinates or lineage**. Exact pair
count falls from 669 to 527 (439 point / 88 segment after joining). No spanning
patches are made. All partially proven loops remain classified ambiguous as
whole loops; valid individual intervals are recorded separately.

6553 is the strongest topology candidate. Its output→source sampled max is
0.000536 mm (global estimator 0.001 at its fixed error), P95 0.000010 and RMS
0.000013. Reverse global max is **1.176427 mm**, P95 0.174022, RMS 0.105544.
The source extremum near (0,0,-8) lies on removed source material requiring an
exterior/internal-sheet decision. Its **74 traced generated faces** remain
unowned despite strong geometric overlap. One closed component and zero exact
pairs are therefore insufficient for the complete requested acceptance.

44375a max deviations to/from source are 2.353905 / 0.396090 mm; 80910 remains
1.414214 / 1.843898 mm. 3001 output→source is numerically zero, but raw reverse
distance reaches 1.6 mm on a source sheet below a stud; removing an internal sheet
is not automatically an exterior defect. These distance tests deliberately use
the same criteria without per-Part tolerance tuning. No candidate receives an
unproven internal-sheet exemption.

The existing normal 3001 audit recognizes `StandardStud` and
`StudReceivingClutch`; that established path remains available. This experiment
does not re-run functional feature certification or a managed Verified Fit
Profile on repaired geometry. **Retained triangle ancestry alone does not prove
a complete supported interface.** Any interface materially dependent on generated
faces is unsafe for compensated ManufacturingMesh. For Studio 23422 no
authoritative bore/fit identity is established; for 6553 a feature-by-feature
generated-region exclusion would still be required. All experimental
`fit_ownership` values are null. Nominal-only becomes possible only after the
remaining geometry checks pass; none is promoted here.

## Isolated-worker behavior, runtime and validation

Each Geogram and geometry-analysis stage runs in a separate child process.
The parent persists Part, phase, command, paths and limits before launch, then
PID/current status and final exit status/timeout/resource reason. State writes
flush/fsync and atomically replace the JSON file. Logs are flushed at exit.
The standalone proof never loads Geogram into BrickSuite.

Bounds: one requested thread, 120 seconds wall and CPU per engine (180 seconds
for Python residual analysis), 2 GiB aggregate child RSS/private memory, 64 MiB
output directory. A 20 ms monitor kills only the spawned tree on a limit breach.
Short overshoot is possible; these are practical supervisory limits, not claimed
OS hard quotas. A production worker would need OS-level lifetime/quota handling.

The actual prior **44375a fatal radial-sort case was reproduced in a bounded
child**. Exit status was **1**; the parent recorded the Part and diagnostic path,
then successfully ran the tetrahedron job with exit status **0**. No BrickSuite
crash was induced. Synthetic tests also exercise nonzero exit, timeout, CPU,
memory and output limits.

Measured representative runs, including Python startup, provenance and sampled
fidelity work (20 ms sampled RSS can miss short peaks):

| Stage | Wall seconds | Peak RSS MiB |
|---|---:|---:|
| 23422 unchanged provenance replay | about 0.11 | about 16 |
| 23422 classify/constrain + sampled measurements | 10.016 | 130.48 |
| 23422 independent exact check | about 0.19 | about 14 |
| 6553 residual analysis | 3.469 | 120.20 |
| 44375a residual analysis | 18.391 | 150.31 |
| 80910 residual analysis | 9.078 | 174.95 |
| 3001 residual analysis | 3.578 | 120.39 |

Global Hausdorff estimates run separately. Per-run exact measurements are in
`build/source-constrained-proof/final/run-summary.json` and each worker-state
file. The constrained primary contributes no geometry change or new repair area;
timing is dominated by proving/rejecting operations and measuring the result.

Validation:

- Updated Geogram proof builds in Release; tetrahedron CTest passes.
- Twelve new synthetic proof/supervisor tests pass.
- Four existing Geogram and three measurement tests pass.
- Qt 6.10.3 MinGW Release BrickSuite build passes (`ninja: no work to do`).
- Full configured Release CTest **108/108 passes**, 57.20 seconds.
- `git diff --check` and separate checks on new untracked files pass.
- Schema **35**, protocol **1.5**, unchanged.
- No production preparation change, unrelated changes, Debug test compilation,
  commit or push. Original benchmark hashes remain unchanged.

No physical print acceptance, macOS/Linux runtime build or global feature/thickness
certificate is claimed. Reproduction commands are in
`experiments/source-constrained-repair/README.md`.

## Production feasibility decision

Do not enable a v0.4.0 fallback from this result. The proof successfully prevents
unsafe patching and preserves traceable provenance, but the primary residual
cannot be closed under the authority evidence available. More blind passes or
looser tolerance would violate the requested constraints.

Revisit only after resolving authoritative input/version parity and establishing
an explicit exterior/material certificate for generated/deleted regions. The
eventual architecture remains: established routes → isolated repair worker →
strict BrickSuite geometry/feature/provenance validation → nominal PreparedMesh
→ compensation only on fully certified retained authoritative interfaces. That
architecture is not implemented or enabled in production by this experiment.

# M37 mesh-repair feasibility proof — 23422

Date: 2026-09-26. Decision: **2 — test another embeddable repair approach before adding a production fallback.** The tested CGAL PMP pipelines produced no accepted repair on the four failure Parts. This does not prove that all automatic repair or all CGAL algorithms are unsuitable. It does reject these specific pipelines as a production-ready solution.

## Scope and build/licensing assessment

Only `experiments/mesh-repair/` and this report were added. The standalone proof is not included by root CMake. No production preparation, ManufacturingMesh, UI, fit, catalog, schema, or protocol code changed. No commit or push. The user reference and installed library were read-only.

CGAL 6.1.1 PMP supports soup cleanup/orientation, border stitching, autorefinement with snap rounding, explicit hole triangulation, intersection detection, and experimental local self-intersection removal. These are separate operations, not a turnkey semantic solid repair. The proof exercised all of those operations. [CGAL PMP documentation](https://doc.cgal.org/6.1/Polygon_mesh_processing/index.html).

PMP is GPL-3.0-or-later or commercially licensed. BrickSuite currently carries LGPL-3.0. A combined distributed build using GPL PMP needs GPL-compliant distribution, or an appropriate commercial CGAL license; CGAL's LGPL foundational packages do not make PMP LGPL. This experiment does not add CGAL to application packaging. [CGAL licensing](https://www.cgal.org/license.html).

Tested: GCC 13.1.0 / Qt 6.10.3 MinGW Release, CGAL 6.1.1, Boost 1.86.0, CMake 3.27. CGAL 6.1.1 documents C++17, CMake >=3.22, Boost >=1.74 and supports Windows GCC >=12.2, Linux GCC, and macOS Apple Clang. BrickSuite's declared CMake minimum is currently 3.16, so production integration would raise that requirement. Windows, macOS and Linux source compatibility is plausible; only Windows was compiled here. No CGAL Qt viewer is required. [Compiler/dependency requirements](https://doc.cgal.org/6.1.1/Manual/thirdparty.html).

The proof selects Boost multiprecision and avoids GMP/MPFR binaries, Eigen, TBB, and runtime CGAL DLLs. Header templates increase compile cost. Full extracted dependency archives occupy about 124.92 MiB (CGAL) + 745.97 MiB (Boost); downloads were 38.58 + 207.91 MiB. Those are developer source footprints, not application payload. The repair executable is about 4–5 MiB plus the usual MinGW runtime. CMake's newer Boost policy rejected a raw archive layout; CMake 3.27 FindBoost worked. A maintained production dependency setup should use an installed Boost CMake package.

## Inputs, reference qualification, and fixed methodology

- Authoritative source: installed `parts/23422.dat`, expanded through BrickSuite's existing loader, retaining source triangle/file/line/reference ancestry. The LDraw file describes a handle with helical groove. Its existing CC BY 4.0 attribution is retained by the source library.
- Unrepaired candidate: existing `PrintMeshConversion`, including its established 0.0004 mm seam weld. No hand repairs or semantic identity substitutions.
- Secondary reference: Ray's Bambu-repaired `23422.3mf`; the reference is experimentally visually known-good, not authoritative LEGO geometry.
- CGAL variants: clean; clean+autorefinement; autorefinement+explicit hole filling; clean+explicit hole filling; the latter followed by experimental local intersection repair. Parameters were not optimized per Part. Snap grid exponent 23 / five iterations and local-repair defaults were retained.

**Reference scale discrepancy:** its model unit declares millimetres, but object bounds are approximately 62.770 × 19.998 × 19.998 mm. Authoritative LDraw is 25.107265 × 8 × 8 mm. Ray confirmed that 250% scaling was NOT intentional. The cause is unresolved. Slicer placement transforms are translations and do not explain the scale. The original archive was not changed. Raw-scale, center-aligned sampled maximum differences are 18.846 mm reference→source and 3.975 mm source→reference.

For secondary shape comparison only, a labeled copy uses the exact factor 0.4, a proper axis permutation, and center translation. There is no fitted scale or nonrigid registration. The geometry is symmetric, so more than one orientation may be equivalent. Without the unrepaired Studio export, differences cannot be assigned specifically to Bambu repair.

## 23422 topology and repair outcome

| Variant | Vertices / faces | Components | Boundary edges | CGAL intersection pairs | Near-degenerate faces | Outcome |
|---|---:|---:|---:|---:|---:|---|
| Unrepaired candidate | 272 / 438 | 6 | 104 | 180 | 0 | Rejected |
| CGAL clean | 272 / 438 | 6 | 104 | 180 | 0 | Rejected |
| CGAL autorefinement | 704 / 974 | 10 | 424 | 3,480 | 0 | Rejected |
| CGAL refine + fill | 704 / 1372 | 10 | 0 | 12,201 | 22 | Rejected |
| CGAL clean + fill | 272 / 520 | 6 | 12 | 234 | 0 | Rejected |
| CGAL local repair | 184 / 344 | 6 | 12 | 61 | 0 | Rejected |
| Bambu reference (normalized) | 931 / 1870 | 1 | 0 | 1,407 | 0 | Secondary visual benchmark only |

Near-degenerate means area <1e-12 mm²; this is reported independently of CGAL's exact-degeneracy predicate. All repaired variants have zero edges with more than two incident faces in their indexed representation: orientation split vertices to create manifold topology. This is **not** an embedded solid certificate. Refine+fill is topologically closed but consists of ten mutually contacting/intersecting shells and includes near-zero-area faces. Clean+fill still has 12 boundary edges. Local repair returns false, leaves 12 boundaries and visibly loses both arm connections.

Refine+fill adds 398 faces across 13 explicitly filled loops. Exact classification of its 12,201 flagged pairs gives 9,531 point contacts, 2,481 segment intersections and 189 coplanar-area overlaps. The Bambu reference's 1,407 pairs comprise 1,202 point contacts, 204 segments and one coplanar-area overlap. These counts are not penetration depths; many are contacts and do not contradict Ray's visual assessment. They do mean the reference is not a zero-intersection numerical acceptance oracle. Exact-construction checks confirmed every flagged pair; inexact construction was not used for the final classification.

Every 23422 CGAL variant retains bounds X ±12.553632, Y ±4, Z ±4 mm. Bounding-box preservation alone misses severe internal/connection damage.

## Bidirectional fidelity to authoritative LDraw

CGAL bounded-error Hausdorff estimates below have a fixed **0.001 mm algorithm error bound**. Values of 0.001 are at this measurement resolution, not proof of exact equality. Percentiles and RMS use 20,000 deterministic area-weighted samples per direction. A barycentric grid additionally locates worst regions. Neither the measurement error bound nor the ownership matching epsilon is a relaxed print-acceptance tolerance.

| Variant / direction | Bounded max estimate mm | Sample P95 mm | Sample P99 mm | Sample RMS mm |
|---|---:|---:|---:|---:|
| candidate → source | 0.001000 | 0.000015 | 0.000033 | 0.000006 |
| candidate source → | 0.001000 | 0.000015 | 0.000033 | 0.000006 |
| refine → source | 0.001000 | 0.000015 | 0.000034 | 0.000007 |
| refine source → | 0.001000 | 0.000015 | 0.000033 | 0.000007 |
| fill → source | 0.396620 | 0.109115 | 0.279365 | 0.051352 |
| fill source → | 0.001000 | 0.000015 | 0.000033 | 0.000006 |
| fill-clean → source | 0.346378 | 0.000069 | 0.229770 | 0.036129 |
| fill-clean source → | 0.001000 | 0.000015 | 0.000033 | 0.000006 |
| local → source | 1.557419 | 0.418038 | 0.646033 | 0.165956 |
| local source → | 1.302000 | 0.510302 | 1.081443 | 0.245903 |
| bambu-normalized → source | 0.392641 | 0.373185 | 0.388312 | 0.162042 |
| bambu-normalized source → | 1.596139 | 0.390623 | 0.759723 | 0.213488 |

Localized worst sampled deviations (canonical millimetres):

- Refine+fill adds body/groove-region surfaces about 0.396620 mm away from source, near (0, 2.395615, 0.273113). Both arm exteriors remain close to source, but shell intersections disqualify the model.
- Clean+fill adds surfaces about 0.346378 mm away near (-3.646840, 0, 0.565680).
- Local repair's worst sampled added surface is near the left arm cut at (-6.246844, -0.000012, 0), 1.546315 mm from source; the right arm has a similar 1.528700 mm error. The bounded global estimate is 1.557419 mm. Missing source surface near (-4.946840, 1.6, 0) is 1.300008 mm from the result; bounded estimate 1.302000 mm.
- Normalized Bambu→source is worst near the inner bore at (2.217055, 0.918278, -0.784233), about 0.392657 mm. Source→Bambu peaks near (-4, 0, 0), about 1.596139 mm, at the arm/body interface. Some removed source faces may be internal interfaces; geometric distance alone does not establish their semantic disposition.

### Feature preservation

- **Center bore:** five axial probes remain unobstructed in source and all variants. This tests sampled paths, not a proof that every intentional opening survives.
- **Cylindrical body and wall thickness:** at Z=2.5 mm, 12 radial probes give source inner radii 2.752–2.800 mm and wall thickness 1.179–1.200 mm. Refine+fill retains these to numerical precision. Local repair changes wall thickness to 0.972–1.554 mm and visibly changes the polygonal inner/outer surfaces.
- **Bambu bore:** after normalization, radial inner radii are about 2.399–2.400 mm and wall thickness about 1.599–1.601 mm. Its roughly 4.8 mm bore is different from the installed LDraw bore; this is not just a scale discrepancy. Its curved surfaces also have different tessellation.
- **Both curved arms:** local repair removes their connections despite unchanged outermost bounds. Refine+fill retains the external arm samples closely but does not produce one valid solid.
- **Intentional openings/helical details:** the cross-section at Z=0.123 mm shows additional internal patch segments in filled variants. No claim of preserved groove topology or functional engagement is justified. Blanket filling is explicitly rejected as an acceptance rule.

### Secondary comparison to normalized Bambu

| Candidate | Candidate → Bambu max estimate mm | Bambu → candidate max estimate mm | RMS each direction mm |
|---|---:|---:|---:|
| fill | 1.596139 | 0.392640 | 0.303037 / 0.161987 |
| fill-clean | 1.596139 | 0.392641 | 0.243917 / 0.161927 |
| local | 1.583922 | 1.301699 | 0.216309 / 0.241804 |

## Ownership recovery

Mapping uses original loader triangle/file/line/reference ancestry. A repaired triangle is geometrically supported only if all vertices and its centroid fit one source triangle within 0.0004 mm and its normal agrees within 0.1 degrees (either winding). Ambiguous ancestry is never guessed. This is conservative post-hoc geometric attribution, not causal provenance or semantic certification.

| 23422 variant | Confident unique source triangle, faces | Confident surface area | Unmatched/generated, faces |
|---|---:|---:|---:|
| candidate | 100.00% | 100.00% | 0.00% |
| refine | 100.00% | 100.00% | 0.00% |
| fill | 82.51% | 88.24% | 17.49% |
| fill-clean | 86.54% | 94.39% | 13.46% |
| local | 72.38% | 57.01% | 27.62% |
| Normalized Bambu | 0.21% | 0.24% | 99.79% |

No multiple-match ambiguity was observed under this strict test. Unmatched Bambu faces are **not** evidence that Bambu invented 99.79% of the shape: different tessellation, bore geometry and vertex perturbations prevent strict original-triangle matching. For CGAL, pure subdivisions can preserve useful ancestry, but patches and removed/merged interfaces break fit feature ownership. A future proof should use the autorefinement visitor to carry source triangle IDs and label every hole patch explicitly unowned.

**Nominal-only is conditional, not a safety escape:** unknown fit ownership may disable compensation on a geometry-valid, fidelity-certified result. It cannot make an open, intersecting, distorted, or unaccounted-for result printable. None of these repaired candidates qualifies, even nominally.

## Small comparison corpus

| Part | Current BrickSuite result / failure class | Refine+fill components / boundaries / intersections | Local repair components / boundaries / intersections | Accepted repair |
|---|---|---:|---:|---|
| 23422 | source_coverage; single-loop closure | 10 / 0 / 12,201 | 6 / 12 / 61 | No |
| 44375a | source_coverage; no closed body/cavity | 9 / 0 / 17,423 | 6 / 0 / 2,895 | No |
| 80910 | source_coverage; boundary does not contact body | 62 / 0 / 64,302 | 48 / 64 / 6,130 | No |
| 6553 | source_coverage; no closed body/cavity | 28 / 0 / 6,041 | 6 / 32 / 966 | No |
| 3001 | success; normal semantic preparation control | 26 / 0 / 24,037 | 12 / 0 / 642 | No |

Current categories were reconfirmed with a five-Part audit, not a random/catalog scan. Normal 3001 preparation/export/reopen succeeds; blindly routing its raw polygon soup through generic repair would regress it. Existing normal routes must always remain first. Full per-variant bounds, face counts, degenerate counts, distances and ownership for the secondary corpus are in the local JSON artifacts.

## Runtime and resource measurements

Windows Release, one repair process per variant; wall time includes reading, repair, strict validation, exact contact classification and OFF writing. Peak RSS is sampled every 10 ms and is approximate, not peak private commit. A 120-second / 4 GiB child-process limit was never reached.

| Part | Refine+fill wall seconds / peak RSS MiB | Local wall seconds / peak RSS MiB |
|---|---:|---:|
| 23422 | 1.390 / 6.52 | 0.035 / 6.12 |
| 44375a | 0.707 / 7.77 | 0.283 / 7.20 |
| 80910 | 3.442 / 8.00 | 0.448 / 6.34 |
| 6553 | 0.284 / 6.46 | 0.108 / 6.31 |
| 3001 | 0.657 / 6.69 | 0.077 / 6.22 |

23422 cleanup: 0.035 s / 5.59 MiB; autorefinement: 0.441 s / 6.32 MiB; clean+fill: 0.046 s / 5.77 MiB. Separate bounded-distance validation reached about 8.29 s / 84.64 MiB across the 23422 comparisons (maximum time and memory occurred in different comparisons). Repair runtime is not the blocker here; geometry acceptance is. These are single-run feasibility measurements, not a benchmark suite.

## Recommendation and smallest future boundary

**Choose option 2.** Test Geogram's surface-arrangement/intersection and repair facilities on this same corpus next; its BSD-3-Clause license is more straightforward for the existing distribution model. This is a candidate for investigation, not a claim of measured superiority or proof it can reconstruct missing author intent. [Geogram license](https://github.com/BrunoLevy/geogram/blob/main/LICENSE), [surface-intersection API](https://github.com/BrunoLevy/geogram/blob/main/src/lib/geogram/mesh/mesh_surface_intersection.h).

Do not substitute Manifold as a general repair engine: its own documentation requires manifold input except limited merging. ManifoldPlus has a non-commercial restriction. Neither is a ready drop-in answer to this task. [Manifold input requirements](https://github.com/elalish/manifold), [ManifoldPlus terms](https://github.com/hjwdzh/ManifoldPlus/blob/master/README.md).

If a later engine proves acceptable, the smallest architecture is: existing preparation routes → isolated repair-candidate service/process → strict topology/intersection, two-way fidelity, feature-opening/wall and source-coverage checks → nominal PreparedMesh. Keep engine name/version, parameters, new-surface masks, original-face lineage, timings and validation evidence with the candidate. Unmatched fit ownership disables fit compensation; validation failure stays failure. Prefer a process boundary for enforceable timeout/memory limits. Do not weaken existing Source Coverage or infer fit ownership from repaired filename/nearest Part identity.

Before changing geometry policy, obtain the unrepaired Studio export to distinguish Studio model/tessellation/unit differences from actual Bambu repair effects. The repaired reference remains useful for visual comparison, but it cannot set numerical tolerances for authoritative LDraw.

## Validation and artifacts

- Qt 6.10.3 MinGW Release BrickSuite build passed (no production rebuild required).
- Isolated CGAL Release build and synthetic repair CTest passed. Three Python measurement/ownership/probe tests passed.
- Full configured production CTest: **108/108 passed**, 62.61 seconds. Production build files were not touched.
- Schema **35**, Protocol **1.5**, unchanged. `git diff --check` passed; no unrelated modifications, no commit, no push.
- Windows was exercised; macOS/Linux builds and physical prints were not.

Reproduction: [experimental README](../experiments/mesh-repair/README.md). Local ignored artifacts under `build/mesh-repair-proof/results/`: `runs.json`, `analysis.json`, `bounded-distances.json`, source/candidate/repaired OFF meshes, ancestry JSON, and comparison/section PNGs. The reference archive was not added to version-controlled files.

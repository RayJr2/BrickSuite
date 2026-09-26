# M37 Geogram mesh-repair feasibility — 2026-09-26

**Decision: option 2 — test a more constrained repair approach before a production
fallback prototype.** Geogram materially improves on CGAL in several cases,
particularly arm preservation and the 6553/control topology. It did **not** turn
the matched 23422 Studio OBJ into one valid printable manifold. Neither broad
production integration nor abandoning automatic repair altogether is justified.

The next experiment should explicitly stitch compatible boundary segments and
reconstruct only source-constrained missing regions, then use arrangement/solid
classification and independent fidelity checks. Keep Geogram as an experimental
arrangement candidate. Unrestricted hole filling and repeated outer-shell
extraction are not sufficient acceptance policies.

No production code or root project files changed. The pre-existing untracked
CGAL experiment/report were preserved. New work is confined to this report and
`experiments/geogram-repair/`; dependencies and private geometry/results are in
ignored `build/geogram-repair-proof/`. No commit or push.

## Engine, build and licensing

Tested **Geogram 1.10.0**, official `geogram_1.10.0.zip` release asset, with
Windows x64 / GCC 13.1.0 from the Qt 6.10.3 MinGW kit, Release, C++17, CMake/Ninja.
It built without source patches. Upstream emitted MinGW format/conversion
warnings in binary mesh I/O; the proof uses OFF and retains the build log for
future integration review. Standalone CMake `add_subdirectory` links the
`geogram` target; no Qt dependency or second preparation implementation is added.
Production adoption should keep its CMake flags/options scoped away from the
application. Upstream has native Linux GCC/Clang and Darwin Clang platform files;
those builds were not exercised on this Windows host.
[Release](https://github.com/BrunoLevy/geogram/releases/tag/v1.10.0),
[MinGW instructions](https://github.com/BrunoLevy/geogram/wiki/compiling_Makefile),
[macOS](https://github.com/BrunoLevy/geogram/wiki/compiling_MacOS),
[Linux](https://github.com/BrunoLevy/geogram/wiki/compiling_Linux).

The tested archive is 18.34 MiB, extracted source 53.93 MiB; the static archive
is 9.66 MiB and proof executable 5.14 MiB, excluding matching MinGW runtime DLLs.
No Boost, CGAL, GMP, Qt or GPU dependency is required by the **repair executable**.
The separate CGAL tools remain experiment-only validators with their prior
licensing implications. Optional graphics, TetGen, Triangle, Lua, HLBFGS, TBB,
legacy numerics, FPG and Exploragram were disabled. Bundled OpenNL, AMGCL,
PoissonRecon, xatlas, libMeshb, RPly, predicates and zlib still compile.

Geogram core is **BSD-3-Clause**: source/binary redistribution is permitted with
the required copyright, conditions and disclaimer retained; no endorsement by
the named authors/institution. It does not impose CGAL PMP's GPL/commercial
choice. Third-party notices still need to accompany whatever is distributed
(including MIT/BSD/zlib/public-domain components). Do not assume the entire
optional source tree is covered by the core license. No GeogramPlus/commercial
add-on was used or evaluated.
[License](https://github.com/BrunoLevy/geogram/blob/v1.10.0/LICENSE),
[third-party inventory](https://github.com/BrunoLevy/geogram/wiki/ThirdParty),
[GeogramPlus](https://github.com/BrunoLevy/geogram/wiki/GeogramPlus).

Relevant capabilities: exact intersection construction and constrained Delaunay
retriangulation; radial sorting into volume regions; outer/internal-shell
classification; duplicate/coincident-vertex cleanup and nonmanifold splitting;
explicit boundary-hole triangulation; coplanar simplification. The general
smooth CVT remesher and point-cloud reconstruction are different operations and
were not used to conceal fidelity errors. They can relocate/reconstruct surfaces
and would need separate thin-wall, opening and ancestry validation.
[Arrangement/boolean pipeline](https://github.com/BrunoLevy/geogram/wiki/BooleanOps),
[remeshing](https://github.com/BrunoLevy/geogram/wiki/Remeshing).

## Inputs and unit correction

Exact read-only user inputs:

- `D:\Lego Studio\Exported_obj\23422.obj`, SHA-256
  `6d299861076f69d6581e1a32afee849386c1e7e193660cb539d654be4a1163ed`.
- `D:\Bambu Studio\Exported_3mf\23422.3mf`, SHA-256
  `d7531ae2cb871a8850cc3da606419e3dff459428fdf6a049840c8d05d72aaad7`.

Both use **0.4 mm per Studio coordinate unit** throughout this report. Proper
axis permutation and center translation only; no fitted scale, ICP or deformation.
3MF component/build translations are recorded and removed from shape comparison.
No substitute 23422 export was used. The installed authoritative model is
`D:\LDraw\parts\23422.dat`, expanded by BrickSuite's existing loader and canonical
`(.4*x,.4*z,-.4*y)` conversion, with source/reference ancestry preserved.

| Input | Vertices / triangles | Components | Boundary / nonmanifold edges | Normalized dimensions mm |
|---|---:|---:|---:|---|
| Matched Studio OBJ | 932 / 1,130 | 11 | 732 / 0 | 25.107264 × 8.000000 × 8.000000 |
| Bambu repaired reference | 931 / 1,870 | 1 | 0 / 0 | 25.108000 × 7.999200 × 7.999200 |
| Installed LDraw welded candidate | 272 / 438 | 6 | 104 / 0 | 25.107265 × 8 × 8 |

The matched OBJ already has the approximately **4.80 mm bore** and **8.00 mm OD**.
The installed LDraw has a coarser polygonal bore with vertex diameter **5.60 mm**.
This source/export difference predates Bambu repair. It must not be described as
a Bambu-induced dimensional alteration or intentional 250% scaling. This matched
pair supersedes that uncertainty in the preceding CGAL report; its original
measurements remain retained as history.

Bambu is a strong topology-repair benchmark, not an authoritative replacement
for installed LDraw. Most matched before/after surface samples agree within
roughly 0.0005 mm; localized changes reflect removed internal sheets and generated
closure regions rather than uniform resizing.

## Experiment and exact 23422 outcome

All runs use a fixed one-thread budget and bounded child processes (120 seconds,
4 GiB RSS/private memory). Five baseline modes separate cleanup, arrangement,
outer extraction, filling before extraction, and filling afterward. Input seam
weld is the existing **0.0004 mm**, not a tuned acceptance tolerance. Following
the upstream example, post-intersection colocation is **exact**, because merging
nearby intersection vertices can alter topology. An initial near-weld postpass
was retained separately under `results/`; final measurements are under `final/`.

Primary candidate is **fill-outer**: cleanup → explicit hole patches → exact
intersection/radial sorting → outer-shell extraction → exact cleanup. No largest
component selection, smoothing or silent fit reconstruction. Supplemental runs
use zero input weld, exact coplanar simplification, or at most three passes.

| 23422 variant | V / F | Components | Boundary edges | Contact/intersection pairs | Near-degenerate faces |
|---|---:|---:|---:|---:|---:|
| Geogram Studio outer only | 2,102 / 2,550 | 34 | 1,648 | 10,838 | 16 |
| Geogram Studio fill-outer | **1,040 / 2,022** | **7** | **90** | **942** | **18** |
| Studio fill-outer + exact simplification | 954 / 1,868 | 7 | 72 | 1,030 | 18 |
| Geogram installed-LDraw fill-outer | 402 / 816 | 1 | 0 | 112 | 0 |
| CGAL matched Studio refine + fill | 2,024 / 3,988 | 16 | 0 | 41,465 | 36 |
| CGAL matched Studio local repair | 858 / 1,694 | 6 | 6 | 170 | 0 |
| Bambu reference | 931 / 1,870 | 1 | 0 | 1,407* | 0 |

All listed native meshes have zero edges incident to more than two faces. This
does not prove an embedded manifold: split/coincident vertices, boundaries and
contacts remain. Near-degenerate means triangle area < 1e-12 mm², unchanged from
the previous proof, not necessarily exact collinearity.

For Studio fill-outer, the 942 exact-classified pairs comprise **836 point
contacts and 106 segment intersections**, no coplanar area pairs; longest
intersection segment 0.523238 mm. All 90 boundary edges are in the central body
region (X between -4 and +4 mm); combined boundary length 25.1131 mm. Exact
simplification reduces edge subdivisions, not that total open-boundary length.
Repeat passes leave 84 then 98 boundary edges, still seven components. Exact
input-only welding performs worse (11 components, 322 boundaries).

The closed installed-LDraw candidate has **112 point-contact pairs**, no segment
or area intersection pairs. It is a substantial topology improvement over the
prior CGAL result but still lacks validated embedded-solid and closure fidelity.

*Bambu's strict checker result from the preceding proof is 1,202 point contacts,
204 segment intersections and one coplanar-area pair. This does not erase its
known-good visual/topological benchmark status. Exact contacts are not equivalent
to volumetric penetration; no candidate is accepted/rejected solely by comparing
these totals. Its native topology is closed and consistently wound.

## Fidelity, localized deviations and features

Distances below are millimeters. Max is the independent global Hausdorff
**estimate with 0.001 mm algorithm error**, not an adjustable acceptance threshold.
P95/RMS use 20,000 fixed-seed area samples, with an additional barycentric grid
for sampled extrema. Directions are explicit; all source triangles, including
internal sheets, participate. Raw source→repair distance is not automatically
loss of an intended exterior surface.

| Direction | Max estimate | P95 | RMS |
|---|---:|---:|---:|
| Studio → Bambu | 1.596139 | 0.000536 | 0.110719 |
| Bambu → Studio | 0.269361 | 0.000480 | 0.022257 |
| Studio → authoritative LDraw | 0.392319 | 0.373576 | 0.159524 |
| LDraw → Studio | 0.400166 | 0.381754 | 0.175529 |
| Geogram Studio fill-outer → LDraw | 0.392319 | 0.374201 | 0.164852 |
| LDraw → Geogram Studio fill-outer | 1.596518 | 0.387452 | 0.206157 |
| Geogram Studio fill-outer → Studio | 0.269372 | 0.000024 | 0.025051 |
| Studio → Geogram Studio fill-outer | 1.596518 | 0.000023 | 0.106385 |
| Geogram Studio fill-outer → Bambu | **0.540400** | **0.000480** | **0.038014** |
| Bambu → Geogram Studio fill-outer | approximately ≤0.001 | 0.000432 | 0.000262 |
| Geogram installed-LDraw fill-outer → LDraw | 0.424261 | 0.000031 | 0.039905 |
| LDraw → Geogram installed-LDraw fill-outer | 1.200000 | 0.000026 | 0.102644 |

The largest Geogram/Bambu discrepancy is localized at approximately
**(0, 2.4, 0) mm**, near the bore/groove, with nearest Bambu point near
(0, 1.859599, 0). Bambu→Geogram sampled max is 0.000977 mm: Geogram largely
contains the benchmark surface but retains additional/problematic regions.
Studio→Bambu's 1.596139 mm extremum is near (4,0,0), the buried arm/body interface.
It must not be interpreted as 1.6 mm shrinkage of the external arms.

The installed-LDraw repair's added patch differs by 0.424261 mm near
(0,2.367433,0); deleted source extremum is (-4,0,0), distance 1.2 mm. Whether a
removed sheet is internal, and whether a new groove patch is intended, require
explicit source-boundary classification. An overall bounding-box check cannot
answer either question.

- **Bore:** five axial probe rays remain unobstructed in Studio, Bambu and both
  Geogram inputs/results. At Z=2.5 mm, Studio repair radial bore is 2.399945–2.400000
  mm (diameter about 4.80 mm); Bambu 2.399389–2.400489 mm. No gross bore cap found,
  but the localized groove discrepancy prevents claiming complete preservation.
- **Cylindrical body / wall:** Studio repair OD approximately 8 mm; radial wall
  1.599961–1.600000 mm, Bambu 1.599200–1.600681 mm. Installed LDraw repair retains
  its own 2.752131–2.800000 mm polygonal bore radius and 1.179485–1.200000 mm wall,
  rather than silently adopting Studio dimensions. These are 12 radial probes
  at one section, not certified global minimum wall thickness.
- **Arms:** both curved arms and the overall 25.107264 × 8 × 8 mm bounds remain
  in the Studio candidate. Section/mesh inspection shows no CGAL-style cuts at
  the connections. Remaining open edges are confined to the central body.
- **Intentional openings:** the center through-bore is probed and sectional
  outlines inspected at Z=0.123 and 2.5 mm. Open-boundary/groove behavior is still
  unresolved; no blanket claim that every opening survives is warranted.

Review `build/geogram-repair-proof/final/23422-comparison.png` and
`23422-sections.png`; machine-readable worst coordinates, nearest triangles,
regions, bounds and feature rays are in `analysis.json` and `supplement.json`.

## Comparison with CGAL

The prior CGAL proof used installed LDraw's 438 triangles, not the new 1,130-face
Studio OBJ. This experiment reran the prior CGAL modes on the **same normalized
OBJ** to avoid confusing input differences with engine quality.

CGAL refine+fill closes 16 separate components and leaves 41,465 exact contact/
intersection pairs. CGAL local repair returns failure, leaves six boundaries,
and visibly cuts both arm connections: maximum estimates to/from Studio are
1.584097 / 1.253031 mm, versus Geogram's largely preserved arms. Its maximum
to/from Bambu is 1.591929 / 1.253424 mm. Geogram's Bambu P95/RMS is much better
(0.000480/0.038014 versus CGAL local 0.250943/0.161267 mm), but this improvement
does not close the remaining central boundaries or validate the groove.

On installed LDraw, prior CGAL fill had 10 components, 12,201 pairs and generated
patches; Geogram reaches one closed component with 112 point contacts. This is
material experimental progress, not production acceptance.

## Ownership feasibility

Same conservative test as CGAL: all three vertices plus centroid supported by
one source triangle within 0.0004 mm; normals agree within 0.1°; source file,
line and full reference-instance ancestry disambiguate matches. Percentages are
**surface area**, not claims that matched faces carry causal provenance.

| Result | Confident geometric source support | Unmatched/new or cross-triangle |
|---|---:|---:|
| Studio original → installed LDraw | 0.233% | 99.767% |
| Studio Geogram fill-outer → installed LDraw | 0.233% | 99.767% |
| Studio Geogram fill-outer → original Studio triangles | 96.724% | 3.276% |
| Installed-LDraw Geogram fill-outer | **96.106%** | **3.894%** |
| 6553 after second pass | about 99.999425% | about 0.000568%, plus tiny ambiguous area |
| 3001 outer extraction | 100% | 0% |

Studio's low match to installed LDraw reflects pre-existing geometry/tessellation
differences, not proof that repair invented 99.8% of its shape. Studio face
support does **not** establish authoritative LDraw reference ownership.

Geogram internally maintains `original_facet_id` and copies facet attributes
during subdivision, which makes ancestry propagation technically plausible.
The adapter would have to preserve pre-cleanup IDs, carry sets through duplicates,
and mark new patches unowned. This proof measures geometric support, not that
end-to-end implementation. Coplanar simplification demonstrably weakens the
single-triangle mapping: installed 23422 support drops from 96.106% to 64.548%
without a corresponding shape change. A future mapping needs region ancestry
sets rather than inventing a single owner for such faces.

Existing fit semantics could survive only on unchanged, uniquely established
source regions with verified feature context. Neither nearest geometry nor a
matching filename grants that permission. **Nominal-only is feasible only after
solid/fidelity validation**; disabling fit does not make an open or geometrically
ambiguous mesh safe to print. No experimental result is promoted here.

## Comparison corpus

All inputs are the preceding proof's installed-LDraw candidates. Validation
methods/tolerances are unchanged. Baseline fill-outer results:

| Part | V / F | Components / boundaries | Exact pair count | Near-degenerate faces | Result |
|---|---:|---:|---:|---:|---|
| 6553 | 583 / 1,046 | 17 / 112 | 550 | 0 | First pass fails; second pass below |
| 44375a | 4,778 / 9,350 | 2 / 204 | 908 | 96 | Open; problematic new closure |
| 80910 | 1,630 / 3,152 | 17 / 112 | 669 | 0 | Open; fidelity loss unresolved |
| 3001 outer only | 484 / 964 | 1 / 0 | 0 | 0 | Good geometric control; normal route already succeeds |

6553's **second pass** yields **511 vertices / 1,030 faces, one component,
zero boundary/nonmanifold edges, zero exact intersection pairs and zero
near-degenerate faces**. Third pass is unchanged. Its output→source sampled
maximum is 0.000536 mm; global estimator reports 0.001 mm at its fixed error.
Source→output estimate 1.176427 mm requires validating removed internal sheets
(sampled extremum near (0,0,-8)). This is a promising individual candidate,
not permission to accept every closed result.

| Baseline direction | Max estimate mm | P95 mm | RMS mm |
|---|---:|---:|---:|
| 6553 repair → source | approximately ≤0.001 | 0.000010 | 0.000012 |
| 6553 source → repair | 1.176427 | 0.174022 | 0.105544 |
| 44375a repair → source | 2.353905 | <0.000001 | 0.049734 |
| 44375a source → repair | 0.396090 | <0.000001 | 0.016666 |
| 80910 repair → source | 1.414214 | 0.000028 | 0.043742 |
| 80910 source → repair | 1.843898 | 1.379168 | 0.386640 |
| 3001 outer → source | approximately ≤0.001 | <0.000001 | <0.000001 |
| 3001 source → outer | 1.600000 | <0.000001 | 0.159677 |

44375a's worst new closure is near (0,0,-1.6) mm. 80910's is near (0,-8,0).
3001's reverse extremum is on the source's Z=0 internal sheet below a stud;
output→source is numerically zero. Internal-sheet removal must be distinguished
from damage to the printable exterior; the raw bidirectional statistic alone
is not an acceptance oracle. Normal BrickSuite's successful 3001 route remains
the control and should not be replaced by repair.

Exact simplification does not close 44375a or 80910. 44375a's third repeat
**exits with a Geogram fatal radial-sort error** (same reference half-plane,
`mesh_surface_intersection.cpp:1701`), captured in its child log. BrickSuite was
not crashed. This is direct evidence for process isolation and bounded attempts
in any future design; an ordinary C++ exception wrapper is insufficient.

## Runtime, resources and validation

Measured process wall times include startup/I/O; independent validation times
are separate. RSS is sampled every 10 ms and can miss short peaks.

| Run | Repair wall seconds | Peak RSS MiB | Exact check seconds |
|---|---:|---:|---:|
| Studio 23422 fill-outer | 0.087 | 8.73 | 0.150 |
| Installed 23422 fill-outer | 0.047 | 8.79 | 0.035 |
| 6553 first pass | 0.084 | 8.89 | about 0.06 |
| 6553 second pass | 0.046 | 8.18 | 0.035 |
| 44375a first pass | 1.616 | 13.87 | 0.243 |
| 80910 first pass | 0.397 | 10.69 | 0.10 |
| 3001 outer only | 0.045 | 8.40 | 0.024 |

CGAL matched-OBJ refine+fill takes 4.738 seconds / 7.70 MiB **including its exact
checker**; local repair 0.097 seconds / 6.87 MiB including its checker. Do not
compare those times directly with Geogram repair-only time. Python sampling,
ownership and global-distance validation add separate costs; detailed process
metrics and return codes are preserved, not claimed to be a real-time UI budget.
The slowest primary global-distance process took 23.37 seconds and the largest
sampled RSS among those validators was 190.14 MiB. These validation costs must
be included in any eventual worker budget.

Validation completed:

- Geogram Release build and CTest tetrahedron test passed.
- Four new synthetic proof tests and three reused measurement tests passed.
- Qt 6.10.3 MinGW Release BrickSuite build passed (`ninja: no work to do`).
- Full configured Release CTest **108/108 passed**, 63.85 seconds.
- `git diff --check` passed; new files also checked for whitespace independently.
- Schema **35**, protocol **1.5**, unchanged.
- No production preparation/ManufacturingMesh, catalog, UI, root CMake or data
  changes; no Debug test compilation, commit or push.

Cross-platform runtime builds and physical print acceptance were not performed.
Private benchmark files remain local. Reproduction commands and precise method
contracts are in `experiments/geogram-repair/README.md`.

## Next M37 architecture step

Retain normal preparation routes and the successful control behavior. Before
designing a production fallback, require the matched 23422 case to pass a
source-aware boundary/region reconstruction experiment with both curved arms,
bore/groove and intentional openings validated. Keep generated patches explicitly
separate from authored source and treat ambiguous fit regions as unowned.

Only after that gate: normal routes → isolated bounded repair worker → manifold,
intersection, exterior coverage, feature and thickness validation → nominal
PreparedMesh. Propagate verified ancestry sets; do not infer fit ownership.
The 44375a fatal error makes worker-process isolation a measured requirement.
This report proposes that boundary, but implements no production route.

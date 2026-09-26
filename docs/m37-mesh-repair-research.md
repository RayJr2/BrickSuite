# M37 mesh-repair research and production decision

This document consolidates the September 2026 repair experiments. Their purpose
was to improve Prepare-for-Printing coverage without losing authoritative LDraw
geometry, intentional openings, or fit ownership. Part 23422 was the primary
case; 6553, 44375a, 80910 and the already-successful 3001 were the comparison corpus.

The experiments did **not** establish a safe general automatic repair fallback.
The production decision was validated external repair, with a distinct explicit
User-Accepted Local Override path for unresolved source correspondence. Historical
rejection by strict validation and later user-reviewed nominal acceptance are
different outcomes, not contradictory measurements or automatic certification.

## Historical inputs and units

Installed LDraw remained authoritative. BrickSuite converts LDraw coordinates to
Z-up millimeters as `(0.4*x, 0.4*z, -0.4*y)`. The matched Studio OBJ and original
Bambu 3MF benchmark also required **0.4 mm per Studio coordinate unit** for the
research comparison. This was explicit normalization of those benchmark files,
not fitted scaling and not intentional 250% scaling by the user. Production
override import does not infer this conversion or automatically rescale files.

The matched Studio input had 932 vertices, 1,130 triangles, 11 components and
732 boundary edges. Its Bambu repair had 931 vertices, 1,870 triangles, one
component and zero boundary/non-manifold edges. Both normalized to approximately
25.107 × 8 × 8 mm. The Studio geometry already had an approximately 4.80 mm bore;
the installed source had a different polygonal bore, approximately 5.60 mm vertex
diameter above its splines. That difference preceded Bambu repair. It must not be
attributed to repair-induced resizing. A uniform 4.80 mm bore is not a valid
description of the installed source's passage and spline geometry.

The original Studio/Bambu pair and the later Bambu repair of BrickSuite's own
millimeter export were **different inputs**. The former was a useful visual and
topological benchmark, not an authoritative LEGO model or zero-intersection
oracle. Exact research contact counts included point/segment contacts and are
not penetration depths; do not equate them with another validator's counts.

## Historical engine findings

| Engine | Tested build and licensing findings | Why no production fallback followed |
|---|---|---|
| CGAL 6.1.1 Polygon Mesh Processing | Windows GCC 13.1.0, Qt 6.10.3 MinGW Release, C++17, Boost 1.86.0, CMake 3.27. Boost multiprecision avoided GMP/MPFR binaries; no CGAL Qt viewer was needed. The assessed CMake requirement was at least 3.22 versus BrickSuite's 3.16 minimum. PMP used GPL-3.0-or-later/commercial licensing, unlike the LGPL foundational packages; distribution would need a separate licensing decision. | Soup cleanup, orientation, stitching, autorefinement, explicit filling and local intersection repair produced no accepted repair on the failure corpus. Closed boundaries did not imply one valid solid; local repair could remove arm connections. |
| Geogram 1.10.0 | Built unmodified on Windows GCC 13.1.0/C++17 using the official release archive. Standalone CMake integration; repair executable needed no Qt, Boost or CGAL. Optional subsystems were disabled, but bundled dependencies remained. Core BSD-3-Clause was more straightforward for distribution; bundled third-party notices still required review. | Arrangement and outer-shell extraction improved several results, but the matched 23422 case retained open boundaries and unresolved groove fidelity. Repeated filling was not a valid acceptance policy. A fatal radial-sort failure also required process isolation. |

These are the recorded findings for the tested versions, not a current legal
assessment or a claim that every algorithm in either library is unsuitable.
macOS/Linux runtime builds were not exercised. CGAL template/dependency build
cost was significant; Geogram had a smaller dependency footprint. Geometry
acceptance, rather than repair-only runtime, was the decisive blocker.

CGAL's installed-source 23422 refine-and-fill result had ten components, zero
boundary edges, 12,201 exact contact/intersection pairs and 22 near-degenerate
faces. Its local repair left boundaries and damaged the arm connections despite
unchanged overall bounds. Blind repair also regressed the successful 3001 control.

Geogram's matched Studio 23422 fill/outer result retained seven components,
90 boundary edges, 942 contact/intersection pairs and 18 near-degenerate faces.
Its installed-LDraw variant reached one closed component but retained 112 point
contacts and unresolved new-surface fidelity. A second pass on 6553 produced good
topology, yet removed-source interpretation and generated-face ownership remained
unproven. 44375a and 80910 still failed; 3001's normal route remained preferable.
Geogram was rejected as a v0.4.0 fallback, not dismissed as a future research tool.

## Source-constrained repair and ownership

The next proof operated on saved residuals rather than restarting blind repair.
It traced input/generated faces through exact replay and allowed seam joins only
with unique authoritative edge support, matching intervals, preserved lineage,
no protected opening, and no newly invalid topology. No coordinates or tolerances
were adjusted merely to obtain acceptance.

All ten remaining 23422 boundary loops lacked complete authoritative support.
The constrained stage authorized zero joins and zero patches, leaving the
seven-component/90-boundary result unchanged. Eight justified seams on 80910
improved its boundaries but did not make it acceptable. A closed loop alone was
not permission to cap it. Studio input lineage could not establish ownership of
different installed-LDraw surfaces.

Nearest-triangle matches and geometric overlap are evidence, not causal ancestry
or a complete fit-feature contract. Generated faces remain repair geometry even
when coincident with authored geometry. Simplification can merge several source
owners without visibly changing shape. Unproven fit ownership must never be
invented, and nominal-only output cannot excuse invalid topology or exterior loss.

## Bambu compatibility and the refused union

The later `23422-unvalidated-repair-source-mm.3mf` loaded after the importer
allowed lib3mf's optional unresolved vendor-metadata namespace warning (`0x80AE`).
This container compatibility change did not relax geometry validation or rewrite
the input file. Other geometry/required-extension failures still reject.

That mesh had 362 vertices, 772 triangles, two components, 18 non-manifold
vertices and 257 intersection pairs, despite zero boundary/non-manifold edges
and correct bounds. Component 1 independently had 16 non-manifold vertices and
233 internal intersections; component 2 was valid. Another 24 intersection pairs
were cross-component. Shared vertices explained the two extra whole-mesh
non-manifold-vertex findings. These were not merely two otherwise-valid solids
overlapping each other: **no Boolean union was attempted on this file**.

## Blender and source-exposure findings

Blender 5.1.2 started from six open source components (438 triangles). Exact-position
welding and normal recalculation did not close them; they were ineligible as
closed Boolean operands. Remesh trials operated on duplicates after preserving
the original scene. No decimation or arbitrary scaling was used to force a pass.

The selected 0.15 mm voxel candidate had 32,328 vertices and 64,656 triangles,
one component and zero reported boundaries, non-manifold edges/vertices,
intersections or degenerates. Its bounds were 24.972664 × 7.969524 × 8 mm versus
source 25.107265 × 8 × 8 mm. The approximately 0.0673 mm centered endpoint change
passed the existing 0.10 mm bounds tolerance. Strict sampled distances were
approximately 0.0918 mm repair-to-source and 1.2343 mm source-to-repair, so it
initially failed the unchanged 0.20 mm bidirectional rule.

The asymmetric analysis found 1,086 distant source samples inside the valid
repair: 456 at arm/body overlaps and 630 on spline-overlapped passage walls.
Passage probes remained open and sampled exposed regions remained close, but
containment did not establish that burying every source patch was permissible.

The source-only exposure experiment found all 65 tested outward directions
blocked at two offsets for those samples. **Blocked finite rays do not prove
complete authored-surface exposure or internal composition.** They cannot rule
out all escape directions or curved paths, and an opposite cavity wall can block
a ray across an intentional void. Repair-volume containment cannot resolve this
source-authority question either. A sufficient exact enclosure certificate for
closed convex source components did not apply to 23422's open components.
All 1,086 samples remained exposure ambiguities; no automatic certificate or fit
ownership was granted. Historical probe measurements were not physical fit tests.

## Current production behavior

Native preparation remains the first route. Local Printable Overrides are an
explicit catalog-Part workflow using the existing nominal PreparedMesh exports:

- Repair-source export preserves all loaded source triangles in millimeters.
  Strict 3MF/STL extraction does not silently discard malformed faces; STL is
  explicitly interpreted as millimeters. Placement translation may be removed,
  but scale and orientation are not fitted.
- Strict acceptance retains topology/orientation, bounds and bidirectional
  sampled-fidelity checks. A separate bounded review gate may identify valid
  solids with unresolved reverse correspondence as reviewable, never automatically
  source-certified. Outside/exposed loss and recognized fit interfaces cannot be
  waived. User acceptance requires explicit confirmation and revalidation.
- The Blender candidate can pass that later user-reviewed nominal path. This
  does not retroactively make the historical strict/exposure proofs successful.
  Rejected imports preserve Source and any previously valid override.
- Atomic local records bind Part/LDraw identity, source geometry/reference ancestry,
  repaired-mesh fingerprints and acceptance/validation versions. Stale acceptance
  requires review again. Catalog records, aliases and source files are unchanged.
- Closed-component normalization is narrowly guarded: two independently valid
  solids with demonstrated contact/intersection, one existing MCUT union, at most
  2,000 input faces, a 10-second deadline and a 512 MiB worker memory limit. Final
  topology and override validation still run; no gap bridging or disconnected
  output is accepted. This is not a new arbitrary-precision Boolean backend.
- Experimental Auto Fit requires its own warning and reads existing compatible
  Verified profiles. In this retained implementation, no safe repaired-surface
  ownership mapping is established, so the attempt fails before transformation
  and produces no experimental export. With the tested Bambu PETG profile, 23422
  reported 23 correction entries considered, zero recognized operand fit semantics,
  zero mapped repaired features and zero corrections applied. Profile availability
  is distinct from applicability; nominal export does not apply a profile.

Current details remain in [STL interoperability](m37-stl-override-interoperability.md),
[user-reviewed overrides](m37-user-reviewed-nominal-override.md), production Help,
and the supported service/viewer regression tests. Those notes include historical
validation snapshots; the current production distinction above supersedes older
statements that no Blender candidate or STL importer existed.

## Crash isolation and future work

Geogram's 44375a fatal radial-sort error terminated an isolated child; the parent
recorded the failure and successfully ran a subsequent control. A C++ exception
wrapper alone cannot contain that failure. Research workers persisted Part, phase,
limits and output/log paths before launch using flushed, atomic state writes,
then recorded PID and exit/resource status. Wall/CPU, memory and output limits
bounded attempts and only the spawned process tree could be terminated. Sampled
resource monitoring could overshoot and was not an OS hard-quota guarantee.

Any future fallback needs an isolated worker with enforceable lifetime/resource
limits, durable crash attribution, explicit generated-face provenance, independent
topology/exterior/opening/wall validation and proven ownership before compensation.
Retain successful native routes and do not relax tolerances to hide unresolved
source authority. Neither the retired engines nor their research tests are normal
application build dependencies.

## Historical recovery and scope

Full research scripts, standalone CMake projects, measurements described in their
READMEs, and the five superseded reports are recoverable from commit
`34305a6e549e54acc1e26b59c2774ee3ec7d522e`. For example, `git show` can read a retired
path at that commit without changing the checkout. The six retired directories
were `experiments/asymmetric-fidelity`, `experiments/blender-repair`,
`experiments/source-constrained-repair`, `experiments/source-exposure`,
`experiments/geogram-repair`, and `experiments/mesh-repair`.

Private meshes, Blender scenes, downloaded dependencies and generated proof
outputs were local ignored artifacts, not guaranteed contents of Git history.
Cleanup preserves the existing `build/` artifacts and all supported production
sources, Help and tests. Schema 35 and Protocol 1.5 are unaffected.

# 23422 source-exposure certificate proof

Decision: **2 — promising evidence, but complete source-arrangement occlusion
certification remains unresolved. Do not enable production acceptance.**

## Architecture and limits

The certificate API accepts only authoritative source mesh, sample position,
BFC-derived normal, conservative source protection, and admissible source cells.
It cannot inspect the repaired mesh. Source triangles must exactly match the
earlier ancestry export before source file/line/reference identities are reused.
The prior outlier list selects the 1,086 points to investigate; their exposure
classification itself uses no repair geometry. Repair proximity is evaluated
separately, after source certificates have been computed.

Bounded workload: at most 2,000 source triangles, 1,086 outliers in this fixed
proof, 65 outward hemisphere directions per point, offsets 1e-6 and 1e-5 mm.
Potential escape directions also receive four angular perturbations of 0.005
radians at both offsets. Edge contacts conservatively count as blocked. Stable
escape is evidence of required exterior, or protected exposure for a passage,
spline, or rim. Fully blocked finite rays do **not** certify internal composition.

A sufficient internal certificate is implemented for an unprotected point strictly
inside a closed, consistently oriented, convex authoritative source component.
It requires exact rational signs of authored triangle halfspaces, with a numerical
margin before exact checking, and verifies every source vertex lies within those
halfspaces. This certifies enclosure rather than mere proximity or AABB membership.
It creates no caps. Component extraction explicitly disables trimesh's default
hole-repair behavior; exact-coordinate indexing does not proximity-weld gaps.
Protected interfaces cannot use this certificate.

23422 has six open source components, so this sufficient enclosure path does not
apply. The experiment does **not** claim to implement a complete exact intersection
arrangement or spherical occlusion-cover solver. That missing step is reported as
ambiguity, not converted into successful certificates. Source-side blocking is
strong evidence but not proof of a complete angular cover or absence of curved
escape paths through unsplit/open arrangement boundaries.

## Classification of all outliers

| Group | Certified internal | Required exterior | Protected exposed | Ambiguous exposure |
|---|---:|---:|---:|---:|
| Arm/body overlaps | 0 | 0 | 0 | 456 |
| Spline-overlapped passage walls | 0 | 0 | 0 | 630 |
| Total | 0 | 0 | 0 | 1,086 |

All 65 directions are blocked at both offsets for every outlier. There are no
stable escape witnesses. All 1,086 outward-normal first hits cross outward-facing
source geometry (material-exit relationship). Normal blockers comprise 588 spline
triangles, 382 arm/transition triangles, 52 outer-body triangles and 64 rim triangles
(counts are sample hits, not unique triangles). The 64 rim hits demonstrate why
one blocked ray across a cavity is insufficient evidence of an internal surface.

`build/source-exposure-proof/certificate.json` records every point, source ancestry,
normal blocker triangle, blocker ancestry, blocker distance, region, and decision.
The 630 passage-wall points retain conservative protection; normal/hemisphere
occlusion does not revoke it. A future exact source-fragment composition certificate
could distinguish those patches from truly exposed passage walls, but this proof
does not establish that exception.

These are **exposure ambiguities**, distinct from the previous unambiguous
containment inside the repaired solid. Zero detected escape witnesses does not
mean zero unresolved exposure. No genuinely exposed source loss was demonstrated,
but absence of all such loss has not been certified either.

## Protected source probes and geometry

Additional source-triangle centroid probes distinguish observable source regions:

- Passage wall: 22 protected/exposed centroid witnesses; 10 remain ambiguous.
- Internal spline surfaces: 70 protected/exposed centroid witnesses.
- Opening rims: 76 protected/exposed centroid witnesses.

All exposed centroid witnesses remain within 0.20 mm of the candidate. The previous
full source lattice also has no outliers on authored spline and opening-rim faces.
This is sampled preservation, not an exact continuous exposed-fragment guarantee.
The existing semantic-builder proof emits no successful fit-feature contract for
23422; no new fit ownership is assigned to either source labels or Blender faces.

The geometry comparison was rerun without scaling/rotation. Central passage remains
open on the established probes; minimum spline-tip radial clearance diameter is
3.7199874 mm source versus 3.7199872 mm repair. Above the splines, cardinal bore ID
is 5.6000 versus 5.5683 mm; cardinal body OD is 8.0000 versus 7.9695 mm. Both arms
remain. Overall bounds are 24.972664 x 7.969524 x 8 mm versus source
25.107265 x 8 x 8 mm. Do not infer a uniform 4.80 mm bore.

The unchanged C++ validator was rerun: one component, zero boundaries,
non-manifold edges/vertices, intersections, and degenerates; repaired exterior
to source 0.0918 mm. Source to repair still reaches 1.2343 mm and rejects.
The revised proof also rejects because no outlier obtained a source-only internal
certificate. No override was installed. Nominal-only status does not bypass fidelity.

## Negative controls and validation

Nine focused tests pass. They include a positive exact closed-source enclosure,
an exposed exterior witness, an exposed protected passage witness, deleted wall,
filled bore, disappearing passage wall, shortened arm, source outside repair,
ambiguous surface buried by repair, protected-surface no-exemption, and no source
hole-filling regression (some tests contain multiple assertions). Existing asymmetric
geometry gates are reused for the repair rejection assertions; the new source-only
certificate is tested independently. No production acceptance hook is added.

Qt 6.10.3 MinGW Release BrickSuite and focused targets build successfully.
All 109 configured CTest tests pass (68.69 seconds). `git diff --check` passes.
Schema 35 and Protocol 1.5 are unchanged. Production behavior, normal Prepare,
ManufacturingMesh ownership, topology validation, and 0.20 mm tolerance are unchanged.
Pre-existing changes preserved; no unrelated edits, commit, or push.

## Reproduction and next step

Use the existing isolated mesh-repair Python dependencies via PYTHONPATH and its
ignored MPLCONFIGDIR. Run `test_certificate.py`, then `run.py`. No new dependencies
or root CMake changes are required. Inputs are the previous source, ancestry and
outlier artifacts. Read `certificate.json` for individual evidence.

The narrow next step is an exact source intersection arrangement with fragment-level
material-side and exposure connectivity certificates, especially around the three
splines. Open components do not prove certification impossible; they mean the
closed-source-component sufficient certificate used here cannot establish it.
Do not adopt finite ray coverage or repaired-volume containment as a replacement.

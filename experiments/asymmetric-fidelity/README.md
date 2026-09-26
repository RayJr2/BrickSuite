# Local override asymmetric-fidelity proof

This is a standalone diagnostic experiment. No production code, normal Prepare,
ManufacturingMesh ownership, topology rules, or 0.20 mm threshold changed.

## Method and reproducibility

Use the existing isolated dependencies in `build/mesh-repair-proof/python` on
PYTHONPATH. Run `test_proof.py`, then `proof.py`, then `finalize.py`. Set MPLCONFIGDIR
to the existing ignored mesh-repair matplotlib directory. Outputs go to
`build/asymmetric-fidelity-proof`. The script consumes the previous Blender proof's
unscaled source/candidate/Bambu OFF extractions, and asserts exact triangle-array
equality with the earlier source mesh before using its saved reference ancestry.
The source is 438 triangles; candidate is the existing 23422-blender-repaired.stl.

Run the existing BlenderOverrideProof harness on that STL first: the experiment's
`valid_topology=True` for this fixed candidate depends on the unchanged C++ validator
confirming one outward-oriented, closed manifold, no intersections or degenerates.
This flag is not a general-purpose topology detector. For Bambu it is false; its
failed topology cannot authorize any internal allowance. Synthetic tests exercise
known constructed solids and explicit failed-topology controls.

The isolated CMake target builds `features.cpp` against unchanged Release service
objects to inspect the native semantic builder. It does not register a product route.

Distance sampling reproduces the current 0.50 mm maximum-edge barycentric lattice
on every triangle. Only translation-to-source-bounds-center is applied, as in the
existing validator. Repaired-to-source maximum remains 0.0917983 mm. For every
source sample farther than 0.20 mm, the proof requires double-precision oriented
solid-angle winding within 1e-7 of one AND agreement of five non-axis-aligned ray
parities. Zero winding plus five outside votes means outside. Disagreement is
ambiguous and rejects. AABB inclusion is never sufficient. This is conservative
numerical sample classification, not exact-arithmetic certification of entire faces.
See [Jacobson et al.'s winding-number method](https://igl.ethz.ch/projects/winding-number/).

An independent 0.25 mm microtriangle-centroid quadrature estimates affected area.
Each centroid carries its microtriangle's area; these sum to source area. Reported
region areas are estimates, not certified continuous partitions. Shared lattice
vertices appear once per incident triangle, matching the existing validator's
sampling rather than a unique-point count.

## 23422 findings

| Source classification | Current lattice samples | Estimated source area mm² |
|---|---:|---:|
| Within 0.20 mm of repaired surface | 22,192 | 575.908833 |
| Numerically proven inside, farther than 0.20 mm | 1,086 | 30.991844 |
| Outside, farther than 0.20 mm | 0 | 0 |
| Numerically ambiguous | 0 | 0 |
| Total | 23,278 | 606.900677 |

The internal area is approximately 5.11% of authored source area. Area quadrature
used 131,946 centroids: 6,638 internal and 125,308 near. No sampled exterior loss
beyond tolerance was found. This does not prove that every point of every triangle
is preserved, nor that an interior point is permitted to lose exposure.

All far samples are recorded in `blender-outliers.csv`, including triangle ID,
coordinate, distance, containment classification, source file/line/reference chain,
and conservative protected-region designation. `outliers.png` maps the regions.

- **340 outer-body samples** at both arm roots, approximately X=±4 mm,
  |Y|≤1.351 mm, |Z|≤1.177 mm: body/arm composition overlaps.
- **116 arm samples** at their body ends, |X|≈2.7–3.647 mm, |Y|≤1.045 mm,
  |Z|≤1.6 mm: the complementary composition region.
- **630 authored passage-wall samples**, |X|≤2.738 mm, Y from −2.230 to 2.8 mm,
  |Z|≤1.177 mm: inner cylindrical patches behind three authored spline/notch forms.
  Every one has a nearer source hit on a radial ray from the passage axis, at
  least 0.20 mm before that patch. This is useful evidence of source occlusion,
  but a one-direction witness is not an authoritative exemption from exposure.

No far samples lie on the 756 spline samples or 2,944 opening-rim samples. The
worst current-lattice source-to-repair distance is 1.234354 mm near an arm/body
junction. Both arms remain; candidate bounds are 24.972664 x 7.969524 x 8 mm.
The approximately 0.1346 mm overall width reduction passes the existing centered
bounds rule (roughly 0.0673 mm at each end), not an enlarged tolerance.

## Protection and acceptance decision

The installed subpart explicitly authors its inverted inner-cylinder reference
at line 23 with radius 7 LDU. Matching ancestry identifies passage-wall patches
without assigning any ownership to Blender faces. Splines and planar opening
rims are conservatively protected. No inference from filename establishes fit.
The native builder fails closure and emits zero functional features; its dedicated
RoundTechnicPassage recognizer also emits zero. No successful fit contract exists
for this source through those paths, and the experiment does not invent one.

**The conservative prototype rejects the candidate:** 630 protected wall samples
have become internal. Their required exposure is ambiguous, even though their
containment is not. Exempting all bore-wall triangles would be unsafe; exempting
only source-proven composition patches would require an additional contract or
certified source-volume/exposure analysis. Radial occlusion alone was not promoted
to that contract. The production override remains rejected and no override is stored.

Passage diagnostics:

- All **1,007 axial probes** through a radius-1.8 mm disk are clear in source and
  repair. This establishes sampled passage continuity, not full cavity equivalence.
- Midplane minimum radial-clearance diameter is 3.7199874 mm source versus
  3.7199872 mm repair across the internal splines.
- At Z=2.5 mm, cardinal bore ID is 5.600000 mm source versus 5.568345 mm repair;
  cardinal body OD is 8.000000 mm versus 7.969524 mm.
- At Z=0, cardinal X bore span is 5.600000 versus 5.442674 mm. A single uniform
  4.80 mm bore is not supported by this installed source's authored geometry;
  these distinct section measurements must not be substituted for a fit specification.

## Bambu diagnostic comparison

Same source lattice: 22,878 near samples, 400 farther samples, maximum 1.201030 mm.
Estimated near area 594.404931 mm²; unmatched area 12.495746 mm². The outliers are
316 outer-body and 84 arm samples. Its passage-wall, spline, and rim samples are
all near. Because the Bambu mesh has two components, 18 non-manifold vertices,
and 257 intersections, far samples are labeled **untrusted**, not proven inside.
No acceptance or safe volume classification is inferred from that invalid mesh.

## Controls and validation

Eight focused controls pass: unchanged solid accepted; unprotected internal sheet
allowed; same sheet protected rejected; deleted exterior wall rejected by topology;
filled annulus bore rejected by protection; shortened closed solid rejected because
source lies outside; displaced source rejected; annulus-axis point inside its AABB
correctly classified outside material; invalid volume cannot prove containment.
(The protected/unprotected sheet assertions share one test.)

Qt 6.10.3 MinGW Release BrickSuite and focused targets build successfully. The
isolated semantic harness builds and runs. All 109 configured CTest tests pass
(73.57 seconds). Existing override validation was rerun on the actual STL and
still reports the original 0.0918 / 1.2343 mm rejection. `git diff --check` passes.
Schema 35 and Protocol 1.5 unchanged. No unrelated edits, commit, or push.

Recommendation: pursue a narrowly scoped source-composition/exposure certificate
before production adoption. The blanket bidirectional rule does reject genuine
internal composition surfaces, but inside-only acceptance cannot protect filled
bores or buried interfaces. This experiment supports further development, not
enabling a general internal-surface exemption or accepting this override today.

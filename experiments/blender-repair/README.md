# Blender 23422 repair proof

Experimental only. No production code, validator thresholds, or fit ownership changed.
Blender 5.1.2 was used. `capture.py` ran in Ray's open scene before geometry edits,
saving a non-overwriting scene copy and world-space geometry snapshot. The open
scene's geometry is unchanged. `proof.py` operates in a separate factory-startup
Blender process, constructs copies, and exports raw coordinates as binary STL:
scale 1, no scene-unit conversion, no rotation.

Outputs are in `build/blender-repair-proof/` (ignored generated artifacts):

- `23422-original-scene.blend`: preserved original scene.
- `23422-blender-repaired.stl`: selected topology-improved but **rejected** candidate.
- `analysis.json`: boundary segments, component metrics, remesh trials, sampled deviations.
- `components.log`: unchanged BrickSuite analysis of each component and pair.
- `final-validation.log`: unchanged Release service validation of candidate and Bambu.
- `comparison.json`: bounds, feature probes, secondary Bambu comparison.

## Operations and results

The six loaded 23422 objects contain 272 vertices / 438 triangles. Their raw bounds
are 25.107265 x 8 x 8 mm, matching the authoritative source export. All matrices
are identity. Camera/light/default cube were excluded from the experiment.

| Object suffix | Role by geometry | Triangles | Open boundary edges | Internal intersections |
|---|---|---:|---:|---:|
| none | center body | 140 | 12 | 0 |
| .001 | arm | 110 | 16 | 0 |
| .002 | internal spline | 26 | 20 | 0 |
| .003 | internal spline | 26 | 20 | 0 |
| .004 | arm | 110 | 16 | 0 |
| .005 | internal spline | 26 | 20 | 0 |

All are open surfaces, not valid closed Boolean operands. BrickSuite finds body
cross-intersection counts 40, 6, 21, 40, 21 with those five other pieces respectively;
all other pairs have zero. Boundary endpoint coordinates are retained in analysis.json.
No component contains degenerate faces or edges with more than two incident faces.

Exact-position weld (`bmesh.ops.remove_doubles`, distance 0) and normal recalculation
do not close any boundaries: still six components, 104 boundaries, 128 intersections.
No near-gap weld, invented caps, decimation, or arbitrary scaling was applied.
Ordinary exact Boolean union was ineligible because all six components are open.

Remesh trials ran on copies only:

- SHARP, octree depth 8, disconnected removal disabled: non-manifold output,
  three components, 286 non-manifold edges, and over 100,000 exported triangles.
- VOXEL 0.05 mm, adaptivity 0: topologically closed in Blender but over the existing
  100,000-triangle import limit (589,104 triangles). Not decimated to force import.
- VOXEL 0.15 mm, adaptivity 0: 32,328 vertices / 64,656 triangles. Chosen as the
  inspectable, within-budget candidate. This coarser trial tests the fidelity tradeoff;
  it does not change acceptance tolerances. No smoothing or post-remesh simplification.

## Existing BrickSuite validation

The standalone Release harness links already-built, unchanged production service
objects and stores any trial override only in a QTemporaryDir. The same candidate
was also imported using **Import Repaired Mesh** in Ray's open Debug viewer.
Both report the same rejection:

> Source fidelity rejected: repair → Source 0.0918 mm; Source → repair 1.2343 mm; limit 0.2 mm.

Final candidate: one component, zero boundaries, zero non-manifold edges/vertices,
zero self-intersections, zero degenerates; topology/orientation and bounds gates
passed before source fidelity failed. Bounds are **24.972664 x 7.969524 x 8 mm**.
No Local Printable Override was accepted or persisted by the viewer. The viewer
continues to display authoritative Source and explicitly reports no validated
Prepared Mesh/ManufacturingMesh export. No fit ownership was inferred.

Independent barycentric-grid probes (sampled maxima, not certified Hausdorff bounds):

| Direction | Max mm | p95 mm | RMS mm |
|---|---:|---:|---:|
| Blender → source | 0.093398 | 0.018451 | 0.009367 |
| Source → Blender | 1.252686 | 0.084744 | 0.089322 |
| Blender → Bambu | 0.085675 | 0.018080 | 0.009310 |
| Bambu → Blender | 0.940127 | 0.192703 | 0.106464 |

The first two use 15 barycentric points per triangle; Bambu comparisons use 5,000
fixed-seed area samples. They are supplemental measurements, not substitutions
for BrickSuite's unchanged 0.5 mm sampling lattice. The worst source probe lies
near (-4,0,0) mm at an arm/body junction: remeshing removes intersecting/internal
source surfaces. The strict validator does not exempt these surfaces.

Ray's current Bambu file remains at 362 vertices / 772 triangles, two components,
18 non-manifold vertices, 257 intersections (233 internal + 24 cross-component),
zero boundary/non-manifold edges/degenerates, and bounds 25.105999 x 7.998 x 7.998 mm.
It still rejects before fidelity checks. Blender materially improves topology but
does **not** produce acceptable source fidelity or exactly preserved dimensions.

Five axial probes remain unobstructed in source, Bambu, and Blender. At Z=2.5 mm,
cardinal body OD changes from 8.000 to about 7.9695 mm; wall thickness remains
about 1.2006 mm versus 1.2000 mm. Both arms remain, with overall width shortening
about 0.1346 mm. A claim that a uniform 4.80 mm bore was preserved is not supported
by this source: above the splines the cardinal inner diameter is 5.60 mm, while
midplane minimum radial clearance across the three splines is about 3.71999 mm
in source and Blender (3.718 mm in current Bambu). These are distinct probes, not
an inferred replacement bore specification. Intentional-opening/fit preservation
is not certified merely because the central axial rays remain open.

## Reproduction

Run capture.py from Blender's Python console in the original loaded scene once.
Then run the installed Blender executable with `--background --factory-startup
--python experiments/blender-repair/proof.py`. Configure this directory's isolated
CMake project with the same Qt/MinGW kit as the existing Release build, build, and
run BlenderOverrideProof with candidate file paths. It reuses the current Release
objects; it is a Windows-host proof harness, not cross-platform product integration.
Run compare.py with the existing mesh-repair proof's isolated Python dependencies.

The experiment harness built successfully; service and actual viewer rejection
were verified; `git diff --check` passed. Production application rebuild/full CTest
were not repeated because no production or root build files changed in this task.
Schema 35 / Protocol 1.5 unchanged. No commit or push.

Conclusion: this bounded Blender proof yields a manifold diagnostic candidate,
not an accepted repair. Further work would require source-aware reconstruction
and feature review; do not relax fidelity checks to accept this STL.

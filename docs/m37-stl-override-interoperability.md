# M37 Local Printable Override STL interoperability

## Scope and architecture

Import Repaired Mesh accepts `.3mf` and `.stl` (case-insensitive suffix). STL is
decoded by `LocalPrintableOverrideStl.cpp` into the same `PrintMesh` used by the
existing 3MF importer. No dependency or preparation route was added.

The existing lib3mf STL reader defaults to ignoring invalid faces and uses a
vertex-merging tree. It is therefore not used for strict override extraction.
The dedicated reader accepts binary STL and a single complete ASCII solid block.
Binary classification checks the exact 84 + 50 * triangle-count length, including
files whose binary header begins with `solid`. ASCII validates the complete facet
grammar, exactly three vertices per facet, closing solid, and absence of trailing
records. Numeric fields must be finite. Malformed/truncated input fails as a whole;
the output mesh is assigned only after successful complete parsing.

Only identical vertex coordinates are shared; there is no proximity welding.
Every triangle is retained, including duplicate, collapsed, and degenerate faces,
for rejection by the common validator. Facet normals do not override winding.
Limits are 64 MiB input, 100,000 triangles, 300,000 vertices, and coordinate
magnitudes at most 100,000 millimeters. Multiple ASCII solid blocks are unsupported
and rejected explicitly. Binary attribute/color words are ignored as nongeometry.

## Units, validation, and persistence

The picker and import diagnostic explicitly say **STL interpreted as millimeters**.
No unit guessing or automatic rescaling occurs. The existing bounds comparison
(0.10 mm), bidirectional sampled source-fidelity checks (0.20 mm distance threshold),
topology, orientation, degeneracy, and intersection validation are unchanged.
Existing placement-translation removal is unchanged; rotation is not fitted.

Both formats enter the same guarded closed-component normalization path. It still
requires independently valid components, detected contact, at most two components,
one Boolean operation, 2,000 input triangles, a 10-second worker deadline, and a
512 MiB worker memory limit. The result must pass the full override validation.
No fit ownership is assigned; accepted overrides remain nominal only.

The existing atomic version-1 override record adds optional `importFormat: stl`
and `interpretedUnits: millimeter` fields for STL imports. Old 3MF records remain
compatible. Reload revalidates geometry and the source fingerprint. Rejection
preserves any existing valid record. No catalog, aliases, mappings, or Source
files are changed.

## Regression evidence

Focused coverage includes binary STL with a misleading `solid` header, ASCII STL,
millimeter bounds, exact vertex sharing, degenerate-face retention, repeated-vertex
facets, open/non-manifold/intersecting geometry, source-fidelity failure, scale
mismatch, malformed/truncated/trailing records, nonfinite coordinates, failure
without partial output, preservation of prior overrides, persistence/replacement/
removal, and successful guarded union from STL. Existing 3MF fixtures remain in
the same test executable, including Bambu metadata compatibility and worker timeout.

The supplied Bambu 23422 reference still loads and is rejected: 362 vertices,
772 faces, two components, zero boundaries/non-manifold edges, 18 non-manifold
vertices, and 257 intersecting pairs (233 internal, 24 cross-component). Its first
component is invalid, so no union is attempted. Native 23422 remains Not Ready;
the installed 3001 control remains Ready. No repaired Blender STL has yet been
provided, so acceptance of Ray's future repair remains a runtime user check.

## Manual Blender test

1. Open catalog Part 23422. Export for External Repair still produces the nominal
   millimeter 3MF; use that with a configured 3MF importer if available.
2. For a direct STL transport, set the viewer Scale to 100% and reset Print
   Orientation. Choose Export 3D Model, Source Mesh, STL. This ordinary export
   respects the viewer scale/orientation, unlike Export for External Repair.
3. In an empty Blender scene, choose Unit System None and import that STL with
   Scale 1, Scene Unit off where available, Forward Y, Up Z. Raw coordinate units
   represent millimeters in this workflow. Confirm dimensions approximately
   25.1073 x 8 x 8 before repairing. Keep dimensions and orientation unchanged.
4. Export only the repaired object as binary STL, Scale 1, Scene Unit off,
   Forward Y, Up Z. Include evaluated repair modifiers if used. ASCII also works
   when exported as one solid block. Do not use an arbitrary scale correction.
5. Back in the catalog viewer, choose Import Repaired Mesh and select the STL.
   Read the millimeter notice and exact acceptance/rejection diagnostic. A valid
   mesh shows Ready — Local Repaired Override (Nominal). Compare Source and
   Prepared views, export the nominal Prepared Mesh, and reopen to check persistence.
   A rejection preserves Source and any prior accepted override.

Blender documents its [STL scale, scene-unit, axis, and selection controls](https://docs.blender.org/manual/id/5.1/files/import_export/stl.html).
This task adds no Blender-specific code or automatic repair operation.

## Validation

Qt 6.10.3 MinGW Release application and focused test targets built successfully.
All 109 configured Release CTest tests passed (68.97 seconds), including
LocalPrintableOverrideService (10.88 seconds) and ExternalLDrawViewer (0.41 seconds).
The installed-library/Bambu reference proof also completed with the findings above.
`git diff --check` passed. Schema remains 35; Protocol remains 1.5. Pre-existing
working-tree changes were preserved; no unrelated changes, commit, or push were made.
No Debug tests were built. macOS/Linux runtime and Ray's interactive Blender repair
workflow were not exercised on this Windows host.

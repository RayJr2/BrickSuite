# M37 Local Printable Override proof

This is an explicit, local, nominal override workflow, not an automatic repair
route. Native preparation, authoritative LDraw, catalog mappings, and batch
printing are unchanged. No new library or schema/protocol change is required.

## Architecture and storage

`LocalPrintableOverrideService` is independent of widgets and repositories.
Its context requires an internal Part ID, Part number, and successfully loaded
catalog LDraw model. Ad-hoc external files cannot acquire an override.

The viewer exports, imports, and loads/validates overrides on workers. Completion
is guarded by the existing viewer generation and lifetime checks. A rejected
import leaves the currently displayed PreparedMesh and existing stored override
intact. Closing a viewer does not give a worker access to a destroyed widget;
an already requested successful import may finish storing the original Part's
override. No catalog identity is inferred from the imported filename or metadata.

Storage uses `QStandardPaths::AppLocalDataLocation/printable-overrides`, with one
`<internal-part-id>-<sha256-resolved-ldraw-id>.json` file per identity. Version 1
contains `version`, `partId`, `partNumber`, `ldrawIdentity`, `sourceFingerprint`,
`meshFingerprint`, `vertices`, and `faces`. One `QSaveFile` commit atomically
replaces the entire validated record; there is no mesh/manifest split transaction.

The source SHA-256 binds canonical loaded geometry, Part/model identity, source
file records, reference hierarchy/transforms, and per-surface ancestry. It does
not rely on modification timestamps. Semantically irrelevant raw-file comments
need not invalidate unchanged loaded geometry/ancestry. The mesh SHA-256 binds
ordered double-precision vertices and indices. Reload checks identity/version,
both fingerprints, and revalidates topology/fidelity. Changed source is diagnosed
as stale and never applied. Removal also works for stale records. Replace uses
the same validation and atomic-write path as first import.

## Source export and import

Repair export uses the existing 3MF writer, declared millimeters, and canonical
`(0.4*x, 0.4*z, -0.4*y)` coordinates. Every triangle in the loaded authoritative
source is exported without preparation, welding, or triangle deduplication.
Viewer Scale and Print Orientation are deliberately excluded from this operation.
The object name and suggested filename label it an unvalidated repair source.

Import uses the existing pinned lib3mf dependency. It requires one build item,
flattens explicit component/build transforms, and requires declared millimeters.
Optional vendor metadata/attribute warnings are permitted; other reader warnings
and geometry errors reject import. Non-mesh and beam-lattice geometry is rejected.
Slicer placement is removed by translating the mesh's bounds center to the source
center. There is no inferred scale, unit conversion, rotation fitting, welding,
orientation repair, or other hidden mesh repair during import.

## Validation policy v1

- Existing `analyzeSource` / `validatePreparedMesh`: one component, finite valid
  indices, no degenerate/duplicate faces, no boundaries, no non-manifold edges or
  vertices, no detected self-intersections, consistent orientation, positive volume.
- After placement translation, each bound must be within **0.10 mm** of source.
- Bidirectional nearest-surface checks use the existing spatial query index.
  Every face receives a deterministic barycentric lattice, including its edges
  and interior, with maximum lattice edge spacing **0.50 mm**. Sampled maximum
  deviation must be at most **0.20 mm** in each direction.
- Imports are capped at 64 MiB on disk, 100,000 faces, 300,000 vertices, hierarchy
  depth 16, and 256 visited objects. Fidelity permits at most 1,000,000 samples
  and 20,000,000 exact triangle queries per direction. Topology uses the existing
  20,000,000 candidate-pair cap. Exceeding limits rejects rather than thinning
  the sample or returning an unchecked result. lib3mf parses the package before
  mesh-count limits are checked; these limits are not a process memory quota.

These are fixed conservative proof policies, not tolerances optimized for a
particular repair. Sampled fidelity is a gross-alteration safeguard, not a
continuous Hausdorff proof, wall-thickness certificate, or LEGO fit guarantee.

## PreparedMesh and fit

An accepted override is explicitly marked `localRepairedOverride` and becomes
the viewer's nominal PreparedMesh. Source remains selectable. The normal OBJ,
STL, and 3MF export path applies the user's chosen Scale/Print Orientation.
3MF/OBJ labels identify the external repair provenance. No functional-feature
ownership is synthesized. The viewer disables Auto Fit/profile ManufacturingMesh
for overrides, and `ManufacturingMeshService` independently rejects them.
Native preparation remains the normal path when no valid override exists.

## Validation and primary proof

Qt 6.10.3 MinGW Release application and configured Release test targets built.
The final full configured CTest run passed **109/109** in **58.51 seconds**.
`git diff --check` passed; new files were also checked for whitespace errors.
Schema remains **35**, Protocol remains **1.5**. No Debug tests were compiled.
Existing research changes were preserved; no commit or push was performed.
Focused synthetic tests cover valid import/reload, nominal 3MF export/reopen,
atomic preservation on rejection, open/non-manifold/degenerate/intersecting meshes,
multiple solids, inward winding, altered surfaces with matching outer bounds,
2.5x scale and non-mm rejection, translation-only placement, stale ancestry and
geometry, identity isolation, tampering, and removal. Viewer tests cover saved
override activation, return to another catalog Part, reload, and removal.

Installed-library proof:

| Case | Observed result |
| --- | --- |
| 23422 native | Still fails: single-loop closure group 1 / incomplete source coverage |
| 23422 repair-source 3MF | 438 triangles; 25.107265 × 8.000000 × 8.000000 mm; reopened successfully |
| Old Studio-scaled Bambu 23422 | Reader succeeds; strict checker rejects 141 self-intersections, despite one component and zero boundary/non-manifold edges; no rescaling attempted |
| 3001 native | Ready: independently validated nominal PreparedMesh |

The generated local repair source is
`build/local-printable-override-proof/23422-unvalidated-repair-source-mm.3mf`.
It is ignored build output, not a checked-in authoritative fixture.

**Accepted 23422 remains pending Ray's external repair of this new millimeter
source.** The older visually verified Bambu result cannot be claimed as passing
BrickSuite's strict checks. Synthetic acceptance proves the storage, viewer, and
export path, not preservation of 23422's bore/arms/walls. No checks were relaxed
to manufacture that acceptance. macOS/Linux builds and interactive repair-tool
acceptance have not been exercised on this Windows host.

## Ray's runtime acceptance steps

1. Open catalog Part **23422** in its 3D Model Viewer; run native Prepare and
   confirm it remains Not Ready when no local override is present.
2. Choose **Export for External Repair...** and save the new millimeter 3MF.
3. Repair that file externally. Preserve scale and orientation, the current
   source bore, both arms, body, intentional openings, and wall thickness.
   Save one repaired solid as millimeter 3MF. Do not substitute the old
   Studio-scaled reference. Slicer bed translation is acceptable.
4. Choose **Import Repaired Mesh...**. If rejected, retain the diagnostic and
   repaired file for inspection; a rejection is not nominal-print acceptance.
5. If accepted, confirm **Ready — Local Repaired Override (Nominal)**. Compare
   Source and Local PreparedMesh Override views and physical dimensions.
6. Close and reopen the Part viewer; confirm the same override revalidates.
7. Set Scale to **100%** and use **Export 3D Model... → Prepared Mesh → 3MF**.
   Reopen it in the slicer and inspect the bore, arms, walls, openings, and bounds.
   Auto Fit remains unavailable for this nominal override.
8. Test **Remove Local Override**, verify native behavior returns, then re-import
   if desired. Open a normal Part such as 3001 to verify its native workflow.

## Bambu metadata compatibility follow-up

The newly supplied `23422-unvalidated-repair-source-mm.3mf` has SHA-256
`3f94e5a00169d78fbac3a274092cfd596758c9cdf6393e1c2b06ed7269813f4b`.
This is Ray's repair of the millimeter export, not the older Studio-scaled file.

Root cause: pinned lib3mf's `CModelReaderNode_ModelBase::ReadMetaDataNode`
splits metadata names such as `BambuStudio:3mfVersion` and
`customXMLNS0:Part`. When `GetNamespaceURI` cannot resolve their prefixes,
it emits `NMR_ERROR_METADATA_COULDNOTGETNAMESPACE` (`0x80AE`) at
`mrwInvalidOptionalValue` severity, then uses the prefix as the metadata
namespace. Non-strict lib3mf parsing continues with readable mesh objects.
BrickSuite's post-read warning whitelist previously promoted that warning to
an import failure. It now tolerates that specific optional metadata warning,
alongside the existing optional metadata/attribute allowances. Other reader
warnings, invalid geometry and required-extension failures are still rejected.
The file is neither rewritten nor sanitized. Standard metadata remains in
lib3mf's parsed model; a synthetic test verifies the Application value.

The actual file now loads, then fails the unchanged mesh validation:

| Measurement | Result |
| --- | ---: |
| Vertices / triangles | 362 / 772 |
| Connected components | **2** (requires 1) |
| Boundary edges | 0 |
| Non-manifold edges | 0 |
| Non-manifold vertices | **18** |
| Degenerate / duplicate faces | 0 / 0 |
| Detected self-intersecting triangle pairs | **257** |
| Consistent orientation | Yes |
| Signed volume | 338.722696 mm³ |
| Bounds dimensions | 25.105999 × 7.998000 × 7.998000 mm |
| Bounds compatibility / source fidelity | Not run: topology rejected first |

No topology predicate, tolerance, ordering or fidelity criterion changed.
Diagnostics now distinguish container failure from post-load validation failure,
include all topology counts, and explicitly identify skipped checks. The viewer
explains unavailable export choices without replacing an existing PreparedMesh.
Its issue overlay continues to represent authoritative Source, not a rejected
import. Native 23422 still fails and native 3001 still succeeds. Acceptance of
23422 remains unproven; no claim of repaired bore/arm fidelity follows from this
container compatibility fix.

Regression fixtures are synthetic stored ZIP/3MF packages with unbound metadata
prefixes: a valid cube still imports and persists, an open cube reaches geometry
rejection, and invalid indices remain a container failure. Both rejection paths
preserve the previous valid override. Input file bytes and Source fingerprints
remain unchanged. The supplied file was also checked byte-for-byte after import.

### STL interoperability recommendation

STL import would be useful for external tools including Blender/Meshmixer, but
is not a trivial format-selector change. BrickSuite currently has an STL writer,
not a validated STL-import adapter. Pinned lib3mf does include an STL reader;
inspection shows a binary-facet path, vertex identification through a vector
tree, and `setIgnoreInvalidFaces(true)` defaults. Reusing it blindly could drop
bad faces before BrickSuite validates them. STL also lacks declared physical
units. A separate bounded adapter should require explicit millimeters, define
and test vertex identity/welding behavior, reject malformed/degenerate input
without silent repair, and run the exact existing topology/fidelity validator.
It should cover binary/ASCII format handling explicitly. No STL import was added
in this task; use repaired millimeter 3MF through the corrected importer for now.

Follow-up validation: Qt 6.10.3 MinGW Release build passed; focused override and
viewer tests passed within the full **109/109 CTest** run (**59.07 seconds**).
`git diff --check` passed. Schema 35 and Protocol 1.5 remain unchanged. Existing
uncommitted work is preserved, with no unrelated edits, commit, or push.

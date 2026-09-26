# M37 user-reviewed nominal overrides

## State and acceptance contract

Native PreparedMesh and strictly validated local overrides retain their existing
geometry rules. A separate review candidate is not Ready and is not persisted.
Only explicit confirmation produces **User-Accepted Local Override (Nominal)**.
This is user-reviewed geometry, not an automatic source-exposure certificate.

The new review gate runs only after normal topology, orientation and bounds
validation. It requires one component, no boundaries, non-manifold edges/vertices,
intersections, duplicate or degenerate faces, positive orientation, and at most
0.10 mm deviation at any centered bounds endpoint. Scale and rotation are never
fitted. The existing 0.50 mm face lattice must measure repair-to-source distance
at most 0.20 mm. Strict automatic acceptance still requires the reverse distance
at most 0.20 mm too.

For review, the source must have open/overlapping composition. Every reverse
outlier must pass oriented solid-angle containment and three independent odd
ray-parity tests inside the valid repaired solid. Its first source-normal blocker
must be a material exit; an opposing void wall cannot excuse a filled cavity.
Every one of 65 outward hemisphere directions must be blocked by source geometry
at both 1e-6 and 1e-5 mm offsets. Any escape, outside sample, containment disagreement,
or recognized operand fit feature denies review. Work is bounded to 2,000 source
faces, one million samples, 500 million primitive tests and 30 seconds for the
additional gate. Exceeding a limit rejects review.

These sampled checks reject demonstrated exterior/opening loss. Finite blocked
rays do **not** certify complete source exposure or prove all continuous surfaces
preserved. The remaining ambiguity is precisely what requires user review.
No tolerance was increased for 23422 and no repaired fit ownership is assigned.

## UI and persistence

After importing a reviewable 3MF/STL, **Accept as Nominal Local Override...** explains
printable-solid validation, unresolved source correspondence, user responsibility
for geometry review, nominal printing and experimental Auto Fit. Cancel is the
default. Confirmation re-reads the import and validates the reviewed source and
canonical repaired-mesh SHA-256 fingerprints before atomically replacing storage.
Rejected or changed candidates preserve the prior valid override.

The existing application-local `printable-overrides/<partId>-<ldrawHash>.json`
record retains Part ID/number, resolved LDraw identity, source geometry and ancestry
fingerprint, mesh fingerprint and geometry. User records additionally contain:

```json
{
  "acceptanceKind": "user-reviewed-nominal",
  "acceptanceVersion": 1,
  "validationVersion": 1,
  "explicitlyConfirmed": true
}
```

Reopening verifies identities, versions and fingerprints and revalidates geometry.
Stale records cannot become Ready. Remove/replace work as before. Source/catalog,
aliases and mappings are never written. Native, strict and user acceptance expose
`native_success`, `strict_override_success`, and `user_override_success` identities
for later audit integration; the audit UI is unchanged.

Nominal 3MF object names, OBJ geometry labels and binary STL headers identify
User-Accepted Local Override provenance. Ordinary viewer orientation and Scale
still apply to exports; use 100% and reset orientation for nominal comparisons.

## Experimental Auto Fit limitation

Each deliberate attempt displays the requested incorrect-region/omitted-region/
inaccurate-fit warning with Cancel and Apply Experimental Auto Fit. Existing managed
Verified profiles are read, never changed. The service inspects source operand
features and compatible corrections. It cannot establish a proven mapping onto
repaired triangles, so this implementation fails safely before transformation.
It does not clear the local-override guard or invoke native regeneration as though
the repair had source ownership. **No repaired-mesh compensation or successful
experimental ManufacturingMesh export is implemented.**

The result tooltip holds an attempt record: `experimental_autofit_failed`, composite
override identity, resolved profile ID (empty if none), source features, applied
corrections (empty), warnings, profile-resolution diagnostic and
`not_run_no_transformation` final validation. Failed attempts are transient viewer
diagnostics, not changes to the stored nominal mesh. No transformed output means
no transformed-output validation or success is claimed.

## Supplied 23422 proof

The supplied Blender STL was tested against installed LDraw 23422 using the actual
service and isolated temporary override storage. Test confirmation is not Ray's
hands-on acceptance and does not install an override in his live application data.

| Measurement | Result |
|---|---:|
| Vertices / triangles | 32,328 / 64,656 |
| Components | 1 |
| Boundary / non-manifold edges / non-manifold vertices | 0 / 0 / 0 |
| Self-intersections / degenerates | 0 / 0 |
| Bounds, mm | 24.972664 × 7.969524 × 8.000000 |
| Source bounds, mm | 25.107265 × 8 × 8 |
| Maximum centered bounds endpoint deviation, mm | 0.067300 |
| Sampled repair → source maximum, mm | 0.091798 |
| Sampled source → repair maximum, mm | 1.234344 |
| Unresolved source samples inside repair | 1,086 |

Strict automatic acceptance still fails reverse correspondence. The mesh becomes
reviewable, explicit test approval persists through service restart, and nominal
3MF export reopens as a valid solid. Source fingerprint stays unchanged.

Experimental fit finds zero operand fit features; source construction still reports
group 1 closure failure. Zero corrections are applied and nominal remains usable.
No fit result is available for Bambu inspection; the nominal export can be inspected.

The supplied Bambu file remains rejected: two components, 18 non-manifold vertices,
257 intersection pairs (233 internal and 24 cross-component). The invalid first
component prevents union. Native 23422 still fails; native 3001 still succeeds.

## Validation and hands-on test

Focused service/UI tests cover strict preservation, no implicit approval, Cancel,
explicit confirmation, source/mesh/version staleness, persistence and removal,
experimental warning/failure, and no state leakage across catalog sessions.
Negative controls reject open/non-manifold/intersecting/degenerate meshes, bounds
mismatch, altered exterior, shortened geometry, filled through-bore, filled enclosed
void and demonstrated source-outside-repair. Qt tests use synthetic fixtures;
the optional installed-library proof uses the supplied Blender and Bambu files.

Qt 6.10.3 MinGW Release application and all configured test targets build. All
109 CTest tests pass (70.56 seconds), including the focused service/UI cases.
The supplied-file proof exits successfully; `git diff --check` passes. No Debug
test build was performed. Automated UI confirmation uses synthetic data; Ray's
live workflow and visual acceptance remain separate.

For Ray:

1. Launch the rebuilt Release BrickSuite and open catalog Part 23422's viewer.
2. Import `build/blender-repair-proof/23422-blender-repaired.stl` from the repository.
   Check the millimeter message and Reviewable status.
3. Review the candidate's bore, splines, walls, arms and openings in Blender/Bambu.
   Choose Accept as Nominal Local Override, then explicitly confirm.
4. Confirm User-Accepted Local Override (Nominal), switch Source/override views,
   close/reopen the viewer and verify the status persists.
5. Set Scale 100%, reset Print Orientation, export Prepared Mesh as 3MF and reopen
   it in Bambu. The 3MF object name identifies user acceptance.
6. Choose Experimental Auto Fit; first Cancel, then try again and Apply. Expect
   safe failure with zero corrections and nominal Ready retained.
7. Import the earlier Bambu repair to verify rejection preserves the accepted
   Blender override. Remove Local Override to return to native preparation.

Schema 35 and Protocol 1.5 are unchanged. No commit or push.

## Export/profile clarity follow-up

Nominal PreparedMesh export now says “Not applicable — nominal PreparedMesh
export” in the Fit Profile field. Local overrides separately list available
compatible Verified profile names. Source export also explicitly says the profile
is not applicable. Native ManufacturingMesh profile filtering remains unchanged.

The experimental warning now offers explicit selection from managed compatible
Verified profiles, independently of whether any correction maps to this repair.
The warning and ownership rejection remain unchanged. Attempt diagnostics and JSON
include selected profile name/ID, recognized source semantics, zero mapped repair
features, considered correction entries, applied entries and unmapped/unsupported
entries. Availability must not be confused with applicability.

Read-only testing with the installed Bambu H2D / PETG / 0.40 mm / LEGO Fit profile
found 23 correction entries, zero recognized operand fit semantics, zero mapped
repaired features and zero applied corrections for 23422. The nominal override
still persists and exports/reopens. No calibration/profile data was written.

The runtime report of successful experimental export cannot be reproduced by this
checkout: its experimental action has no export path and deliberately fails before
transformation. No nominal mesh is relabeled as experimental output in this UI-only
follow-up. The executable/export paths were requested to reconcile that difference;
Bambu reopening of a new experimental file remains unverified.

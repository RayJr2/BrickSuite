# M37 closed-component union proof

## 23422 conclusion

The two edge-connected components of Ray's repaired millimeter 3MF are **not
both valid closed solids**. No union was attempted on this file. It remains
rejected as a nominal local override; no validation rule was bypassed.

The tested file is `23422-unvalidated-repair-source-mm.3mf`, SHA-256
`3f94e5a00169d78fbac3a274092cfd596758c9cdf6393e1c2b06ed7269813f4b`.
Component order is deterministic, starting with the first unvisited source face.
Splitting uses the same edge adjacency as `analyzeSource`; vertex-only contact
does not merge disconnected face fans. No vertices are welded or faces repaired.

| Independent check | Component 1 | Component 2 |
| --- | ---: | ---: |
| Vertices | 340 | 24 |
| Triangles | 728 | 44 |
| Boundary edges | 0 | 0 |
| Non-manifold edges | 0 | 0 |
| Non-manifold vertices | **16** | 0 |
| Degenerate / duplicate faces | 0 / 0 | 0 / 0 |
| Detected self-intersecting triangle pairs | **233** | 0 |
| Consistently oriented | Yes | Yes |
| Signed volume, mm³ | 336.673077 | 2.04961828 |
| `validateBooleanOperand` | **Reject** | Pass |

The original 257 intersecting triangle pairs comprise **233 within component 1**
and **24 between components**. The components share original vertex indices
85 and 86 (zero-based), accounting for the two additional whole-mesh
non-manifold-vertex findings (18 overall versus 16 internal). Thus the defects
are not solely relationships between otherwise-valid solids.

Independent component analysis took approximately **5 ms** on this Windows run.
Boolean operations on 23422: **zero**. Post-union topology and source-fidelity
metrics: **not applicable / not run**. Bounds remain 25.105999 × 7.998000 ×
7.998000 mm; matching dimensions do not establish valid topology or fidelity.
Native 23422 preparation still fails; native 3001 preparation still succeeds.

## Guarded import normalization

The existing override importer retains its unchanged first validation. A rejected
multi-component mesh can enter a narrowly bounded normalization path:

1. Split by edge connectivity and validate each component independently with
   the existing solid validator.
2. Require exactly two valid components, intersecting/touching bounds without
   a positive gap, and detected cross-component surface intersection/contact.
   Mere proximity, disjoint solids and containment without demonstrated surface
   contact do not qualify. An invalid individual component aborts before MCUT.
3. Run one union through the existing `McutMeshBooleanService`, in an isolated
   child of the current executable. MCUT's existing general-position behavior
   is unchanged; this does not introduce a new arbitrary-precision Boolean
   backend or claim exact arithmetic guarantees beyond that implementation.
4. Independently validate the returned topology in the parent, then rerun the
   complete Local Printable Override validator, including bounds compatibility
   and bidirectional sampled source fidelity.
5. Atomically persist only an accepted nominal result. Every failure leaves a
   previous override intact. No caps, bridges, repairs of individual components,
   retry loops or disconnected final outputs are accepted.

The worker enters before QApplication, single-instance locking, settings,
database or normal GUI startup. It has no catalog access. It uses a private
temporary binary input/output protocol and existing packaged application/MCUT
dependencies, avoiding an extra deployable executable.

Limits: **2 components, 1 operation, 2,000 input faces, 6,000 input vertices,
10,000 output faces, 30,000 output vertices, 10-second worker deadline,
512 MiB worker memory cap**. A timeout kills the worker and permits up to three
seconds for process teardown; no result is accepted afterward. Windows uses a
per-process Job Object memory limit. Unix uses `RLIMIT_AS` (virtual address
space); failure to establish a limit rejects before Boolean dispatch. Existing
topology-analysis pair budgets still apply. macOS/Linux runtime behavior was
not exercised on this Windows host and may conservatively reject workers if
their loaded libraries already need more address space than the limit.

Accepted records include an optional `normalization` object with method
`closed-component-union-v1`, backend version, input component/triangle count,
operation/output triangle counts, elapsed time, deadline and memory limit.
Reload validates the persisted final mesh again and identifies union provenance.
Existing records without this optional object remain compatible. Repaired and
union-generated faces acquire no LDraw ownership; Auto Fit remains unavailable.

## Regression and build evidence

- Already-valid single solid: accepted without union provenance.
- Two overlapping valid boxes: union accepted, full source validation passed,
  nominal override stored/reloaded with normalization metadata.
- Two separated solids and a positive-gap pair: rejected without bridging.
- Three components and excessive triangle workload: rejected by limits.
- Invalid individual component: rejected before union; previous override retained.
- Overlapping valid solids producing a union inconsistent with authoritative
  Source: rejected by unchanged post-union fidelity; previous override retained.
- Deliberately stalled test child: killed at the 10-second deadline.
- Actual Release `BrickSuite.exe` worker: synthetic box union accepted with
  28 vertices / 52 faces, using the normal independently validated MCUT path.
- Ray's file remained byte-identical. Existing research work was preserved.

Qt 6.10.3 MinGW Release build passed. Focused tests passed. Full configured
CTest: **109/109 passed in 69.97 seconds**. `git diff --check` passed.
Schema **35**, Protocol **1.5** unchanged. No Debug tests were compiled.
No unrelated changes, commit, or push.

# Source-constrained residual repair proof

This is a standalone experiment, not a BrickSuite preparation route. It consumes
the saved Geogram residuals from the preceding experiment. It never selects a
largest component, smooths surfaces, fills arbitrary holes or changes tolerances
to obtain a passing mesh. The full results are in
`docs/m37-source-constrained-repair-proof.md`.

## Inputs and reproduction

Use the existing Geogram 1.10.0 and CGAL proof Release builds and the pinned
Python requirements in `experiments/mesh-repair/requirements.txt`. No new library
is required. Rebuild `experiments/geogram-repair` for its optional per-face
provenance sidecar. That option changes diagnostics only; the runner requires
vertex arrays AND face arrays to exactly match the previously saved residual.
An unmatched replay is rejected rather than assigned speculative ancestry.

```text
python experiments/source-constrained-repair/run.py
  --geogram-bin <geogram-proof-build> --cgal-bin <cgal-proof-build>
  --prior <prior-CGAL-results> --geogram-results <prior-Geogram-final-results>
  --out <ignored-result-directory>
  --obj <original-user-23422.obj> --reference <user-23422.3mf>
python experiments/source-constrained-repair/finalize.py --out <result-directory>
```

The first invocation is one command. Set `PYTHONPATH` to the isolated analysis
dependencies and `MPLCONFIGDIR` to an ignored build-directory cache. The runner
verifies both benchmark hashes against the previous Geogram manifest. All mesh
coordinates are already normalized to millimeters at 0.4 mm per Studio unit.
No new scale fitting or substitute benchmark is allowed.

Starts: saved Studio 23422 fill-outer; 6553 repeat2; 44375a/80910 fill-outer;
3001 outer-only. The unchanged Geogram passes are replayed solely to recover
lineage. No additional general repair passes are applied to the residuals.
6553's two lineage arrays are composed back to the original input.

## Constraints and provenance

Each remaining boundary path records incident output faces, original input face
IDs, supporting authoritative triangle IDs, source file/line/reference chains,
nearby source triangles with distances, nearby opposing boundary intervals,
generated incident faces, graph closure and patch-gate results.

The existing 0.0004 mm seam tolerance and 0.1-degree normal test identify possible
source support. **They do not authorize a join.** A seam requires:

1. Non-generated faces with traced input lineage and consistent authoritative
   geometric support of both input and output triangles.
2. Distinct, unambiguous authoritative source triangles sharing an exact authored
   edge with exactly two incident source triangles.
3. Reversed residual intervals on that same authored edge, agreeing within
   1e-9 mm numerical equality, and a unique partner on each side.
4. No protected opening; no new collapsed triangle, near-degenerate face or
   nonmanifold edge after joining. Otherwise the join batch is rolled back.

Joining changes vertex indices only and preserves per-face lineage. Differently
segmented intervals without a complete correspondence proof remain rejected;
this is not a general edge-splitting or arbitrary surface-intersection solver.
The real 80910 joins preserve every triangle coordinate exactly.

A graph-theoretically closed loop is not permission to cap. All generated-patch
gates must be proven: complete authoritative boundary, no intentional opening,
unambiguous material side, strict local fidelity and preserved wall/features.
No corpus residual met these gates, so this proof deliberately adds **zero**
patches. There is no speculative patch generator hidden behind an acceptance
test. Existing generated faces from the earlier pass are recorded separately.

The diagnostic Geogram facet attribute starts at input index + 1. New faces
default to zero; subdivision/reordering copies attributes. Zero is serialized
explicitly as `generated repair geometry`. Surviving faces retain their input
lineage; a second geometric test guards authoritative mapping. Generated faces
never receive retained authoritative ancestry, even when geometrically coincident
with it. `fit_ownership` remains null for every experimental face: input triangle
ancestry is necessary but not sufficient to certify a functional feature.

`supports()` is deliberately conservative: a face spanning several source
triangles may remain unowned. No nearest-face fallback invents ownership.
Inherited generated area is now measured from traced creation history, unlike
the previous experiment's conservative post-hoc overlap estimate.

## Worker and diagnostics

`worker.py` launches each engine/analysis stage in a separate process, with one
thread requested, wall/CPU budgets of 120 seconds (180 for Python residual
analysis), 2 GiB aggregate child RSS/private-memory limit, and 64 MiB output
directory limit. It polls every 20 ms and kills only its spawned process tree on
limit breach. These are sampled supervisory bounds with possible short overshoot,
not a claimed OS hard quota. Windows children use `CREATE_NO_WINDOW`.

Before starting, it flushes/fsyncs and atomically replaces `worker-state.json`
with Part, phase, limits, command, log/output paths and status. It persists PID
when running and exit code, timeout/reason, wall/CPU/RSS/output metrics at exit.
Nonzero exit does not crash the parent. Automatic resume and a production worker
protocol are not implemented. Production would additionally need OS-enforced
limits and worker-lifetime management when the parent exits.

Outputs per Part:

- `lineage.json`: verified replay lineage.
- `residual-analysis/boundaries.json`: every input residual loop and edge.
- `residual-analysis/provenance.json`: every output face, retained ancestry and
  generated/source distinction.
- `residual-analysis/constrained.off`: diagnostic candidate, never accepted by
  file existence alone.
- `residual-analysis/measurements.json`: topology, feature probes, deviations,
  independent exact checks, acceptance failures/unproven gates.
- Per-stage `worker-state.json` and `worker.log`: crash/resource attribution.

`validated-summary.json` aggregates final results; `23422-boundary-rims.png`
locates the rejected primary loops. Existing CGAL exact intersection/contact
classification and fixed-error Hausdorff estimates are reused unchanged. Contact
counts are not penetration depths. Sampling is not a global certificate. Raw
source-to-output distance includes removed internal sheets; no exemption is
inferred without a material/exterior proof.

## Tests

Set `PROOF_TEST_TMP` to an ignored build directory and `GEOGRAM_PROOF_EXE` to the
Release executable's absolute path, then:

```text
python experiments/source-constrained-repair/test_proof.py
python experiments/geogram-repair/test_proof.py
python experiments/mesh-repair/test_analysis.py
ctest --test-dir <geogram-proof-build> --output-on-failure
```

New synthetic tests cover successful authored seam stitching without patches or
coordinate movement; rejection of proximity-only, ambiguous, generated and
nonmanifold source evidence; protected openings; generated-face lineage; child
failure recovery; wall, CPU, memory and output bounds. Private Part files are
not test fixtures. No Debug compilation is needed.

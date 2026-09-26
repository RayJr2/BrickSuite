# Isolated Geogram feasibility proof

This standalone project is never included in BrickSuite's root CMake. It does
not create an accepted PreparedMesh, alter fit ownership, or ship a new library.
See `docs/m37-geogram-repair-feasibility.md` for results and limitations.

## Build

Download the **official release asset** `geogram_1.10.0.zip` from
https://github.com/BrunoLevy/geogram/releases/tag/v1.10.0 into an ignored build
directory. The GitHub-generated source archive lacks bundled submodules.
Keep the dependency unmodified and outside this source directory.

```text
cmake -S experiments/geogram-repair -B <proof-build> -G Ninja
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=<kit-gcc>
  -DCMAKE_CXX_COMPILER=<kit-g++> -DGEOGRAM_SOURCE_DIR=<extracted-release>
cmake --build <proof-build> -j 6
ctest --test-dir <proof-build> --output-on-failure
```

The configure lines above form one command. Tested: Windows x64, Qt's MinGW
13.1.0, C++17, Geogram 1.10.0. No Geogram source patches were needed. The tool
does not link Qt. Deploy the matching GCC, C++ and pthread runtime DLLs beside
the executable on Windows. Geogram automatically selects Win64-gcc, Linux
GCC/Clang or Darwin Clang settings; macOS/Linux remain untested here.

Optional graphics, Lua, HLBFGS, TetGen, Triangle, TBB, legacy numerics, FPG and
Exploragram are disabled. The release still compiles bundled OpenNL, AMGCL,
PoissonRecon, xatlas, libMeshb, RPly, predicates and zlib code. BSD-3-Clause for
Geogram does not replace those components' notices. No commercial add-ons used.

## Run and measure

First build the preceding standalone `experiments/mesh-repair` proof. Its
source dumper, CGAL exact intersection classifier and Hausdorff estimator are
independent validators; they are not part of a proposed Geogram dependency.
Use its pinned Python requirements in an isolated environment. Set
`MPLCONFIGDIR` to an ignored build-directory cache.

```text
python experiments/geogram-repair/run.py --bin <proof-build>
  --cgal-bin <cgal-proof-build> --prior <cgal-results>
  --obj <exact-user-23422.obj> --reference <exact-user-23422.3mf> --out <results>
python experiments/geogram-repair/analyze_results.py --out <results>
  --prior <cgal-results> --cgal-bin <cgal-proof-build>
python experiments/geogram-repair/sensitivity.py --bin <proof-build>
  --cgal-bin <cgal-proof-build> --prior <cgal-results> --out <results>
python experiments/geogram-repair/supplement.py --out <results>
  --prior <cgal-results> --cgal-bin <cgal-proof-build>
```

Each script invocation is one command. `prior` must contain the installed
LDraw source/candidate OFF files and ancestry JSON from the previous proof.
The exact user OBJ/3MF are required, hashed, and read-only. OBJ position indices
are preserved across UV/normal seams. No OBJ material file is needed for geometry.

Both Studio exports are explicitly scaled **0.4 mm per Studio unit**. Only proper
axis permutations and bounding-box center translation register them to canonical
LDraw coordinates; scale is never fitted. The observed proper axis rotation is
the same for OBJ and 3MF. Bambu component/build translations are checked and
recorded separately. The file's millimeter metadata does not imply that its raw
coordinates have already been normalized to LDraw millimeters.

Primary variants use the existing 0.0004 mm input seam tolerance, followed by
**exact (zero-tolerance) post-intersection colocation**, matching upstream's
intersection example. Modes:

- `clean`: duplicate cleanup, orientation/connectivity, vertex colocation.
- `arrange`: intersect/retriangulate without outer-shell extraction.
- `outer`: intersect, radial sort, remove internal shells, exact cleanup.
- `fill-outer`: explicit unrestricted hole filling before `outer`.
- `outer-fill`: explicit hole filling after `outer`.
- `fill-outer-simplify`: fill/extract, then simplify at exactly zero angular
  tolerance. This may combine source triangles and reduce ownership attribution.

The sensitivity script checks exact input colocation, simplification, and at
most three fill/extract passes. No acceptance tolerance is adjusted. Generated
patches are repair surfaces, never declared LDraw authored. No largest-component
selection, smoothing, voxelization or silent surface deletion outside the named
outer-shell operation occurs. Delaunay intersection triangulation is the upstream
default; neighbor intersections enabled, normalization disabled, thread budget 1.
OFF output retains 17 significant digits.

Child processes are bounded to 120 seconds and 4 GiB RSS/private memory. A fatal
engine exit is recorded; only the isolated child can be killed. `runs.json` and
`sensitivity.json` retain return codes, timings, sampled peak RSS and logs.
`analysis.json`, `supplement.json` and `bounded-distances.json` retain geometry
measurements. Sensitivity records reuse the old runner's `cgal` JSON key for
engine stdout; the key does not mean the Geogram repair was performed by CGAL.

The measurement algorithms match the preceding CGAL proof: native indexed
topology; near-degenerate area < 1e-12 mm²; exact classification of CGAL-reported
intersection/contact pairs; fixed-seed area samples (20,000 primary / 5,000
corpus), barycentric grids and point-to-triangle distances; global Hausdorff
estimates with fixed 0.001 mm algorithm error. Sampled max is a lower bound.
P95/RMS are area weighted samples. The Hausdorff error is measurement precision,
not an acceptance concession. Point/segment contacts do not measure penetration.
Source-to-output distances include deleted internal source sheets: they require
interpretation and are not automatically exterior geometry loss.

Ownership requires vertices and centroid supported by one original triangle
within 0.0004 mm and normal agreement within 0.1 degree, with reference-instance
ancestry disambiguation. It is conservative geometric support, not propagated
causal provenance. Cross-triangle patches may be unmatched even when coplanar.
No fit ownership is created by these scripts.

## Focused tests

Set `GEOGRAM_PROOF_EXE` to the executable's absolute path and `PROOF_TEST_TMP` to
an ignored build directory, then run:

```text
python experiments/geogram-repair/test_proof.py
python experiments/mesh-repair/test_analysis.py
ctest --test-dir <proof-build> --output-on-failure
```

Fixtures cover OBJ seam indexing, fixed unit normalization, explicit hole filling,
preservation of a through bore, immutable inputs, and a closed tetrahedron.
The reused tests cover triangle distances, conservative attribution and cap
detection. Passing fixture tests does not accept any experimental Part mesh.

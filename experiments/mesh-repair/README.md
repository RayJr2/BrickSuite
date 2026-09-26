# M37 isolated mesh-repair feasibility proof

This standalone CMake project is **not** included by BrickSuite's root CMake.
It does not change production preparation, export, fit, catalog, or database
behavior. All repair outputs are experiments, not accepted PreparedMeshes.
See `docs/m37-mesh-repair-feasibility.md` for the decision and measured results.

## Dependencies and licensing

Tested with CGAL 6.1.1, Boost 1.86.0 headers, GCC 13.1.0 from the Qt 6.10.3
MinGW kit, and CMake 3.27. CGAL PMP is GPL-3.0-or-later OR commercial, unlike
CGAL's LGPL foundational packages. These proof executables are not staged in
BrickSuite's application package. Production distribution needs a deliberate
GPL-compliance or commercial-license decision first.

The proof uses Boost multiprecision; GMP/MPFR, Eigen, TBB and CGAL's Qt viewer
are not needed. Only `source_dump` links Qt Core/Gui and the existing loader and
source conversion code. Repair and distance tools are plain C++ executables.

Download official CGAL/Boost source archives into an ignored build directory.
Do not vendor them into this experimental source directory. Configure separately:

```text
cmake -S experiments/mesh-repair -B build/mesh-repair-proof/bin -G Ninja
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=<kit-g++>
  -DCMAKE_PREFIX_PATH=<Qt-kit> -DCGAL_DIR=<CGAL-6.1.1>
  -DBOOST_ROOT=<boost_1_86_0> -DBoost_NO_BOOST_CMAKE=ON
  -DCGAL_CMAKE_EXACT_NT_BACKEND=BOOST_BACKEND -DCGAL_DISABLE_GMP=ON
cmake --build build/mesh-repair-proof/bin -j 2
ctest --test-dir build/mesh-repair-proof/bin --output-on-failure
```

The configure options above are one command, split for readability. CMake 3.27's
FindBoost works with the source archive layout. Newer CMake/CGAL package policy
can require an installed BoostConfig package instead. On Windows, deploy Qt's
runtime beside `source_dump` with the kit's `windeployqt`; do not mix toolchains.

Install `requirements.txt` into an isolated Python environment. Set
`MPLCONFIGDIR` to an ignored build-directory cache. Then:

```text
python experiments/mesh-repair/test_analysis.py
python experiments/mesh-repair/run_proof.py --bin <proof-bin> --library <LDraw-root> --reference <23422.3mf> --out <proof-results>
python experiments/mesh-repair/analyze.py <proof-results>
python experiments/mesh-repair/measure_distances.py --bin <proof-bin> --out <proof-results>
```

The corpus is fixed: 23422, 44375a, 80910, 6553, 3001. Processes are limited to
120 seconds and 4 GiB working set/private memory. Only an isolated proof child
may be terminated; no BrickSuite worker/thread is interrupted. Source loading
also has a 120-second timeout. Original models and reference files are read-only.

## Stages and measurement contracts

- Source: installed LDraw expanded by the existing loader, with triangle/line/
  reference ancestry. Canonical conversion is `(0.4*x, 0.4*z, -0.4*y)` mm.
- Candidate: existing `PrintMeshConversion` with its 0.0004 mm seam weld.
- `inspect`: soup orientation to create a measurable Surface_mesh; this may
  split nonmanifold vertices, so retain the independent candidate topology too.
- `clean`: `repair_polygon_soup`, orientation, conversion, border stitching,
  isolated-vertex removal.
- `refine`: cleanup plus `autorefine_triangle_soup` with iterative snap rounding
  before orientation. Fixed defaults: grid exponent 23, five iterations.
- `fill`: refine plus explicit triangulation of every remaining boundary loop.
- `fill-clean`: cleanup and explicit hole filling without autorefinement.
- `local`: fill-clean plus experimental `remove_self_intersections`, defaults
  including preserved genus and seven iterations. Its return value is recorded.

Hole patches are generated repair surfaces, never asserted to be LDraw-authored.
Autorefinement success is not successful solid repair: orientation, manifold
splitting, and hole filling can introduce contacts/overlaps afterward.

`runs.json` records CGAL intersection predicates, exact-construction contact
classification, process wall time, and sampled peak RSS. Contact counts are not
equivalent to penetration depth. Performance includes validation and file I/O;
the 10 ms memory sampler can miss short peaks. Self-intersection classification
uses an exact-constructions kernel because inexact intersection construction
was unreliable on near-coplanar contacts. Repair uses EPICK with snap rounding.

`analysis.json` contains topology, area-weighted fixed-seed samples (20,000 for
23422, 5,000 for secondary corpus comparisons), a barycentric grid, percentiles,
RMS, worst coordinates, feature rays and geometric ancestry. Sampled maxima are
lower bounds, not certified global maxima. `bounded-distances.json` separately
records CGAL Hausdorff estimates with a fixed 0.001 mm algorithm error bound.
That bound is measurement precision, not a loosened acceptance tolerance.

Geometric attribution requires all three vertices and the centroid to lie in one
original triangle within the existing 0.0004 mm seam tolerance and normals within
0.1 degrees (ignoring winding sign). Multiple matches are explicitly classified.
This is conservative geometric support, **not** causal provenance or permission
to infer fit ownership. A future implementation should propagate original face
IDs through CGAL's autorefinement visitor and mark generated patches unowned.

The Bambu file is read as one millimetre mesh object; component/build placement
is recorded separately. Its raw dimensions are reported. A separately labeled
0.4x shape comparison tests the LDU/mm discrepancy with proper axis permutations
and center translation only: no fitted scale, ICP warping, or tolerance tuning.
The author did not intentionally scale this reference. An unrepaired Studio
export would be needed to attribute differences specifically to Bambu repair.

The mesh/JSON/log/PNG artifacts stay under the ignored proof output directory.
Do not distribute the user's reference archive or treat it as authoritative
LEGO geometry. Installed LDraw geometry retains its own attribution/license.

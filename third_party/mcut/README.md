# MCUT dependency

BrickSuite pins upstream [MCUT](https://github.com/cutdigital/mcut) release
`v1.3.0`, commit `047d75ffe6e33ede572cb25217047a4756188401`, under the
GNU Lesser General Public License version 3 or later. MCUT is built as a shared
library. Its `mio` build dependency is pinned separately at commit
`474d060bddd1d9a3e69c439e31ae6f3dae3d55ad` (Apache License 2.0).
MCUT does not build `mio` when BrickSuite disables tutorials, so it is not in
the resulting runtime dependency graph; the declaration is nevertheless pinned
to prevent MCUT's moving `mio/main` reference from becoming active if that
upstream configuration path is enabled later.

`cmake/ApplyMcutMinGwPatch.cmake` carries one compatibility correction for the
pinned MCUT source: the `_Acquires_lock_` SAL annotation in
`include/mcut/internal/tpool.h` is restricted to MSVC rather than all Windows
compilers. The patch is required by GCC/MinGW and deliberately fails when the
expected v1.3.0 source text is absent.

`cmake/BrickSuiteMcut.cmake` also force-includes `stdint.h` privately when
compiling MCUT C++ sources with GCC. The pinned `source/math.cpp` uses the
global `uint32_t` type without its defining header; GCC 15 no longer provides
it through incidental standard-library includes. This adjustment does not
change the fetched source or propagate compiler options to BrickSuite.

The upstream v1.3.0 tag retains `1.2.0` in its CMake/API version metadata. The
tag and pinned commit above are BrickSuite's dependency provenance authority.
No MCUT implementation code is copied into BrickSuite production sources.

`cmake/BrickSuiteMcutPortability.cmake` generates a narrow source/header overlay
for the same pinned revision on every platform; downloaded sources stay intact:

- General-position perturbation names `std::minstd_rand0` instead of
  implementation-defined `std::default_random_engine`, preserving the engine
  used by the validated Linux/libstdc++ path and its seed of 1.
- MCUT alone uses non-contracted floating-point arithmetic (`-ffp-contract=off`
  for GCC/Clang, `/fp:strict` for MSVC). On ARM64, the original arithmetic and
  engine combination diverged in source-backed extraction. Neither correction
  alone restored 3037; together they pass the unchanged seven-operation,
  1,720-triangle manifold and deterministic-repeat assertions.
- CDT rejects NaN and infinities before bounds construction or KD-tree insertion.
  MCUT's duplicate-vertex perturbation can normalize a zero vector; the original
  NaN bounds caused unbounded root expansion. This guard throws a caught backend
  exception instead of fabricating coordinates or accepting invalid topology.

The integration identity is `1.2.0-047d75f-portability1` so cached preparation
does not reuse the previous backend identity. `McutMeshBoolean` includes a bounded
child regression for NaN and both infinities, before initial and subsequent CDT
insertion. `PrintCompositionRouting` retains the full 3037 Ready contract.
Linux and Windows must rerun the shared geometry/calibration tests after adopting
this change; macOS results alone do not validate those platforms.

The previously Linux-only `BrickSuiteMcutLinuxQueue.cmake` synchronization overlay
now applies to every platform. A macOS calibration stall showed the same caller/
API-thread lost wakeup, and the original 100,000-transition queue regression
failed on this Mac. The existing synchronized notification is reused unchanged.
Its historical filename/output directory is retained for Linux provenance.
The portable `McutMeshBoolean` test reuses this regression in a contained child;
Linux retains its separately registered queue test as well.

The engine difference is documented by the
[libstdc++ random engine reference](https://gcc.gnu.org/onlinedocs/libstdc%2B%2B/latest-doxygen/a00641.html).
The arithmetic setting follows the
[Clang floating-point controls](https://clang.llvm.org/docs/UsersManual.html).

BrickSuite links MCUT dynamically and stages `libmcut.dll` beside development
and test executables on Windows. Release packaging must ship the corresponding
shared library, its LGPL notice, and the information/source offer needed to let
recipients replace or relink that library. Equivalent shared-library packaging
is required on macOS and Linux.

For macOS Intel packages, ship the compatible MCUT `.dylib` in the application
bundle's Frameworks directory and ensure its install name and the BrickSuite
load command use an `@rpath`/`@loader_path` location inside that bundle. Verify
the final bundle with `otool -L` and include the MCUT license files and notices.

For Linux packages, ship a compatible `libmcut.so` in the package's private
library location (or declare an exact distribution dependency), retain a
relocatable `$ORIGIN`-based runtime search path where bundling is used, and
verify the packaged executable with `ldd`. Include the MCUT license files and
notices. Neither platform package may rely on a developer build tree or an
unrelated system search path to find MCUT.

The pinned `mio` declaration is a configuration reproducibility safeguard only.
With MCUT tutorials, tests, and documentation disabled, `mio` is not compiled,
linked, or distributed by BrickSuite and is not a runtime dependency.

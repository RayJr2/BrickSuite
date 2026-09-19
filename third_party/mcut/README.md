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

The upstream v1.3.0 tag retains `1.2.0` in its CMake/API version metadata. The
tag and pinned commit above are BrickSuite's dependency provenance authority.
No MCUT implementation code is copied into BrickSuite production sources.

BrickSuite links MCUT dynamically and stages `libmcut.dll` beside development
and test executables on Windows. Release packaging must ship the corresponding
shared library, its LGPL notice, and the information/source offer needed to let
recipients replace or relink that library. Equivalent shared-library packaging
is required on macOS and Linux.

The pinned `mio` declaration is a configuration reproducibility safeguard only.
With MCUT tutorials, tests, and documentation disabled, `mio` is not compiled,
linked, or distributed by BrickSuite and is not a runtime dependency.

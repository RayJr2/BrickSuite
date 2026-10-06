# Qt Windows software OpenGL notices

These are notice-only upstream extracts; no renderer or new dependency is added.
The existing windeployqt path ships opengl32sw.dll. The audited Qt 6.10.3 MinGW
SDK copy is byte-identical to Qt's signed Mesa 11.2.2 archive:

https://download.qt.io/development_releases/prebuilt/llvmpipe/windows/opengl32sw-64-mesa_11_2_2-signed_sha256.7z

DLL SHA-256: `b04de4541863bc7d8879040a78889c4849c1b1da2784c4630f734c146c2998ce`.
Packaging rejects a different software-renderer binary pending notice review.

- `Mesa-LICENSE.txt`: text content of `docs/license.html` (HTML presentation markup removed) from
  https://archive.mesa3d.org/older-versions/11.x/11.2.2/mesa-11.2.2.tar.xz
- `LLVM-LICENSE.TXT` and `LLVM-COPYRIGHT.regex`: unchanged upstream LLVM 3.6.0
  `llvm/LICENSE.TXT` and `llvm/lib/Support/COPYRIGHT.regex`.
- `LLVM-MD5-NOTICE.txt`: initial notice comment from the same tag's
  `llvm/lib/Support/MD5.cpp`, without implementation code.
  Source: https://github.com/llvm/llvm-project/tree/llvmorg-3.6.0/llvm

Qt identifies the Mesa 11.2.2 renderer as LLVM 3.6 in its build documentation:
https://wiki.qt.io/MesaLlvmpipe . This does not establish an LLVM patch version.
The notice extraction does not change the software-renderer binary or its use.

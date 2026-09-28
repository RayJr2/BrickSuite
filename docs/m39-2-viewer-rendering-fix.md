# M39.2 macOS viewer rendering correction

The relocated Release bundle could load LDraw part 3001 successfully while its
viewport remained black, including the orientation axes. Cocoa reported that
the OpenGL context could not share with its requested context and fell back to
an unshared context. The viewer's framebuffer existed, but the window compositor
could not access its texture.

The viewport requested OpenGL 3.3 Core locally while application startup left
Qt's default surface format unchanged. [Qt's QOpenGLWidget documentation](https://doc.qt.io/qt-6/qopenglwidget.html)
requires setting the default format before QApplication on macOS when requesting
a Core profile. Startup now does so using the same format helper as the viewport.
The worker dispatch remains before GUI configuration. Windows and Linux retain
their existing default-format behavior and the viewer retains its existing
OpenGL 3.3, 24-bit depth and four-sample request.

## Validation (2026-09-28)

- Rebuilt Release application and focused tests using the macOS 13-targeted Qt.
- Native `LDrawViewportRenderTest --legacy-default` reproduced the Cocoa sharing
  warning and failed the compositor-sharing assertion. The same test with startup
  configuration passed context sharing, visible solid geometry, wireframe, axes
  and resize checks. Framebuffer pixels alone would not detect this regression;
  the sharing assertion is essential.
- Existing `ExternalLDrawViewer` and `PartViewerMath` CTests passed (2/2).
- Relocated acceptance copy used the actual Release viewer objects and bundled
  dependencies with an isolated INI settings directory and an in-memory database.
  The installed LDraw library was read only. Native visual inspection showed
  part 3001, its edges and colored axes. Counts matched the reported case:
  700 triangles, 472 hard edges, 224 conditional edges, no omitted degenerates.
- New production bundle: `/private/tmp/bricksuite-m392-viewer-fix/BrickSuite.app`.
  All 27 Mach-O files are arm64 with a macOS 13.0 deployment target, the dependency
  audit reports no errors, and deep strict ad-hoc signature verification passes.
  This bundle excludes the optional package diagnostic executable.
- No user database, credentials or production preferences were changed by the
  acceptance harness. It does not exercise printing or exports. Windows/Linux
  execution and actual macOS 13 runtime execution were not available. User
  acceptance in the production app remains separate from this technical check.
- The existing clipped Status label is still visible in the dialog; this change
  is limited to the blank OpenGL viewport.

## Repeating the focused native check

Build the `LDrawViewportRenderTest` target and run it on a native desktop.
It is intentionally excluded from automatic offscreen CTest execution. Use
`--legacy-default` only to demonstrate the original macOS failure.

For a relocated full-dialog visual check, compile `deployment/macos/ViewerAcceptanceMain.cpp`
using `build_probe.py --source`, then create a disposable copy with
`stage_native_acceptance.py`. Run that copy's executable with an installed LDraw
library directory as its sole argument. Use an isolated HOME and CFFIXED_USER_HOME.
The harness uses production viewer code, an in-memory schema and temporary INI
settings, and opens part 3001. Never use this harness as a release entry point.

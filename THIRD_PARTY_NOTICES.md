# Third-Party Notices

BrickSuite is developed by RF StateSide, LLC and is licensed under the
GNU Lesser General Public License version 3.0 (LGPL-3.0-only).

This document summarizes major third-party software, services, data, and
trademarks used or referenced by BrickSuite v0.4.0. Distribution packages
should be reviewed when their contents change so that any additional
third-party notices required by bundled components are included.

## Qt

BrickSuite v0.4.0 is built with Qt 6.10.3 and uses the following Qt modules:

- Qt Core
- Qt Concurrent
- Qt Gui
- Qt Widgets
- Qt SQL
- Qt Network
- Qt WebSockets
- Qt OpenGL
- Qt OpenGLWidgets
- Qt Svg where required by deployed plugins/resources

Qt is developed by The Qt Company and the Qt Project and is available under
commercial and open-source licensing options. BrickSuite's open-source build
uses Qt components subject to their applicable open-source license terms.

Qt licensing information:
https://www.qt.io/licensing/

Qt open-source LGPL obligations:
https://www.qt.io/development/open-source-lgpl-obligations

When distributing BrickSuite binaries with Qt libraries, the distributor is
responsible for satisfying the license requirements applicable to the
specific Qt libraries and third-party components included in that
distribution.

## SQLite

BrickSuite uses SQLite as its local database engine through Qt SQL.

The SQLite project states that the deliverable SQLite code and documentation
are dedicated to the public domain.

SQLite copyright/public-domain information:
https://sqlite.org/copyright.html

SQLite downloads:
https://sqlite.org/download.html

## MCUT

BrickSuite uses MCUT v1.3.0, pinned to upstream commit
`047d75ffe6e33ede572cb25217047a4756188401`, from:
https://github.com/cutdigital/mcut

MCUT is licensed under the GNU Lesser General Public License version 3 or
later. BrickSuite uses MCUT as a shared library. Binary distributions must
retain the applicable MCUT copyright and license notices and satisfy the LGPL
requirements that permit users to replace or relink the shared library.

BrickSuite applies one narrow MinGW compatibility patch to the pinned source:
the MSVC SAL `_Acquires_lock_` annotation in `tpool.h` is limited to MSVC. The
patch does not alter MCUT geometry behavior and fails closed if the pinned
source text is not present.

MCUT's optional `mio` dependency is pinned to commit
`474d060bddd1d9a3e69c439e31ae6f3dae3d55ad` to avoid a moving configuration
reference. BrickSuite disables the upstream tutorials/tests that use it, so
`mio` is not linked or distributed in the current runtime.

## lib3mf

BrickSuite uses lib3mf v2.5.0, pinned to upstream commit
`64bb454d1fcb53effa57d3cef752a10d740d41a2`, from
https://github.com/3MFConsortium/lib3mf. lib3mf is distributed under the
BSD 2-Clause License. The static build includes its pinned LibreSSL, zlib, libzip, cpp-base64,
and fast_float sources. CMake stages their upstream license texts with
lib3mf's LICENSE under deployment/licenses/lib3mf, preserving source-relative
paths. Windows packages this directory under licenses/lib3mf; macOS and Linux
collect the corresponding upstream texts into their platform notice directories.

## Rebrickable

BrickSuite interoperates with Rebrickable reference data, CSV exports,
images, and API services.

Rebrickable API:
https://rebrickable.com/api/

Users are responsible for complying with Rebrickable's applicable terms,
API requirements, and usage limits. BrickSuite is an independent application
and is not affiliated with or endorsed by Rebrickable.

## Brickset

BrickSuite optionally interoperates with Brickset API services for Set
enrichment, usage information, provider links, and instruction metadata.

Brickset:
https://brickset.com/

BrickSuite users supply their own Brickset API credentials and are
responsible for complying with Brickset's applicable terms and API
requirements. BrickSuite is an independent application and is not affiliated
with or endorsed by Brickset.

## Platform Credential Services

BrickSuite stores provider API credentials using platform credential
facilities:

- Windows Credential Manager on Windows
- macOS Keychain on macOS
- Secret Service / keyring on Linux

On Linux, BrickSuite's current secure-storage integration invokes
`secret-tool`, commonly provided by the `libsecret-tools` package. Availability
and licensing of that operating-system package are determined by the user's
Linux distribution.

These platform facilities are used for local credential storage and are not
embedded provider credentials or BrickSuite-owned secrets.

## Inno Setup

The Windows BrickSuite installer is built using Inno Setup.

Inno Setup is a packaging/build tool and is not itself the BrickSuite
application runtime. Distribution maintainers should review the applicable
Inno Setup licensing information when changing the Windows packaging process.

Inno Setup:
https://jrsoftware.org/isinfo.php

## LEGO Trademark

LEGOÃ‚Â® is a trademark of the LEGO Group of companies, which does not sponsor,
authorize, or endorse BrickSuite.

References to LEGO products, part numbers, Sets, and related terminology are
used for identification and interoperability purposes.

## Other Third-Party Components

Qt itself may contain or depend upon third-party components distributed under
their own license terms. Binary release packaging should retain all notices
and license files required by the actual Qt runtime and other components
shipped with that release.

All other trademarks, product names, company names, services, and third-party
content referenced by BrickSuite are the property of their respective owners.
Their use does not imply sponsorship, affiliation, authorization, or
endorsement.

## macOS runtime packaging

The macOS bundle also deploys Qt Concurrent, Network, WebSockets, OpenGL,
OpenGLWidgets, and the Qt DBus dependency pulled in by Qt Gui. The source-built
Qt plugins include Cocoa, macOS style/network information, QSQLITE, OpenSSL TLS,
SVG/icon support, and GIF/ICO/JPEG/WebP image readers. PNG support is built into
Qt Gui. Qt's bundled third-party source notices accompany these modules.

OpenSSL 3.6.4 provides `libssl.3.dylib` and `libcrypto.3.dylib` under the Apache
License 2.0. Source and license information: https://openssl-library.org/source/.
The statically linked lib3mf sources also include their pinned LibreSSL code;
its `COPYING` file is included with the lib3mf notices, alongside zlib, libzip,
cpp-base64, and fast_float notices.

`deployment/macos/package_macos.py` collects license files and Qt attribution
metadata into `Contents/Resources/Licenses`, preserving source-relative paths.
Historical local acceptance used an ad-hoc signed bundle. The current packager
also uses ad-hoc signing, retains the macOS 13 target and separate ARM64/x86_64
artifacts, and does not perform Developer ID signing or notarization. Public
distribution must also satisfy applicable source/relinking distribution obligations;
including license texts alone does not establish distribution compliance.

## Windows runtime packaging

The installer retains LICENSE and THIRD_PARTY_NOTICES.md beside BrickSuite.exe,
with component texts under licenses and a relative-path licenses/manifest.txt.
CMake supplies the MCUT and lib3mf notice trees, including MCUT's CDT notice.
OpenSSL 3 runtime DLLs and their license come from the CMake-selected OpenSSL root.
MinGW GCC runtime (including the runtime exception), mingw-w64, and winpthreads
texts come from the configured compiler installation. Qt's existing software OpenGL
runtime is Mesa 11.2.2 with LLVM 3.6; its upstream notice extracts and binary
provenance are under licenses/SoftwareOpenGL. A different renderer binary
requires notice review. The SDK's D3D compiler is also redistributed by
windeployqt; the Qt installer-supplied Microsoft SDK terms are retained under
licenses/WindowsSDK. These additions do not change runtime selection.

Qt notice sources must match the deployed SDK version. The collector retains
license texts and qt_attribution.json metadata from qtbase, qtsvg,
qtimageformats, and qtwebsockets, including referenced license files. This is
a conservative source-module notice superset, not a claim that every component
mentioned in those sources is linked. Qt's Windows plugins and embedded
third-party code are covered by these source notices. Missing required inputs
stop packaging before Inno Setup runs. Set BRICKSUITE_WINDOWS_QT_SOURCE only
when the matching Qt sources are outside the SDK's sibling Src directory.

## Linux runtime packaging

The authoritative Linux packager assembles a private runtime closure containing
Qt, MCUT, OpenSSL, ICU, zlib, zstd, and Brotli as selected by its dependency policy.
lib3mf and its bundled dependencies are statically linked. Notices reside under
share/licenses, with a required-license inventory in share/build-metadata.json.
Qt notices are selected through the shipped binaries' SPDX dependency graph.

The baseline package uses Ubuntu 22.04 and its locked OpenSSL source; the explicit
local deployment profile records its own runtime origins. glibc, the C++/GCC
runtime, graphics/window-system libraries, and other allowlisted system services
remain host-provided, as documented by deployment/linux/runtime-requirements.json
and the artifact's share/dependencies.json. They are not represented as privately
bundled libraries. Platform notice directories intentionally differ according to
what each artifact distributes.

# Windows installation and packaging

## Install or update

Download the x64 installer from the [matching BrickSuite release](https://github.com/RayJr2/BrickSuite/releases), close BrickSuite, and run the installer. Follow its license and destination prompts, then launch BrickSuite from the Start menu. Windows is the primary validated development/release environment; this guide does not establish a minimum Windows version.

Create a database backup before upgrading. Uninstalling removes application files, not your database or settings.

## Developer packaging and notices

Run the existing Release packaging entry point after CMake configure/build:

```powershell
.\deployment\windows\package_windows.bat "<Release-build-directory>"
```

Qt and OpenSSL discovery still comes from CMakeCache.txt. The notice collector
also reads the configured MinGW compiler path. Install the matching Qt Sources
component; by default it uses the SDK's sibling `Src` directory. For a custom
source location set `BRICKSUITE_WINDOWS_QT_SOURCE` to that matching tree.
Each of qtbase, qtsvg, qtimageformats and qtwebsockets must match the SDK version.
No notice sources are downloaded by the packaging script.

CMake generates the lib3mf and MCUT notice trees. The collector validates every
required input before copying, preserves component-relative paths, verifies
copied bytes by SHA-256, and writes `licenses/manifest.txt`. Missing/empty inputs,
unreadable attribution files, mismatched Qt sources or an unreviewed software
OpenGL binary stop packaging before Inno Setup. A previous installer is not a
successful result of a failed packaging invocation; always check its exit code.

The installed application directory contains `LICENSE`,
`THIRD_PARTY_NOTICES.md`, and `licenses/`. About currently displays trademark
information and the BrickSuite license link, not a component-license browser.
The installer recursively includes the staged tree without changing paths.

Run the synthetic Windows regression tests without a Qt installation:

```powershell
python -B deployment/windows/test_notices.py
```

After packaging, inspect `deploy/BrickSuite/licenses/manifest.txt` and the Inno
Setup log. The tests exercise missing/empty inputs, version mismatch, staged
content/path parity, additional SDK runtime notices and the real batch-script
failure gate. A software-renderer update requires reviewing the upstream
notices and hash under `third_party/windows-software-opengl`; do not update the
hash merely to bypass a failure.

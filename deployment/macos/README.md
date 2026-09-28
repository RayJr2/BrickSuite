# macOS 13.0 source build and disposable packaging

BrickSuite v0.4.0 targets macOS **13.0+**. Build every bundled dependency for
that minimum; changing Info.plist or rewriting a Mach-O minimum is not a
substitute for rebuilding. The M39.2 local artifact is **arm64 only**. A minimum
load command establishes the deployment target, not runtime acceptance on an
older OS. Test on macOS 13 before claiming that runtime acceptance.

Requirements: Xcode/SDK, CMake, Ninja, Python 3.9+, clean Qt 6.10.3 sources,
verified OpenSSL 3.6.4 sources, and the repository's pinned MCUT/lib3mf sources
(with lib3mf submodules). Qt runtime binaries from an installed kit and Homebrew
are not packaging inputs. Apple's system frameworks/libz/libSystem are OS
components, not bundled dependencies to rebuild.

## Rebuild dependencies

From the repository root, set these paths for the build host:

```sh
export CMAKE=/path/to/cmake
export NINJA=/path/to/ninja
export QT_SOURCE=/path/to/Qt/6.10.3/Src
export DEPS_ROOT="$PWD/build/m39-macos13"
export OPENSSL_SOURCE="$DEPS_ROOT/sources/openssl-3.6.4"
export ARCH=arm64
export JOBS=8
mkdir -p "$DEPS_ROOT/sources"
curl -fL https://github.com/openssl/openssl/releases/download/openssl-3.6.4/openssl-3.6.4.tar.gz -o "$DEPS_ROOT/sources/openssl-3.6.4.tar.gz"
curl -fL https://github.com/openssl/openssl/releases/download/openssl-3.6.4/openssl-3.6.4.tar.gz.sha256 -o "$DEPS_ROOT/sources/openssl-3.6.4.tar.gz.sha256"
(cd "$DEPS_ROOT/sources" && shasum -a 256 -c openssl-3.6.4.tar.gz.sha256 && tar xzf openssl-3.6.4.tar.gz)
bash deployment/macos/build_dependencies.sh
```

Observed SHA-256: `9bffaa1ad1e07b354c21bd3324ec02fa15579f45a7d0494b3e74bc449b7333ef`.
Use disposable sources: OpenSSL builds in its source tree. Qt builds separately.
The script sets `MACOSX_DEPLOYMENT_TARGET=13.0`, the explicit compiler minimum,
and Qt's CMake deployment target/architecture. Qt compiles its bundled image,
compression, SQLite, and other selected third-party sources. Only Qt Base, Svg,
ImageFormats, and WebSockets are built. Secure Transport is disabled in this
custom runtime; the OpenSSL backend is required for BrickSuite's P-256 Host path.
No dynamic OpenSSL provider modules are required for this configuration.

## Rebuild BrickSuite and pinned dependencies

Set `MCUT_SOURCE` and `LIB3MF_SOURCE` to the pinned sources from
`cmake/BrickSuiteMcut.cmake` and `cmake/BrickSuiteLib3mf.cmake`. An existing
FetchContent source checkout is usable; existing compiled libraries are not.
Omit the two source overrides to let CMake fetch the pins when networking is
available. Use a fresh application build directory to avoid inherited targets.

```sh
"$CMAKE" -S . -B "$DEPS_ROOT/app" -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 -DCMAKE_OSX_ARCHITECTURES="$ARCH" \
  -DCMAKE_PREFIX_PATH="$DEPS_ROOT/qt" -DOPENSSL_ROOT_DIR="$DEPS_ROOT/openssl" \
  -DFETCHCONTENT_SOURCE_DIR_MCUT="$MCUT_SOURCE" \
  -DFETCHCONTENT_SOURCE_DIR_LIB3MF="$LIB3MF_SOURCE"
"$CMAKE" --build "$DEPS_ROOT/app" --parallel "$JOBS" --target BrickSuite
```

MCUT and static lib3mf (including its bundled LibreSSL/zlib/libzip code) rebuild
under the same target. The GUI and dedicated helper inherit it. The CMake
project also defaults an unset/empty macOS deployment target to 13.0.

## Stage, audit, sign, and relocate

Use a **new** output path. The packager never deletes existing output.
The following optional probe is a separate diagnostic executable, not a product
startup mode. Omit `--probe` for release staging.

```sh
python3 deployment/macos/build_probe.py "$DEPS_ROOT/app" "$DEPS_ROOT/BrickSuitePackageProbe"
python3 deployment/macos/package_macos.py \
  --build "$DEPS_ROOT/app" --qt "$DEPS_ROOT/qt" --qt-source "$QT_SOURCE" \
  --openssl "$DEPS_ROOT/openssl" --openssl-source "$OPENSSL_SOURCE" \
  --lib3mf-source "$LIB3MF_SOURCE" --arch "$ARCH" \
  --probe "$DEPS_ROOT/BrickSuitePackageProbe" \
  --output /private/tmp/bricksuite-m392-final/BrickSuite.app
python3 deployment/macos/audit_bundle.py \
  /private/tmp/bricksuite-m392-final/BrickSuite.app --arch "$ARCH" \
  --report /private/tmp/bricksuite-m392-final/audit.json
codesign --verify --deep --strict /private/tmp/bricksuite-m392-final/BrickSuite.app
```

The packager:

1. Copies the Release app/helper and an explicit plugin allowlist.
2. Adds both rebuilt OpenSSL versioned libraries under `Contents/Frameworks`.
3. Runs rebuilt Qt 6.10.3 `macdeployqt APP -no-plugins -no-strip
   -always-overwrite`, passing `-executable=PATH` for the helper, each staged
   plugin, both OpenSSL libraries, and the optional probe. Framework deployment
   follows those actual link dependencies, including QtDBus through QtGui.
4. Normalizes non-system dependency edges to `@rpath`, sets a relative
   `@loader_path` Frameworks search path for every Mach-O file, and removes
   development run paths. It fails if an expected bundled dependency is absent.
5. Converts existing ICO artwork to ICNS, writes `qt.conf`, gathers notices,
   and audits all Mach-O architectures, minimum OS values, and dependency paths.
6. Signs nested code/frameworks first, then the app, using an ad-hoc signature;
   verifies with `codesign --verify --deep --strict`.

Final allowlist: Cocoa; QSQLITE; OpenSSL TLS; Apple network information; macOS
style; SVG icon engine; GIF, ICO, JPEG, SVG, WebP image readers. PNG is built
into QtGui. No Mimer/ODBC/PostgreSQL, QML/Quick, VirtualKeyboard, or Secure
Transport plugins ship. Licenses and source-relative Qt attribution metadata
are under `Contents/Resources/Licenses`. Ad-hoc signing is only for local
acceptance, not Developer ID/notarization or public distribution.

On this sandboxed development host, Apple's `iconutil` rejected even correctly
sized icons inside the sandbox and succeeded outside it. Run packaging with
normal host permissions when necessary; do not remove the icon or weaken the
audit/signature checks to mask that restriction.

## Clean-environment runtime acceptance

```sh
mkdir -p /private/tmp/bricksuite-m392-home
cd /private/tmp/bricksuite-m392-final
env -i PATH=/usr/bin:/bin:/usr/sbin:/sbin \
  HOME=/private/tmp/bricksuite-m392-home \
  CFFIXED_USER_HOME=/private/tmp/bricksuite-m392-home TMPDIR=/private/tmp \
  ./BrickSuite.app/Contents/MacOS/BrickSuitePackageProbe
```

The probe verifies automatically selected OpenSSL, ephemeral P-256 TLS with
matching/wrong fingerprints, actual bundled MCUT union/subtraction, watchdog
and deadline rejection, the actual BrickSuite self-launch union mode, isolated
SQLite Schema 35, verified backup, compiled resources, and loaded-image closure.
The one-byte/zero-ms containment cases are deliberate low-budget checks, not
measurements of hard 512 MiB enforcement. The production macOS watchdog samples
resident/physical/high-water memory; transient overshoot remains possible.

Use the same clean environment with `Contents/MacOS/BrickSuite` for the normal
app. The Keychain is a user-level facility: changing HOME does not isolate it.
macOS CFPreferences can also reuse the user's existing settings despite the
isolated home. Do not change live credentials, Host trust, or data to force tests.

`BrickSuitePackageProbe --credentials` is a separately authorized acceptance
step. It reads existing provider/Host entries, attempts to write the **same**
existing values, makes one connection test per configured provider, and tests
an existing readable Host identity. It never deletes entries or prints secrets.
Run with the real user's HOME so the ordinary Keychain search list is available.
`--brickset-read-only` checks the existing `BricksetApiKey` account and makes
one provider connection test without a Keychain write. Preserve this account
spelling; it differs from `BrickSetApiKey` in the initial milestone request.
If macOS authorization is unavailable, cancel prompts and record the checks as
pending. Do not reset the Keychain to make acceptance pass.

For native preference/backup changes without touching production preferences,
create a separate, explicitly non-release copy:

```sh
python3 deployment/macos/build_probe.py "$DEPS_ROOT/app" \
  "$DEPS_ROOT/BrickSuiteNativeAcceptance" \
  --source deployment/macos/NativeAcceptanceMain.cpp
python3 deployment/macos/stage_native_acceptance.py \
  /private/tmp/bricksuite-m392-final/BrickSuite.app \
  "$DEPS_ROOT/BrickSuiteNativeAcceptance" \
  /private/tmp/bricksuite-m392-ui/BrickSuiteNativeAcceptance.app
```

Its tiny wrapper selects INI settings under `$HOME/package-test-settings`, then
calls the **production main function**. It has a distinct test bundle ID and
uses the same rebuilt libraries, code, resources, and helpers. Launch it with
HOME and CFFIXED_USER_HOME pointing to a disposable directory. CredentialStore
is unchanged, so opening Settings can still request Keychain authorization:
`updateNetworkPresentation()` calls the existing Host `loadOrCreate()` path.
This copy is useful for UI acceptance but is not the final product bundle.

## Development regression tests

CTest targets are excluded from the default build. Build all configured target
names from `app/CTestTestfile.cmake`, then run CTest:

```sh
python3 - <<'PYTHON'
import os, re, subprocess
from pathlib import Path
build = Path(os.environ['DEPS_ROOT']) / 'app'
targets = sorted(set(Path(p).name for p in re.findall(
    r'^add_test\([^ ]+ "([^"]+)"', (build / 'CTestTestfile.cmake').read_text(), re.M)))
subprocess.run([os.environ['CMAKE'], '--build', str(build), '--parallel',
                os.environ.get('JOBS', '8'), '--target', *targets], check=True)
PYTHON
```

 Standalone executables do
not get the app bundle's Frameworks-based OpenSSL discovery:

```sh
env DYLD_LIBRARY_PATH="$DEPS_ROOT/openssl/lib" \
  /path/to/ctest --test-dir "$DEPS_ROOT/app" --output-on-failure \
  --parallel 1 --timeout 180 --output-junit /private/tmp/m392-ctest.xml
```

This variable is for development tests only. It is absent from all packaged
runtime acceptance commands. The final Mach-O audit JSON is suitable input for
M39.3's architecture/dependency gate, but the x86_64 artifact and its native
runtime acceptance have not been performed by this phase.

## M39.3 GitHub architecture artifacts

`.github/workflows/macos-artifacts.yml` is a manual `workflow_dispatch` workflow
with `macos-15`/`arm64` and `macos-15-intel`/`x86_64` matrix entries. Both check
out the same immutable `github.sha`; Xcode 16.4 is selected explicitly using
`DEVELOPER_DIR`. Nothing publishes a GitHub Release or pushes repository changes.

CI uses Python 3.12, CMake 3.31.6 and Ninja 1.11.1.3. It fetches the official
Qt 6.10.3 and OpenSSL 3.6.4 source archives from `source-lock.json`, verifies
SHA-256 before extraction, and invokes the same `build_dependencies.sh` used
locally. Qt's supported source configure/build/install method produces a native
single-architecture prefix for macOS 13.0. Only required modules are extracted
from the Qt source archive to limit disk usage. No installed Qt binaries or
Homebrew runtime libraries are release inputs. MCUT/mio/lib3mf pins remain
authoritative in the existing CMake files, including lib3mf's pinned submodules.

`ci_release.py` builds the app/helper and every executable referenced by the
configured CTest declarations, including EXCLUDE_FROM_ALL targets. It runs the
full configured suite serially, retaining existing per-test deadlines and using
180 seconds as the default for tests without one. It records actual configured,
passed, failed and skipped counts from CTest JSON/JUnit, with conditional LDraw
tests listed separately as **unregistered**. An initial failure is never hidden
by an automatic retry. The native viewport test remains manual UI acceptance.

The existing MCUT test's bounded 64 MiB allocation against a 32 MiB test budget
must report termination, parent survival and temporary-directory cleanup, as must
its deadline fixture. This exercises the shared sampled macOS watchdog on each
architecture; it does not establish an instantaneous kernel memory ceiling.

After the normal suite, the shared packager stages a production app without
probes. Metadata, exact plugin allowlist, every Mach-O architecture/minimum,
dependency closure and deep strict ad-hoc signatures are gates. A disposable
copy gets `PackageProbe.cpp`, compiled from the actual Release objects. In a
clean environment with isolated HOME it checks bundled OpenSSL 3.6.4, ephemeral
P-256 identity/TLS, matching/wrong fingerprints, both real bundled worker paths,
SQLite/schema/backup, Protocol 1.5, Help/F1/icon resources and provider construction.
No credentials, credential flags, external provider requests or DYLD paths are
used in this CI probe. The production app is unchanged by the probe.

Archives use `ditto -c -k --sequesterRsrc --keepParent`. Extraction is tested for
byte identity, executable permissions, framework symlinks, dependency closure
and codesign verification. Only after all gates pass does upload-artifact publish:

| GitHub artifact | Archive inside it |
| --- | --- |
| `bricksuite-macos-arm64` | `BrickSuite-v0.4.0-macOS-arm64.zip` |
| `bricksuite-macos-x86_64` | `BrickSuite-v0.4.0-macOS-x86_64.zip` |

Each includes a SHA-256 file, build metadata (source SHA, runner/OS/toolchain,
architecture, Qt prefix/version, deployment target, pins and gate results),
bundle audit, JUnit and packaged-probe log. The job summary records the archive
size/hash and GitHub artifact link. Failed jobs may upload diagnostics only.
These are **non-notarized, ad-hoc-signed development/release-candidate artifacts**.
Insert any future Developer ID signing/notarization step before final archive
creation and verification, using intentionally provisioned secrets.

Run local release-script regression checks with Python 3.12:

```sh
python3 -m unittest discover -s deployment/macos -p test_release_tools.py -v
```

Ray must review and push the implementation commit before dispatching this
workflow from GitHub's Actions page. Select the reviewed branch; both matrix
entries use its dispatch SHA, even if that branch advances while jobs run.
GitHub does not execute a workflow file present only in a local checkout.
See `docs/m39-3-macos-ci.md` for validation evidence and the M39.4 handoff.

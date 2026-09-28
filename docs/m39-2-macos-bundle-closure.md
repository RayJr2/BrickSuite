# M39.2 — macOS 13.0 bundle closure

Date: 2026-09-28. Baseline: `01cba69`. No commit or push.
User decision: BrickSuite v0.4.0 targets **macOS 13.0+**; rebuild bundled
runtime dependencies rather than inheriting development-machine binaries.

Dependency closure is demonstrated locally. Full runtime acceptance remains
open for Keychain authorization, installed
LDraw, and an actual macOS 13 machine. No Intel CI, Linux, Developer ID,
notarization, or public release was performed.

## Artifact and process

The disposable final artifact is `bricksuite-m392-final2/BrickSuite.app` under the
system temporary directory, outside the repository, build directory, Qt, and
Homebrew. It contains the production Release executable and a separate optional
`BrickSuitePackageProbe` acceptance executable. Omit `--probe` when staging a
release artifact. The separately named native acceptance copy is **not** the
product artifact: its entry-point wrapper selects isolated INI preferences.

Exact source-build, staging, audit, signing, relocation, and acceptance commands
are in [the packaging recipe](../deployment/macos/README.md).

| Requested evidence | Result |
| --- | --- |
| Bundle layout | `Contents/MacOS`: BrickSuite, BrickSuiteMeshBooleanWorker, optional BrickSuitePackageProbe. `Contents/Frameworks`: 11 Qt frameworks, MCUT, libssl, libcrypto. `Contents/PlugIns`: 11 allowlisted plugins. `Contents/Resources`: qt.conf, ICNS, and Licenses. Help itself remains compiled into the executable, not a duplicate source tree. |
| Source rebuild | Qt 6.10.3, OpenSSL 3.6.4, pinned MCUT, pinned static lib3mf and its included dependencies rebuilt with macOS 13.0/arm64 settings. Qt's selected bundled third-party sources also rebuilt. Apple OS frameworks/libz/libSystem remain system dependencies. No Homebrew runtime binary was used. |
| macdeployqt | Rebuilt Qt 6.10.3 tool with `-no-plugins -no-strip -always-overwrite`; explicit `-executable` inputs cover helper, staged plugins, SSL/crypto and acceptance probe. Then normalize dependency paths and audit the result. |
| Qt plugins | Cocoa, QSQLITE, OpenSSL TLS, Apple network information, macOS style, SVG icon engine; GIF, ICO, JPEG, SVG and WebP readers. PNG is built into QtGui. No Mimer, ODBC, PostgreSQL, Quick/QML or VirtualKeyboard. |
| OpenSSL | Both versioned dylibs under Frameworks. Clean-environment probe automatically selected `openssl`, reported OpenSSL 3.6.4, created/validated ephemeral P-256 identity, completed localhost encryption with a matching fingerprint and rejected a wrong pin. Existing persisted Host identity could not be accepted because of Keychain authorization. |
| MCUT | Main/helper linkage is relative. Actual bundled helper produced a valid 12 mm³ union and 4 mm³ difference. A one-byte test budget and zero-ms deadline rejected safely. These are containment tests, not a claim of zero transient overshoot at the production 512 MiB sampled budget. |
| Local Override | Probe invoked the actual relocated BrickSuite executable's existing self-launch worker argument, transmitted operands, read the result, and independently validated topology and 12 mm³ volume. No geometry behavior or workload limits changed. |
| Recursive Mach-O audit | **28 files, zero errors**. Every slice arm64; every `LC_BUILD_VERSION minos` exactly **13.0**. All non-system dependencies and run paths resolve inside the bundle. Probe observed 16 loaded bundle images and no non-Apple image outside the bundle. |
| Architecture inventory | Main/helper/probe, all 11 Qt frameworks, all 11 plugins, MCUT and both OpenSSL libraries: arm64 only. No universal leftovers or x86_64 artifact. |
| Metadata | ID `com.rfstateside.bricksuite`; short/full versions `0.4.0`; name BrickSuite; minimum 13.0; existing icon converted to BrickSuite.icns; copyright 2026 RF StateSide, LLC. SDK-26 compatibility appearance flag included. |
| Signature | Inside-out ad-hoc signing followed by `codesign --verify --deep --strict`: valid on disk and satisfies designated requirement. Reverified after relocation. This is not public-distribution signing. |
| Native launch/resources | Final production app launched with a minimal environment and isolated Application Support path. F1 opened native Help with embedded images. Settings and 3D Models opened in the earlier packaged pass. Native isolated-settings copy displayed light/system and dark UI, F1 opened the LDraw 3D Models topic, and folder browse/persistence worked. Printing overview resource existence passed the probe; its complete page was not separately navigated in the final native run. |
| API/Keychain | Rebrickable entry readable; one production-service connection test succeeded, HTTP 200. Unchanged-value write did not succeed after Keychain authorization trouble. The first probe used the request’s incorrectly cased `BrickSetApiKey`; source review corrected it to the unchanged production account `BricksetApiKey`. A read-only final probe found that entry and Brickset returned HTTP 200. BrickSuiteHostTlsIdentity read was blocked; no existing-identity TLS acceptance claim. Service remains `RFStateSide.BrickSuite`; production account names unchanged. |
| Backup/data | Probe created and verified an isolated Schema 35 backup. Native File → Backup completed with success confirmation under the disposable Documents folder; SQLite integrity check returned `ok`, schema 35. No restore or destructive live-data test. |
| LDraw | No installed library. Native folder picker saved an empty disposable placeholder and correctly reported missing `parts`/`p`. Source-backed printing, 3037/3021/2456 acceptance remains pending. |
| Licensing | 192 files, about 1.4 MiB, under Resources/Licenses: BrickSuite, Qt module licenses/attribution and bundled component notices, MCUT, lib3mf with included dependencies/LibreSSL, OpenSSL. Source/relinking/public distribution obligations still need the later release process. |

Qt frameworks: Core, Concurrent, DBus (linked through Gui), Gui, Network,
OpenGL, OpenGLWidgets, Sql, Svg, WebSockets, Widgets.

## Data locations and isolation limits

The production entry point retains Qt's native settings identity. Runtime
Application Support and Documents bases honored the isolated HOME plus
CFFIXED_USER_HOME. Native CFPreferences nevertheless reused the user's saved
BrickSuite preferences, including remote Host mode. The first normal launch
therefore showed Host-unavailable/reconnecting status; the final launch showed
Host authentication failed. No trust/pinning/authentication bypass was applied.
That saved connection is not an accepted two-machine test.

The normal runtime created its database and log under the disposable
`Library/Application Support/RFStateSide/BrickSuite`. Source-derived relative
locations below that verified runtime base are:

- `cache/parts`, `cache/sets`, `cache/minifigs` for images;
- `LegoCalibration/Workspaces` (alongside Sessions/Profiles) for calibration;
- `printable-overrides` for Local Printable Overrides.

Documents-derived artifacts use `BrickSuite/Calibration Artifacts` and
`BrickSuite/Print Capability Audits`. These path derivations were inspected;
new source-backed workspaces/overrides/artifacts were not generated during this
package smoke. Native backup creation verified the actual disposable Documents
base. Production native settings/Keychain service names are unchanged.

The separate native acceptance wrapper called the production main function
with INI settings under its disposable home. Saved values were verified:
`Theme=dark`, `ExplanatoryTooltips=false`, and the placeholder `LibraryPath`.
F1 remained usable. Actual hover-popup suppression was not separately observed.
No installed LDraw acceptance is implied by saving the placeholder.

## Keychain limitation

The user reported that the supplied password did not unlock the Keychain.
Computer automation could not control SecurityAgent and no attempt was made to
bypass it. No Keychain reset, entry deletion, credential replacement, Host
fingerprint clearing, or verification weakening was performed.

Reopening Settings in the disposable native copy later blocked. A one-second
stack sample identified the existing path:
`SettingsDialog::updateNetworkPresentation → BrickSuiteHostIdentity::loadOrCreate
→ generate → CredentialStore::write → SecKeychainAddGenericPassword`.
The test process was terminated to stop further requests. This native Settings
path can attempt to create an identity when one is not found; it was not changed
as part of dependency packaging. Successful identity storage/loading remains
unverified. Do not interpret the aborted run as a successful Keychain write.

The final2 bundle refresh changes the diagnostic Brickset lookup and adds a
read-only provider-check option; production application source is unchanged
from the final native F1 pass. The refreshed bundle was audited/signed again,
and its full core probe plus read-only Brickset check passed.

The first credential probe also encountered an already-existing backup filename
from the preceding run; its repeat backup failed safely. The acceptance probe
now uses a unique temporary backup directory, and the final rerun passed.

## F1 fix and regression evidence

Native testing found that Qt's `HelpContents` shortcut maps to Command-? on
macOS, not F1. HelpManager now retains platform bindings and adds F1 for main,
Settings, and reference dialogs. Final production-app F1 and isolated native
Settings → 3D Models F1 were visibly verified. No help content duplication,
schema, protocol, MCUT algorithm, or frozen M37/M38 behavior changed.

Fresh Release application/helper and all 115 configured test executables built.
An initial standalone CTest run without OpenSSL's development path gave
112/115: the three TLS tests failed using cert-only. The app-bundle probe had
already succeeded without environment overrides. With the **rebuilt** OpenSSL
path supplied only to standalone CTest, 115/115 passed in 16.47 seconds.

After the F1 change, parallel CTest had one 120-second FitCalibrationArtifact
timeout. It passed alone in 5.73 seconds. Final **serial 115/115 passed in
60.99 seconds** with a 180-second per-test timeout. Cause of the intermittent
parallel timeout is unproven; no geometry tolerance, assertion or deadline was
relaxed to obtain these results. The eight installed-LDraw tests remain
unregistered, as in M39.1A.

An empty cached CMAKE_OSX_DEPLOYMENT_TARGET was explicitly configured in a
separate check and correctly became 13.0. `git diff --check` passed.

Native isolated logs had a missing-font alias performance warning and four
accessibility-label warnings during UI automation, with no critical entries
or plugin/dyld/SQL/MCUT failures. Known source build warnings (including an
existing PrintCapabilityAuditDialog string encoding warning) were not edited.
The unavailable saved remote connection and Keychain authorization limitations
are reported separately, not hidden as packaging successes.

## Before M39.3 / release acceptance

The arm64 dependency/signing recipe is established. M39.2 is **not fully
runtime-accepted**: resolve Keychain access and repeat unchanged-value storage,
existing Host-identity loading/pinning, and authenticated native Settings checks;
both provider connection smokes now pass. Review the
intermittent parallel geometry timeout. Run the rebuilt app on a real macOS 13
machine before claiming oldest-OS runtime acceptance. Installed-LDraw and
two-machine acceptance remain later M39 checks. Intel artifact work was not
started and does not follow automatically from this report.

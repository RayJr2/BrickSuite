# M39.3A — Intel packaging signing closure

## Cause and evidence

The original `package_macos.py` traversed sorted Mach-O paths, omitted framework
binaries from its first loop, and signed framework bundles only afterward. Its
actual order was standalone Frameworks dylibs, **BrickSuite main**, MCUT helper,
plugins, Qt framework bundles, app bundle. Signing the main executable causes
codesign to recognize its containing application and inspect nested code. An
unsigned helper therefore failed before the script reached that helper.

The user reported the [Intel GitHub job](https://github.com/RayJr2/BrickSuite/actions/runs/36425646069/job/108938813866)
built successfully and passed 115/115 CTests, then failed while signing the main
binary with `code object is not signed at all`, identifying the helper as the
subcomponent. The public job page confirms the packaging-stage failure. Detailed
logs require sign-in in the available browser, and the workflow does not retain
the failed staged app. The remote Intel bundle and its pre-signing inventory were
therefore **not directly inspected**; no remote per-file signature inventory is
claimed.

Before changing the signer, local evidence established the mechanism:

- The existing raw ARM64 MCUT helper, QtCore framework binary, Cocoa plugin and
  OpenSSL libssl each display `flags=0x20002(adhoc,linker-signed)` and
  `Signature=adhoc`. The old main-first order also relied on incidental signatures
  for plugins and frameworks it had not yet signed.
- Minimal app/helper fixtures compiled with the same Apple clang invocation and
  macOS 13 minimum differed only by `-arch arm64` versus `-arch x86_64`.
  ARM64 outputs arrived linker-ad-hoc-signed; x86_64 outputs arrived unsigned.
- Signing main first passed for the ARM64 fixture and reproduced the **exact**
  unsigned `BrickSuiteMeshBooleanWorker` subcomponent error for the Intel fixture.
  This confirms the ordering defect and explains why ARM64 masked it, without
  treating local fixtures as direct inspection of the GitHub VM.

## Correction

One architecture-independent signer is called by the production packager, CI
probe-copy staging, and native acceptance-copy staging. Its deterministic order is:

1. Real Mach-O framework binaries.
2. Framework bundle envelopes, deepest first.
3. Standalone Frameworks dylibs and Qt plugins, sorted by path.
4. Nested executables, including the MCUT helper and optional acceptance probe.
5. The main executable identified by Info.plist.
6. The containing `.app`, last.

Every object is explicitly replaced using `codesign --force --sign -
--timestamp=none`. The signer requires the main and MCUT helper to be present as
Mach-O files, propagates any signing failure immediately, and retains the final
`codesign --verify --deep --strict --verbose=2` gate. `--deep` is not used as a
signing shortcut. No Developer ID, timestamps, signing secrets or notarization
were introduced.

## Regression coverage and local validation

The existing workflow already discovers `test_release_tools.py`, so both matrix
jobs automatically run the new tests without changing runner selection or gates.
The native tests compile small ARM64 **and** x86_64 Mach-O fixtures on macOS;
they sign/verify Intel code without executing it or claiming Intel app acceptance.

- The suite covers unsigned helper reproduction, missing-helper rejection,
  identical architecture-independent ordering, main/app last, forced replacement,
  no timestamps, immediate propagation of signing failure, and deep/strict checks.
- Both architecture fixtures include initially unsigned framework code, a
  framework bundle, plugin, standalone dylib, helper, probe and main executable.
  Both pass inside-out signing and repeat signing of already signed inputs.
- Removing the helper signature after sealing the app makes deep/strict
  verification fail; re-signing recovers it. Removing the helper itself fails
  preflight rather than silently producing an artifact.
- Full local ARM64 packaging and its 27-file architecture/dependency/macOS 13
  audit pass with the revised signer. The final bundle passes deep/strict codesign.
- A separate copy has all 27 Mach-O signatures deliberately removed before
  re-signing, to demonstrate the full Qt/OpenSSL/MCUT bundle does not depend on
  incidental linker signatures. Re-signing and deep/strict verification pass.
- All **13 packaging regression tests passed** locally (no skips), including
  the native unsigned fixtures for both architectures.
- The re-signed CI probe copy passes Protocol 1.5, bundled OpenSSL 3.6.4/P-256 TLS
  and fingerprint rejection, dedicated MCUT operations and bounded rejection,
  Local Override self-launch, SQLite/Schema 35/backup, Help/F1/icon resources,
  provider construction and runtime image closure. No live credentials were used.
- ZIP creation/extraction preserves bytes, modes and framework symlinks. The
  extracted app passes architecture/dependency and deep/strict signature checks.
  Local validation ZIP: `/private/tmp/bricksuite-m393a-local/BrickSuite-v0.4.0-macOS-arm64.zip`,
  30,019,265 bytes, SHA-256
  `510444285c4f7ff020216df1383177360887d451abcd00d160777fedb55b0c8f`.
  This is local validation evidence, not the future GitHub acceptance artifact.

No production application source, geometry, Schema 35, Protocol 1.5, deployment
target, architecture/dependency audit, CTest policy, TLS/worker probe, archive,
checksum or upload gate was changed. The full product CTest suite is not rerun for
this signing-only change; packaging regressions and the local bundled runtime
probe provide focused validation. GitHub will still build and run all configured
tests on **both** architectures.

## Remaining Intel evidence

Nothing in the supplied failure indicates another Intel build/test defect.
Intel packaged TLS/worker/resource probes, archive validation and upload occur
after signing and are therefore still unproven until the rerun. The public job
also shows a non-blocking Node 20 action-runtime deprecation warning; that is
separate from the codesign failure and action versions are unchanged here.

After reviewing and pushing the local correction commit, Ray should dispatch
the existing workflow with **both matrix entries**. No push or GitHub rerun is
performed by this task. Real Intel/macOS 13 runtime acceptance remains M39.4 work.

# M39.1A — macOS runtime safety and test closure

Validated on 2026-09-28: macOS 26.7 arm64, Qt 6.10.3, Apple Clang 17, CMake 3.30.5, OpenSSL 3.6.4. This follows the [M39 readiness audit](m39-macos-readiness-audit.md); its original 110/115 result remains historical evidence.

## Result

**115/115 configured Release tests pass, zero skipped**, in the final parallel run (15.99 seconds). A complete serial run also passed (62.45 seconds). Release and Debug applications build. All configured test executable targets were built explicitly. Schema 35, Protocol 1.5, M37/M38 geometry/ownership rules, and CredentialStore remain unchanged. No commit, push, packaging, signing, Intel CI, or Linux work was performed.

Eight installed-LDraw cases remain **unregistered**, not skipped, because this Mac has no configured installed library. Source-backed 3037's seven-operation route, 3021's bounded failure, and 2456's scalable/local route still require the installed-LDraw pass. Two-machine acceptance remains M39.4. Technical native checks do not replace Ray's workflow acceptance.

## 1. Shared worker memory containment

`BoundedGeometryWorker.h` now supplies child admission and parent supervision for both the dedicated MCUT executable and Local Printable Override's self-launch union worker.

On macOS, the parent uses `proc_pid_rusage(RUSAGE_INFO_V4)` and compares the maximum of resident bytes, physical footprint, and lifetime peak physical footprint against **512 MiB**. The child waits for a private stdin admission byte before reading operands or invoking MCUT; the parent sends it only after successfully sampling the child. Failure to sample a still-running child fails closed. Samples occur between QProcess waits of at most 10 ms. Crossing the budget kills/reaps the child and rejects its result as resource-limited. The existing 3-second start deadline, operation deadlines, kill/wait, wire bounds, parent mesh validation, and RAII temporary-directory cleanup remain. Local Override also gets the explicit start deadline through the shared runner.

This is a sampled memory watchdog, **not a kernel address-space reservation**. Allocation can overshoot between samples or while the parent is descheduled. A short-lived burst can finish between samples; lifetime footprint detects prior peaks while the process remains observable. The implementation does not claim a hard instantaneous 512 MiB cap. No production Boolean is moved into the main process, and no retry/fallback bypasses containment. Windows retains its Job Object process-memory cap; other Unix hosts retain RLIMIT_AS. Those branches were reviewed but not executed on this Mac.

The audited macOS RLIMIT_AS attempt returned EINVAL at 512 MiB. The installed macOS SDK defines RLIMIT_RSS as a source-compatibility alias of RLIMIT_AS, so it cannot provide a distinct replacement limit, and private/entitlement-dependent process policies were not adopted. Apple's public [libproc declaration](https://github.com/apple-oss-distributions/xnu/blob/main/libsyscall/wrappers/libproc/libproc.h) supports querying resource usage for another process; the [physical-footprint field](https://developer.apple.com/documentation/kernel/rusage_info_v3/1577481-ri_phys_footprint) represents measured memory rather than virtual address reservations. Runtime tests below demonstrate actual termination, not merely compilation.

## 2. Worker evidence

`McutMeshBooleanTest` executes real isolated cube union/subtraction, validates their meshes and expected volumes, checks repeated results, and rejects invalid/open/multiple-component operands. Controlled transport cases use the same runner and child admission path. A 64 MiB allocation is tested against a **32 MiB test-only budget**; no large real geometry or uncontrolled pressure is used. The production ceiling cannot be raised through the test seam.

Final parallel-run observations:

| Case | Elapsed | Result and cleanup |
| --- | ---: | --- |
| Valid transport result | 19 ms | Accepted; temporary workspace gone; PID reaped |
| Worker exits unsuccessfully | 18 ms | Backend failure; workspace gone; PID reaped |
| 200 ms deadline | 203 ms | ResourceLimitExceeded; killed; workspace gone; PID reaped |
| Malformed output | 13 ms | Backend failure; workspace gone; PID reaped |
| Missing output | 12 ms | Backend failure; workspace gone; PID reaped |
| 32 MiB memory budget crossed | 23 ms | ResourceLimitExceeded; killed; workspace gone; PID reaped |

The parent test process survives and completes the remaining assertions. Missing executable also fails closed. Local Override passes with the same 512 MiB policy and unchanged two-component, 2,000-face/6,000-vertex input, 10,000-face output, ten-second operation deadline, topology and Source-fidelity checks.

## 3. Disposition of the five original failures

| Test | Root cause and disposition |
| --- | --- |
| McutMeshBoolean | Unsupported macOS RLIMIT_AS blocked worker startup. Shared watchdog fixes execution; passes. |
| LocalPrintableOverrideService | Same containment defect in the separate self-launch path. Uses shared implementation; passes without broadening repair capability. |
| LDrawSemanticOperand | Synthetic round-passage failure was downstream of worker startup failure. Passes after containment repair; semantic ownership and coverage unchanged. |
| FitCalibrationWorkspaceUi | QMessageBox window-title assumptions were not portable. Assertions now use message text, icon and standard buttons; passes without production wording changes or platform skips. Ordinary QDialog titles remain valid test identifiers. |
| FitCalibrationArtifact | Z bound used an inconsistent tolerance for a general-position-perturbed Boolean fixture. Existing X/Y envelope tolerance now applies to all axes; passes with no production geometry change. |

## 4. Axle-hole bounds investigation

Two independent in-memory generations returned exactly the same measured envelope:

| Axis | Minimum (mm) | Maximum (mm) | Size (mm) | Nominal size (mm) |
| --- | ---: | ---: | ---: | ---: |
| X | -0.00022439953485507618 | 112 | 112.00022439953486 | 112 |
| Y | 0 | 20.000575902691399 | 20.000575902691399 | 20 |
| Z | -0.00067561430679007268 | 8.0000000000000018 | 8.0006756143067914 | 8 |

This measurement precedes serialization, excluding export rounding as the cause. `McutMeshBooleanService::dispatchBoolean` already requests `MC_DISPATCH_ENFORCE_GENERAL_POSITION`. The pinned MCUT header documents perturbation in all three axes. The fixture consists of the 112 × 16 × 8 base, an overlapping marker extending Y to 20, and seven subtractions; its expected nominal envelope remains 112 × 20 × 8. Every discrepancy is below **0.001 mm**, the test's pre-existing X/Y tolerance. Z previously required 0.000001 mm. The correction applies the existing 0.001 mm envelope tolerance consistently to Z; no geometry, candidate dimensions, topology, determinism, or ownership assertion was removed. This is small backend perturbation, not evidence of a materially different calibration geometry. Windows comparison was not rerun here.

## 5. TLS conclusion: OpenSSL required with the current Qt backend

The requirement is specific to **Qt 6.10.3's Secure Transport implementation and BrickSuite's existing P-256 server identity**, not a claim that Apple's TLS stack fundamentally lacks EC support.

Trace:

- `BrickSuiteHostIdentity` generates a P-256 key and self-signed certificate using libcrypto, loads them into QSslKey/QSslCertificate, and validates curve, dates, key/certificate match and identity. Fingerprints remain SHA-256 over DER.
- Host uses QWebSocketServer SecureMode with TLS 1.2-or-later, the existing certificate/key, and application-layer client authentication.
- Remote uses TLS 1.2-or-later, peer verification and exact fingerprint pinning; only the existing permitted trust/hostname errors may be accepted after a pin match. No security-policy or Protocol 1.5 change was made.
- A disposable localhost probe generated a fresh ephemeral identity. **Secure Transport loaded and validated the 256-bit key successfully**, then failed server setup with `SecPKCS12Import failed: -26275` / `Unable to init SSL Context`.
- The same probe with OpenSSL completed encryption and confirmed the exact peer fingerprint. No stored production identity was read or changed.

Qt's installed 6.10.3 source provides the explanation: `src/plugins/tls/securetransport/qtls_st.cpp` routes the certificate/key through `_q_makePkcs12`; `src/plugins/tls/shared/qsslsocket_qt.cpp::_q_PKCS12_key` asserts RSA or DSA and only emits algorithm identifiers for those algorithms. It has no EC key-bag encoding. Thus this is a backend capability defect, not a backend-specific assumption to remove from BrickSuite tests. See the [Qt Secure Transport implementation](https://github.com/qt/qtbase/blob/v6.10.3/src/plugins/tls/securetransport/qtls_st.cpp) and [shared PKCS#12 implementation](https://github.com/qt/qtbase/blob/v6.10.3/src/plugins/tls/shared/qsslsocket_qt.cpp).

### M39.2 handoff, without starting packaging

Both versioned **libssl.3.dylib and libcrypto.3.dylib** must be supplied with the matching architecture and Qt OpenSSL TLS plugin. libcrypto is already a direct application dependency for identity/authentication; libssl is additionally needed by the dynamically loaded TLS backend. Inspection of the installed libssl shows only libcrypto and system libSystem dependencies. Their non-system install names must be rewritten to bundle-relative resolution, including the app's direct libcrypto reference.

Qt 6.10.3's `qsslsocket_openssl_symbols.cpp::libraryPathList` explicitly searches an APPL bundle's `Contents/Frameworks` on macOS and matches versioned ssl/crypto filenames. This establishes a supported discovery path without DYLD_LIBRARY_PATH. It does **not** constitute a relocated-bundle runtime test. M39.2 must verify actual backend selection, loaded-library closure and Host/Remote TLS after relocation with Homebrew/Qt development paths and DYLD_LIBRARY_PATH absent. No packaging architecture blocker was found; no bundle was modified in this phase.

## 6. Host/Remote and Keychain

Final Release localhost results: SecureHostFoundation passed (0.264 s), HostReadExecutor passed (0.556 s), RemoteInventoryMutation passed (1.244 s). No segmentation fault or authentication hang occurred. The existing by-value request-ID captures remain. Current tests used the explicitly discoverable development OpenSSL runtime; this is not a deployment configuration.

CredentialStore was not redesigned. RebrickableApiKey, BrickSetApiKey and BrickSuiteHostTlsIdentity behavior was preserved; no stored values were exposed. SecKeychain modernization remains deferred. The native presentation probe replaced credential access only inside its disposable executable, so Settings could be inspected without accessing production keys.

## 7. Native Builds acceptance and reproduced layout corrections

A Cocoa/native Qt presentation harness linked the Release application's actual widgets, replacing only application startup and credential access. It used an isolated test database, isolated settings, one synthetic Build, Part, Color, inventory lot, requirement and exact allocation. Ray's workshop was not mutated. No offscreen platform was used for these checks. Native screenshots were observed through the desktop tools; widget captures from that native session preserve the whole wide window when the desktop capture clips its off-screen edge.

| Native content size | Dark | Light |
| --- | --- | --- |
| 1000 × 750 | Pass | Pass |
| 1200 × 800 | Pass | Pass |
| 1600 × 943 (available desktop height) | Pass | Pass |

Verified readable two-column fields, balanced width growth, normally sized Add Build, wrapping requirement status, all eight action labels, populated row/action-combo fit, collapse freeing table space, and wider/narrower resizing without overlap. The original native minimum height was 852 px. Explicit compact margins and vertical spacing bring the Builds widget minimum to 704 px, enabling the requested 750 px content height. Business workflows and table identities are unchanged.

Native evidence: [narrow Dark](images/m39/builds-native-1000x750-dark.png), [narrow Light](images/m39/builds-native-1000x750-light.png), [medium Dark](images/m39/builds-native-1200x800-dark.png), [medium Light](images/m39/builds-native-1200x800-light.png), [wide Dark](images/m39/builds-native-1600x943-dark.png), [wide Light](images/m39/builds-native-1600x943-light.png).

Native spot checks also covered Add Inventory, populated My Inventory filters/table, Procurement, Pulling, Settings Server/API, LEGO Fit Calibration and Print Capability Audit. Settings Server reproduced clipped identity-status and reachability text: wrapped status labels now expand with the form, and the explanatory reachability text spans the form. Buttons and port control retain normal sizing. No other production layout was changed. Tall calibration presentation, narrow legacy form fields and broad help-image refresh remain M40 visual-review topics rather than speculative redesigns here. Source-dependent printing was not run without LDraw.

## 8. Validation record and remaining gate

- Explicit build of all 115 configured test targets: passed.
- Release application and helper: passed; final Settings correction rebuilt and verified natively.
- Debug application and helper: passed.
- Five originally failing tests: all passed independently, then in full suites.
- Full serial CTest: 115 passed, 0 failed, 0 skipped, 62.45 s.
- Final full parallel CTest: 115 passed, 0 failed, 0 skipped, 15.99 s.
- One earlier parallel run timed out FitCalibrationPackage and FitCalibrationUnifiedGeneration while a Debug build was also active. Both passed individually (4.94/5.19 s), in the full serial run and in the final parallel run. Cause was not proven; no assertion was disabled and no production geometry changed to mask it.
- `git diff --check`: passed. Existing audit/layout/help/framework changes were preserved. No new schema migration, provider call, protocol change, credential change, or persistent-domain behavior change.

Test output was checked for failures and expected diagnostics; the deliberately failed worker cases and isolated Settings identity-storage error are test fixtures. Production user logs/data were not collected. Earlier framework/deprecation observations remain in the readiness audit.

**No unresolved M39.1A code/test blocker prevents beginning M39.2.** The next concrete gate is a relocated `.app` that discovers its bundled OpenSSL backend and both libraries without developer environment variables, and launches both bounded workers. Installed-LDraw acceptance and two-machine acceptance remain explicitly pending; they are not claimed by the passing synthetic/local tests.

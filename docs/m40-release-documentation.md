# M40.4 — Release documentation and repository hygiene

Baseline: clean `23a13a7` (M40.3). No commit, push, tag, publication, or release
cutover was performed. Schema 35 and Protocol 1.5 are unchanged.

## Repository findings and disposition

The audit covered the 1,101 tracked paths, root files, untracked/ignored status,
generated-file extensions, binary signatures, large files, and source/build/test/
documentation references. No unrelated pre-existing changes were present.

Removed only these nine generated root exports:

- `11262-ldraw-aware-proof.obj`
- `11477-ldraw-aware-proof.obj`
- `2780-ldraw-aware-proof.obj`
- `3001-ldraw-aware-proof.obj`
- `3003-ldraw-aware-proof.obj`
- `3622-ldraw-aware-proof.obj`
- `3673-ldraw-aware-proof.obj`
- `3700-ldraw-aware-proof.obj`
- `4274-ldraw-aware-proof.obj`

These total **18,340,919 bytes** in the checkout. Git history is unchanged, so
this does not shrink historical clones. They entered during family-foundation
work (`b23534c`, `13114b8`, `bd90162`). `LDrawSemanticOperandTest` writes
`<id>-ldraw-aware-proof.obj` and defaults to its current working directory unless
`--proof-dir` is supplied. No tracked consumer reads these exports; repository-wide
reference searches found only that writer. CMake, tests, QRC, packaging, and Help
do not require the deleted files. The generator and tests remain intact.

Intentionally retained generated geometry/evidence:

| Files | Classification and reason |
|---|---|
| `antistud-bore-proof/11262-antistud-bore-manufacturing.3mf` | Historical Part geometry proof from `13114b8`; not a current fit guarantee. |
| `antistud-bore-proof/stud-clutch-antistud-bore-perpendicular-coarse-v1.3mf` and matching `-session.json` | Historical fixture/companion definition with no observations; not a live session template. Added a README explaining that boundary. |
| `wall-pocket-proof/BrickSuite-3005-wall-pocket-nominal-prepared.3mf` and `BrickSuite-3024-wall-pocket-nominal-prepared.3mf` | Deliberately documented nominal PreparedMesh inspection examples. |
| `wall-pocket-proof/BrickSuite-wall-pocket-brick-perpendicular-coarse-v1.3mf` and `BrickSuite-wall-pocket-plate-perpendicular-coarse-v1.3mf` | Deliberately documented physical calibration geometry proofs. Added a historical-use caveat to the existing README. |

These six 3MFs are not configured build/test inputs, but their family-proof
purpose warrants retention rather than deletion based on their extension.
None is asserted to be Verified evidence. Generate fresh calibration identities
through the supported service/UI.

Other intentionally retained source/assets:

- `resources/rebrickable/{colors,part_categories,parts,sets}.csv`: catalog data;
  `resources/rebrickable/API/{PartsInSetsResponse,SetDetailsResponse}.txt`: provider
  response examples, not runtime logs or private inventory exports.
- `data/part_reference_manifest.csv` and `data/Part_Reference_Manifest.xlsx`:
  canonical reference data and authoring workbook.
- `docs/part-reference/archive/part_reference_manifest_v0_35.csv` and
  `docs/part-reference/Part_Reference_Manifest_Audit_v0_35.xlsx`: historical audit assets.
- All 52 tracked PNGs and two ICOs: application controls/artwork, current Help/
  README screenshots, or deliberately retained historical evidence. M40.3's
  reference dispositions remain intact; no screenshot was recaptured or deleted.
- Deployment source locks, policy/runtime JSON, the current update manifest,
  licenses, third-party notices, and M37/M39 validation documents: required source
  or historical evidence, unchanged.

After deletion, no tracked OBJ/STL, EXE/DLL, ZIP/tarball, log, DB/SQLite, or Python
bytecode file remains. The remaining binary types are PNG/ICO, the six historical
3MFs, and the two XLSX workbooks above. No unexplained generated binary remains.

Ignored local trees include `build/`, earlier `build-*` directories, `.qtcreator/`,
local `docs/dev/` samples, and local experiment artifacts. These were not deleted.
Some isolated Qt-test/Creator directories deny enumeration in the sandbox; their
contents were not inspected or claimed clean. They are ignored and not tracked.

The largest remaining files are the catalog `parts.csv` (5,750,554 bytes),
`sets.csv` (2,724,411), current/reference archive manifests (718,638 / 713,335),
multi-size application ICO (395,001), and public PartsInSets response example
(300,253). These have intentional data/artwork purposes, not accidental packages.

## Ignore protection

Added root-scoped rules for semantic proof OBJ exports, `.qtcreator/`,
`cmake-build-*`, `CMakeFiles`, `CMakeCache.txt`, `Testing`, `deploy`, and root
`.db`/`.sqlite`/`.sqlite3` files. Existing rules cover build trees, compiled
libraries, logs, bytecode, editor state, credentials, and private sample data.
`git check-ignore --no-index` confirms representative outputs are protected and
`tests/fixtures/example.obj` remains visible. No global OBJ/STL/3MF/CSV/database
extension rule was added; intentional fixtures can still be reviewed normally.
Local exports and validation packages belong beneath ignored build directories.

## Documentation changes

- **README:** current capabilities, direct part-out and Remote limitation,
  WCIB/procurement, Host/Remote, supported printing/calibration limits, Box,
  platform installation/status, dependencies, packaging links, and Support.
  Removed the brittle Part Reference count. All four M40.3 image paths remain.
- **About:** concise workshop/Host/printing description; “Trademarks” heading
  distinguished from installed software notices. Existing three link destinations
  and centralized Support behavior are unchanged.
- **CONTRIBUTING:** current Qt modules/OpenSSL/pinned dependencies, deployment
  links, scoped test execution/LDraw configuration, repository hygiene, Codex
  disclosure, and acceptance expectations. Replaced the nonexistent-future
  SECURITY placeholder with the actual document link.
- **SECURITY:** current public v0.3.0 / upcoming v0.4.0 status; no invented contact
  address or new reporting process.
- **CHANGELOG:** prepared, explicitly Unreleased v0.4.0 user-facing summary and
  limitations. Corrected v0.3.0 to 2026-09-04, supported by the existing update
  manifest and `v0.3.0` tag's commit date. Earlier history remains unchanged.
- **Windows:** normal-user install/update paragraph before developer packaging.
- **macOS:** separate ARM64/Intel installation, 13.0+ target, honest acceptance
  status, ad-hoc signing/non-notarization, per-app confirmation; no Gatekeeper bypass.
- **Linux:** user install path linked separately from official baseline generation
  and local Qt Creator Deploy. No scripts or deployment behavior changed.

Exact README Support wording:

> BrickSuite is free and open-source software. If you find it useful and would like to support continued development, testing, documentation, and maintenance, you can make a voluntary contribution through PayPal.
>
> Support is completely optional and does not unlock additional features or services. Payments are handled by PayPal, not by BrickSuite.

Link: [Support BrickSuite with PayPal](https://www.paypal.com/ncp/payment/WB8RKBVN6DTYW).
This equals `AppConstants::SupportUrl`; application code remains centralized.
No current Donate/Donation wording was found in tracked Markdown/Help.

Exact Codex disclosure:

> BrickSuite development uses OpenAI Codex as one of the tools for code analysis, implementation, testing, review, and documentation. Codex does not replace project review. Contributors are not required to use Codex. AI-generated or AI-assisted changes are held to the same review and testing standards as other code.

Exact test policy:

> Code changes must include appropriate automated tests to be considered for acceptance. New behavior and features require focused tests; bug fixes should include a test reproducing the corrected behavior where practical. Preserve existing tests. Documentation-only changes do not need artificial code tests. Tests must be deterministic and must not depend on private user data or credentials.

## Stale-reference audit and validation

Current-facing v0.3 build guidance and SECURITY v0.2 status were corrected.
Public-release v0.3.0 wording remains intentional until cutover. The historical
v0.2 changelog's “My Loose Inventory” and M40.3's defect record remain historical.
No current BrickVault branding or obsolete Storage-type administration instruction
was found. Development headings and explicitly Unreleased v0.4.0 are intentional.

- Qt 6.10.3 MinGW Release application and explicit `PrintingHelpUxTest` builds pass.
- `PrintingHelpUx` passes, including About link activation, keyboard access,
  Dark/Light contrast, Help resources and responsive image checks.
- Native Release About visually reviewed in Dark and Light using isolated
  temporary settings; no clipping. No user database or credentials used.
- All 25 local Markdown links/images/anchors in the eight edited public/platform
  documents resolve. README image paths, LICENSE/notices, and Help source unchanged.
- Public Online Help, PayPal, GitHub Releases and RF StateSide URLs returned HTTP
  200. GNU LGPL endpoint timed out; its unchanged URL is not claimed live-verified.
- `git diff --check` passes; Schema 35 / Protocol 1.5 unchanged. Full CTest and
  platform packaging/runtime suites were not rerun for this wording/cleanup pass.

Files changed are limited to the documentation listed above, `.gitignore`,
`src/ui/about/AboutDialog.cpp`, the two historical-proof README files, this report,
and the nine deleted OBJ exports. No feature, calibration, storage, fit algorithm,
version/channel, update manifest, screenshot, or packaging implementation changed.

M40.5 still owns final release date/channel, tag, release uploads, artifact
URLs/hashes, update manifest, and Online Help publication verification. Intel
macOS manual acceptance remains pending. No known code/documentation blocker
remains; the GNU endpoint reachability check remains an external validation limit.

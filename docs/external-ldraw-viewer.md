# External LDraw viewer testing

The Tools menu exposes external `.dat` / `.ldr` loading in Release and Debug.
User instructions live in `resources/help/ldraw_models.html`.

`LDrawModelViewerRequest::externalFilePath` is separate from catalog IDs and
resolver candidates. The viewer clears catalog identity for an external request.
`LDrawLibraryService::loadExternalFile` canonicalizes the explicit root path and
uses the existing parser, installed-library index, MPD handling, provenance, and
input limits. Dependencies cannot escape the normal library rules. The external
root's semantic source key is `@external/model`, so its filename cannot masquerade
as a recognized primitive. Installed dependencies retain their normal identities.

The load result retains the external path and a SHA-256 of the root bytes. Both
participate in preparation-cache identity; edited external files cannot reuse an
old result merely because size and timestamp match. No catalog writes occur.
This explicit path/origin boundary can support a future separately authorized
override association without implementing one here.

Preparation uses the existing coordinator and service. External exports keep
Part identity empty and offer nominal PreparedMesh once validated. Profile
ManufacturingMesh and Auto Fit are unavailable without trusted catalog/profile
context. The source-based Auto Fit resolver also rejects external origin even if
a caller supplies a guessed Part number.

New loads clear geometry, analysis, preparation, and export availability. Async
load, analysis, and manufacturing results are generation-checked. Preparation
completion also requires an active preparation in this viewer, preventing a late
completion from a closed viewer from matching a reopened viewer's generation.

Validation commands (Release tests only):

```text
ctest --test-dir <release-build> -R "ExternalLDrawViewer|LDrawLibrary|ManufacturingMeshService" --output-on-failure
<release-build>/ManufacturingMeshServiceTest --external-files <installed-LDraw-root>
ctest --test-dir <release-build> --output-on-failure
```

The optional installed-library check copies 3001 and 23422 into a temporary
directory, checks nominal preparation/3MF export and rejected Source Coverage,
then prepares a `.ldr` referencing installed 3001 and returns to catalog loading.
Synthetic loader tests cover missing dependencies, traversal, and edited content.
The offscreen viewer test covers failed preparation/load, switching, reopening,
and late completion. Actual GPU rendering and native file-picker interaction
remain manual acceptance checks.

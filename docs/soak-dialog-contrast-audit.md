# Post-M39 dialog contrast audit

Baseline: `5de3588`; Qt 6.10.3; Linux. Schema 35 / Protocol 1.5 unchanged.

## Findings and scope

- **Needs correction → corrected:** About's two rich QLabel links and Set Details' asynchronously populated Brickset link retain Qt's parsed blue anchor foreground on a dark background. Changing the Link palette alone did not repair the rendered About document. `ThemeRichTextLabel` supplies palette-derived anchor CSS and updates on palette/style changes, preserving text selection, URLs and underlines.
- **Needs correction → corrected:** the shared Dark text-selection pair was white on bright blue (3.96:1). The existing Help calculation now lives in `ThemeContrast`, and ThemeManager uses it for shared selections (5.30:1). Existing teal selected rows/buttons remain unchanged (7.39:1 against white).
- **Pass:** Light blue anchors remain unchanged. Help uses the same contrast utility and retains its in-place document refresh and search selections.
- **Pass, source review:** no additional explicit warning/error/success foregrounds in dialog code. These messages use theme text roles, icons and wording; their semantics are preserved. The only additional explicit dialog stylesheet colors are gray image borders, not text. Part Reference's selected card border already uses `palette(highlight)`.
- **Pass, existing contrast handling:** Inventory/Build color text uses `ColorComboHelper::readableColor`; combo swatches intentionally retain physical color identities. The 3D viewport's colored XYZ axis labels are graphics annotations, not dialog body/status text, and are unchanged.
- **Disabled controls:** existing gray disabled button text against the dark panel is approximately 3.1:1; disabled palette text against input base is approximately 3.8:1. No new disabled-color overrides or changes to enabled state were introduced. Normal Qt disabled styles and existing stylesheet distinctions are preserved.

All classes below were inspected through their implementation and the shared theme rules. “Pass (source)” is **not** a claim that every populated or remote runtime state was visually exercised. Selection correction applies to inherited text controls throughout this inventory. Hyperlinks absent from a surface are **Not applicable**. Source-only remote/provider/data-dependent cases remain candidates for normal soak acceptance.

## Named dialog inventory (50 classes)

| Dialog | Implementation | Finding | Runtime coverage |
|---|---|---|---|
| AboutDialog | `src/ui/about/AboutDialog.cpp` | Needs correction → corrected: rich anchors | Dark and Light, synthetic/empty state |
| AddInventoryDialog | `src/ui/inventory/AddInventoryDialog.cpp` | Pass (source); inherited selection correction where present | Dark and Light, synthetic/empty state |
| AddPartReferenceDialog | `src/ui/parts/AddPartReferenceDialog.cpp` | Pass (source); inherited selection correction where present | Source review only; state-dependent |
| AllocateBuildRequirementDialog | `src/ui/builds/AllocateBuildRequirementDialog.cpp` | Pass (source); inherited selection correction where present | Source review only; state-dependent |
| BrickLinkColorMappingStatusDialog | `src/ui/mappings/BrickLinkColorMappingStatusDialog.cpp` | Pass (source); inherited selection correction where present | Dark and Light, synthetic/empty state |
| BrickLinkPartMappingTestDialog | `src/ui/mappings/BrickLinkPartMappingTestDialog.cpp` | Pass (source); inherited selection correction where present | Dark and Light, synthetic/empty state |
| BrickLinkWantedListResultDialog | `src/ui/procurement/BrickLinkWantedListResultDialog.cpp` | Pass (source); inherited selection correction where present | Dark and Light, synthetic/empty state |
| BricksetInstructionsDialog | `src/ui/catalog/BricksetInstructionsDialog.cpp` | Pass (source); inherited selection correction where present | Dark and Light, synthetic/empty state |
| CatalogCollectionDialog | `src/ui/collection/CatalogCollectionDialog.cpp` | Pass (source); inherited selection correction where present | Source review only; state-dependent |
| CollectionExportDialog | `src/ui/collection/CollectionExportDialog.cpp` | Pass (source); inherited selection correction where present | Dark and Light, synthetic/empty state |
| CollectionItemDialog | `src/ui/collection/CollectionItemDialog.cpp` | Pass (source); inherited selection correction where present | Source review only; state-dependent |
| CorrectInventoryDialog | `src/ui/inventory/CorrectInventoryDialog.cpp` | Pass (source); inherited selection correction where present | Source review only; state-dependent |
| DatabaseStatusDialog | `src/ui/database/DatabaseStatusDialog.cpp` | Pass (source); inherited selection correction where present | Dark and Light, synthetic/empty state |
| DisassembleSetDialog | `src/ui/builds/DisassembleSetDialog.cpp` | Pass (source); inherited selection correction where present | Source review only; state-dependent |
| EditBuildDialog | `src/ui/builds/EditBuildDialog.cpp` | Pass (source); inherited selection correction where present | Dark and Light, synthetic/empty state |
| EditBuildRequirementDialog | `src/ui/builds/EditBuildRequirementDialog.cpp` | Pass (source); inherited selection correction where present | Source review only; state-dependent |
| EditInventoryDialog | `src/ui/inventory/EditInventoryDialog.cpp` | Pass (source); inherited selection correction where present | Dark and Light, synthetic/empty state |
| FitCalibrationDialog | `src/ui/parts/FitCalibrationDialog.cpp` | Pass (source); inherited selection correction where present | Dark and Light, synthetic/empty state |
| FoundInventoryDialog | `src/ui/inventory/FoundInventoryDialog.cpp` | Pass (source); inherited selection correction where present | Source review only; state-dependent |
| GlobalRebrickableImportDialog | `src/ui/import/GlobalRebrickableImportDialog.cpp` | Pass (source); inherited selection correction where present | Dark and Light, synthetic/empty state |
| HelpDialog | `src/ui/help/HelpDialog.cpp` | Pass; shared helper extraction, existing Help regression coverage | Dark and Light, synthetic/empty state |
| ImportInventoryDialog | `src/ui/inventory/ImportInventoryDialog.cpp` | Pass (source); inherited selection correction where present | Source review only; state-dependent |
| ImportPullListDialog | `src/ui/builds/ImportPullListDialog.cpp` | Pass (source); inherited selection correction where present | Source review only; state-dependent |
| InteractiveBuildPullingDialog | `src/ui/builds/InteractiveBuildPullingDialog.cpp` | Pass (source); inherited selection correction where present | Dark and Light, synthetic/empty state |
| InventoryColorAuditReviewDialog | `src/ui/inventory/InventoryColorAuditReviewDialog.cpp` | Pass (source); inherited selection correction where present | Source review only; state-dependent |
| InventoryExportDialog | `src/ui/inventory/InventoryExportDialog.cpp` | Pass (source); inherited selection correction where present | Dark and Light, synthetic/empty state |
| InventoryHistoryDialog | `src/ui/inventory/InventoryHistoryDialog.cpp` | Pass (source); inherited selection correction where present | Dark and Light, synthetic/empty state |
| InventoryImportPreviewDialog | `src/ui/inventory/InventoryImportPreviewDialog.cpp` | Pass (source); inherited selection correction where present | Source review only; state-dependent |
| LDrawModelViewerWindow | `src/ui/parts/LDrawModelViewerWindow.cpp` | Pass (source); inherited selection correction where present | Dark and Light, synthetic/empty state |
| LogViewerDialog | `src/ui/logging/LogViewerDialog.cpp` | Pass (source); inherited selection correction where present | Dark and Light, synthetic/empty state |
| LostInventoryDialog | `src/ui/inventory/LostInventoryDialog.cpp` | Pass (source); inherited selection correction where present | Source review only; state-dependent |
| ManufacturerEditDialog | `src/ui/reference/ManufacturerEditDialog.cpp` | Pass (source); inherited selection correction where present | Dark and Light, synthetic/empty state |
| ManufacturingMeshDiagnosticDialog | `src/ui/parts/ManufacturingMeshDiagnosticDialog.cpp` | Pass (source); inherited selection correction where present | Source review only; state-dependent |
| MarkLostInventoryDialog | `src/ui/inventory/MarkLostInventoryDialog.cpp` | Pass (source); inherited selection correction where present | Source review only; state-dependent |
| MinifigDetailsDialog | `src/ui/catalog/MinifigDetailsDialog.cpp` | Pass (source); inherited selection correction where present | Source review only; state-dependent |
| MissingPartsExportDialog | `src/ui/builds/MissingPartsExportDialog.cpp` | Pass (source); inherited selection correction where present | Dark and Light, synthetic/empty state |
| MoveInventoryDialog | `src/ui/inventory/MoveInventoryDialog.cpp` | Pass (source); inherited selection correction where present | Dark and Light, synthetic/empty state |
| PartDetailsDialog | `src/ui/parts/PartDetailsDialog.cpp` | Pass (source); inherited selection correction where present | Source review only; state-dependent |
| PartReferenceDialog | `src/ui/parts/PartReferenceDialog.cpp` | Pass (source); inherited selection correction where present | Dark and Light, synthetic/empty state |
| PartResolverTestDialog | `src/ui/parts/PartResolverTestDialog.cpp` | Pass (source); inherited selection correction where present | Dark and Light, synthetic/empty state |
| PrintCapabilityAuditDialog | `src/ui/parts/PrintCapabilityAuditDialog.cpp` | Pass (source); inherited selection correction where present | Dark and Light, synthetic/empty state |
| ProcurementPreviewDialog | `src/ui/procurement/ProcurementPreviewDialog.cpp` | Pass (source); inherited selection correction where present | Dark and Light, synthetic/empty state |
| ReferenceDataDialog | `src/ui/reference/ReferenceDataDialog.cpp` | Pass (source); inherited selection correction where present | Dark and Light, synthetic/empty state |
| RemoteCollectionMutationDialog | `src/ui/collection/RemoteCollectionMutationDialog.cpp` | Pass (source); inherited selection correction where present | Source review only; state-dependent |
| RemoteInventoryMutationDialog | `src/ui/inventory/RemoteInventoryMutationDialog.cpp` | Pass (source); inherited selection correction where present | Source review only; state-dependent |
| RemoveInventoryDialog | `src/ui/inventory/RemoveInventoryDialog.cpp` | Pass (source); inherited selection correction where present | Source review only; state-dependent |
| SetDetailsDialog | `src/ui/catalog/SetDetailsDialog.cpp` | Needs correction → corrected: rich anchors | Source review only; state-dependent |
| SetImportPreviewDialog | `src/ui/builds/SetImportPreviewDialog.cpp` | Pass (source); inherited selection correction where present | Source review only; state-dependent |
| SettingsDialog | `src/ui/settings/SettingsDialog.cpp` | Pass (source); inherited selection correction where present | Dark and Light, synthetic/empty state |
| StorageLocationDialog | `src/ui/storage/StorageLocationDialog.cpp` | Pass (source); inherited selection correction where present | Dark and Light, synthetic/empty state |

## Inline custom dialogs

The following construction sites supplement the named classes. All use ordinary theme-inheriting controls; no separate unsafe anchor or text-color override was found. Standard table selection is unchanged; inherited text selections receive the shared correction.

| Source construction site | Title expression / context | Finding |
|---|---|---|
| `src/ui/builds/BuildsWidget.cpp:1702` | `QStringLiteral("Edit Build")` | Pass (source) |
| `src/ui/builds/BuildsWidget.cpp:1851` | `QStringLiteral("Build")` | Pass (source) |
| `src/ui/builds/BuildsWidget.cpp:2596` | `QStringLiteral("Delete Build Requirement")` | Pass (source) |
| `src/ui/builds/BuildsWidget.cpp:2618` | `QStringLiteral("Edit Build Requirement")` | Pass (source) |
| `src/ui/builds/BuildsWidget.cpp:2734` | `QStringLiteral("Store Spare")` | Pass (source) |
| `src/ui/builds/BuildsWidget.cpp:2769` | `QStringLiteral("Allocate Build Requirement")` | Pass (source) |
| `src/ui/builds/BuildsWidget.cpp:2834` | `writable ? QStringLiteral("Host Build Pulling")                                     : QStringLiteral("Host Build Pulling (Read-only)")` | Pass (source) |
| `src/ui/builds/BuildsWidget.cpp:3277` | `"Allocate Available"` | Pass (source) |
| `src/ui/builds/BuildsWidget.cpp:3676` | `"Store Spare"` | Pass (source) |
| `src/ui/builds/WhatCanIBuildWidget.cpp:296` | `QStringLiteral("Missing Parts — %1").arg(x.setNumber)` | Pass (source) |
| `src/ui/builds/WhatCanIBuildWidget.cpp:299` | `sourcesOnly?QStringLiteral("Advisory Collection Sources — %1").arg(x.setNumber):QStringLiteral("Missing Parts — %1").arg(x.setNumber)` | Pass (source) |
| `src/ui/catalog/MinifigDetailsDialog.cpp:387` | `"Create Build From Stock"` | Pass (source) |
| `src/ui/catalog/MinifigsCatalogWidget.cpp:372` | `"Import Rebrickable Minifig Themes"` | Pass (source) |
| `src/ui/catalog/SetDetailsDialog.cpp:622` | `"Create Build From Stock"` | Pass (source) |
| `src/ui/collection/MyCollectionWidget.cpp:726` | `QStringLiteral("Disassemble Collection Item")` | Pass (source) |
| `src/ui/parts/FitCalibrationDialog.cpp:145` | `"New Calibration Workspace"` | Pass (source) |
| `src/ui/parts/FitCalibrationDialog.cpp:370` | `"Custom Calibration Package"` | Pass (source) |
| `src/ui/parts/LDrawModelViewerWindow.cpp:279` | `tr("Export 3D Model")` | Pass (source) |
| `src/ui/storage/StorageWidget.cpp:459` | `         "Add Storage Location"` | Pass (source) |
| `src/ui/storage/StorageWidget.cpp:600` | `"Edit Storage Location"` | Pass (source) |

## Standard Qt prompt inventory

These files contain QMessageBox, QInputDialog, QFileDialog or QProgressDialog uses (these are owning files, not a count of independent dialogs). Message text, informative/detailed text and rich-text entry points were inspected. No additional owned anchor HTML or explicit text-color styling was found. The Host token detail box is plain text; no token was generated or captured. Settings contains API and Host/Remote controls; backup/restore uses file/confirmation dialogs rather than dedicated subclasses.

| Implementation | Qt prompt types |
|---|---|
| `src/app/Application.cpp` | QMessageBox |
| `src/main.cpp` | QMessageBox |
| `src/ui/MainWindow.cpp` | QFileDialog, QInputDialog, QMessageBox |
| `src/ui/builds/AllocateBuildRequirementDialog.cpp` | QMessageBox |
| `src/ui/builds/BuildsWidget.cpp` | QFileDialog, QMessageBox |
| `src/ui/builds/DisassembleSetDialog.cpp` | QMessageBox |
| `src/ui/builds/EditBuildDialog.cpp` | QMessageBox |
| `src/ui/builds/EditBuildRequirementDialog.cpp` | QMessageBox |
| `src/ui/builds/ImportPullListDialog.cpp` | QMessageBox |
| `src/ui/builds/InteractiveBuildPullingDialog.cpp` | QMessageBox |
| `src/ui/builds/MissingPartsExportDialog.cpp` | QFileDialog, QMessageBox |
| `src/ui/builds/SetImportPreviewDialog.cpp` | QMessageBox |
| `src/ui/builds/WhatCanIBuildWidget.cpp` | QMessageBox |
| `src/ui/catalog/MinifigDetailsDialog.cpp` | QFileDialog, QMessageBox |
| `src/ui/catalog/MinifigsCatalogWidget.cpp` | QFileDialog, QMessageBox |
| `src/ui/catalog/PartsCatalogWidget.cpp` | QFileDialog, QMessageBox |
| `src/ui/catalog/SetDetailsDialog.cpp` | QFileDialog, QMessageBox |
| `src/ui/catalog/SetsCatalogWidget.cpp` | QFileDialog, QMessageBox |
| `src/ui/collection/CatalogCollectionDialog.cpp` | QMessageBox |
| `src/ui/collection/CollectionExportDialog.cpp` | QFileDialog, QMessageBox |
| `src/ui/collection/CollectionItemDialog.cpp` | QMessageBox |
| `src/ui/collection/MyCollectionWidget.cpp` | QMessageBox |
| `src/ui/database/DatabaseStatusDialog.cpp` | QMessageBox |
| `src/ui/import/GlobalRebrickableImportDialog.cpp` | QFileDialog |
| `src/ui/inventory/AddInventoryDialog.cpp` | QInputDialog, QMessageBox |
| `src/ui/inventory/CorrectInventoryDialog.cpp` | QMessageBox |
| `src/ui/inventory/EditInventoryDialog.cpp` | QMessageBox |
| `src/ui/inventory/FoundInventoryDialog.cpp` | QMessageBox |
| `src/ui/inventory/ImportInventoryDialog.cpp` | QFileDialog, QMessageBox |
| `src/ui/inventory/InventoryColorAuditReviewDialog.cpp` | QMessageBox |
| `src/ui/inventory/InventoryExportDialog.cpp` | QFileDialog, QMessageBox |
| `src/ui/inventory/InventoryImportPreviewDialog.cpp` | QFileDialog, QInputDialog, QMessageBox |
| `src/ui/inventory/MarkLostInventoryDialog.cpp` | QMessageBox |
| `src/ui/inventory/MoveInventoryDialog.cpp` | QMessageBox |
| `src/ui/inventory/MyInventoryWidget.cpp` | QMessageBox |
| `src/ui/inventory/RemoteInventoryMutationDialog.cpp` | QMessageBox |
| `src/ui/inventory/RemoveInventoryDialog.cpp` | QMessageBox |
| `src/ui/logging/LogViewerDialog.cpp` | QMessageBox |
| `src/ui/mappings/BrickLinkPartMappingTestDialog.cpp` | QMessageBox |
| `src/ui/parts/AddPartReferenceDialog.cpp` | QMessageBox |
| `src/ui/parts/FitCalibrationDialog.cpp` | QFileDialog, QInputDialog, QMessageBox, QProgressDialog |
| `src/ui/parts/LDrawModelViewerWindow.cpp` | QFileDialog, QMessageBox |
| `src/ui/parts/ManufacturingMeshDiagnosticDialog.cpp` | QFileDialog, QMessageBox |
| `src/ui/parts/PartDetailsDialog.cpp` | QMessageBox |
| `src/ui/parts/PartReferenceDialog.cpp` | QMessageBox |
| `src/ui/parts/PrintCapabilityAuditDialog.cpp` | QFileDialog, QMessageBox |
| `src/ui/procurement/BrickLinkWantedListResultDialog.cpp` | QFileDialog |
| `src/ui/procurement/ProcurementPreviewDialog.cpp` | QMessageBox |
| `src/ui/reference/ManufacturerEditDialog.cpp` | QMessageBox |
| `src/ui/reference/ReferenceDataDialog.cpp` | QMessageBox |
| `src/ui/settings/SettingsDialog.cpp` | QFileDialog, QInputDialog, QMessageBox |
| `src/ui/storage/StorageWidget.cpp` | QMessageBox |

## Validation

- Linux XCB desktop checks used the production `Application` initialization, actual menu actions and actual dialog classes linked from Release objects. The disposable driver opened dialogs, captured widget renders and canceled them; it did not submit business operations. Separate temporary XDG data/config/cache and a verified private Secret Service isolated the run from the real database and credentials. Synthetic inventory/build/workspace records were created only in that disposable database.
- Reviewed Dark and Light renders for **28 named classes**, marked above. Settings includes General, Builds, Appearance, Database Backup, This Computer, Host Client, Devices, Rebrickable, Brickset and 3D Models. Both Storage modes were viewed. Inventory History and Pulling tables had no movements/allocations; export, procurement, instructions and calibration surfaces included empty/unconfigured states. 3D Viewer controls were checked without running geometry preparation.
- About's header, version, copyright, body, license, both underlined links, notice and Close button are readable. Standalone live-switch renders and document assertions cover Dark → Light → Dark. Link text selection is retained through changes. Actual mouse clicks on both About links were intercepted with `QDesktopServices::setUrlHandler` and verified against their unchanged destinations; no browser/network request was launched.
- Backup/Restore file dialogs were opened and canceled, but native OS picker contents are outside `QWidget::grab`; blank wrapper captures are **not** a visual pass. Their Qt integration and confirmation messages were source-reviewed. No backup or restore was performed.
- Set Details' live provider response was not fetched: its corrected shared label was tested with dynamically supplied synthetic Brickset markup. No external API keys, Host pairing, remote mutations or token-generation prompts were exercised. The other 22 named classes and inline dialogs have source coverage, not exhaustive visual acceptance of every populated/error state.
- About/Help regression test (`PrintingHelpUx`): **passed Release and Debug**, and **passed Release on native XCB**. Coverage includes parsed label/document contrast, original Light links, actual About URL activation, mouse selection, active/inactive selection palettes, disabled button contrast, existing Help navigation/search/F1, and live refresh without accumulated CSS wrappers.
- Existing `ColorComboHelper` and `PartSearchCompleterHelper`: **passed**. Part Search initially hit the sandbox's test-directory write restriction; rerun with permission passed using its unique test-mode database.
- **Release and Debug application builds passed** with Qt 6.10.3. Existing third-party/debug and deprecated test-focus warnings remain; no new production compile error. The isolated GUI log contained expected private-desktop portal/DBus shutdown notices. The log viewer's no-log-file state was readable because the disposable driver does not install the normal executable's file-log handler. No theme-related runtime error was observed.
- `git diff --check`: **passed**. Schema **35**, Protocol **1.5**. No deployment, database/protocol/credential logic, geometry or user data changed. No commit or push.

## Remaining acceptance

No additional concrete dialog contrast defect remains open from this audit. Windows/macOS should receive the same shared Qt files and visually check About, Set Details, text selections and live theme changes during normal soak testing. Linux native file pickers, remote/provider-populated states and the source-only rows above still require normal runtime acceptance; source review is not a substitute for those checks.

## Changed files

- `src/settings/ThemeContrast.h`: extracted the existing Help contrast calculation.
- `src/settings/ThemeManager.cpp`: palette-derived Dark link and selection roles.
- `src/ui/common/ThemeRichTextLabel.h`: shared parsed-anchor styling and live refresh with selection preservation.
- `src/ui/about/AboutDialog.cpp`: use the shared label for wrapped text.
- `src/ui/catalog/SetDetailsDialog.cpp`, `src/ui/catalog/SetDetailsDialog.h`: use the shared label for dynamic provider links.
- `src/ui/help/HelpDialog.cpp`: reuse the extracted calculation; existing Help behavior retained.
- `tests/PrintingHelpUxTest.cpp`, `CMakeLists.txt`: About/shared-theme regression coverage and production metadata for the test build.
- `docs/soak-dialog-contrast-audit.md`: this inventory, findings and validation record.

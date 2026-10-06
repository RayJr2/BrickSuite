# M40.3 — Screenshot refresh record

Baseline: `ebac049` (M40.2), initially clean. Captured on Windows using the Qt 6.10.3 MinGW **Release** application objects and native widgets. Primary theme remains **Dark**; only `settings_appearance_light.png` uses Light for the Appearance comparison. No Debug/Test menu is present in the final product screenshots.

## Coverage and disposition

All 30 Help topics were reviewed: **27 illustrated, 3 justified exceptions** below. The set changes from **74 image references / 94 QRC image entries** to **39 references / 34 unique images / 34 QRC entries**. Of the final Help files, **22 existing filenames were refreshed and 12 new files added**. No old active product screenshot was retained unchanged: review found stale actions, labels, data, or presentation in each selected predecessor.

The physical Help image directory contains 35 PNGs: 34 current product screenshots plus `builds_release_spares_complete.png`, retained solely because `PUBLIC_RELEASE_AUDIT.md` references that historical evidence. It is not in the Help QRC or any current Help page. `docs/images/m39/` and all other historical evidence remain unchanged.

Counts below are image occurrences per page; reused files count once on each page. “Refreshed” describes page coverage even when the page now reuses a differently named current screenshot.

| Help topic | Before | Final | Classification | Main UI / final image files | Theme | Exception rationale |
|---|---:|---:|---|---|---|---|
| [Allocate Available](../resources/help/allocate_available.html) | 4 | 1 | Existing screenshot refreshed | `builds_allocated.png` | Dark | — |
| [Backup / Restore](../resources/help/backup_restore.html) | 4 | 1 | Existing screenshot refreshed | `settings_database_backup.png` | Dark | — |
| [BrickSuite Server](../resources/help/bricksuite_server.html) | 0 | 1 | New screenshot added | `bricksuite_server.png` | Dark | — |
| [Builds](../resources/help/builds.html) | 8 | 3 | Existing screenshot refreshed | `builds_overview.png`, `what_can_i_build.png`, `builds_interactive_pulling.png` | Dark | — |
| [Calibration Packages & Fit Profiles](../resources/help/calibration_packages.html) | 0 | 1 | New screenshot added | `fit_calibration_package.png` | Dark | — |
| [Database Status & Integrity](../resources/help/database_status.html) | 1 | 1 | Existing screenshot refreshed | `database_status_integrity.png` | Dark | — |
| [LEGO Fit Calibration](../resources/help/fit_calibration.html) | 0 | 1 | New screenshot added | `fit_calibration_workspace.png` | Dark | — |
| [Getting Started](../resources/help/getting_started.html) | 3 | 1 | Existing screenshot refreshed | `getting_started_workspace.png` | Dark | — |
| [BrickSuite Help](../resources/help/index.html) | 0 | 0 | No screenshot — justified exception | — | — | Navigation index: screenshots would duplicate the immediately linked workflow pages without explaining a Help Home operation. |
| [My Inventory](../resources/help/inventory.html) | 5 | 2 | Existing screenshot refreshed | `inventory_overview.png`, `inventory_add.png` | Dark | — |
| [LDraw 3D Models](../resources/help/ldraw_models.html) | 0 | 2 | New screenshot added | `settings_ldraw.png`, `ldraw_viewer_source.png` | Dark | — |
| [Local Printable Override / External Repair](../resources/help/local_printable_override.html) | 0 | 1 | New screenshot added | `ldraw_viewer_source.png` | Dark | — |
| [Application Log](../resources/help/logging.html) | 3 | 1 | Existing screenshot refreshed | `application_log_viewer.png` | Dark | — |
| [Lost / Found](../resources/help/lost_found.html) | 5 | 1 | Existing screenshot refreshed | `lost_found_report_lost.png` | Dark | — |
| [Minifigs Catalog](../resources/help/minifigs_catalog.html) | 1 | 1 | Existing screenshot refreshed | `minifigs_catalog_overview.png` | Dark | — |
| [Missing Parts](../resources/help/missing_parts.html) | 4 | 1 | Existing screenshot refreshed | `missing_parts_preview.png` | Dark | — |
| [MOCs](../resources/help/mocs.html) | 4 | 1 | Existing screenshot refreshed | `builds_overview.png` | Dark | — |
| [My Collection](../resources/help/my_collection.html) | 1 | 1 | Existing screenshot refreshed | `my_collection_overview.png` | Dark | — |
| [Part Reference](../resources/help/part_reference.html) | 2 | 2 | Existing screenshot refreshed | `part_reference_gallery.png`, `part_reference_dimension_grid.png` | Dark | — |
| [Parts Catalog](../resources/help/parts_catalog.html) | 3 | 1 | Existing screenshot refreshed | `parts_catalog_overview.png` | Dark | — |
| [Prepare for Printing](../resources/help/prepare_printing.html) | 0 | 2 | New screenshot added | `prepare_printing_ready.png`, `printing_nominal_export.png` | Dark | — |
| [Print Capability / Troubleshooting](../resources/help/print_troubleshooting.html) | 0 | 0 | No screenshot — justified exception | — | — | A diagnostic reference covering many distinct failures. A single Ready/Not Ready example would be arbitrary; Release documentation must not illustrate the Debug-only audit as an available Release action. Viewer and nominal examples are immediately linked in the printing topics. |
| [3D Printing](../resources/help/printing.html) | 0 | 1 | New screenshot added | `prepare_printing_ready.png` | Dark | — |
| [Quick Start](../resources/help/quick_start.html) | 5 | 3 | Existing screenshot refreshed | `settings_apis_overview.png`, `rebrickable_import_preview.png`, `inventory_add.png` | Dark | — |
| [Rebrickable Import](../resources/help/rebrickable_import.html) | 6 | 1 | Existing screenshot refreshed | `rebrickable_import_preview.png` | Dark | — |
| [Lists & Reference Data](../resources/help/reference_data.html) | 1 | 1 | Existing screenshot refreshed | `reference_data_manufacturers.png` | Dark | — |
| [Sets Catalog](../resources/help/sets_catalog.html) | 4 | 3 | Existing screenshot refreshed | `sets_catalog.png`, `sets_catalog_details.png`, `set_part_out.png` | Dark | — |
| [Settings](../resources/help/settings.html) | 6 | 2 | Existing screenshot refreshed | `settings_appearance.png`, `settings_appearance_light.png` | Dark + Light | — |
| [Storage](../resources/help/storage.html) | 4 | 2 | Existing screenshot refreshed | `storage_hierarchy.png`, `storage_add_location.png` | Dark | — |
| [Troubleshooting](../resources/help/troubleshooting.html) | 0 | 0 | No screenshot — justified exception | — | — | Cross-workflow symptom/reference index. An arbitrary error dialog could imply a diagnosis; each relevant illustrated workflow and the diagnostic pages are linked instead. |

Previously unillustrated topics now covered: **BrickSuite Server, Calibration Packages & Fit Profiles, LEGO Fit Calibration, LDraw 3D Models, Local Printable Override / External Repair, Prepare for Printing, and 3D Printing Overview**. Their new screenshots and reuse are identified in the matrix. The other three previously unillustrated topics are the individually justified exceptions; Ray retains acceptance of those choices.

## Final image inventory

All entries below are Windows Release native PNG captures, visually reviewed at full size. Workshop, Inventory, Set, Minifig, provider-enrichment, and process context are synthetic. Part thumbnails use installed LDraw geometry rendered through the existing viewer, and Part Reference uses public built-in definitions. Synthetic Set/Minifig records deliberately have no provider photo; their native “No Image” state is not a missing Help resource.

| Final file under `resources/help/images/` | Disposition | Topic/use | Theme |
|---|---|---|---|
| `application_log_viewer.png` | Refreshed | logging | Dark |
| `bricksuite_server.png` | New | bricksuite_server | Dark |
| `builds_allocated.png` | New | allocate_available | Dark |
| `builds_interactive_pulling.png` | Refreshed | builds | Dark |
| `builds_overview.png` | Refreshed | builds, mocs | Dark |
| `database_status_integrity.png` | Refreshed | database_status | Dark |
| `fit_calibration_package.png` | New | calibration_packages | Dark |
| `fit_calibration_workspace.png` | New | fit_calibration | Dark |
| `getting_started_workspace.png` | Refreshed | getting_started | Dark |
| `inventory_add.png` | Refreshed | inventory, quick_start | Dark |
| `inventory_overview.png` | Refreshed | inventory | Dark |
| `ldraw_viewer_source.png` | New | ldraw_models, local_printable_override | Dark |
| `lost_found_report_lost.png` | Refreshed | lost_found | Dark |
| `minifigs_catalog_overview.png` | Refreshed | minifigs_catalog | Dark |
| `missing_parts_preview.png` | New | missing_parts | Dark |
| `my_collection_overview.png` | Refreshed | my_collection | Dark |
| `part_reference_dimension_grid.png` | Refreshed | part_reference | Dark |
| `part_reference_gallery.png` | Refreshed | part_reference | Dark |
| `parts_catalog_overview.png` | Refreshed | parts_catalog | Dark |
| `prepare_printing_ready.png` | New | prepare_printing, printing | Dark |
| `printing_nominal_export.png` | New | prepare_printing | Dark |
| `rebrickable_import_preview.png` | Refreshed | quick_start, rebrickable_import | Dark |
| `reference_data_manufacturers.png` | Refreshed | reference_data | Dark |
| `set_part_out.png` | New | sets_catalog | Dark |
| `sets_catalog.png` | Refreshed | sets_catalog | Dark |
| `sets_catalog_details.png` | Refreshed | sets_catalog | Dark |
| `settings_apis_overview.png` | Refreshed | quick_start | Dark |
| `settings_appearance.png` | Refreshed | settings | Dark |
| `settings_appearance_light.png` | New | settings | Light |
| `settings_database_backup.png` | Refreshed | backup_restore | Dark |
| `settings_ldraw.png` | New | ldraw_models | Dark |
| `storage_add_location.png` | Refreshed | storage | Dark |
| `storage_hierarchy.png` | Refreshed | storage | Dark |
| `what_can_i_build.png` | New | builds | Dark |

README keeps its existing four image references and unchanged prose. All four PNGs were refreshed from the same native captures: `bricksuite_inventory.png`, `bricksuite_my_collection.png`, `bricksuite_part_reference.png`, and `bricksuite_interactive_pulling.png`. No new gallery section was added.

## Workflow review

- **Inventory / Storage:** current My Inventory label and actions, resolved Add Part controls, hierarchy with Box and valid leaves, and Add Storage with Box selected.
- **Sets:** current modeless Set Details, synthetic Brickset enrichment, movable splitter, Catalog Parts List, and direct Part Out action. Direct part-out shows one copy, catalog spares included, Bin default, 36 required + 2 spare pieces = 38; it is visibly separate from Collection disassembly.
- **Builds / procurement:** six synthetic MOC requirements; real Allocate Available reserves 52 of 60 pieces, leaving eight Red 3001 pieces missing; pulling has zero recorded pulls. What Can I Build produces a real local result. Procurement shows unresolved provider mappings honestly and does not claim that XML is ready.
- **Printing:** actual installed 3001 Source load and native preparation reach Ready. Nominal export says Fit Profile is not applicable. External-repair controls are shown without fabricating an accepted override, fit mapping, or Verified evidence.
- **Calibration:** the real package-generation service generates and registers a Standard Stud coarse session. All observations remain zero, actual print orientation unconfirmed, and nothing Verified. The second image shows the Recommended Calibration Package selection, not a claim of completed multi-family printing.
- **Host / Settings:** unpaired Host Client setup with localhost example and empty credentials/fingerprint; actual Appearance page in both themes; installed LDraw settings; empty API key field. No live pairing or provider service was exercised.
- **Maintenance:** the disposable database backup and read-only integrity check succeed at Schema 35. Backup Help uses the current policy screen; obsolete native file pickers and restore-completion images were deliberately removed. No live database restore was performed. Application Log contains only synthetic documentation messages.
- **Readability:** final captures were reviewed for clipping, dark contrast, selection/disabled text, privacy, and current controls. Windows native widget minimum sizes explain blank space on simple Settings pages. Normal scrollable tables/cards remain scrollable; the wider Dimension Grid capture keeps every standard-width column visible. macOS/Linux chrome was not captured.

## Removed assets

Repository-wide tracked-text filename checks preceded deletion. **71 obsolete Help PNGs and 4 unreferenced README-directory PNGs** were removed; no files used outside product documentation were removed. Old sequential allocation, import, Lost/Found, backup/restore, and Build lifecycle images were reduced to the current representative states. Nearby screenshot-specific examples were adjusted or identified as separate textual examples; workflow rules remain unchanged.

Under `resources/help/images/`:

- `allocate_after_manual.png`
- `allocate_available_result.png`
- `allocate_before.png`
- `allocate_manual_dialog.png`
- `application_log_clear.png`
- `application_log_folder.png`
- `backup_restore_backup.png`
- `backup_restore_backup_complete.png`
- `backup_restore_restore.png`
- `backup_restore_restore_confirmation.png`
- `backup_restore_successful.png`
- `builds_actions.png`
- `builds_after_reconciled.png`
- `builds_auto_allocate.png`
- `builds_completed.png`
- `builds_complete_set_requirements.png`
- `builds_disassembled.png`
- `builds_disassemble_set.png`
- `builds_export_pull_list.png`
- `builds_import_pull_list_pulled.png`
- `builds_missing_parts.png`
- `builds_new_complete_set.png`
- `builds_new_complete_set_preview.png`
- `builds_planned_details.png`
- `builds_reconcile.png`
- `builds_release_spares.png`
- `builds_requirements.png`
- `getting_started_inventory.png`
- `getting_started_storage.png`
- `inventory_actions.png`
- `inventory_edit.png`
- `inventory_history.png`
- `inventory_move.png`
- `inventory_part_resolver.png`
- `lost_found_history.png`
- `lost_found_inventory_before.png`
- `lost_found_mark_lost_confirmation.png`
- `lost_found_return.png`
- `missing_parts_allocate_result.png`
- `missing_parts_csv.png`
- `missing_parts_export.png`
- `missing_parts_requirements.png`
- `mocs_import_csv.png`
- `mocs_new_build.png`
- `mocs_ready.png`
- `mocs_requirements.png`
- `parts_catalog_actions.png`
- `parts_catalog_add_inventory.png`
- `parts_catalog_details.png`
- `parts_catalog_search.png`
- `quick_start_brickset_api.png`
- `quick_start_inventory_import.png`
- `quick_start_parts_catalog_import.png`
- `quick_start_part_relationships.png`
- `quick_start_rebrickable_api.png`
- `quick_start_rebrickable_downloads.png`
- `quick_start_sets_catalog_import.png`
- `quick_start_storage.png`
- `rebrickable_import_complete.png`
- `rebrickable_import_file.png`
- `rebrickable_import_inventory.png`
- `rebrickable_import_operations.png`
- `rebrickable_import_sync_preview.png`
- `sets_catalog_create_build.png`
- `sets_catalog_instructions.png`
- `sets_catalog_overview.png`
- `sets_catalog_search.png`
- `settings_general.png`
- `settings_rebrickable.png`
- `storage_deactivate_parent.png`
- `storage_edit_location.png`

Under `docs/images/`:

- `bricksuite_builds.png`
- `bricksuite_complete_set.png`
- `bricksuite_parts_catalog.png`
- `bricksuite_part_resolver.png`

## Release-polish corrections discovered during capture

1. `MyInventoryWidget` still displayed “My Loose Inventory” inside the correctly named My Inventory tab. Only that heading was corrected.
2. Qt Help image max-width handling caused distorted spacing and horizontal overflow. `HelpDialog` now sizes embedded images to available content width while preserving their native aspect ratio, without changing image files, navigation, or history. Image paragraphs are separate and use normal line height. A narrow → wide resize regression was added to `PrintingHelpUx`.

## Validation

- Qt 6.10.3 MinGW Release configure and application build: passed; focused test targets built explicitly.
- `PrintingHelpUx`: passed, including all 30 compiled pages, local links/fragments, image resources/alt text, Light/Dark contrast/navigation behavior, and the new image resize check.
- `SetDetailsInventoryWorkflow` and `InventoryZeroQuantityWorkflow`: passed. Initial sandbox attempts could not initialize their isolated Qt test databases; rerunning with the required filesystem access passed. No user database was involved.
- Static HTML/QRC checks: 30 pages, 39 image occurrences, 34 unique current PNGs; all local targets, fragments, image alt text, and exact image QRC membership pass.
- Online Help staged locally using the same HTML-and-images copy layout as `publish-help.yml`; source/staged files match. The historical unreferenced PNG is copied with the existing images directory, but is not linked by product Help. Nothing published.
- README: all four local image references resolve; prose unchanged.
- Native Help visual review: Dark and Light at 1100×760, narrow Dark at 800×700; image proportions preserved and horizontal scrollbar maximum zero on the representative illustrated page.
- Every final product screenshot was visually reviewed. Capture harness, sample database, logs, calibration artifacts, and staging remain outside tracked product files.
- `git diff --check`: passed. Schema **35** and Protocol **1.5** unchanged. No commit or push.

No technical M40.3 blocker remains after the recorded checks. Ray still performs final visual acceptance, including the three exception decisions. README prose, About wording, changelog/version updates, packaging cutover, and publication remain M40.4/M40.5 work. Full configured CTest and macOS/Linux runtime acceptance were not run for this documentation-focused change; the focused Release checks above were run.

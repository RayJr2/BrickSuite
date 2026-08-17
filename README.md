# BrickSuite

**The Digital Twin Platform for Your Brick Workshop.**

BrickSuite is a Windows desktop application for managing a physical brick collection as a digital twin. Version 1.0 combines storage organization, loose-part inventory, Rebrickable reference data, Sets and MOCs, build allocation, missing/lost-part workflows, image caching, backup/restore, and application diagnostics in one local application.

## Version 1.0

BrickSuite v1.0.0 is the first feature-complete release baseline.

The application is designed around a simple principle: physical inventory, storage locations, and build activity should remain traceable. Records that may be referenced elsewhere are deactivated or archived rather than casually deleted.

### Core capabilities

- **Workspaces** provide the top-level container for a brick workshop.
- **Hierarchical Storage** models areas, cabinets, shelves, cases, drawers, bins, trays, compartments, and dividers.
- **Operational storage locations** keep inventory assignment and filtering focused on usable leaf locations rather than parent containers.
- **My Inventory** tracks part, color, quantity, condition, ownership type, and storage location.
- **Inventory history** records quantity changes, moves, build activity, Lost/Found activity, and other inventory movements.
- **Parts Catalog** provides searchable and paged Rebrickable part reference data with part details and images.
- **Sets Catalog** provides searchable, year-filtered, paged Set reference data with Set details and images.
- **Table summaries** show the number of matching Parts, Sets, or Inventory Records alongside `Page X of Y` paging.
- **Rebrickable CSV inventory import** imports owned parts and can suggest a destination storage location from the exported CSV filename.
- **Rebrickable API support** provides connection testing and part/Set detail operations.
- **Background image caching** downloads general part, color-specific part, and Set images while respecting conservative request pacing.

## Inventory and Storage

BrickSuite supports the same part/color combination in multiple physical locations. Inventory can be searched and filtered by text, category, color, and storage location.

Normal inventory operations include:

- Add loose parts.
- Increase or decrease quantities.
- Edit inventory records.
- Move part or all of a quantity between storage locations.
- Review movement/history records.
- Import Rebrickable owned-parts CSV files.
- Preserve inventory and storage state across application restarts.

Storage locations form a hierarchy. Locations can be activated or deactivated subject to integrity rules; BrickSuite prevents deactivation when doing so would conflict with child locations or loose inventory. Inactive records remain preserved so existing database references are not broken.

## Sets, MOCs, and Builds

BrickSuite supports both LEGO Set and MOC workflows.

Builds can progress through their supported lifecycle states and may be archived when they are no longer part of the active working list. Cancelled Builds are supported, and the **Show Archived** preference is remembered between sessions.

Two inventory workflows are supported:

- **Build from Stock** — allocate loose inventory to a Build.
- **Complete Set** — assemble a complete Set from loose inventory and later disassemble it back into storage.

For MOCs, Rebrickable CSV files can be imported to create requirements.

### Allocate Available

**Allocate Available** searches loose inventory and automatically allocates available matching parts to Build requirements. This avoids manually selecting `Action -> Allocate` for every requirement and is especially useful for large Sets and MOCs.

Allocation preserves inventory quantities and movement history and reports satisfied, partial, and missing requirements.

## Missing Parts

Build requirements are continuously compared with available inventory.

BrickSuite can:

- Show required, available, and missing quantities.
- Preserve correct part colors in shortage calculations.
- Refresh requirements after inventory changes.
- Export Missing Parts to CSV.

## Lost and Found Inventory

Parts associated with Builds can be marked Lost without destroying their history.

The Lost Inventory workflow supports:

- Partial or complete quantity loss.
- Persistent outstanding Lost quantities.
- Lost movement/history records.
- Returning found parts to a selected loose-inventory storage location.
- Partial or complete Found/Return operations.
- Automatic removal from the outstanding Lost list once the full quantity has been returned.

## Rebrickable Data

BrickSuite uses Rebrickable data for its reference catalogs and import workflows.

Version 1.0 supports:

- Colors and part categories.
- Parts Catalog CSV updates.
- Sets Catalog CSV updates.
- Owned-parts CSV import.
- MOC CSV import.
- Rebrickable API connection testing and detail retrieval.

Catalog imports update existing records and insert newly discovered records rather than deleting catalog records simply because they are absent from a later import.

## Images and Cache

BrickSuite maintains a local image cache for faster browsing and offline reuse of previously downloaded images.

The image services support:

- General part images.
- Color-specific part images.
- Set images.
- Background population of required part/color images.
- Conservative request pacing to reduce the risk of API throttling.

Color-specific cache entries use the application's mapped color data so cached images correspond to the correct BrickSuite/Rebrickable color.

## Backup and Restore

BrickSuite includes integrated SQLite database backup and restore.

Backup/restore includes:

- User-selected backup files.
- Progress feedback.
- Backup verification.
- SQLite integrity checking.
- Schema-version validation.
- Automatic pre-restore safety backup.
- Restoration of the complete BrickSuite database, including storage, inventory, Builds/MOCs, movement history, and Lost Inventory.

Because the database is the authoritative local data store, regular backups are recommended.

## Application Logging

BrickSuite v1.0 includes an application-wide diagnostic logger.

The logger captures Qt `Info`, `Warning`, `Critical`, and fatal messages with timestamps while intentionally avoiding routine UI-action noise.

Logging covers high-value operations such as:

- Application/database startup and schema initialization.
- Reference-data seeding.
- Catalog and inventory imports.
- Build allocation and completion.
- Database backup and restore.
- Inventory and storage persistence failures.
- Image/cache failures.
- Other important database and workflow errors.

The built-in **Application Log Viewer** is non-modal and auto-refreshing, allowing it to remain open on a second monitor while BrickSuite is tested or used. Its size and screen position are remembered between sessions. The viewer can also clear the log or open the log folder.

## User Interface

Version 1.0 includes:

- Light and Dark themes.
- Persistent main-window geometry and state.
- Persistent splitter/panel positions.
- Remembered default workspace behavior.
- Remembered **Show Archived** Build preference.
- Persistent Log Viewer geometry.
- Search/filter empty-state messages.
- Consistent result summaries and `Page X of Y` paging.

## Technology

BrickSuite v1.0 is implemented with:

- **C++17**
- **Qt 6.10.2**
- **Qt Widgets**
- **Qt SQL**
- **Qt Network**
- **SQLite**
- **CMake**
- **Windows 11**

The primary development environment for v1.0 is Qt Creator 19.0.0 with the Qt 6.10.2 MinGW 64-bit toolchain.

## Building BrickSuite

BrickSuite v1.0 is developed with C++17, CMake, and Qt 6.10.2. The v1.0
development environment uses Qt Creator 19.0.0 and the Qt 6.10.2 MinGW
64-bit toolchain on Windows 11.

The CMake project requires these Qt 6 modules:

- Qt Core
- Qt Gui
- Qt Widgets
- Qt SQL
- Qt Network

A typical build begins by configuring the repository's `CMakeLists.txt`
with a Qt 6 development environment in which those modules are available.

### SQLite

BrickSuite uses SQLite as its local database engine through Qt SQL.

A separate SQLite command-line installation is not normally required to
run BrickSuite when the required Qt runtime components and SQLite SQL
driver are present.

Developers who want the SQLite command-line tools for inspecting,
troubleshooting, or working directly with `BrickSuite.db` can obtain
them from the official SQLite project:

https://sqlite.org/download.html

## Local Application Data

BrickSuite uses Qt's `AppLocalDataLocation` for application-managed local data.

On the v1.0 Windows development/test system this resolves under:

```text
%LOCALAPPDATA%\RFStateSide\BrickSuite\
```

This location contains the live SQLite database and application log, with image-cache data maintained beneath the BrickSuite application-data hierarchy.

Typical files include:

```text
BrickSuite.db
BrickSuite.log
```

User-interface preferences are stored through `QSettings`.

## Database

BrickSuite v1.0 uses SQLite and currently initializes **database schema version 11**.

Schema migrations are applied by the application as required. The database contains the persistent relationships between workspaces, storage, inventory, Builds, requirements, allocations, movement history, Lost Inventory, and reference catalogs.

The application favors preserving database identity and history. Where practical, records are deactivated or archived rather than deleted.

## Reference Catalog Size

Catalog contents change as new Rebrickable data is imported. During final v1.0 testing in August 2026, a monthly `parts.csv` update processed 64,293 rows and added 49 new parts to the existing catalog, bringing the local Parts Catalog to approximately **64,301 parts**.

The application displays current matching totals directly below the Parts Catalog, Sets Catalog, and My Inventory tables, so the live database remains the authoritative source for current counts.

## v1.0 Validation

BrickSuite v1.0 underwent a fresh-database regression pass covering:

- First-run database creation and reference-data seeding.
- Real storage hierarchy creation.
- Rebrickable inventory imports.
- Storage management.
- My Inventory operations.
- Parts and Sets catalogs.
- Build from Stock.
- Complete Set workflow.
- MOC workflow.
- Missing Parts.
- Lost Parts and Found/Return.
- Backup and restore.
- Settings/UI persistence.
- Restart and recovery behavior.

Issues discovered during regression testing were corrected before the v1.0 feature-complete baseline, followed by additional workflow and diagnostic smoke testing.

## Project Status

**BrickSuite 1.0.0 — Feature Complete**

Version 1.0 establishes the production-use baseline. Further ideas and enhancements can be evaluated from real-world use and developed as post-v1.0 work without expanding the initial release indefinitely.

## Copyright and License

Copyright © 2026 RF StateSide, LLC.

BrickSuite is free and open-source software licensed under the
**GNU Lesser General Public License, version 3.0 (LGPL-3.0-only)**.

You may use, study, modify, and redistribute BrickSuite subject to the
terms of the LGPL-3.0. See the repository `LICENSE` file for the complete
license terms.

Contributions are welcome. See `CONTRIBUTING.md` for the contribution
guidelines.

## Trademark and Third-Party Notice

LEGO® is a trademark of the LEGO Group of companies, which does not sponsor, authorize, or endorse BrickSuite.

Rebrickable is a trademark or brand name of its respective owner. BrickSuite is an independent application and is not affiliated with or endorsed by Rebrickable.

All other trademarks, product names, company names, services, and third-party content referenced by BrickSuite are the property of their respective owners. Their use is for identification and interoperability purposes only and does not imply sponsorship, affiliation, authorization, or endorsement.

---

**BrickSuite**  
*The Digital Twin Platform for Your Brick Workshop.*

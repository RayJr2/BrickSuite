# AGENTS.md

## Scope

These instructions apply to the entire BrickSuite repository.

BrickSuite is a cross-platform C++17 desktop application built with Qt 6, Qt Widgets, Qt SQL, Qt Network, SQLite, and CMake. Preserve compatibility with Windows, macOS, and Linux.

## Before Making Changes

- Inspect `git status` before modifying anything.
- Preserve all pre-existing and unrelated working-tree changes.
- Read the relevant implementation, schema, documentation, and surrounding conventions before editing.
- Keep changes narrowly scoped to the requested task.
- Do not opportunistically redesign, rename, reformat, or refactor adjacent systems.
- Completed or frozen behavior should not be casually redesigned or refactored unless the requested task requires it.
- Do not edit generated build output, IDE metadata, temporary files, or user application data.
- Never commit, push, reset, checkout, restore, clean, discard changes, or perform other destructive Git operations unless the user explicitly authorizes that exact action.

## Build

The reference development stack is:

- C++17
- Qt 6
- Qt Widgets
- Qt SQL
- Qt Network
- SQLite through Qt's `QSQLITE` driver
- CMake 3.16 or newer

Typical configuration and build:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/<version>/<kit>
cmake --build build
```

Use an appropriate installed Qt kit for the host platform. Preserve platform-specific behavior:

- Windows uses the GUI subsystem and Windows deployment tooling.
- macOS builds an application bundle and uses the Security framework where configured.
- Linux installs the executable under `bin`.

Do not introduce platform-specific assumptions into shared code without suitable guards and behavior for the other supported platforms.

## Architecture

BrickSuite uses a layered application architecture:

- `src/models`: domain data and identities.
- `src/repositories`: persistence, SQL, and database-to-model mapping.
- `src/services`: business rules and coordination of multi-step workflows.
- `src/api`: provider abstractions, network operations, provider status, and error handling.
- `src/import`: external file and catalog import logic.
- `src/database`: database connection, schema creation, migrations, backup, and restore.
- `src/ui`: Qt Widgets presentation, dialogs, validation feedback, and user interaction.
- `src/app`: application composition and shared application context.
- `src/settings`: persistent user preferences and theme behavior.
- `resources/help`: the single source of truth for built-in and published user help.

For new multi-step workflows, prefer the application-service boundary:

- UI handles presentation, input collection, confirmation, progress, and result display.
- Services own business rules, workflow orchestration, and multi-repository transactions.
- Repositories own persistence and query mapping.

Do not put new cross-entity business workflows or multi-repository transactions directly in UI classes. This boundary is intentional preparation for a possible future client/server architecture.

Straightforward presentation-specific reads or established simple CRUD patterns may follow existing code, but new workflow logic should not increase coupling between the UI and persistence layers.

## Database and Migration Safety

Existing user databases must be preserved.

- Never solve a schema or migration problem by deleting, replacing, or recreating `BrickSuite.db`.
- Implement schema changes as sequential migrations in `DatabaseSchema`.
- Increment `DatabaseSchema::CurrentSchemaVersion` when adding a migration.
- Make migrations transactional and provide correct rollback behavior.
- Preserve foreign-key enforcement and existing database identities.
- Use prepared queries and bound values for data-dependent SQL.
- Use transactions for operations that must succeed or fail as a unit.
- Validate migrations against representative existing databases as well as newly created databases.
- Keep backup, restore, and schema-version verification behavior compatible with existing data.

Persistent records may participate in inventory history, Build history, provenance, or other relationships. Prefer archive, deactivate, or explicit lifecycle-state transitions over deletion.

## Domain Invariants

### Build requirements and allocations

- `BuildRequirement` owns logical requirement identity.
- `BuildAllocation.buildRequirementId` identifies the exact requirement fulfilled by an allocation.
- Preserve the exact requirement relationship throughout allocation, pulling, reconciliation, completion, cancellation, release, and disassembly.
- Do not reconcile or associate allocations by Part/Color alone when an exact `buildRequirementId` relationship exists.
- Build substitutions use the requirement's effective Part/Color identity for physical fulfillment.
- Substitution behavior must not erase or replace the logical identity of the original requirement.

### Inventory and provenance

- Inventory quantity changes must preserve the appropriate movement and history records.
- Manufacturer provenance belongs to physical inventory.
- Preserve manufacturer provenance through inventory movements, Build allocation and pulling, Build completion, Lost/Found operations, reconciliation, release, and disassembly.
- Do not silently merge inventory records when doing so would lose meaningful ownership, condition, manufacturer, location, or provenance distinctions.
- Multi-step inventory changes must be atomic when partial completion would create inconsistent state.

### Storage

- Operational inventory storage destinations are active leaf locations.
- Parent or container locations remain hierarchy nodes and normally are not valid inventory destinations.
- Validate destination activity and leaf status in the business/service layer for new workflows; UI filtering alone is not sufficient.
- Preserve storage hierarchy integrity when activating, deactivating, moving, or selecting locations.

### Historical behavior

- Preserve historical records and stable persistent identities.
- Prefer archive, deactivate, and lifecycle-state transitions over deletion when records participate in history.
- Do not rewrite past movements, allocations, or provenance merely to make current state easier to calculate.
- Treat completed or frozen workflows as historical records unless an explicitly supported transition applies.

## APIs and External Providers

BrickSuite integrates with providers such as Rebrickable and Brickset through centralized API and service infrastructure.

- Do not add ad hoc provider calls directly to UI code.
- Preserve centralized request pacing, throttling, status tracking, and error handling.
- Preserve established caching and provider fallback behavior.
- Avoid repeated network calls for data already cached or known to be unavailable.
- Keep provider-specific identities distinct from BrickSuite's internal catalog identities.
- Never hard-code API keys, credentials, tokens, passwords, or private endpoints.
- Do not log credentials or include them in errors, fixtures, screenshots, documentation, or commits.

## UI, Settings, and Logging

- Follow existing Qt Widgets and signal/slot conventions.
- Keep user-facing validation and error messages clear and actionable.
- Preserve persistent settings, window state, and light/dark theme behavior.
- Keep developer-only diagnostics behind the existing Debug-build conventions.
- Avoid blocking the UI thread with database, import, filesystem, or network work that can be meaningfully long-running.
- Update relevant help topics and screenshots when a user-visible workflow changes.

Use logging selectively:

- `Info` for meaningful completed operations.
- `Warning` for rejected, suspicious, or recoverable conditions that merit diagnosis.
- `Critical` for genuine database or operational failures.
- Do not log routine clicks, navigation noise, credentials, or private user data.

## Documentation and Release Metadata

- `resources/help` is the shared source for built-in Help and published online Help. Do not create a separate copy of the same help content.
- Update affected help and developer documentation when behavior, setup, or user workflows change.
- Keep version and release metadata consistent across CMake, application constants, update manifests, installer metadata, the changelog, and public documentation when performing release work.
- Do not update unrelated release metadata as part of an ordinary feature or bug fix.

## Validation

After making changes, perform validation proportional to the risk and scope when the environment permits:

1. Configure and build with an appropriate Qt kit.
2. Build the relevant Debug or Release configuration.
3. Exercise focused technical checks for the changed code.
4. For database work, test both a new database and migration from representative existing data.
5. For inventory or Build work, verify quantities, exact requirement relationships, manufacturer provenance, movements, allocations, and rollback behavior.
6. For UI, packaging, menu, or deployment changes, verify Release behavior where relevant.
7. Check the application log for unexpected warnings or critical errors.
8. Consider Windows, macOS, and Linux behavior.

A successful build and technical validation do not constitute user acceptance. Runtime workflow acceptance remains a separate user validation step. Report clearly:

- What was validated.
- What could not be validated.
- Which runtime workflows still require user confirmation.

## Sensitive and Local Data

Do not commit or expose:

- API credentials or tokens.
- Live BrickSuite databases or backups.
- Application logs containing private data.
- Personal inventory exports.
- Private certificates or signing material.
- Machine-specific build paths or IDE state.
- Generated caches or temporary artifacts.

Use synthetic, minimal fixtures when test data is required.

## Change Discipline

- Match the existing coding and naming style in the files being changed.
- Prefer the smallest coherent implementation that preserves existing invariants.
- Avoid broad formatting-only changes mixed with functional work.
- Do not add dependencies without an explicit need and consideration of all supported platforms.
- Do not alter public workflows, persistence semantics, or completed/frozen behavior beyond the requested scope.
- If a requested change conflicts with database history, provenance, exact requirement identity, storage validity, or cross-platform compatibility, stop and explain the conflict before proceeding.

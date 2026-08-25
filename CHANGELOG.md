# Changelog

All notable changes to BrickSuite are documented in this file.

BrickSuite follows semantic versioning for public releases.

## [0.2.0] - 2026-08-25

### Added

- Brickset API provider integration alongside Rebrickable.
- Provider connection testing, status reporting, request throttling, and fallback behavior.
- Brickset Set Details enrichment, including additional metadata, ratings, provider links, and instruction access.
- Missing Parts procurement and store/export workflows.
- Rebrickable inventory comparison and synchronization operations:
  - Append
  - Replace
  - Subtract
  - Compare Only
- Rebrickable reconciliation export files for append/subtract updates.
- Manufacturer and provenance handling across inventory workflows.
- Part identity resolution and Part Resolver support for exact, alias, alternate, older, superseded, and mapped part numbers.
- Rebrickable `part_relationships.csv` import support.
- Inventory and catalog enhancements, including current manufacturer/material/mapping information.
- Complete Set Build workflow:
  - populate requirements from Set data
  - preview imported Set requirements before committing
  - track regular and spare pieces
  - release spare pieces to My Loose Inventory
  - disassemble completed Sets back into loose inventory while preserving history
- Build/MOC lifecycle improvements, including Cancelled, Archived, Complete, and Disassembled states.
- Allocate Available workflow improvements.
- Lost / Found workflow enhancements.
- Single-instance application protection.
- Cross-platform build support validated on Windows, macOS, and Ubuntu Linux.
- Automated Windows deployment and installer packaging using `windeployqt` and Inno Setup.
- LGPL license presentation during Windows installation.
- Build-mode Test menu behavior:
  - visible in Debug builds
  - omitted in Release builds
- GitHub-hosted Check for Updates support with platform-specific download keys for:
  - Windows
  - macOS
  - Linux x64
  - Linux ARM
- Secure API credential storage:
  - Windows Credential Manager
  - macOS Keychain
  - Linux Secret Service / keyring via `secret-tool`
- Safe migration of legacy plaintext API keys out of application settings.
- Expanded built-in Help for v0.2.0, including a new Quick Start guide.
- Web-published BrickSuite Help through GitHub Pages.
- GitHub issue templates for Bug Reports and Feature Requests.
- Updated public README, CONTRIBUTING, SECURITY, THIRD_PARTY_NOTICES, and release-audit documentation.

### Changed

- Updated development baseline to Qt 6.10.3.
- Reworked Help and screenshots to match the current v0.2.0 UI and workflows.
- Updated Settings to use an APIs section with separate Rebrickable and Brickset provider tabs.
- Updated Sets Catalog Help and Set Details workflows for Brickset enrichment and instructions.
- Updated My Inventory Help and UI documentation for Manufacturer, provenance, and Part Resolver behavior.
- Updated Parts Catalog documentation for relationship data, external IDs, and identity resolution.
- Updated Builds documentation for Complete Set, spare release, and disassembly workflows.
- Improved public repository presentation, online documentation, contributor guidance, and repository metadata.
- Consolidated deployment files under `deployment/windows/`.
- Cleaned and hardened `.gitignore` rules to avoid accidentally excluding legitimate source/help files.

### Fixed

- Corrected QSpinBox arrow resources and styling across Light and Dark themes.
- Corrected About dialog icon rendering so the multi-resolution application icon is displayed sharply.
- Corrected missing Help-resource tracking caused by overly broad `.gitignore` rules.
- Corrected Windows installer icon staging and Inno Setup integration issues encountered during packaging automation.
- Corrected update-check dialog newline formatting.
- Corrected update manifest branch/path handling for the public GitHub repository.

### Security

- API keys are no longer stored as new plaintext `QSettings` values.
- Existing plaintext API-key settings are removed only after successful migration to the operating system credential store.
- Repository history and fresh-clone contents were reviewed for credentials, private databases, logs, certificates, and other sensitive artifacts.
- Added `SECURITY.md` with public/private reporting guidance.

### Distribution

- Windows installer validated on a separate Windows machine without a Qt development environment.
- Release candidate tested both:
  - as a new user with no existing database
  - with an existing populated BrickSuite database and image cache
- macOS and Ubuntu source builds validated with Qt 6.10.3 Release builds.
- macOS and Linux packaging remain deferred to later milestones.

## [0.1.0]

Initial BrickSuite development baseline prior to the v0.2.0 public-release work.

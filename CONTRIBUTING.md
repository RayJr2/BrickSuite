# Contributing to BrickSuite

Thank you for your interest in improving BrickSuite.

BrickSuite is maintained by RF StateSide, LLC and is released under the
GNU Lesser General Public License version 3.0 (LGPL-3.0-only).

## Ways to Contribute

Contributions may include:

- Bug reports and reproducible test cases.
- Documentation corrections or improvements.
- User-interface and workflow improvements.
- Performance or reliability fixes.
- New inventory, storage, Build, Set, or MOC capabilities.
- Cross-platform build and compatibility improvements.

Use the repository's GitHub issue templates when practical:

- **Bug report** for reproducible problems.
- **Feature request** for enhancement ideas.

## Before Starting a Larger Change

For a significant feature or architectural change, please open a GitHub
issue first and describe the proposed behavior. This gives maintainers
and contributors an opportunity to discuss scope and approach before a
large amount of code is written.

Small bug fixes and documentation corrections may be submitted directly
as pull requests.

## Development Baseline

BrickSuite v0.2.0 uses:

- C++17
- Qt 6.10.3
- Qt Widgets
- Qt SQL
- Qt Network
- SQLite
- CMake

The primary reference development environment is Windows 11 with Qt Creator
and the Qt 6.10.3 MinGW 64-bit toolchain.

BrickSuite has also been successfully built and runtime-tested from source
on macOS and Ubuntu Linux using Qt 6.10.3 Release builds.

## Building BrickSuite

BrickSuite uses CMake and Qt 6.10.3.

A typical command-line configuration is:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.10.3/<kit>
cmake --build build
```

The exact Qt kit path and generator vary by operating system and development
environment. Qt Creator can open the repository's `CMakeLists.txt` directly
and configure an installed Qt 6.10.3 kit.

See `README.md` for the current public build overview and platform notes.

## Debug and Release Builds

BrickSuite intentionally exposes additional internal diagnostics in Debug
builds.

- **Debug builds** include the developer-oriented **Test** menu.
- **Release builds** omit the Test menu and represent the expected
  end-user application configuration.

Please verify Release behavior when a change affects menus, deployment,
runtime dependencies, or public-facing workflows.

## Pull Requests

Please keep pull requests focused on one logical change when practical.

Before submitting a pull request:

1. Build BrickSuite successfully from a clean build directory.
2. Exercise the affected workflow.
3. Verify that existing inventory quantities and database relationships
   remain consistent.
4. Check the BrickSuite Application Log for unexpected Warning or
   Critical entries.
5. Describe what changed and how it was tested.
6. Consider whether the change affects Windows, macOS, or Linux behavior.
7. Update Help/documentation when the user-visible workflow changes.

Database schema changes should include an appropriate migration rather
than requiring users to delete an existing BrickSuite database.

Changes affecting imports, inventory movement, Build requirements,
allocations, Lost/Found, disassembly, or synchronization should be tested
against representative existing data as well as the expected new behavior.

BrickSuite generally preserves persistent record identity. Existing
records that may be referenced elsewhere should be deactivated or
archived instead of deleted unless a deliberate schema/design change
requires otherwise.

## API and Provider Changes

BrickSuite currently integrates with Rebrickable and Brickset.

API-related changes should follow the existing provider/service structure
and centralized network/request-handling design rather than adding ad-hoc
provider calls directly from UI code.

In particular:

- Respect BrickSuite's centralized request pacing and throttling behavior.
- Preserve provider connection/error handling.
- Preserve Brickset-to-Rebrickable fallback behavior where applicable.
- Avoid repeated API calls for data that BrickSuite already caches or marks
  unavailable.
- Never hard-code API credentials or provider secrets.

## Help and Documentation

BrickSuite Help content lives under `resources/help`.

That Help content serves two purposes:

- It is compiled into BrickSuite's built-in Help system.
- The same content is published as the online BrickSuite Help using GitHub
  Pages.

When a user-visible workflow changes, update the relevant Help topic and
screenshots as part of the same change when practical.

Avoid creating a second, separate copy of Help content for the web. The
repository's `resources/help` content is intended to remain the single
source of truth.

## Coding and Logging

Please follow the existing C++/Qt structure and naming conventions.

Logging should remain selective:

- `Info` for meaningful completed operations.
- `Warning` for rejected or suspicious conditions that merit diagnosis.
- `Critical` for genuine operational or database failures.
- Avoid logging routine clicks, navigation, or other high-volume UI noise.

## Licensing of Contributions

By submitting a contribution to this repository, you agree that your
contribution may be distributed as part of BrickSuite under the
GNU Lesser General Public License version 3.0 (LGPL-3.0-only).

Do not submit code, images, data, or other material that you do not have
the right to contribute under compatible terms.

## Third-Party Data and Trademarks

BrickSuite interoperates with third-party services and data sources,
including Rebrickable and Brickset. Contributions must respect applicable
third-party terms, licenses, trademarks, and API requirements.

LEGO® is a trademark of the LEGO Group of companies, which does not
sponsor, authorize, or endorse BrickSuite.

## Security and Private Data

Do not commit API keys, passwords, access tokens, private certificates,
private databases, application logs, personal inventory exports, or other
sensitive data.

Do not include credentials or private data in public GitHub issues,
screenshots, logs, test fixtures, or pull requests.

If you believe you have found a security-sensitive issue, avoid posting
exploit details, credentials, or private data in a public issue. See
`SECURITY.md` for the preferred reporting process once that document is
available.

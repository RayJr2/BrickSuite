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

## Before Starting a Larger Change

For a significant feature or architectural change, please open a GitHub
issue first and describe the proposed behavior. This gives maintainers
and contributors an opportunity to discuss scope and approach before a
large amount of code is written.

Small bug fixes and documentation corrections may be submitted directly
as pull requests.

## Development Baseline

BrickSuite v0.1.0 uses:

- C++17
- Qt 6.10.2
- Qt Widgets
- Qt SQL
- Qt Network
- SQLite
- CMake

The v0.1.0 reference development environment is Windows 11 with Qt Creator
19.0.0 and the Qt 6.10.2 MinGW 64-bit toolchain.

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

Database schema changes should include an appropriate migration rather
than requiring users to delete an existing BrickSuite database.

BrickSuite generally preserves persistent record identity. Existing
records that may be referenced elsewhere should be deactivated or
archived instead of deleted unless a deliberate schema/design change
requires otherwise.

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
including Rebrickable. Contributions must respect applicable third-party
terms, licenses, trademarks, and API requirements.

LEGO® is a trademark of the LEGO Group of companies, which does not
sponsor, authorize, or endorse BrickSuite.

## Security and Private Data

Do not commit API keys, passwords, access tokens, private databases,
application logs, personal inventory exports, or other sensitive data.

If you believe you have found a security-sensitive issue, avoid posting
credentials or private data in a public issue.

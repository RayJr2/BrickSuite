# Third-Party Notices

BrickSuite is developed by RF StateSide, LLC and is licensed under the
GNU Lesser General Public License version 3.0 (LGPL-3.0-only).

This document summarizes major third-party software, services, data, and
trademarks used or referenced by BrickSuite v0.2.0. Distribution packages
should be reviewed when their contents change so that any additional
third-party notices required by bundled components are included.

## Qt

BrickSuite v0.2.0 is built with Qt 6.10.3 and uses the following Qt modules:

- Qt Core
- Qt Gui
- Qt Widgets
- Qt SQL
- Qt Network
- Qt Svg where required by deployed plugins/resources

Qt is developed by The Qt Company and the Qt Project and is available under
commercial and open-source licensing options. BrickSuite's open-source build
uses Qt components subject to their applicable open-source license terms.

Qt licensing information:
https://www.qt.io/licensing/

Qt open-source LGPL obligations:
https://www.qt.io/development/open-source-lgpl-obligations

When distributing BrickSuite binaries with Qt libraries, the distributor is
responsible for satisfying the license requirements applicable to the
specific Qt libraries and third-party components included in that
distribution.

## SQLite

BrickSuite uses SQLite as its local database engine through Qt SQL.

The SQLite project states that the deliverable SQLite code and documentation
are dedicated to the public domain.

SQLite copyright/public-domain information:
https://sqlite.org/copyright.html

SQLite downloads:
https://sqlite.org/download.html

## Rebrickable

BrickSuite interoperates with Rebrickable reference data, CSV exports,
images, and API services.

Rebrickable API:
https://rebrickable.com/api/

Users are responsible for complying with Rebrickable's applicable terms,
API requirements, and usage limits. BrickSuite is an independent application
and is not affiliated with or endorsed by Rebrickable.

## Brickset

BrickSuite optionally interoperates with Brickset API services for Set
enrichment, usage information, provider links, and instruction metadata.

Brickset:
https://brickset.com/

BrickSuite users supply their own Brickset API credentials and are
responsible for complying with Brickset's applicable terms and API
requirements. BrickSuite is an independent application and is not affiliated
with or endorsed by Brickset.

## Platform Credential Services

BrickSuite stores provider API credentials using platform credential
facilities:

- Windows Credential Manager on Windows
- macOS Keychain on macOS
- Secret Service / keyring on Linux

On Linux, BrickSuite's current secure-storage integration invokes
`secret-tool`, commonly provided by the `libsecret-tools` package. Availability
and licensing of that operating-system package are determined by the user's
Linux distribution.

These platform facilities are used for local credential storage and are not
embedded provider credentials or BrickSuite-owned secrets.

## Inno Setup

The Windows BrickSuite installer is built using Inno Setup.

Inno Setup is a packaging/build tool and is not itself the BrickSuite
application runtime. Distribution maintainers should review the applicable
Inno Setup licensing information when changing the Windows packaging process.

Inno Setup:
https://jrsoftware.org/isinfo.php

## LEGO Trademark

LEGO® is a trademark of the LEGO Group of companies, which does not sponsor,
authorize, or endorse BrickSuite.

References to LEGO products, part numbers, Sets, and related terminology are
used for identification and interoperability purposes.

## Other Third-Party Components

Qt itself may contain or depend upon third-party components distributed under
their own license terms. Binary release packaging should retain all notices
and license files required by the actual Qt runtime and other components
shipped with that release.

All other trademarks, product names, company names, services, and third-party
content referenced by BrickSuite are the property of their respective owners.
Their use does not imply sponsorship, affiliation, authorization, or
endorsement.

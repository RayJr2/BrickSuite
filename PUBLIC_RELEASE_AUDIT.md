# Public Repository Preparation Audit

This audit covers the files supplied for the BrickSuite v0.1.0 public
repository preparation pass. It does **not** inspect Git commit history,
because the supplied archive does not contain the repository's `.git`
history.

## Current Tree

- Source files receiving RF StateSide, LLC / LGPL headers: 147
- Hard-coded credentials found after review: 0
- Runtime/private data files found in supplied tree: 0

The automated text scan initially flagged `UserSettings.cpp` and
`SettingsDialog.cpp` because they contain the Rebrickable API-key setting
name, API-key UI field, and API-key handling code. Those locations were
reviewed and do **not** contain an actual API key or other hard-coded
credential.

No `.db`, `.log`, or owned-parts `rebrickable_parts_*.csv` files were
present in the supplied tree.

## Before Making GitHub Public

Before changing repository visibility, inspect the actual Git history for
credentials or private files that may have existed in older commits.
Deleting a secret from the current tree does not remove it from Git
history.

Also verify the final binary distribution separately. A source repository
scan cannot determine which Qt DLLs/plugins or other third-party runtime
components will ultimately be packaged with a Windows release.

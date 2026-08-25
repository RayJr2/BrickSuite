# Public Repository Preparation Audit

This document records the BrickSuite v0.2.0 public-repository security and
hygiene review performed during M24.5.

The review used a **fresh clone of the public `master` branch** so the audit
represented files actually available from GitHub rather than only a local
development working tree.

## Security Review

At the time of this audit, the Git repository contained 108 commits.

The current tree and Git history were reviewed for common indicators of
accidentally committed secrets or private runtime data, including:

- API keys and credential assignments
- passwords and access tokens
- private keys and certificates
- `.env` / credential files
- BrickSuite SQLite databases and sidecar files
- application logs
- personal inventory exports
- machine-specific absolute paths
- other obviously private runtime artifacts

No actual API keys, passwords, access tokens, private certificates, private
keys, BrickSuite databases, application logs, or personal inventory exports
were identified in the reviewed repository history.

References to API-key setting names and credential-handling code were reviewed
and are expected implementation details rather than embedded credentials.

This review reduces the risk of accidental disclosure but is not a guarantee
that every possible secret pattern can be detected automatically.

## API Credential Storage

BrickSuite v0.2.0 no longer stores newly saved Rebrickable or Brickset API
keys as plaintext application settings.

The secure credential backends are:

- Windows Credential Manager
- macOS Keychain
- Linux Secret Service / keyring through `secret-tool`

Legacy plaintext API-key values are migrated only after secure storage
succeeds. The old plaintext value is then removed. If migration fails, the
existing value is preserved rather than silently losing the credential.

Secure credential storage and migration were runtime-tested successfully on
Windows, macOS, and Ubuntu Linux.

## Repository Hygiene Findings

The audit identified and corrected the following repository-hygiene issues.

### `.gitignore`

Two root ignore files existed: `.gitignore` and an accidental plain
`gitignore` file.

The plain file contained newer runtime/private-data exclusions that Git was
not actually applying. Those rules were merged into the real `.gitignore`,
and the duplicate plain file was removed.

Overly broad `*debug*` and `*release*` patterns were also removed. Those
patterns could hide legitimate source or documentation files whose filenames
happened to contain those words.

### Missing Help Resources

The broad `*release*` ignore rule had caused a required Help screenshot named
`builds_release_spares_complete.png` to remain untracked. This was detected
when a clean macOS clone could not build the Qt Help resource collection.

The ignore rule was corrected and the required Help resources were added to
Git. Clean-clone builds subsequently succeeded.

### Duplicate Windows Deployment Files

An obsolete duplicate deployment directory existed under:

`resources/deployment/windows/`

The active and maintained Windows packaging files are under:

`deployment/windows/`

CMake references the latter location, and no source reference to the
`resources/deployment/windows/` copy was found. The obsolete duplicate was
removed to avoid future drift and confusion.

## Runtime / Generated Files

The repository should not contain:

- build directories
- generated deploy/staging directories
- generated installers
- local databases
- logs
- image caches
- API credentials
- personal inventory exports
- IDE/machine-specific configuration

The root `.gitignore` contains rules intended to prevent common examples of
these files from being committed accidentally.

## Distribution Review

Source-repository hygiene and binary-distribution review are separate tasks.

Before publishing a Windows release, the final installer should still be
validated independently to confirm that:

- the expected BrickSuite executable is packaged
- required Qt runtime libraries/plugins are present
- the LGPL license is included
- no development-only files or credentials are included
- user data remains outside the application installation directory

BrickSuite's automated Windows deployment pipeline performs staging and
runtime-file validation before Inno Setup creates the installer.

## Ongoing Practice

Before major public releases:

1. Build from a clean clone.
2. Review `git status` before committing.
3. Scan the current tree and recent history for secrets/private artifacts.
4. Validate the final distribution package separately.
5. Rotate any credential immediately if it is ever exposed publicly.

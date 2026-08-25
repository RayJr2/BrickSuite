# Security Policy

## Supported Version

BrickSuite is currently in the v0.2.0 release line.

Security fixes will generally target the current public release and the active
development branch.

## Reporting a Security Issue

Please do **not** report security-sensitive issues through a public GitHub
issue.

If you believe you have found a vulnerability, credential-handling problem,
privacy issue, or other security-sensitive defect, please contact
RF StateSide, LLC privately before publishing details.

Until a dedicated security-reporting address or GitHub private vulnerability
reporting workflow is published, use the private contact method listed by
RF StateSide, LLC for the BrickSuite project.

When reporting a security issue, include only the information needed to
reproduce and understand the problem.

Please do not send:

- API keys
- passwords
- access tokens
- private certificates
- private databases
- unredacted application logs
- personal inventory exports
- other secrets or private data

If logs, screenshots, database excerpts, or exported data are relevant,
redact unrelated personal information and credentials before sharing them.

## API Credential Storage

BrickSuite stores provider API credentials using the operating system's
secure credential facility rather than storing new credentials in plaintext
application settings.

Current backends are:

- **Windows:** Windows Credential Manager
- **macOS:** macOS Keychain
- **Linux:** Secret Service / keyring through `secret-tool`

BrickSuite supports migration of legacy plaintext API-key values from
application settings. A legacy value is removed only after secure storage
succeeds. If migration fails, BrickSuite preserves the existing value rather
than silently losing the user's credential.

BrickSuite does not intentionally log API keys or other provider credentials.

## Provider and Network Security

BrickSuite currently integrates with Rebrickable and Brickset.

API-related changes should continue to use BrickSuite's centralized
provider/network architecture, including:

- connection validation
- request pacing and throttling
- provider-specific error handling
- fallback behavior where supported
- local caching where appropriate

Contributors should not introduce hard-coded credentials, embedded provider
secrets, or ad-hoc network requests that bypass the existing provider
infrastructure.

## Local Data

BrickSuite stores user-managed application data locally, including its SQLite
database, logs, and image cache.

On Windows, application-managed local data normally resides beneath:

```text
%LOCALAPPDATA%\RFStateSide\BrickSuite\
```

Equivalent platform-appropriate locations are used on macOS and Linux
through Qt's standard application-data paths.

Uninstalling BrickSuite intentionally does not delete the user's local
BrickSuite data.

Users are responsible for protecting access to their operating-system
account and local files. BrickSuite provides database backup and restore
functionality, but backups should also be treated as private data.

## Public Issues and Pull Requests

Never include credentials or private data in:

- GitHub issues
- pull requests
- screenshots
- logs
- test fixtures
- example configuration files
- committed database files
- build artifacts

If a public issue is later determined to contain sensitive information,
remove or redact the exposed material as quickly as possible and rotate any
affected credential.

## Dependencies and Third-Party Services

BrickSuite depends on Qt and interoperates with third-party data and API
providers.

Security reports involving a third-party dependency or provider should
distinguish between:

- a BrickSuite defect
- a dependency issue
- a provider-side issue

Do not publish third-party credentials or private provider data while
reporting the problem.

## Disclosure

Please allow reasonable time for investigation and remediation before
publicly disclosing a security-sensitive issue.

RF StateSide, LLC will evaluate reported issues based on reproducibility,
impact, affected versions, and whether the issue is within BrickSuite's
control.

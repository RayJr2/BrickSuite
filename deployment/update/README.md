# Update manifest and architecture selection

## Audited contract

Before this change, `Updater` selected `windows`, generic `macos`, `linux64`, or
`linuxarm` using OS/compiler macros. Linux's non-ARM branch also incorrectly
treated any other architecture as x86_64. There was no update DTO or updater test:
the service parsed `QJsonObject` directly.

The root contains `version`, `release`, `releaseDate`, `changelog`, and `downloads`.
Only `version`, `changelog`, and the chosen URL are consumed by the application.
`downloads.<key>` accepts a string or an object with `url` / `downloadUrl`.
The legacy `platforms.<key>` object representation is also accepted. Version and
release notes are shared by all artifacts in that manifest.

The application displays the available version and opens the selected URL in
the browser after confirmation. It does not download/install files or verify
SHA-256 values. No hash-verification guarantee is added by this change.

The v0.3.0 tag uses the same manifest URL and Windows entry contract. Only Windows
has previously shipped publicly. Its `downloads.windows.url` and root metadata
remain compatible; older Windows clients ignore new map keys. There is no prior
public macOS/Linux compatibility obligation.

## v0.4.0 selection

| Running build | Exact manifest key |
|---|---|
| Windows x86_64 | `windows` |
| macOS arm64 | `macos-arm64` |
| macOS x86_64 | `macos-x86_64` |
| Linux x86_64 | `linux64` |

The generic `macos` and unsupported `linuxarm` entries are removed. No filename
guessing or cross-architecture/generic fallback is allowed. Missing, empty, or
malformed entries produce “No compatible update package is available” when a
newer release exists. Unsupported OS/architectures also fail safely. A manifest
that is not newer still reports no update without offering any artifact.

Detection uses Qt's compiler-target `Q_PROCESSOR_ARM_64` / `Q_PROCESSOR_X86_64`
macros in BrickSuite itself, together with OS macros. These describe the running
executable's build, not the physical machine. Thus an Intel executable translated
by Rosetta still chooses Intel; a native ARM64 executable chooses ARM64. No
hardware/marketing-string probe is involved. Universal2 is not a shipped artifact.

[Qt's build-architecture documentation](https://doc.qt.io/qt-6/qsysinfo.html#buildCpuArchitecture)
distinguishes the compiled architecture from the emulated/physical CPU;
[Apple's Rosetta documentation](https://developer.apple.com/documentation/apple-silicon/about-the-rosetta-translation-environment)
describes translation of Intel applications. Compiler-target selection avoids
depending on what a translated process's OS CPU query reports.

## Release gate — do not publish placeholders

The checked-in manifest retains v0.3.0 metadata and the existing Windows/Linux
URLs. Both new Mac entries have **empty URLs**: no artifact is invented or offered.
This is a representation change, not publication of v0.4.0. No live manifest was
updated during implementation.

At M40.5, after approved artifacts exist:

1. Verify both Mac archives were built from the same approved release commit and
   carry the same v0.4.0 version. Retain their independent packaging/checksum evidence.
2. Set `downloads.macos-arm64.url` to the actual ARM64 archive URL and
   `downloads.macos-x86_64.url` to the actual Intel archive URL. These are distinct
   artifact URLs, not the generic latest-release page or a Universal2 archive.
3. Set the shared root version/date/channel/changelog and the final Windows/Linux
   URLs. Each Mac entry inherits that same version and release-note reference.
   Do not add placeholder hashes; the updater does not use hashes. Publish actual
   archive SHA-256 evidence with the release as produced by packaging.
4. Run `UpdaterTest`, independently verify each URL's downloaded artifact
   architecture and checksum, and manually check native ARM64, Intel, and an
   Intel process under Rosetta where available. Windows v0.3.0 compatibility must
   retain the root version and `downloads.windows.url` object shape.
5. Publish the live manifest only with explicit release authorization.

No packaging or release script currently writes `update.json`; CMake only lists
it as a project source and configures the application manifest URL. macOS CI and
packagers already emit separate architecture archives/checksums. Those scripts
and their behavior are unchanged.

## Validation

`UpdaterTest` parameterizes OS and compiled architecture, without relying on the
test host. It covers both Mac selections, no cross-selection, generic fallback
refusal, unsupported architectures, Windows/Linux x64, malformed/missing entries,
legacy Windows objects/string aliases, and the repository manifest structure.
Qt 6.10.3 MinGW Release build and focused Updater test are the implementation
checks. Native macOS/Rosetta runtime acceptance remains a release-platform check;
Windows tests are not claimed to substitute for it. Schema 35 / Protocol 1.5 and
publication metadata are unchanged.

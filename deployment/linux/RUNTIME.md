# BrickSuite 0.4.0 Linux x86_64

Download the matching v0.4.0 archive from [BrickSuite Releases](https://github.com/RayJr2/BrickSuite/releases).

For Ubuntu 22.04 or newer on x86_64:

```sh
tar -xzf BrickSuite-v0.4.0-Linux-x86_64.tar.gz
cd BrickSuite
./install.sh
```

Approve prerequisite installation if requested, then approve installation to
`/opt/BrickSuite`. Launch **Applications → BrickSuite**, or run `bricksuite`.
The installer prints the OS/version/architecture and missing runtime packages.
It inspects a simulated package plan and asks for explicit `y`/`yes` consent
(`[y/N]`, default No) before installing missing prerequisites and their required
APT dependencies. Empty input, EOF and noninteractive input decline. Plans involving
removals, upgrades, core runtime repair or system Qt installation are refused.
It does not open the application automatically. Python 3,
normally included with Ubuntu, is needed to run the installer.

To upgrade or reinstall, extract the new archive separately and run its
`./install.sh`. Close BrickSuite first. The installer stages and verifies the
complete bundle before replacing application files, with rollback on detected
copy, replacement or verification failures. It reports the installed version.
An unmanaged `/opt/BrickSuite` or conflicting system launcher/icon is left alone;
move a conflicting manual installation aside before retrying.

To uninstall:

```sh
/opt/BrickSuite/uninstall.sh
```

Confirm removal. Only managed application files, desktop entry, icon and command
launcher are removed. Databases, backups, settings, logs, LDraw, Host identities,
paired devices and Secret Service credentials remain in their established user
locations. Shared prerequisite packages are never uninstalled.

## Advanced: portable use and dependency details

Built on Ubuntu 22.04 with GCC 11 and Qt 6.10.3. You can still run
`./BrickSuite/BrickSuite` from an extracted archive without system installation.
The directory may be moved, including to a path with spaces. Automatic installation
in the candidate installer is eligible on Ubuntu 22.04+ x86_64, or specifically
Linux Mint 22.3 x86_64 with Ubuntu Noble base evidence (`ID=linuxmint`, `ID_LIKE`
containing both `ubuntu` and `debian`, and `UBUNTU_CODENAME=noble`). Other
Ubuntu-like distributions, LMDE, unknown Mint versions and missing/conflicting
base evidence are refused. Mint final-package/manual acceptance is still pending;
this policy does not change the already released v0.4.0 installer. Portable
compatibility elsewhere is separate. The build baseline remains Ubuntu 22.04 /
glibc 2.35. `./install.sh --check` checks requirements without sudo or file changes.

Pop!_OS 24.04, Zorin OS 18 and KDE neon with explicit Ubuntu Noble metadata are
recognized but **not qualified for automatic installation**. The installer explains
their Ubuntu base and stops before planning/installing dependencies. Recognition
does not claim runtime support; manual portable use remains separate. Other
Ubuntu-like identities are not authorized by `ID_LIKE=ubuntu` alone. Eventual KDE
neon qualification also requires testing credentials in its KDE/Secret Service
environment.

The installer detects actual x86_64 shared libraries through the system loader
cache, commands through PATH, session DBus socket units, available Secret Service
provider binaries and installed fonts. It does not query or unlock your keyring.
Ubuntu 24.04+ and qualified Mint 22.3/Noble use `libglib2.0-0t64` if that library
is missing. Mint's own version number is not used as an Ubuntu version.
APT/dpkg must be available and the native package architecture must be `amd64`;
sudo is needed only when not running as root. Finding `secret-tool` does not prove
an unlocked, working Secret Service.

Ownership conflicts are checked before offering package changes. If an APT
operation fails, some dependencies may already be installed, but BrickSuite
program files have not changed. Resolve the reported package/runtime failure
before retrying; the installer does not remove system dependencies to undo it.
Personal files must not be stored in `/opt/BrickSuite`, which is replaced/removed
as a managed program tree. Normal user data and externally selected files remain
outside that tree and are not scanned or deleted.

For advanced manual setup on Ubuntu 22.04, runtime prerequisites are:

```sh
sudo apt-get install libstdc++6 libgcc-s1 libc6 libgl1 libegl1 libopengl0 libglx0 \
  libglvnd0 libglib2.0-0 libdbus-1-3 libgssapi-krb5-2 libfontconfig1 fontconfig libfreetype6 fonts-dejavu-core \
  libx11-6 libx11-xcb1 libxcb1 libxcb-cursor0 libxcb-icccm4 libxcb-image0 \
  libxcb-keysyms1 libxcb-glx0 libxcb-randr0 libxcb-render0 libxcb-render-util0 libxcb-shape0 \
  libxcb-shm0 libxcb-sync1 libxcb-xfixes0 libxcb-xkb1 libxcb-util1 \
  libxcb-xinerama0 libxkbcommon0 libxkbcommon-x11-0 libwayland-client0 \
  libwayland-cursor0 libwayland-egl1 libsecret-tools gnome-keyring dbus dbus-user-session
```

Use the graphics drivers appropriate to your computer. System glibc, libstdc++,
libgcc, desktop libraries and GPU drivers are deliberately not bundled. Keeping
the host C++ runtime avoids overriding libraries needed by newer GPU drivers.
The private runtime includes Qt, ICU, MCUT, OpenSSL 3.5.8 and selected compression
libraries. See `share/dependencies.json` and `share/abi-audit.json`.

Credential storage requires the system `secret-tool` command, a working session
DBus and an unlocked Secret Service provider (for example GNOME Keyring).
BrickSuite does not provide a plaintext fallback. Launch from your logged-in
desktop session. Missing/unavailable credential services are reported as errors.

X11/XWayland and native Wayland plugins are included. A functioning OpenGL/EGL
stack is needed for the 3D Viewer. Diagnostic selection is possible with
`QT_QPA_PLATFORM=xcb ./BrickSuite` or `QT_QPA_PLATFORM=wayland ./BrickSuite`.
Choose an installed LDraw library in Settings before using source-backed models.

For portable use without the installer, desktop integration is optional. Copy `share/applications/BrickSuite.desktop` to
`~/.local/share/applications/`, editing Exec to the quoted absolute path of this
bundle's launcher and Icon to its `share/icons/hicolor/256x256/apps/BrickSuite.png`.
No file associations are installed.

Licenses and third-party notices are under `share/licenses`. Source URLs, hashes,
and revisions are in `share/source-lock.json`; the application revision is in
`share/build-metadata.json`. Qt and MCUT are dynamically linked and can be replaced
with compatible modified builds in `lib`; preserve the SONAMEs and relative RPATHs.
The MCUT Linux queue overlay and its rationale are included. Two Qt Widgets
warning-message filenames have their vendor build prefix normalized to a relative
prefix; the exact equal-length remapping recipe and record are included with the
Qt notices. No Qt executable instructions are changed by that normalization. The corresponding
BrickSuite source and build instructions must accompany redistribution or remain
available as required by the included licenses. This archive does not contain
proprietary user data, API keys, an LDraw library, or private signing material.

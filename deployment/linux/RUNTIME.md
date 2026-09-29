# BrickSuite 0.4.0 Linux x86_64

For Ubuntu 22.04 or newer on x86_64:

```sh
tar -xzf BrickSuite-v0.4.0-Linux-x86_64.tar.gz
cd BrickSuite
./install.sh
```

Approve prerequisite installation if requested, then approve installation to
`/opt/BrickSuite`. Launch **Applications → BrickSuite**, or run `bricksuite`.
The installer prints the OS/version/architecture and missing runtime packages.
It asks before sudo, installs only missing prerequisites (plus their required
APT dependencies), and does not open the application automatically. Python 3,
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
is supported only on Ubuntu 22.04+ x86_64; portable compatibility elsewhere is
separate. `./install.sh --check` checks requirements without sudo or file changes.

The installer detects actual x86_64 shared libraries through the system loader
cache, commands through PATH, session DBus socket units, available Secret Service
provider binaries and installed fonts. It does not query or unlock your keyring.
Ubuntu 24.04+ uses `libglib2.0-0t64` if that library is missing.

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

# AirMouse

Webcam hand-gesture mouse for Ubuntu 24.04 (X11 and Wayland) and Windows 11.

Point with the index finger. Pinch thumb+index to left-click. Thumb+middle for right-click. Open palm to clutch. Fist to lock.

All inference is on-device. This machine’s daily-driver path is **GNOME on X11** (XTest). Wayland uses a virtual uinput pointer.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Run from the repo so assets resolve, or set `AIRMOUSE_ASSETS`:

```bash
./build/airmouse
```

Pause / resume: `Ctrl+Alt+Shift+C`, or left-click the tray icon. On Wayland bind a GNOME shortcut to:

```bash
gdbus call --session --dest org.airmouse.App --object-path /org/airmouse/App \
  --method org.airmouse.App.TogglePause
```

Wayland input needs `/dev/uinput` access once:

```bash
sudo cp packaging/linux/udev/99-airmouse-uinput.rules /etc/udev/rules.d/
sudo usermod -aG input "$USER"
# then log out
```

Config lives at `~/.config/airmouse/config.json`.

## Releases

GitHub Actions builds Linux and Windows packages on every `v*` tag (and on demand via **Actions → Release → Run workflow**).

```bash
git tag v0.1.0
git push origin v0.1.0
```

That publishes:

- `airmouse_<version>_amd64.deb` — Debian / Ubuntu
- `airmouse-<version>-1.x86_64.rpm` — Fedora / RHEL / openSUSE
- `airmouse-<version>-linux-x86_64.tar.gz` — generic Linux
- `airmouse-<version>-windows-x64.exe` — Windows 11 NSIS installer
- `airmouse-<version>-windows-x64.msi` — Windows 11 MSI
- `airmouse-<version>-windows-x64.zip` — portable Windows build (`airmouse.exe`)

CI on `main` and pull requests compiles and tests both platforms without making a GitHub Release.

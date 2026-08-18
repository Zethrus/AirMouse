# CamMouse

Webcam hand-gesture mouse for Ubuntu 24.04 (X11 and Wayland) and Windows 11.

Point with the index finger. Pinch thumb+index to left-click. Thumb+middle for right-click. Open palm to clutch. Fist to lock.

All inference is on-device. This machine’s daily-driver path is **GNOME on X11** (XTest). Wayland uses a virtual uinput pointer.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Run from the repo so assets resolve, or set `CAMMOUSE_ASSETS`:

```bash
./build/cammouse
```

Pause / resume: `Ctrl+Alt+Shift+C`, or left-click the tray icon. On Wayland bind a GNOME shortcut to:

```bash
gdbus call --session --dest org.cammouse.App --object-path /org/cammouse/App \
  --method org.cammouse.App.TogglePause
```

Wayland input needs `/dev/uinput` access once:

```bash
sudo cp packaging/linux/udev/99-cammouse-uinput.rules /etc/udev/rules.d/
sudo usermod -aG input "$USER"
# then log out
```

Config lives at `~/.config/cammouse/config.json`.

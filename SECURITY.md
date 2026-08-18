# Security policy

AirMouse is a local desktop utility. It does not talk to a backend, store
accounts, or ship API keys.

## Reporting a vulnerability

Please **do not** open a public issue for security problems.

Use [GitHub private vulnerability reporting](https://github.com/Zethrus/AirMouse/security/advisories/new)
or email **contact@zethr.us**.

Include:

- What you found and how to reproduce it
- Affected version or commit
- Impact if it is exploited

You should hear back within a few days.

## What this project stores

- Config under `~/.config/airmouse/` (or `%APPDATA%\AirMouse\` on Windows)
- Webcam frames stay in memory for landmark inference and are not written to disk
- No telemetry, no cloud calls from the application itself

# Grabbable Palantir HUD

The AirMouse overlay is a native always-on-top panel (`220×148`, click-through, pinned top-right). This plan makes it draggable anywhere on the virtual desktop with 1:1 tracking and a spring settle, restyles it as a compact optical instrument (Palantir Gotham / movie HUD, not a rounded toast), and replaces the ad-hoc colors with an **app-wide design-token sheet** that overlay, settings, and any later surface all read.

Daily-driver path stays GNOME on X11 (Cairo) with a matching Windows layered-window path (GDI+).

## Current state

| Surface | What it does today |
|---|---|
| `theme.hpp` | Five hex colors + four HUD numbers. Settings and both overlays hard-code alphas, font sizes, paddings, and radii next to them. |
| `overlay_x11.cpp` | `override_redirect`, empty XShape input (fully click-through), created at `(DisplayWidth − 220 − 24, 24)`. Duplicates `rounded_rect` / `set_hex`. |
| `overlay_win.cpp` | `WS_EX_LAYERED \| WS_EX_TRANSPARENT` + `HTTRANSPARENT`. `apply_layout()` **resets** to top-right on every show / DPI / display change. Paint logic is a second copy of the X11 HUD. |
| `settings_x11.cpp` | Custom Cairo window. Copies the same `rounded_rect` / `set_hex`. Uses `theme::` colors but invents its own 380×392 layout, 8 px radii, 12/18 px type. |
| `settings_win.cpp` | Stock Win32 checkboxes on `COLOR_WINDOW`. No tokens at all. |
| `HudConfig` | `enabled`, `chips`, `show_camera` — no position. |
| Main loop | `overlay->poll()` every ~16 ms. |

The screenshot is this HUD sitting on a browser window: dark rounded card, `NO HAND` / `IDLE`. The browser chrome behind it is not part of the HUD.

**Hard constraint:** AirMouse *is* the pointer. If the whole panel becomes hit-testable, a pinch-click over the HUD grabs chrome instead of the app underneath. The panel stays click-through except for a dedicated grip.

## Design direction

Subject: a camera-driven targeting glass for the operator's hand. Audience: someone who leaves this on all day. Job: show live hand state without stealing clicks, and get out of the way when dragged.

**Signature (the one risk):** a corner-bracketed constellation over a faint ranging grid. The index-finger ray is the only hot stroke. Everything else is steel. Optical bench, not a toast, not acid-green cyberpunk.

Rejected: scanlines, hexagon overlays, neon green, stadium radii, gold pills, cream paper.

### Wireframe

```
┌─ ∷  AIRMOUSE              ● LIVE ─┐   grip rail (only hit target)
│ ⌜                               ⌝ │
│     ·  ·  ·  ·  ·  ·  ·  ·  ·     │   8×5 ranging ticks
│        constellation / NO HAND    │
│                                   │
│ ⌞                               ⌟ │
│ IDLE                    ▰▰▰▱▱ .72 │   pose + 5-seg confidence
└───────────────────────────────────┘
```

Hover grip: rail hairline 0.14 → 0.40, dots 0.30 → 0.90, cursor → grab, 120 ms. Press: panel lifts (alpha +0.06, 1 px ice rim) in 80 ms. Motion is **1:1 with the pointer** — no lerp while dragging. Release near an edge/corner: critically-damped spring into the snap. Otherwise it stays where it dropped.

Pose chips become a slim bracketed toast (`[ PINCH ]`).

## App-wide design tokens

`theme.hpp` becomes the **single source of truth** for every painted surface. No HUD-prefixed fork, no leftover gold in settings. Changing a token restyles overlay + settings on the next rebuild.

Structure — namespaced constexpr, no macros, no runtime theme loader:

```cpp
namespace airmouse::theme {

namespace color {
  inline constexpr uint32_t void_   = 0x070A0E;  // cool night fill
  inline constexpr uint32_t panel   = 0x0C1218;  // opaque settings chassis
  inline constexpr uint32_t paper   = 0xD7E0E6;  // primary text
  inline constexpr uint32_t dim     = 0x6B7780;  // secondary text
  inline constexpr uint32_t accent  = 0x7FD4C8;  // ice teal — live only
  inline constexpr uint32_t warn    = 0xD4A054;  // NO CAM / LOST
  inline constexpr uint32_t hair    = 0xC5D0D6;  // strokes, always with an alpha
}

namespace alpha {
  inline constexpr double glass     = 0.82;
  inline constexpr double lift      = 0.88;
  inline constexpr double hair      = 0.16;
  inline constexpr double hair_dim  = 0.10;
  inline constexpr double grid      = 0.06;
  inline constexpr double bone      = 0.20;
  inline constexpr double ray       = 0.70;
  inline constexpr double chip      = 0.92;
}

namespace type {
  inline constexpr float micro   = 8.f;   // AIRMOUSE, LIVE, grid labels
  inline constexpr float caption = 9.f;   // chips
  inline constexpr float label   = 11.f;  // pose, settings chips
  inline constexpr float body    = 12.f;  // settings rows
  inline constexpr float title   = 18.f;  // settings wordmark
}

namespace space {
  inline constexpr int xxs = 2;
  inline constexpr int xs  = 4;
  inline constexpr int sm  = 8;
  inline constexpr int md  = 12;
  inline constexpr int lg  = 16;
  inline constexpr int xl  = 24;
}

namespace radius {
  inline constexpr double tight = 4;
  inline constexpr double panel = 8;
  inline constexpr double chip  = 8;
}

namespace motion {
  inline constexpr float hover_tau = 0.12f;
  inline constexpr float lift_tau  = 0.08f;
  inline constexpr float spring_tau = 0.055f;
  inline constexpr float snap_px   = 28.f;
}

namespace hud {
  inline constexpr int w = 236;
  inline constexpr int h = 164;
  inline constexpr int grip_h = 22;
  inline constexpr int pad = 24;          // default work-area inset
  inline constexpr int well_inset = 10;
  inline constexpr int bracket = 12;
  inline constexpr int grid_x = 8;
  inline constexpr int grid_y = 5;
  inline constexpr int seg_w = 4;
  inline constexpr int seg_h = 8;
  inline constexpr int seg_gap = 2;
  inline constexpr int segs = 5;
}

namespace settings {
  inline constexpr int w = 380;
  inline constexpr int h = 424;           // + one row for Reset HUD
}

// Back-compat aliases so existing call sites compile during the cutover:
inline constexpr uint32_t kVoid   = color::void_;
inline constexpr uint32_t kPaper  = color::paper;
inline constexpr uint32_t kDim    = color::dim;
inline constexpr uint32_t kAccent = color::accent;
inline constexpr uint32_t kHair   = color::hair;
inline constexpr int kHudW = hud::w;
inline constexpr int kHudH = hud::h;
inline constexpr double kHudRadius = radius::panel;
inline constexpr double kHudAlpha = alpha::glass;

}  // namespace airmouse::theme
```

Rules for using tokens:

- Painters never invent a hex, font size, radius, or duration. If a new value is needed, it is named in `theme.hpp` first.
- Alphas live in `theme::alpha`, not as magic `0.14` next to a stroke.
- Layout numbers for the HUD live in `theme::hud`; settings window size in `theme::settings`.
- Existing `kVoid` / `kHudW` aliases stay for one pass so the cutover is mechanical, then call sites move to the namespaced tokens and the aliases drop.

Fonts stay IBM Plex Mono (instrument / HUD / chips) and IBM Plex Sans (settings title + body). No new faces.

## Modular code shape

Platform files become thin windowing shells. Logic and paint recipes are shared and unit-tested.

```
src/airmouse/ui/theme.hpp        // tokens only — no functions
src/airmouse/ui/hud_layout.hpp   // pure geometry: clamp, default, snap, grip
src/airmouse/ui/hud_drag.hpp     // pure state machine: hover / drag / spring
src/airmouse/ui/hud_metrics.hpp  // where every HUD element sits, from tokens
src/airmouse/ui/cairo_draw.hpp   // Linux paint primitives (header-only)
src/airmouse/ui/gdi_draw.hpp     // Windows paint primitives (header-only, #ifdef)
src/airmouse/ui/overlay.hpp      // + placement API
src/airmouse/ui/overlay_x11.cpp  // X11 window + events + “call drag + paint”
src/airmouse/ui/overlay_win.cpp  // Win32 window + events + “call drag + paint”
src/airmouse/ui/settings_x11.cpp // same cairo_draw + tokens
src/airmouse/ui/settings_win.cpp // owner-drawn onto tokens (see below)
tests/test_hud_layout.cpp
tests/test_hud_drag.cpp
```

### `hud_layout.hpp` — placement

- `Rect {x,y,w,h}`, `WorkArea` alias
- `default_position(work)` → top-right + `theme::hud::pad`
- `clamp_position(work, x, y)` → HUD fully inside work area
- `snap_target(work, x, y)` → if within `theme::motion::snap_px` of an edge/corner, snapped origin; else input
- `grip_rect()` / `in_grip(lx, ly, scale)`
- `offscreen(work, x, y)` → true if no overlap; caller treats as unplaced
- `spring_step(pos, vel, target, dt, tau)`
- `ease_exp(current, target, dt, tau)`

### `hud_drag.hpp` — one state machine, both platforms

```cpp
struct HudDrag {
  enum class Phase { Idle, Hover, Dragging, Settling };

  Phase phase = Phase::Idle;
  float x = 0, y = 0, vx = 0, vy = 0;
  float hover_t = 0, lift_t = 0;
  bool placed = false;

  void place(const HudConfig&, WorkArea);
  HudConfig config() const;

  void on_enter();
  void on_leave();
  bool on_press(float pointer_x, float pointer_y);   // false if not in grip
  void on_move(float pointer_x, float pointer_y, WorkArea);
  bool on_release(WorkArea);                         // true → persist
  bool tick(float dt, WorkArea);                     // true → dirty
};
```

Overlays do not reimplement grab-offset, snap, or easing. They translate OS events into these calls and apply `drag.x/y` to `XMoveWindow` / `UpdateLayeredWindow`.

### `hud_metrics.hpp` — paint coordinates

One function, `HudMetrics hud_metrics(float scale)`, returns every rect/point the painters need (grip, wordmark, pip, well, brackets, grid origin/step, footer, chip band, confidence segments). Both Cairo and GDI+ read this. Changing the wireframe is a one-file edit.

### Draw helpers

`cairo_draw.hpp` / `gdi_draw.hpp` expose the same small vocabulary, taking tokens not literals:

- `fill_round_rect`, `stroke_round_rect`
- `hairline`, `dot`, `brackets` (four L-corners), `range_ticks`
- `label(face, size, color, alpha, x, y, text)`
- `segments(n, filled, x, y)` — the 5-bar confidence meter
- `chip_brackets(text)` — `[ PINCH ]` toast

`overlay_x11.cpp` and `settings_x11.cpp` both include `cairo_draw.hpp`. The duplicated `rounded_rect` / `set_hex` in those two files is deleted.

### Settings on the same tokens

X11 settings already custom-paints — switch it to `theme::color/type/space/radius` and `cairo_draw`. Same ice chassis, same chip language as the HUD, plus a **Reset HUD position** chip.

Windows settings currently uses stock `COLOR_WINDOW` controls, so it cannot pick up tokens without being owner-drawn. Restyle it as a small owner-drawn window (same 380×424 metrics, same chip hit-testing as X11) so both settings windows share layout constants from `theme::settings`. No new Win32 common-control theming rabbit hole.

## Drag, by platform

Shared `HudDrag` plus:

**X11.** Replace the empty input shape with one rectangle over the grip (`XShapeCombineRectangles` ShapeInput). Add `ButtonPress/Release`, `PointerMotion`, `Enter/Leave`. `XGrabPointer` on press, `XMoveWindow` on move, `XUngrabPointer` on release. Cursor `XC_fleur` while hover/drag. Keep `override_redirect` + `_NET_WM_STATE_ABOVE`. Still XWayland on Wayland — no layer-shell this pass.

**Windows.** Drop `WS_EX_TRANSPARENT`; keep layered / topmost / tool / noactivate. `WM_NCHITTEST`: `HTCLIENT` in the grip (scaled), `HTTRANSPARENT` elsewhere — per-pixel alpha is **not** enough, the 0.82 panel would steal clicks. `SetCapture` / `ReleaseCapture`. Move via existing `UpdateLayeredWindow` `pt_dst`. Cursor `IDC_SIZEALL`. `apply_layout()` only rescales and **clamps**; it never writes top-right over a placed origin.

**Work area.** Clamp to the virtual desktop (`query_screen_geometry()`). Snap against the current monitor work area: Windows `MonitorFromRect` + `rcWork`; X11 `_NET_WORKAREA`, else `DisplayWidth/Height`.

**Motion budget.** `poll()` already ~60 Hz. While `hover_t`, `lift_t`, or the spring is in flight, `tick()` returns dirty and painters redraw. Use real `dt` from `steady_clock`. No position interpolation during drag.

## Persistence

```cpp
struct HudConfig {
  bool enabled = true;
  bool chips = true;
  bool show_camera = false;
  bool placed = false;   // false → default top-right
  int x = 0;
  int y = 0;
};
```

Written **on drop only**. On launch / display / DPI change: if `placed`, clamp; if `offscreen(...)`, treat as unplaced. Tray hide/show keeps the last origin.

Overlay API:

```cpp
virtual void set_placement(const HudConfig&) = 0;
virtual HudConfig placement() const = 0;
virtual void set_on_moved(std::function<void(HudConfig)>) = 0;
```

`main.cpp` pushes `cfg.hud` after create and on settings change; saves when `on_moved` fires.

## Paint order (both overlays, from `hud_metrics`)

1. Chassis `color::void_ @ alpha::glass`, radius `radius::panel`, hair `alpha::hair`.
2. Grip rail: 6-dot cluster, `AIRMOUSE` at `type::micro`, pip + `LIVE`/`IDLE`/`CAM`. Pip = accent / dim / warn.
3. Rule under the rail at `hud::grip_h`, `alpha::hair_dim`.
4. Four L-brackets in the well (`hud::bracket`).
5. Range ticks `hud::grid_x × hud::grid_y` at `alpha::grid`.
6. Constellation: bones `alpha::bone`, index ray `alpha::ray`, tips paper @ 0.30, index tip accent @ 0.95. Empty: `NO HAND` / `NO CAM`.
7. Footer: pose at `type::label`; `hud::segs` confidence bars + two-digit value.
8. Chip: `chip_brackets`, fill `alpha::chip`.

Lift (`lift_t → 1`): chassis alpha `alpha::lift`, hair 0.28, 1 px accent rim @ 0.35.

`show_camera` stays unused.

## File-by-file

| File | Change |
|---|---|
| `src/airmouse/ui/theme.hpp` | Full token sheet + temporary `k*` aliases |
| `src/airmouse/ui/hud_layout.hpp` | **New.** Pure placement |
| `src/airmouse/ui/hud_drag.hpp` | **New.** Pure drag / hover / spring |
| `src/airmouse/ui/hud_metrics.hpp` | **New.** Scaled paint geometry |
| `src/airmouse/ui/cairo_draw.hpp` | **New.** Shared Cairo primitives |
| `src/airmouse/ui/gdi_draw.hpp` | **New.** Shared GDI+ primitives |
| `src/airmouse/ui/overlay.hpp` | Placement API |
| `src/airmouse/ui/overlay_x11.cpp` | Thin shell: shape, grab, `HudDrag`, paint via metrics |
| `src/airmouse/ui/overlay_win.cpp` | Thin shell: hit-test, capture, `HudDrag`, no layout reset |
| `src/airmouse/ui/settings_x11.cpp` | Tokens + `cairo_draw`; Reset HUD chip; drop local helpers |
| `src/airmouse/ui/settings_win.cpp` | Owner-drawn onto the same tokens/metrics; Reset HUD |
| `src/airmouse/config.hpp` + `config.cpp` | `placed`, `x`, `y` |
| `src/airmouse/main.cpp` | Wire placement + persist on drop |
| `tests/test_hud_layout.cpp` | Clamp, default, snap, grip, off-screen |
| `tests/test_hud_drag.cpp` | Press outside grip ignored; drag offset; snap-on-release; spring settles |
| `CMakeLists.txt` | Add both test sources |
| `README.md` | HUD is draggable by the top rail; position persists |

Header-only modules stay out of `AIRMOUSE_SOURCES`. No new compiled UI `.cpp` unless a helper grows past ~40 lines.

## Implementation order

1. Expand `theme.hpp`. Mechanically point existing painters at the aliases so nothing regresses.
2. `hud_layout.hpp` + `test_hud_layout`. Contract first.
3. `hud_drag.hpp` + `test_hud_drag`.
4. `hud_metrics.hpp`, `cairo_draw.hpp`. Delete duplicated helpers in the X11 files.
5. Config + overlay API + `main.cpp` wiring.
6. X11: input shape + `HudDrag` + persist. Then Windows hit-test + `HudDrag` + stop the layout reset.
7. Restyle both overlays from `hud_metrics` + tokens.
8. Settings onto the same tokens; Reset HUD control. Drop `k*` aliases once every call site is namespaced.

## Risks

- **Virtual pointer vs. grip.** A 22 px rail can still be clipped by the camera mouse. Acceptable. Do not expand hit-testing to the card.
- **XGrabPointer vs. XTest/uinput.** Grab is only live during a real button-1 on the grip. No event-source filtering — XTest is hard to distinguish.
- **Wayland.** Overlay remains XWayland override-redirect. No `zwlr_layer_shell`.
- **Win settings rewrite.** Owner-drawing the Windows settings window is more code than a stock button, but it is the only way tokens apply “throughout the app.” Keep the hit-test table identical to X11 so behavior cannot drift.
- **Alias window.** `kVoid` / `kHudW` exist only during the cutover. Last step deletes them so we do not keep two names for one token.

## Verification

Native overlay — no browser pass.

- `cmake --build build -j && ctest --test-dir build --output-on-failure` — `HudLayout.*`, `HudDrag.*`, existing suite.
- Linux daily driver:
  - Hover grip → cursor + rail light up.
  - Drag across the desktop / second monitor — 1:1, no stutter.
  - Drop near a corner → spring into `hud::pad`. Drop in the open → stays.
  - Restart → same pixel origin. Hide/show tray → same origin, not top-right.
  - Resolution / monitor loss → reclamps, never lost off-screen.
  - Pinch-click through the constellation well still hits the window underneath.
  - Settings uses the same ice/teal language as the HUD. Reset HUD → top-right.
  - Pose chips appear as `[ PINCH ]`.
- Windows path compiles in CI; drag is the same `HudDrag`. If no Windows box is available, say so after the Linux pass.

## Out of scope

- Wayland layer-shell HUD
- Whole-card dragging
- Gesture-to-reposition
- Runtime theme switching / user-editable palettes
- Implementing `show_camera`
- Multi-HUD / per-monitor clones

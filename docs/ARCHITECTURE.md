# Code map

Start with `src/x11/main.cpp` for command dispatch, then `tapzap_core.hpp`
for the color math and `gamma_backend.hpp` for the display-backend interface.
The five variants are independently preserved implementations. Similar files
are intentionally kept in each variant until changes can be validated across
the affected sessions; there is no shared runtime library yet.

## Files within each variant

Paths below are relative to `src/<variant>/`. Omarchy/Hyprland puts its
application files one level deeper, in `src/omarchy-hyprland/src/linux/`.

| File | Responsibility |
|---|---|
| `main.cpp` | CLI dispatch, color self-test, and offline image commands |
| `tapzap_core.hpp` | Warmth/brightness factors, gamma ramps, and lookup tables; no display access |
| `gamma_backend.hpp` | `IGammaBackend`: initialize, apply a filter, restore, and enumerate outputs |
| `gamma_headless.cpp` | Non-display backend used when native display support is unavailable |
| `gamma_xrandr.cpp` | Capture and modify X11 CRTC gamma ramps |
| `ui_cairo.cpp`, `ui.hpp` | Xlib/Cairo interface, input handling, and UI event loop |
| `platform.hpp` | Settings persistence, autostart, and platform utilities |
| `ipc.hpp` | Unix socket commands for an existing app instance |
| `pwm.hpp`, `logind_brightness.hpp` | Experimental hardware-brightness integration |
| `tray.cpp` | Separate system-tray process |
| `ppm.hpp` | P6 PPM reading and writing for offline image commands |
| `assets/` | Bundled font subsets and their upstream license |

## Session-specific code

| Variant | Additional files |
|---|---|
| GNOME Wayland | `gamma_gnome.cpp` and `gnome_helper.py` communicate with Mutter; `gamma_wayland.cpp` provides the optional wlroots path |
| Plasma Wayland | `gamma_plasma.cpp`, `plasma_helper.py`, and `make_profile.py` manage KWin color profiles |
| Sway | `gamma_wayland.cpp` uses `wlr-gamma-control`; protocol definitions live in `protocols/` |
| Omarchy / Hyprland | `gamma_hyprland.cpp` and `ctm/engine.c` implement CTM control; `integration/` contains the launcher and desktop integration |

Some snapshots also contain fallback backend files. A filename's presence
does not mean the active compositor supports it. See the
[support matrix](SUPPORT-MATRIX.md) and [build scripts](BUILDING.md).

## Checks and provenance

- `tests/test_color_core.py` compiles the color contract against all five
  variants without applying display changes.
- The other root tests check licenses, local documentation links, issue forms,
  and the source manifest.
- `scripts/ci-build-variant.sh` requires the native backend/UI/tray features
  expected for the selected variant; Omarchy additionally runs isolated
  protocol and package tests.
- `docs/SOURCE-MANIFEST.json` records both each imported snapshot hash and
  the current public file hash. `make format` refreshes the latter.

Formatting follows `.clang-format` for project C/C++ and `pyproject.toml` for
Python. Fonts, protocol XML, and upstream license text retain their original
bytes. Read [known issues](KNOWN-ISSUES.md) before changing recovery or display
ownership behavior. The code map is an aid to review, not evidence that these
experimental paths are fully validated.

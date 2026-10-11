# Commands

Run development commands from the repository root after following the
[setup instructions](../README.md#start-here).

| Command | Purpose |
|---|---|
| `make` or `make help` | Show the available development commands |
| `make test` | Run all root tests without accessing a display |
| `make verify` | Verify source hashes, license notices, and private-path patterns |
| `make check` | Run verification, tests, syntax, formatting, and secret scans |
| `make build VARIANT=x11` | Build native X11 and run its color self-test |
| `make build VARIANT=gnome-wayland` | Build the GNOME variant |
| `make build VARIANT=plasma-wayland` | Build the Plasma variant |
| `make build VARIANT=sway` | Build the Sway variant |
| `make build VARIANT=omarchy-hyprland` | Build, test, and package the Hyprland trial from a clean commit |
| `make format` | Format project C/C++ and Python, then update public source hashes |
| `make check-format` | Check the pinned formatting rules without changing files |
| `make scan-secrets` | Scan working files and all available local Git refs with Gitleaks |

Native builds require Linux and the [matching development packages](BUILDING.md).
Omarchy packaging refuses a dirty Git tree. Other builds leave their executable
at `src/<variant>/build/tapzap`; Omarchy uses
`src/omarchy-hyprland/build/tapzap`.

## Offline application commands

After building, run these from the selected variant directory. They operate
on color data or local image files and do not enable a display filter.

```sh
./build/tapzap selftest
./build/tapzap csv 1001
./build/tapzap dump-ramp 1.0 0.70 256
./build/tapzap gradient gradient.ppm 1100 240
./build/tapzap swatches swatches.ppm
./build/tapzap apply 1.0 0.70 input.ppm output.ppm
```

Intensity runs from `0.0` (neutral) to `1.0` (red-only factors); brightness
is a multiplier in `[0.0, 1.0]`. PPM input must be binary P6 with max value
255. Generated colors describe software values, not measured panel output.

## Commands that control a display

`tapzap gui [intensity brightness enabled]` opens the app. `tapzap on`,
`off`, `toggle`, and `show` send commands to an existing instance.
`tapzap run <intensity> <brightness>` applies a filter and requests restoration
immediately afterward. These commands access display state: they are not
build checks, and unresolved [recovery issues](KNOWN-ISSUES.md) apply.

Use the [backend setup](BUILDING.md), [first-run guidance](INSTALLING.md), and
[recovery steps](TROUBLESHOOTING.md) before evaluating them. Running `tapzap`
with no arguments prints usage and exits with status 2. The preserved CLI
does not implement a `--help` option.

# Building from source

Tap Zap Linux currently keeps each desktop-session backend in its own source
directory. Build on Linux for the session you intend to use; a successful
compile does not prove that the compositor, GPU, physical display, recovery
path, or tray integration works on your machine. All variants remain
experimental while the documented recovery findings are unresolved.

## Common test

From the repository root:

```sh
python3 -m unittest discover -s tests -v
```

The test compiles the C++17 color-core contract separately against each
variant. Use a C++17 compiler (`g++` or `clang++`) and Python 3.

## X11

```sh
cd src/x11
./build.sh
./build/tapzap selftest
```

For the live XRandR backend and Cairo UI, install `pkg-config`, X11, XRandR,
Xext, XFixes, Cairo, and FreeType development packages. On Debian/Ubuntu, the
package names are `libx11-dev libxrandr-dev libxext-dev libxfixes-dev
libcairo2-dev libfreetype6-dev`. Without these packages the preserved script
can produce a headless or UI-stub build; do not treat that as a usable X11 app.

## GNOME Wayland

```sh
cd src/gnome-wayland
./build.sh
./build/tapzap selftest
```

The native wlroots path needs `wayland-client` and `wayland-scanner`; the X11
fallback/UI also needs X11, XRandR, Xext, XFixes, Cairo, and FreeType. GNOME
compositor control uses `gnome_helper.py` and is specific to GNOME/Mutter.
For a manual local run, the helper path and private state directory must be set
in the environment before launching the app:

```sh
export TAPZAP_GNOME_HELPER="$PWD/gnome_helper.py"
export TAPZAP_GNOME_STATE="${XDG_STATE_HOME:-$HOME/.local/state}/tapzap/gnome"
```

The helper requires the system Python 3 GObject introspection bindings
(`python3-gi` on Debian/Ubuntu). Do not run the UI to validate a build while
the restoration findings remain open.

## KDE Plasma Wayland

```sh
cd src/plasma-wayland
./build.sh
./build/tapzap selftest
```

The KWin integration uses `plasma_helper.py` and `make_profile.py`. The X11 fallback/UI needs X11,
XRandR, Xext, XFixes, Cairo, and FreeType development packages.
KWin helper runs require `kscreen-doctor` and the system LittleCMS 2 library.
For a manual local run, set the helper and state paths before launching:

```sh
export TAPZAP_PLASMA_HELPER="$PWD/plasma_helper.py"
export TAPZAP_PLASMA_STATE="${XDG_STATE_HOME:-$HOME/.local/state}/tapzap/plasma"
```

Do not run the UI to validate a build while the restoration findings remain
open.

## Sway / wlroots Wayland

```sh
cd src/sway
./build.sh
./build/tapzap selftest
```

The native gamma path needs `wayland-client` and `wayland-scanner`; the X11
fallback/UI needs X11, XRandR, Xext, XFixes, Cairo, and FreeType development
packages.

## Omarchy / Hyprland

```sh
cd src/omarchy-hyprland
bash build.sh
bash test.sh
```

This is an experimental Hyprland CTM-v2 integration, not a generic Wayland
backend. It requires Linux x86_64, `wayland-client`, `wayland-scanner`, Cairo,
FreeType, X11/Xext/XRandR, GIO, D-Bus, and the matching Hyprland protocol.
`test.sh` additionally needs `wayland-server`, `dbus-run-session`, and Python
packages listed in that script. The build runs the native color self-test.
After the source commit is clean, `bash package.sh` creates a local trial
archive, records the source revision, includes the GPL and component notices,
and reruns the build, integration, and isolated install/uninstall checks. This
is a packaging check, not a recommendation to distribute or install while the
known reliability findings remain open.

## Important build limitation

The imported X11/GNOME/Plasma/Sway scripts originated as permissive beta
scripts: they may silently omit a backend or UI when development packages are
missing. Until the repository's strict backend-presence checks pass, inspect
the build output and do not interpret exit status alone as backend evidence.
See [the support matrix](SUPPORT-MATRIX.md) and
[known issues](KNOWN-ISSUES.md) before running a build.

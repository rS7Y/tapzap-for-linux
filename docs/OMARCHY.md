# TAP ZAP for Omarchy

TAP ZAP runs as a small tray add-on: open the lightning icon, choose DAY,
EVENING or NIGHT, adjust warmth and brightness, then switch the filter ON or
OFF. Closing the panel leaves the tray and active filter running. Quit restores
the display and exits.

This independent, experimental add-on targets Linux x86_64 with Hyprland CTM
v2 and XWayland. Native checks on Omarchy 4.0.2 / Hyprland 0.56.2 and one Dell
are recorded in [Validation](VALIDATION.md). Other GPUs, HDR, hotplug and
suspend/resume remain unverified. Read [Known issues](KNOWN-ISSUES.md).

## Install

A public binary release is still pending. For a verified local package, keep
the archive and its `.sha256` file together, then run from the repository root:

```sh
python3 src/omarchy-hyprland/install.py --archive /path/to/tapzap-omarchy-0.2.0-linux-x86_64.tar.gz
```

No administrator access is needed. The installer checks the download and its
contents, adds the application launcher and `tapzap` command, and integrates
outside-click dismissal with an existing `hypr/hyprland.lua`. It backs up
changed integration files and reloads Hyprland. It preserves unrelated commands
and settings. If another app owns `tapzap`, use `tapzap-omarchy` instead.

After the installer reaches `main` and an Omarchy release is published, the
download installer will be available with:

```sh
curl -fsSL https://raw.githubusercontent.com/rS7Y/tapzap-for-linux/main/install-omarchy.sh | bash
```

Open **TAP ZAP** from the application launcher or run `tapzap`. New preferences
start OFF. Existing preferences are retained. Enable **Start at login** in
Settings if wanted; it starts the app in the tray. Quit other display-filter
apps and older TAP ZAP prototypes before running this add-on.

## Everyday commands

| Command | Action |
|---|---|
| `tapzap` | Open the panel, or start the app |
| `tapzap on` / `tapzap off` | Enable or disable a running filter |
| `tapzap toggle` | Toggle a running filter |
| `tapzap hide` | Hide the panel, keeping the tray and filter running |
| `tapzap status` | Read the running app's state |
| `tapzap quit` | Restore the display and exit |
| `tapzap version` | Show installed version and source commit |
| `tapzap update` | Download and install the latest published Omarchy release |
| `tapzap update /path/to/package.tar.gz` | Update from a verified local archive |
| `tapzap uninstall` | Remove commands, launcher, login entry and Lua integration |

Updates restart the app in the tray and preserve preferences. Previous versions
and integration backups remain under the user's data directory
`tapzap-omarchy-trial/`; this historical directory name keeps existing installs
compatible. Preferences live in the user's config directory
`TapZapOmarchy/settings.ini`. Uninstall preserves both for reinstall or rollback.

## Build a package

Install the [build dependencies](BUILDING.md#omarchy--hyprland), then run from
a clean source commit:

```sh
make build VARIANT=omarchy-hyprland
```

The archive and checksum appear in `src/omarchy-hyprland/dist/`. Packaging runs
native build, color, GUI, tray, protocol and isolated installer checks. It
includes the GPL, third-party notices and source revision. Creating an archive
does not publish a release.

# TAP ZAP for Omarchy

TAP ZAP is a native Omarchy bar plugin. Open its lightning icon to use two
vertical faders, DAY / EVENING / NIGHT presets and the ZAP switch. The rounded
panel keeps TAP ZAP's dark surfaces, red warmth channel, white brightness
channel, Anybody headings and metallic fader caps. Clicking outside closes
the panel while the filter keeps running. Quit restores the display.

One managed engine serves all monitors. It exits when the plugin is unloaded
or the shell exits; a lifetime guardian lets it flush preferences and restore
brightness before exiting. A session marker resumes a previously started
engine after plugin reload; explicit Quit removes the marker. Restart still
starts the filter OFF and watches future schedule events. The original
Cairo/tray interface remains available with `TAPZAP_CAIRO=1 tapzap` for testing
older shells. Quit the native engine before switching interfaces.

This independent, experimental add-on targets Linux x86_64 with Hyprland CTM
v2 and the native Omarchy shell plugin API. The retained Cairo interface also
requires XWayland. Native checks on Omarchy 4.0.2 / Hyprland 0.56.2 and one Dell
are recorded in [Validation](VALIDATION.md). Other GPUs, HDR, hotplug and
suspend/resume remain unverified. Read [Known issues](KNOWN-ISSUES.md).

## Install

A public binary release is still pending. For a verified local package, keep
the archive and its `.sha256` file together, then run from the repository root:

```sh
python3 src/omarchy-hyprland/install.py --archive /path/to/tapzap-omarchy-0.3.0-linux-x86_64.tar.gz
```

No administrator access is needed. The installer checks the download and its
contents, adds the application launcher and `tapzap` command, and installs
`app.tapzap` in the user's Omarchy plugin directory. It enables the bar widget
through Omarchy shell IPC. An upgrade removes the earlier TAP ZAP outside-click
Lua block and backs up changed integration files. It reloads only compositor
configuration, without changing the user's rounding, theme or input settings. It preserves unrelated commands
and settings. If another app owns `tapzap`, use `tapzap-omarchy` instead.

After the installer reaches `main` and an Omarchy release is published, the
download installer will be available with:

```sh
curl -fsSL https://raw.githubusercontent.com/rS7Y/tapzap-for-linux/main/install-omarchy.sh | bash
```

Open **TAP ZAP** from the application launcher or run `tapzap`. New preferences
start OFF, including after a restart. Existing preferences are retained. Enable
**Launch at login** in Settings to start the engine hidden and watch future
schedule events. Opening the panel also starts the engine. Quit other display-filter
apps and older TAP ZAP prototypes before running this add-on.

## Everyday commands

| Command | Action |
|---|---|
| `tapzap` | Open the panel, or start the app |
| `tapzap on` / `tapzap off` | Enable or disable a running filter |
| `tapzap toggle` | Toggle a running filter |
| `tapzap hide` | Hide the panel, keeping the engine and filter running |
| `tapzap status` | Read the running app's state |
| `tapzap quit` | Restore the display and exit |
| `tapzap version` | Show installed version and source commit |
| `tapzap update` | Download and install the latest published Omarchy release |
| `tapzap update /path/to/package.tar.gz` | Update from a verified local archive |
| `tapzap uninstall` | Remove plugin, commands, launcher and login entry |

Updates start the engine hidden and preserve preferences. Previous versions
and integration backups remain under the user's data directory
`tapzap-omarchy-trial/`; this historical directory name keeps existing installs
compatible. Preferences live in the user's config directory
`TapZapOmarchy/settings.ini` and `TapZapOmarchy/schedule.ini`. Uninstall preserves both for reinstall or rollback.

## Daily schedules

Settings → Auto Schedule lets you select DAY, EVENING and NIGHT start times in
24-hour local time. Defaults are 07:00, 18:00 and 22:00. Selected times must be
unique, and at least one preset must be selected before enabling scheduling.
Save Times commits the draft; unchanged saves preserve a manual hold.

Each start fades warmth to its preset over one minute. Manual warmth or a
preset holds until the next selected start. Brightness remains under manual
control; when a schedule starts from OFF, brightness fades from neutral output
to the saved level. Explicit OFF waits for a strictly future start. Restart
starts OFF and watches future events; it does not replay the last manual ON
state. A running engine catches missed starts after wake. Time-zone or backward
clock changes rebase future starts. Repeated daylight-saving times use the first
occurrence; nonexistent times use the next valid local minute.

Native-engine commands also include:

```sh
tapzap preset day
tapzap set warmth 0.6
tapzap set brightness 0.8
tapzap schedule save 420 1 1080 1 1320 1
tapzap schedule enabled 1
```

Warmth is intensity from 0 (6500K) to 1 (red-only 0K). The default brightness
floor is 10%; Extreme Dim unlocks 0%. PWM-Safe is offered only when the system
has a supported backlight. Its status reports the hardware readback, not a
physical flicker measurement. Copy Feedback prepares diagnostics locally for
you to paste into a support message.

## Build a package

Install the [build dependencies](BUILDING.md#omarchy--hyprland), then run from
a clean source commit:

```sh
make build VARIANT=omarchy-hyprland
```

The archive and checksum appear in `src/omarchy-hyprland/dist/`. Packaging runs
native build, color, fallback GUI/tray, headless-engine, protocol and isolated
installer checks. It
includes the GPL, third-party notices and source revision. Creating an archive
does not publish a release.

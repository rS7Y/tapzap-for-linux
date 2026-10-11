# TAP ZAP for Omarchy

TAP ZAP is a native Omarchy bar plugin. Open its lightning icon to use two
vertical faders, DAY / EVENING / NIGHT presets and the ZAP switch. The square-edged
panel keeps TAP ZAP's dark surfaces, red warmth channel, white brightness
channel, Anybody headings and metallic fader caps. Clicking outside closes
the panel while the filter keeps running. The gear opens Settings;
there is no separate close button. Quit restores the display.

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

The current release is [Omarchy 0.3.0](https://github.com/rS7Y/tapzap-for-linux/releases/tag/omarchy-v0.3.0).
Use Linux x86_64 with glibc 2.38+, Omarchy's native Quickshell shell and Hyprland
CTM v2. Tested versions are Omarchy 4.0.2, Hyprland 0.56.2 and Quickshell 0.3.1.
The engine uses Cairo, FreeType, X11/Xext/XRandR, D-Bus, the C++ runtime and
Wayland client libraries already present on the tested Omarchy installation.
Other versions and hardware require their own checks. No Qt or Omarchy libraries
are bundled. The optional Cairo fallback also needs XWayland and GTK/AppIndicator.

From a terminal on your **unlocked Omarchy desktop**, download the archive and
checksum together, verify them, then install:

```sh
mkdir -p ~/Downloads/tapzap-omarchy-0.3.0
cd ~/Downloads/tapzap-omarchy-0.3.0
curl --fail --location --remote-name https://github.com/rS7Y/tapzap-for-linux/releases/download/omarchy-v0.3.0/tapzap-omarchy-0.3.0-linux-x86_64.tar.gz
curl --fail --location --remote-name https://github.com/rS7Y/tapzap-for-linux/releases/download/omarchy-v0.3.0/tapzap-omarchy-0.3.0-linux-x86_64.tar.gz.sha256
sha256sum --check tapzap-omarchy-0.3.0-linux-x86_64.tar.gz.sha256
tar -xzf tapzap-omarchy-0.3.0-linux-x86_64.tar.gz
python3 tapzap-omarchy-0.3.0-linux-x86_64/install.py --archive tapzap-omarchy-0.3.0-linux-x86_64.tar.gz
```

Continue only when the checksum reports **OK**. The checksum detects incomplete
or changed downloads; it is not a signature or independent security review.
You can also download both files from the release page without using curl.
No sudo or pkexec is required. The installer checks payload hashes and adds
**TAP ZAP** to the launcher, the `tapzap` command and the `app.tapzap` bar widget.
If another application owns `tapzap`, use `tapzap-omarchy` for these commands.

Install, update and remove from an unlocked desktop. The installer refuses a
locked or undetermined session because Omarchy 4 plugin rescans can reload its
lock service. Do not lock the screen until installation finishes. Native updates
restart the Omarchy shell to clear cached QML components; desktop applications
stay open. Input settings, theme and unrelated plugins are preserved. Integration
files that need replacement are backed up.

### Marketplace / Git-managed plugin

The repository also has a root plugin manifest, so Omarchy's plugin manager can
clone it directly:

```sh
omarchy plugin add https://github.com/rS7Y/tapzap-for-linux.git --enable
```

**Manual setup is required:** this command installs the QML interface only.
Run the verified archive installation above to install the matching native
engine and launcher. The installer recognizes this repository's Git checkout
and preserves its files, branch and local edits. Do not use Add over an existing
package-managed `app.tapzap` link; the package installation already includes and
enables the same widget.

For an existing package installation, use `tapzap update`. For a Git-managed
plugin, update both parts from an unlocked desktop:

```sh
omarchy plugin update app.tapzap
# Review and accept the plugin diff, then update the engine:
tapzap update
```

Quit restores the display and stops the engine. `tapzap uninstall` removes the
engine, commands, launcher and login entry, retaining preferences and backups.
A Git-managed plugin is disabled and retained so local edits are not deleted;
remove that checkout separately when ready:

```sh
tapzap uninstall
omarchy plugin remove app.tapzap
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
Click a clock to open its hour/minute picker. Use the +/− buttons, type the
digits, or use the Up/Down keys in a field. Done returns to the schedule;
Save Times commits the draft. Unchanged saves preserve a manual hold.

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
physical flicker measurement. The PWM row follows the Mac UI with a status
dot and separate OFF, pending and ON states. **Report a bug / Get support** in
Settings opens the repository's GitHub issue-report chooser. It does not
submit a report or upload diagnostics automatically.

## Setup checks and recovery

After installation, run `tapzap version` to see the installed version and source
commit. `tapzap status` reports the running engine, filter, hardware-readback
state and next schedule start. If the engine has been quit, open TAP ZAP first;
"not running" then is expected.

- **Command not found:** open a new terminal, or run `~/.local/bin/tapzap-omarchy`.
- **Panel says connecting:** confirm `~/.local/bin/tapzap-omarchy` exists. A plugin
  added with Omarchy's Git manager still needs the verified engine installation.
- **Icon missing:** run `omarchy plugin enable app.tapzap`. Check that you are
  using Omarchy's native shell rather than a different bar implementation.
- **Install refuses the session:** unlock the desktop and retry from its terminal.
  Do not kill the lock screen or compositor to bypass the guard.
- **Schedule does not start immediately:** it waits for a strictly future selected
  time. Enable Launch at login if the engine should watch schedules after login.
- **Unexpected display output:** use `tapzap off`, then `tapzap quit`. Quit other
  color-filter applications and read [Troubleshooting](TROUBLESHOOTING.md).

For bugs, use Settings → Report a bug / Get support. Reports are public; review
any logs or screenshots before attaching them.

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

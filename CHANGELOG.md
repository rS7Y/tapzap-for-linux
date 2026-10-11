# Changelog

Changes to the Linux source repository are recorded here. Website ZIPs that
predate the public source snapshot are not treated as releases of this source.

## Unreleased

- Add the native Omarchy bar plugin with rounded console C, original TAP ZAP
  typography and colors, two vertical faders, presets and keyboard controls.
- Make native popup commands wait for plugin discovery after installation.
- Separate its managed filter engine from the window. Add daily local-time
  schedules, one-minute fades, manual holds, OFF/startup rules and DST tests.
- Migrate the earlier outside-click hook to native popup dismissal; retain
  the optional Cairo interface and preserve user input/theme configuration.

- Add the Omarchy tray add-on installer, short `tapzap` command, verified
  archive/download updates, version reporting and uninstall. Preserve
  preferences, older versions and integration backups; keep login startup optional.

- Keep isolated Omarchy tests off the live desktop's X11 display, and check
  licenses after builds without treating generated protocol files as project code.
- Record native Omarchy checks of public source `90d2720` and their limits.
- Format project C/C++ and Python consistently without changing application
  behavior; preserve backend boundaries and upstream assets.
- Add a code map, CLI reference, and `make` commands for development checks.
- Add full-history secret scanning and pinned formatter checks to CI.
- Publish the Linux-only source tree, build notes, support boundaries, and
  third-party notices under GPL-3.0-or-later.
- Keep all five backend variants experimental while the recovery and reliability
  findings in [known issues](docs/KNOWN-ISSUES.md) remain open.
- No binary release is included or claimed to match this source revision.

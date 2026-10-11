# Changelog

Changes to the Linux source repository are recorded here. Website ZIPs that
predate the public source snapshot are not treated as releases of this source.

## Unreleased

- Match the bar and ZAP button mark to the Mac artwork and icon sizes; use a
  single 45% OFF opacity on the bar. Shrink the gear by 10% and center it on
  the visible TAP ZAP title lettering.

- Reduce Auto Schedule button text and match Mac/Windows preset behavior:
  manual warmth suppresses preset highlights until an explicit preset click,
  including after release, status polling and keyboard warmth edits.

- Use a simple monochrome Settings gear, a readable system font for Settings
  and Auto Schedule headings, a plain Quit label, and centered ZAP icon/text.

- Enlarge the Settings gear and remove the redundant close button. Match the
  PWM-Safe row to the Mac UI and keep the bar and ZAP bolts in the same proportions.
- Open schedule clocks in an hour/minute picker with mouse and keyboard controls;
  enlarge schedule copy and report invalid drafts before saving.
- Route Settings support to GitHub's issue-report chooser.
- Keep the classic compact TAP ZAP layout in the native Omarchy bar plugin, with
  square outer edges, a vector ZAP bolt, vertical faders and original typography.
  Schedule and Settings controls remain available under the gear button.
- Refresh the unlocked Omarchy shell after native plugin updates so cached QML
  cannot leave the old layout on screen. Desktop applications remain running.
- Refuse native plugin installation, updates and removal while the compositor is
  locked or its lock state cannot be determined; Omarchy rescans reload its locker.
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

# Changelog

Changes to the Linux source repository are recorded here. Website ZIPs that
predate the public source snapshot are not treated as releases of this source.

## Unreleased

## COSMIC prototype 0.1.0 — 2026-10-17

Publish a separate experimental Pop!_OS 24.04 amd64 DEB with patched compositor,
complete matching source and checksums. Requires its own prototype login session;
stock COSMIC is retained. VM checks cover installation, early login/autostart,
controls, OFF/Quit, two virtual displays and both s2idle and S3/deep sleep.
An October 18 RTC follow-up fixed the separate QEMU lab wake path with unchanged
app binaries. Physical GPU/panel, optical output, PWM, HDR and physical hotplug
remain unvalidated. [Download and setup](docs/COSMIC.md).

## Mint 0.1.0 — 2026-10-17

Publish a source-matched experimental x86_64 candidate for Linux Mint 22.3
Cinnamon X11, built from `5e4d0b8` inside Mint. Includes native XRandR/Cairo
binaries, tray helper, exact source, licenses, checksums and test evidence.
Fix the Cinnamon hide/reopen crash caused by requesting focus before mapping.
Color self-test, 35 source tests, virtual gamma controls/restoration and ten
popover reopening cycles passed. Native Mint physical-display, PWM, suspend
and hotplug testing remain pending. See [Mint setup](docs/MINT.md).

## Omarchy 0.3.0 — 2026-10-12

Source-matched experimental x86_64 native Omarchy release. Includes verified
archive setup, updates/removal, repository-root plugin-manager integration and
preservation of Git-managed plugin edits. Live settings and real-time schedule
checks are recorded in [Validation](docs/VALIDATION.md).


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

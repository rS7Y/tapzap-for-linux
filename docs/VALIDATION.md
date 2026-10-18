# Linux validation and limits

Updated 2026-10-18. Tap Zap for Linux is free and experimental. The checks
below describe the evidence available for these separate backend snapshots;
they do not certify a general Linux release or guarantee that a filter can be
restored after every compositor, driver, display, or process failure.

## COSMIC prototype release correspondence

[COSMIC prototype 0.1.0](COSMIC.md) is a separate prerelease for Pop!_OS 24.04
amd64. Tag `cosmic-v0.1.0` points to `3ecfdb5844ebb97e25771bea281f6711e0d5e8a3`.
Its DEB SHA-256 is
`bba3956e5e8b085876fd8672eb3ca7cf24f8a1d156ab2121c54a22e56ca33a88`.
The release includes complete matching source, locked dependencies, build
information and original validation reports. Experiment code remains on its
branch/tag; this main-branch guide does not add a stock COSMIC backend.

Native install/remove/reinstall, early-login/autostart, controls, OFF/Quit,
two virtual displays and s2idle → S3/deep → s2idle passed in the Pop!_OS VM.
The original release report used host-assisted wake; the later
[RTC follow-up](https://github.com/rS7Y/tapzap-for-linux/blob/32502dafe05807f81c789324ef64af66b0ceff47/experiments/cosmic/vm-rtc/validation/SUMMARY.md)
fixed the separate QEMU 10.2.2/Q35 RTC/SCI lab defect and passed five awake RTC
alarms plus both sleep modes with unchanged app binaries and no host wake injection.
That follow-up is separate from the original release assets. It does not change
the prerelease tag or imply hardware suspend validation.

These checks used virtual KMS and software rendering. Physical GPU/panel,
optical output, PWM, HDR and physical hotplug remain unvalidated.

## Evidence available

| Area | Environment / evidence | What it does not establish |
|---|---|---|
| Mint 22.3 Cinnamon X11 candidate | On October 15, public source `5e4d0b8` built inside a Linux Mint 22.3 live QEMU/KVM guest on the Dell, with Cinnamon 6.6.4, GCC 13.3, glibc 2.39 and one virtual display. Color self-test, all 35 source tests, GUI/tray registration, DAY 4000K, full red, 50% brightness, exact virtual gamma restoration after OFF/Quit and Quit/reopen passed. A reproduced Cinnamon focus/mapping crash was fixed; ten hide/reopen cycles including keyboard focus passed. | This is VM software/gamma readback, not native Mint physical display filtering, hardware brightness/PWM, HDR, suspend, hotplug, arbitrary crash restoration or broad GPU coverage. |
| Fedora 44 session snapshots | On October 14, the operator confirmed whole-desktop warmth, full red, dimming and normal OFF appearance on GNOME Wayland, Plasma Wayland, Sway, Xfce X11 and Plasma X11 on the same Dell. | Those installed September candidates differ from the current public source and the Mint package. These confirmations are one-device visual checks, not measured optical/PWM evidence or source-matched release validation. |
| X11, GNOME, Plasma, Sway snapshots | Saved September 5–6, 2026 work used Ubuntu 24.04 x86_64 on a Dell XPS 15 9570 with Intel UHD 630/i915, alongside Xvfb and isolated protocol/helper tests. | It is not new validation of the public source commit, every backend/session, other GPUs, HDR, hotplug, suspend/resume, or physical optical output. GNOME and Sway visual confirmation was still pending in the saved notes. |
| Omarchy / Hyprland public source | On October 11, source `90d2720` built on a Dell XPS 15 9570 running Omarchy 4.0.2 / Hyprland 0.56.2, with one eDP-1 display at 3840×2160 and scale 3. Native checks passed for OFF startup, tray registration, DAY/EVENING/NIGHT presets, brightness slider, Settings navigation, hide/show, toggle, OFF, and quit. After quit, a new CTM owner acquired and released the real compositor successfully. The operator confirmed visible warm/red changes and their reversal. Original app settings and filter state were restored. | This establishes native GUI, visible tint changes, and compositor protocol behavior on that machine, not optical color measurements, PWM reduction, other GPUs/panels/Hyprland versions, HDR, hotplug, suspend/resume, or arbitrary crash recovery. Helper-failure and timeout recovery were tested only against the isolated protocol server. |
| Source color-core contract | The repository test compiles and runs independently authored C++17 endpoint, range, monotonicity, and gamma-ramp checks against all five source variants. | A color-curve test is not a live backend, compositor, display, recovery, PWM, or visual test. |
| September 7 beta archives | The five archived download hashes matched their old publication manifest. | None of the existing app archives is bound to this repository revision or a reproducible public build. They are not source-matched releases. |

## Native Omarchy console (October 11)

The native bar plugin was built from source `d510aa1` on the same Dell and
Omarchy/Hyprland versions above. Mouse checks passed for DAY/NIGHT, brightness
fader dragging, ZAP OFF, Settings, login startup and Extreme Dim toggles. The
main panel, Settings and daily schedule editor retained the same rounded frame.
A real plugin rescan resumed the schedule engine OFF and watching future starts.

Automated checks include the existing isolated Cairo/tray suite, a headless
engine with a private CTM server, a hidden native QML load using installed
Omarchy modules and a Wayland display, and deterministic local-time schedule/DST
cases. Set `WAYLAND_DISPLAY` for the QML check; its IPC and D-Bus are isolated,
and the closed panel never maps or takes focus. The engine
checks cover invalid commands, preset coefficients at protocol precision,
manual holds, unchanged saves, OFF, restart, write-failure reporting and cleanup.
These are software checks; they do not add physical PWM, optical, suspend,
hotplug, HDR or broad hardware coverage. Follow-up lifecycle tests additionally
model a killed shell child and a delayed retiring engine before replacement.

### Classic UI refinements

Source `ac5538f` was installed and checked on the same Dell. The square outer
frame, larger Settings gear without a close button, shared bar/button bolt,
Mac-style PWM status row and readable schedule text were visually inspected.
All three time pickers saved changes: DAY via typed digits (08:15), EVENING via
mouse increment (18:01), and NIGHT via keyboard arrow (22:01). Schedule ON/OFF,
PWM ON/OFF with hardware-brightness readback at 100%, outside-click dismissal
and programmatic reopening passed. Support launched Chromium with the GitHub
issue chooser URL; GitHub required sign-in. No report was submitted.

Original filter, brightness, PWM and schedule preferences were restored after
testing. These checks establish the tested controls and software readback on
this Dell; they do not establish physical PWM reduction or multi-monitor behavior.

### Omarchy 0.3.0 release checks (October 12)

The classic interface at `441eaac` was checked on the same Dell, with the
following live mouse and keyboard checks:

- PWM-Safe OFF/ON, including supported hardware brightness readback at 100%.
- Launch at login OFF/ON and corresponding creation/removal of the autostart entry.
- Extreme Dim ON/OFF; zero-brightness and floor enforcement use isolated engine tests.
- DAY, EVENING and NIGHT clock pickers: hour/minute buttons, typed digits,
  keyboard arrows, draft isolation and Save Times persistence.
- Schedule ON/OFF, individual phase selection, duplicate-time rejection and
  empty-selection rejection without changing persisted times.
- A real future local-time DAY start from OFF, its intermediate transition and
  completed 60-second fade to 4000K, with saved brightness retained.
- Manual warmth holding until the next start, unchanged-save hold preservation,
  Quit, reopening OFF, and retained future schedule/times.

All original filter, brightness, login, PWM and schedule preferences were restored.
The release adds the repository-root plugin entry point and recognition of a
Git-managed plugin checkout. Isolated installer tests cover install, update and
removal while preserving checkout files/local edits, plus refusal of lookalike
remote URLs. Native QML checks load both packaged and repository entry points.
These controls and software readbacks do not prove physical PWM reduction or
expand the hardware coverage beyond this machine.

## Open reliability findings

The September 6 audit found the following issues in the X11/GNOME/Plasma/Sway
source snapshots. They remain open unless a later entry in this repository
specifically records a reproduced fix and test for that finding:

- **P1 — F01–F06:** X11 calibration recovery after a crash; preserving newer
  external gamma settings when quitting OFF; restoring original backlight
  state after abrupt exit; clipboard-triggered UI crash; nonfinite settings
  bypassing dimming limits; and a silent IPC client blocking the UI/display
  maintenance. These findings affect shared XRandR, PWM-Safe, Cairo, or config
  code as described in the relevant variant source.
- **P1 — F07–F09:** a wlroots timeout can leave the filter active while the UI
  says OFF (Sway and GNOME); fallback launches can downgrade the shared
  autostart router; and unplugged filtered displays can block Plasma OFF and
  later launches.
- **P2 — F10–F12:** Plasma's SDR gate can be skipped for an already captured
  output; GNOME recovery identity is limited to one D-Bus lifetime; and
  malformed GNOME/Plasma recovery journals can prevent helper startup.
- **P2 — F13–F15:** IPC ownership/path edge cases; settings writes that can
  fail silently or truncate the live file; and backend failures not always
  represented truthfully or actionably in the UI.

See [KNOWN-ISSUES.md](KNOWN-ISSUES.md) for the practical status by backend and
[TROUBLESHOOTING.md](TROUBLESHOOTING.md) for safe first steps. These findings
are why all five source variants remain experimental and why successful CI
must not be presented as physical-display coverage.

## Safety boundary

Tap Zap changes software display output. That is separate from changing a
panel's physical backlight and does not prove that PWM flicker is reduced.
PWM-Safe behavior depends on the hardware and backend and is experimental.
Keep the documented OFF/quit and recovery steps accessible before testing;
stop if the display does not return to the user's expected state.

## Release correspondence

`mint-v0.1.0` publishes the unchanged Mint archive built and tested from
`5e4d0b8303a8a03f1924d411ff70670b8dde7445`. Its `BUILD-INFO.json` records
the revision, compiler, libc and per-binary hashes; `SOURCE.tar.gz` inside
the archive contains that exact public source. `SHA256SUMS` verifies its
payload and the adjacent `SHA256SUMS.txt` verifies the downloaded archive.
The [Mint guide](MINT.md) records the VM-only scope, dependencies, shared
settings path, hide-to-tray behavior and open X11 recovery findings.

`omarchy-v0.3.0` identifies the first source-matched native Omarchy package.
The release archive records the full tagged commit and source-manifest digest
in `BUILD-INFO.json`, includes the GPL and component notices, and hashes each
payload file in `SHA256SUMS.json`. The adjacent SHA-256 file verifies the
archive after download. Build with `bash src/omarchy-hyprland/package.sh` from
a clean checkout; the script runs native color, engine, protocol, QML and
isolated installer checks before completing the package.

This is an experimental x86_64 Omarchy release. Other backend website ZIPs
remain unmapped to this source. A checksum proves file identity; it does not
prove reproducible builds, physical output, or broad compatibility.

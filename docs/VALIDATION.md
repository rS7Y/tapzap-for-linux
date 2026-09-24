# Linux validation and limits

Updated 2026-09-24. Tap Zap for Linux is free and experimental. The checks
below describe the evidence available for these separate backend snapshots;
they do not certify a general Linux release or guarantee that a filter can be
restored after every compositor, driver, display, or process failure.

## Evidence available

| Area | Environment / evidence | What it does not establish |
|---|---|---|
| X11, GNOME, Plasma, Sway snapshots | Saved September 5–6, 2026 work used Ubuntu 24.04 x86_64 on a Dell XPS 15 9570 with Intel UHD 630/i915, alongside Xvfb and isolated protocol/helper tests. | It is not new validation of the public source commit, every backend/session, other GPUs, HDR, hotplug, suspend/resume, or physical optical output. GNOME and Sway visual confirmation was still pending in the saved notes. |
| Omarchy / Hyprland snapshot | Saved Omarchy 4.0.2 / Hyprland 0.56.2 Intel-display work opened the integrated app OFF and exercised its tray/show path. | Enabling the filter on that physical display was still pending. This is not evidence for other GPUs, panels, Hyprland versions, HDR, hotplug, or suspend/resume. |
| Source color-core contract | The repository test compiles and runs independently authored C++17 endpoint, range, monotonicity, and gamma-ramp checks against all five source variants. | A color-curve test is not a live backend, compositor, display, recovery, PWM, or visual test. |
| September 7 beta archives | The five archived download hashes matched their old publication manifest. | None of the existing app archives is bound to this repository revision or a reproducible public build. They are not source-matched releases. |

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

There are currently no binaries in this repository that claim to match a
source tag. A future release must identify its exact source commit, build
command, runtime requirements, component notices, tested environment, open
issues, and SHA-256 checksum. Recomputing a ZIP checksum alone proves only the
identity of that ZIP, not which source produced it.

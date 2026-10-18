# Known issues

This list summarizes unresolved findings in the imported experimental source
snapshots. Do not deliberately reproduce the failure modes. They remain open
until a later change documents a reproduced fix and a relevant test.

## COSMIC prototype status

The [COSMIC 0.1.0 prototype](COSMIC.md) requires the separate patched login
session on Pop!_OS 24.04 amd64; stock COSMIC lacks the filter channel. Native
install, early login/autostart, OFF/Quit and both sleep modes passed in a VM
with two virtual displays. Physical GPU/panel, optical output, PWM, HDR and
physical hotplug remain unvalidated. The older backend findings below remain
open for those snapshots and are not a new audit of the COSMIC prototype.

## Mint candidate status

The [Mint 0.1.0 candidate](MINT.md) fixes a separate X11 popover reopening
crash under Cinnamon: asking for focus before the window manager completed
mapping could terminate the app with `BadMatch`. The fix uses window-manager
activation; ten hide/reopen cycles with focus passed inside the Mint VM.
This does not resolve the high-priority recovery/configuration findings below.
Native Mint display testing is still pending.

## High priority

- X11 shared code: gamma calibration/recovery after a crash; OFF ownership when
  another tool has changed gamma; abrupt-exit backlight restoration; clipboard
  input crashing the UI; non-finite settings bypassing dimming limits; and a
  silent IPC client blocking UI/display maintenance (audit F01–F06).
- GNOME and Sway wlroots paths: a gamma-control timeout may leave a filter
  active while the interface reports OFF (F07).
- Shared launch routing: a fallback launch can downgrade the autostart router
  (F08).
- Plasma: OFF or next launch can block when a filtered output is unplugged
  (F09).

## Other open findings

- Plasma may skip its SDR requirement check for an output already captured
  (F10).
- GNOME recovery state is tied to one D-Bus owner lifetime (F11).
- Malformed GNOME/Plasma recovery journals can prevent helper startup (F12).
- IPC ownership/path edge cases, settings writes that can fail silently or
  truncate the live file, and backend errors that are not always reflected
  clearly in the UI remain open (F13–F15).

The source audit and evidence boundaries are detailed in
[validation](VALIDATION.md). If you encounter one, stop testing and follow the
[safe troubleshooting steps](TROUBLESHOOTING.md). Share a short, redacted
report using the [GitHub forms](https://github.com/rS7Y/tapzap-for-linux/issues/new/choose).

# Known issues

This list summarizes unresolved findings in the imported experimental source
snapshots. Do not deliberately reproduce the failure modes. They remain open
until a later change documents a reproduced fix and a relevant test.

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
report using the [GitHub forms](https://github.com/rS7Y/tapzap-linux/issues/new/choose).

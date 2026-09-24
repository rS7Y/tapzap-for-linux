# Linux support and evidence matrix

Every row below is experimental. These are separate session/backend builds,
not a distro checklist. The evidence is from earlier saved test snapshots and
does not certify the public source revision or your system.

| Build | Intended session/backend | Saved evidence | Important limits | Status |
|---|---|---|---|---|
| X11 | X11/XRandR | Earlier Ubuntu 24.04 x86_64 work; see [validation](VALIDATION.md). | Crash/recovery, external gamma ownership, backlight, UI, and IPC findings remain open. | Experimental; not broadly supported |
| GNOME Wayland | GNOME/Mutter helper; optional wlroots protocol path | Earlier Ubuntu 24.04 x86_64 helper/protocol work. | Visual confirmation and general hardware coverage are not established; helper needs a GNOME session and GObject introspection. | Experimental; not broadly supported |
| KDE Plasma Wayland | KWin color-profile helper | Earlier Ubuntu 24.04 x86_64 helper work. | SDR/profile constraints, display removal, recovery journal, and multi-display behavior need more validation. | Experimental; not broadly supported |
| Sway / wlroots | `wlr-gamma-control` protocol | Earlier Ubuntu 24.04 x86_64 protocol work. | Compositor protocol support varies; timeout/OFF behavior and visual confirmation remain open. | Experimental; not broadly supported |
| Omarchy / Hyprland | Hyprland CTM v2 integration | Earlier Omarchy 4.0.2 / Hyprland 0.56.2 Intel session exercised launch/tray path. | Physical filter enable, other GPUs, HDR, hotplug, and suspend/resume are unverified. | Experimental; not broadly supported |

## Common scope

- The saved desktop test machine was an Intel UHD 630/i915 Dell XPS 15 9570
  running Ubuntu 24.04 x86_64. It is one environment, not a compatibility
  guarantee.
- No ARM64, HDR, broad GPU/driver, suspend/resume, or monitor hotplug matrix is
  established.
- Software gamma/CTM changes are not the same as hardware backlight changes.
  Hardware dimming availability depends on the platform and backend.
- Existing website ZIP hashes matched their prior publication manifest, but
  those ZIPs are not mapped to a public source revision. See
  [release correspondence](VALIDATION.md#release-correspondence).

Read [known issues](KNOWN-ISSUES.md), [safe first steps](TROUBLESHOOTING.md),
and [building](BUILDING.md) before trying source builds.

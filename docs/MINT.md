# TAP ZAP for Linux Mint 22.3 Cinnamon (X11)

Experimental x86_64 candidate, published 2026-10-17 as [Mint 0.1.0](https://github.com/rS7Y/tapzap-for-linux/releases/tag/mint-v0.1.0).
Built from public source `5e4d0b8303a8a03f1924d411ff70670b8dde7445` inside Linux Mint 22.3, Cinnamon 6.6.4, with glibc 2.39.

**Testing so far used a Mint virtual machine. Testing on a native Mint installation is still pending.** This build does not establish physical screen filtering, hardware brightness/PWM, HDR, suspend/resume, hotplug or broad GPU compatibility. The X11 recovery and configuration findings in [known issues](https://github.com/rS7Y/tapzap-for-linux/blob/main/docs/KNOWN-ISSUES.md) remain open.

## Requirements

- Linux Mint **22.3 Cinnamon**, **native X11**, x86_64. Cinnamon on Wayland and XWayland are not covered.
- System libraries for X11, XRandR, Xext, XFixes, Cairo, FreeType, D-Bus, GTK3 and Ayatana AppIndicator. No runtime system libraries are bundled.
- An unlocked local desktop, one SDR display initially, and access to its writable XRandR gamma table.
- `tar`, `sha256sum` and a terminal. ARM64, Mint 21.x, musl and older glibc installations are not targets for this binary.

Check your session before running:

```sh
printf 'Desktop: %s\nSession: %s\n' "$XDG_CURRENT_DESKTOP" "$XDG_SESSION_TYPE"
```

Use this candidate only if you are on Cinnamon and the session says `x11`. If libraries are missing, `ldd bin/tapzap` and `ldd bin/tapzap-tray` show unresolved dependencies; install them from Mint's package manager before launching. Leave other screen-color utilities off during the first check.

## Download and verify

Download the TAR.GZ, `SHA256SUMS.txt` and `BUILD-INFO.json` together from the [Mint release](https://github.com/rS7Y/tapzap-for-linux/releases/tag/mint-v0.1.0). The same archive is offered by the [Linux download chooser](https://tapzap.app/linux/).

From the folder containing the download:

```sh
sha256sum -c SHA256SUMS.txt &&
tar -xzf tapzap-linux-mint-22.3-cinnamon-x86_64-20261015.tar.gz &&
cd tapzap-linux-mint-22.3-cinnamon-x86_64 &&
sha256sum -c SHA256SUMS &&
./bin/tapzap selftest &&
./bin/tapzap gui
```

Archive SHA-256:

```text
4911d120ace213f9e7fd23463bdfda414cdf4be5114ee885aba1d81c569f1167
```

Keep `bin/` and `assets/` together. Run the app as your own user, without sudo. The checksum proves file identity; it is not a signature or proof of physical output. The archive includes its exact source, licenses and test evidence. The older `install.sh` / `run.sh` website instructions are for the September packages, not this Mint candidate.

## First run, tray and Quit

A fresh profile starts **OFF**. This build reads and writes `~/.config/TapZap2/settings.ini` (or `$XDG_CONFIG_HOME/TapZap2/settings.ini` when set), which may be shared with other source-built X11 variants. Existing settings can resume the saved filter state. Do not run two TAP ZAP instances together. Leave PWM-Safe, Extreme Dim and launch at login off for the first evaluation.

Click the tray icon and choose Show to reopen the popover. Clicking away or closing its window hides it to the tray; it does not quit. Use the gear → **Quit**, or Quit in the tray menu, to exit and request normal gamma restoration. If the tray is hidden, run `./bin/tapzap show` from the extracted folder.

Try warmth, red, dimming and OFF one at a time, checking a white window outside TAP ZAP. Stop if colors do not return as expected. Keep the [recovery instructions](https://github.com/rS7Y/tapzap-for-linux/blob/main/docs/TROUBLESHOOTING.md) accessible. Avoid crash and hotplug experiments; the known recovery findings still apply.

## Validation recorded on October 15

- Native XRandR/Cairo/tray build inside the Mint VM; color self-test and all 35 source tests passed.
- GUI and Cinnamon tray registration passed.
- DAY 4000K, full red and 50% brightness passed against the virtual display's gamma readback.
- OFF and normal Quit restored that virtual gamma table exactly; Quit/reopen passed.
- A Cinnamon hide/reopen crash was fixed. Ten close-to-tray/reopen cycles, including keyboard focus, passed.

These are software and virtual-display checks, not a physical Mint laptop test. No Auto Schedule feature was validated or added to this X11 candidate; the native Omarchy plugin is a separate build.

## Update, remove and report

Quit before replacing this folder. Download and verify a later candidate separately; there is no automatic updater for this package. Turn off launch at login in Settings if enabled before moving or removing the folder. Removing the folder leaves settings intact; do not delete recovery data while a display may be filtered.

[Report a problem](https://github.com/rS7Y/tapzap-for-linux/issues/new/choose) with the Mint version, Cinnamon version, native X11/Wayland session, GPU, display and reproduction steps. Issues are public: remove personal details and private logs before sharing.

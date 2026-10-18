# Installing and first run

Omarchy has a [source-matched 0.3.0 release and native plugin installer](OMARCHY.md),
with verified downloads, optional login startup, daily schedules, updates and
uninstall. Follow that guide from an unlocked Omarchy desktop. Marketplace/Git
installs require the native engine as a manual dependency.
Mint 22.3 Cinnamon X11 has a [source-matched experimental candidate](MINT.md),
built and tested inside a Mint VM. Native Mint display testing is still pending.
Pop!_OS 24.04 COSMIC Wayland has a [source-matched amd64 DEB prototype](COSMIC.md),
VM-tested and requiring its separate **TAP ZAP COSMIC (prototype)** login session.
Other variants currently require source builds; older website beta ZIPs are not
source-matched to this repository.

The source variants remain experimental while the recovery findings in
[known issues](KNOWN-ISSUES.md) are open. If you choose to evaluate a local
build, read [the safety notes](TROUBLESHOOTING.md) first and build only for the
session listed in the [support matrix](SUPPORT-MATRIX.md).

## Safe source checks

Follow [Building](BUILDING.md), then run only:

```sh
./build/tapzap selftest
```

Run that command from the selected backend's source directory after building.
It checks the color-curve math; it does not enable a filter or validate a
compositor, display, GPU, tray, or recovery behavior. There are no generic
`sudo make install` commands. The Omarchy installer is intended for a completed
and checksum-verified archive, not for the source directory itself; use its
`--archive` option as described in the [Omarchy guide](OMARCHY.md).

For the other experimental builds, leave launch-at-login disabled during
evaluation. Keep source-built app installations per-user. The COSMIC DEB installs its separate
prototype session system-wide using the package manager; follow its own guide. If the display does not return to its prior state,
stop and use [safe recovery steps](TROUBLESHOOTING.md).

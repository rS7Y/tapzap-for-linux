# Installing and first run

There is no binary release in this repository that is verified against the
public source revision. The older website beta ZIPs are not source-matched.
No general-purpose Linux install or autostart package is offered here yet.

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
and checksum-verified trial archive, not for the source directory itself.

Do not enable launch-at-login or install a build system-wide while evaluating
this experimental source. If the display does not return to its prior state,
stop and use [safe recovery steps](TROUBLESHOOTING.md).

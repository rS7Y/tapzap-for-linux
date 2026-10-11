# Tap Zap Omarchy / Hyprland add-on

This directory contains an experimental x86_64 integration for Omarchy using
Hyprland's CTM v2 protocol. It is not a generic Wayland build and is not an
official Omarchy package.

For installation, native bar controls, schedules, updates, login startup and removal,
read the [Omarchy add-on guide](https://github.com/rS7Y/tapzap-for-linux/blob/main/docs/OMARCHY.md).
[Omarchy 0.3.0](https://github.com/rS7Y/tapzap-for-linux/releases/tag/omarchy-v0.3.0)
is the source-matched experimental x86_64 release. Follow the guide to download,
verify and install its archive and checksum. Git-managed plugins need the same
native-engine setup; `omarchy plugin add` alone installs only the interface.

Read the repository's [known issues](https://github.com/rS7Y/tapzap-for-linux/blob/main/docs/KNOWN-ISSUES.md),
[validation limits](https://github.com/rS7Y/tapzap-for-linux/blob/main/docs/VALIDATION.md), and
[safe recovery steps](https://github.com/rS7Y/tapzap-for-linux/blob/main/docs/TROUBLESHOOTING.md)
before building. Limited native GUI and CTM filter checks on one Dell are
recorded in the validation notes; broad hardware compatibility and optical
output measurements are not verified.

## Build and test

From this directory on Linux x86_64, install the documented system build
dependencies, then run:

```sh
bash build.sh
bash test.sh
```

`plugin/` contains the native Omarchy QML interface. `src/linux/service.cpp`
owns the filter and settings; `schedule.hpp` contains the local-time scheduler.
The original `ui_cairo.cpp` is the optional fallback.

`test.sh` exercises the fallback GUI/tray and native engine against isolated
X11/Wayland servers. It is not a physical
display or recovery test. Do not install an archive unless it has been rebuilt
from a reviewed source commit, its checksums verified, and its open issues
reviewed. The installer is scoped to the current user's data directories and
does not use `sudo`.

The package includes its GPL text and third-party notices. The full source
tree is available at [rS7Y/tapzap-for-linux](https://github.com/rS7Y/tapzap-for-linux).

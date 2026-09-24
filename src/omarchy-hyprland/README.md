# Tap Zap Omarchy / Hyprland trial package

This directory contains an experimental x86_64 integration for Omarchy using
Hyprland's CTM v2 protocol. It is not a generic Wayland build and is not an
official Omarchy package.

Read the repository's [known issues](https://github.com/rS7Y/tapzap-for-linux/blob/main/docs/KNOWN-ISSUES.md),
[validation limits](https://github.com/rS7Y/tapzap-for-linux/blob/main/docs/VALIDATION.md), and
[safe recovery steps](https://github.com/rS7Y/tapzap-for-linux/blob/main/docs/TROUBLESHOOTING.md)
before building. The physical filter-enable path and broad hardware
compatibility are not verified.

## Build and test

From this directory on Linux x86_64, install the documented system build
dependencies, then run:

```sh
bash build.sh
bash test.sh
```

`test.sh` exercises protocol and setup integration checks. It is not a physical
display or recovery test. Do not install an archive unless it has been rebuilt
from a reviewed source commit, its checksums verified, and its open issues
reviewed. The installer is scoped to the current user's data directories and
does not use `sudo`.

The package includes its GPL text and third-party notices. The full source
tree is available at [rS7Y/tapzap-for-linux](https://github.com/rS7Y/tapzap-for-linux).

# TAP ZAP for Linux

Adjust screen warmth and brightness from your desktop. On Omarchy, set daily
schedules for DAY, EVENING and NIGHT.

Free and open source. The native Omarchy plugin is available now; other Linux
builds are experimental.

| Main panel | Settings | Auto Schedule |
|---|---|---|
| <img src="docs/screenshots/omarchy/main.png" width="256" alt="Warmth and brightness faders"> | <img src="docs/screenshots/omarchy/settings.png" width="256" alt="Settings"> | <img src="docs/screenshots/omarchy/auto-schedule.png" width="256" alt="Daily schedules"> |

[More screenshots](docs/SCREENSHOTS.md)

## Install on Omarchy

Download [TAP ZAP 0.3.0](https://github.com/rS7Y/tapzap-for-linux/releases/tag/omarchy-v0.3.0)
and follow the [setup guide](docs/OMARCHY.md). It covers installation, updates
and removal. No sudo is needed.

The marketplace installs the interface; the native engine needs the manual
setup in that guide.

**Beta:** tested on Omarchy 4.0.2, Hyprland 0.56.2 and one Intel SDR laptop.
Other GPUs, ARM64, HDR, multiple displays, hotplug and suspend remain unverified.
See [tested limits](docs/VALIDATION.md) and [known issues](docs/KNOWN-ISSUES.md).

## Linux Mint 22.3 Cinnamon (X11)

Download the [Mint 0.1.0 experimental candidate](https://github.com/rS7Y/tapzap-for-linux/releases/tag/mint-v0.1.0)
and follow the [Mint setup guide](docs/MINT.md). The x86_64 archive includes
its exact source, native binaries, tray helper, licenses and checksums.

Built and tested inside a Mint 22.3 / Cinnamon 6.6.4 virtual machine: warmth,
red, dimming, virtual gamma restoration and tray reopening passed. Testing on
a native Mint installation is still pending. This is an X11 candidate;
Cinnamon Wayland, physical PWM and hardware recovery are not validated.

## Other Linux desktops

Choose the build for your desktop session:

| Session | Source |
|---|---|
| X11 | [src/x11](src/x11/) |
| GNOME Wayland | [src/gnome-wayland](src/gnome-wayland/) |
| KDE Plasma Wayland | [src/plasma-wayland](src/plasma-wayland/) |
| Sway / wlroots | [src/sway](src/sway/) |
| Omarchy / Hyprland | [src/omarchy-hyprland](src/omarchy-hyprland/) |

Read the [support matrix](docs/SUPPORT-MATRIX.md) and [build guide](docs/BUILDING.md)
first. Older website beta ZIPs are not source-matched to this repository.

## Development

```sh
git clone https://github.com/rS7Y/tapzap-for-linux.git
cd tapzap-for-linux
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements-dev.txt
make help
make test
```

Tests need Python 3.10+ and a C++17 compiler. They do not access your display.
See [Contributing](CONTRIBUTING.md) for dependencies and checks.

- [Code map](docs/ARCHITECTURE.md)
- [Commands](docs/COMMANDS.md)
- [Changes](CHANGELOG.md)
- [Troubleshooting](docs/TROUBLESHOOTING.md)

## Report a problem

[Report a bug or request a feature](https://github.com/rS7Y/tapzap-for-linux/issues/new/choose).
Include your version, desktop, GPU and steps to reproduce it.
Remove license keys, personal details and private logs before posting.
For security reports, see [SECURITY.md](SECURITY.md).

## License

[GPL-3.0-or-later](LICENSE). Fonts and protocols have their own
[licenses](THIRD-PARTY-NOTICES/README.md).

[macOS and Windows](https://tapzap.app/download) are separate paid editions.
Linux is free.

## Patched COSMIC prototype

A separate Pop!_OS 24.04 amd64 experiment adds a composed-desktop RGB filter
through a patched COSMIC compositor. It requires its own selectable session;
stock COSMIC does not expose this prototype channel. See the
[COSMIC prototype build and setup guide](experiments/cosmic/README.md).
Physical hardware, optical output, PWM and HDR remain unvalidated.

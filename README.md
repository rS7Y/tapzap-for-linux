# Tap Zap for Linux

Tap Zap for Linux is a free, experimental desktop display-control tool. This
repository contains the Linux source under **GPL-3.0-or-later**; macOS and
Windows editions remain separate paid products. Linux has five distinct
session-specific implementations, so choose by display server/backend—not by
distro name alone.

> **Beta:** restoration, backend-failure, and multi-display issues remain open.
> Read the [support matrix](docs/SUPPORT-MATRIX.md),
> [known issues](docs/KNOWN-ISSUES.md), and
> [validation limits](docs/VALIDATION.md) before running a build.

## Choose a build

| Your session | Source directory | Status |
|---|---|---|
| X11 | [`src/x11/`](src/x11/) | Experimental; x86_64 evidence only |
| GNOME on Wayland | [`src/gnome-wayland/`](src/gnome-wayland/) | Experimental; GNOME/Mutter-specific helper |
| KDE Plasma on Wayland | [`src/plasma-wayland/`](src/plasma-wayland/) | Experimental; KWin-specific helper |
| Sway / wlroots on Wayland | [`src/sway/`](src/sway/) | Experimental; requires the wlroots gamma protocol |
| Omarchy / Hyprland | [`src/omarchy-hyprland/`](src/omarchy-hyprland/) | Experimental; Hyprland CTM v2, x86_64 |

See [the full matrix](docs/SUPPORT-MATRIX.md) for prerequisites and evidence.
ARM64, HDR, other GPU/driver combinations, and many hotplug or suspend/resume
paths are unverified.

## Start here

```sh
git clone https://github.com/rS7Y/tapzap-for-linux.git
cd tapzap-for-linux
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements-dev.txt
make help
make test
```

These tests need Python 3.10+ and a C++17 compiler; they do not connect to a
display. On Debian/Ubuntu, install `git make g++ python3-venv` first.
For a native Linux build, install the [backend dependencies](docs/BUILDING.md),
then run `make build VARIANT=x11` (or the matching variant from the table).
`make check` also needs [Gitleaks](CONTRIBUTING.md#secret-checks).

## Find your way around

| Location | Contents |
|---|---|
| [`src/`](src/) | Five desktop-session implementations and their required assets |
| [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) | Code map, backend boundaries, and where to start reading |
| [`docs/COMMANDS.md`](docs/COMMANDS.md) | Development commands and application CLI reference |
| [`tests/`](tests/) | Color-core, licensing, documentation, and source-integrity tests |
| [`scripts/`](scripts/) | Native CI builds and source-manifest maintenance |
| [`THIRD-PARTY-NOTICES/`](THIRD-PARTY-NOTICES/README.md) | Font and protocol licenses |

## Build and install

There is not yet a binary release built and verified from this public source
revision. The Linux ZIPs currently linked from the website are earlier beta
artifacts and are **not source-matched** to this repository. Do not infer source
correspondence from their checksums.

To build locally, use the commands in [Building](docs/BUILDING.md), then follow
the [installation and safe first-run steps](docs/INSTALLING.md). Do not install
system-wide or use `sudo` for the preserved per-user installer. Check
[Troubleshooting](docs/TROUBLESHOOTING.md) before experimenting with display
settings.

## Report a problem

Search [existing reports](https://github.com/rS7Y/tapzap-for-linux/issues), then
[choose a GitHub report form](https://github.com/rS7Y/tapzap-for-linux/issues/new/choose):

- [Bug report](https://github.com/rS7Y/tapzap-for-linux/issues/new?template=bug_report.yml)
- [Compatibility report](https://github.com/rS7Y/tapzap-for-linux/issues/new?template=compatibility.yml)
- [Feature request](https://github.com/rS7Y/tapzap-for-linux/issues/new?template=feature_request.yml)

Include the app/build version, distro and version, desktop and session,
GPU/driver, display/HDR context, exact steps, and expected versus actual
behavior. Issues are public: remove license keys, usernames, hostnames,
home-directory paths, and unreviewed logs or screenshots. See
[`SECURITY.md`](SECURITY.md) for private security reports.

## Project links

- [Linux beta guide and current downloads](https://tapzap.app/linux-beta/)
- [Linux source repository on GitHub](https://github.com/rS7Y/tapzap-for-linux)
- [Changes](CHANGELOG.md)
- [Contributing](CONTRIBUTING.md)
- [Third-party notices](THIRD-PARTY-NOTICES/README.md)
- [GitHub releases](https://github.com/rS7Y/tapzap-for-linux/releases) — no source-matched binary release yet

After the Linux setup and support information: need Tap Zap for
[macOS or Windows](https://tapzap.app/download)? Those editions are paid;
Linux remains free. The paid link is optional and does not gate Linux source or
downloads.

## License

Tap Zap-authored Linux code, documentation, and project artwork are licensed
under [GPL-3.0-or-later](LICENSE). Third-party fonts and protocol definitions
retain their separate licenses as recorded in
[`THIRD-PARTY-NOTICES/`](THIRD-PARTY-NOTICES/README.md). No macOS or Windows
implementation is part of this repository.

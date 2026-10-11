# Contributing to Tap Zap for Linux

Thanks for helping improve the Linux version. This repository is limited to
the free, experimental Linux builds. macOS and Windows code and release
processes live elsewhere and are not part of this project.

## Before opening a change

- Read the [support matrix](docs/SUPPORT-MATRIX.md), [known issues](docs/KNOWN-ISSUES.md),
  and [validation limits](docs/VALIDATION.md).
- Keep X11, GNOME/Mutter, KDE/KWin, Sway/wlroots, and Omarchy/Hyprland changes
  in their own backend unless there is evidence that shared behavior is truly
  identical.
- Do not ask people to reproduce crashes, unplug displays, or test recovery
  paths listed as unresolved. Use non-display tests where possible.
- Separate what you observed from what you infer. A successful build or unit
  test is not evidence of compositor, GPU, physical-display, or restoration
  compatibility.

## Tests and pull requests

Set up the development environment using the [README](README.md#start-here),
then run `make check`. See the [code map](docs/ARCHITECTURE.md) for entry
points and the [command reference](docs/COMMANDS.md) for individual checks.
The formatter versions are pinned in `requirements-dev.txt`; use the activated
virtual environment so local formatting agrees with CI.

Run `make format` before submitting source changes. It expands dense C/C++
statements, applies consistent Python formatting, and refreshes the source
manifest. Do not format third-party protocol XML or license files. Preserve
the five backend boundaries; a shared-code refactor needs its own validation.

Run the repository tests and the build/self-test for every backend your change
touches. Include the exact commands and results in the pull request. For
display-specific work, include only evidence you have reviewed and redacted;
never include license keys, personal paths, hostnames, or unredacted logs.

Keep the source manifest current when changing imported source or assets. Add
new dependencies and assets to the third-party notices before distributing
them. Preserve upstream license files verbatim. Do not add binaries or describe
an archive as source-matched without recording its exact source revision,
build inputs, test evidence, notices, and checksum.

After a change to the imported source tree, run
`python3 scripts/update-source-manifest.py` and
`python3 scripts/verify-source-release.py` before the unit tests.

## Secret checks

Install [Gitleaks 8.30.1](https://github.com/gitleaks/gitleaks/releases/tag/v8.30.1)
and make it available on `PATH`. On macOS, `brew install gitleaks` provides the
tool. CI downloads the pinned Linux release and verifies its SHA-256.

```sh
make scan-secrets
```

This scans working files and all local Git refs with the default rules and
redacted output. Fetch complete history before auditing a clone made with
`--depth`; CI uses a full checkout. Secret scans detect patterns, so also
review every staged diff. Never commit credentials, `.env` files, private keys,
deployment configuration, customer data, local logs, or unreviewed audit
reports. Ignore rules are a convenience; do not bypass them with `git add -f`.
Do not suppress a scanner finding without reviewing its cause.

If a secret was committed, deleting the current file is insufficient: stop
sharing it, revoke or rotate the affected credential, and report privately
using [SECURITY.md](SECURITY.md). History repair requires a coordinated review.

## License and reports

Tap Zap-authored Linux code, documentation, and artwork are available under
GPL-3.0-or-later. Third-party components keep their own licenses; see
[third-party notices](THIRD-PARTY-NOTICES/README.md). No contributor license
agreement is required. By submitting a contribution, you confirm you have the
right to submit it under these terms.

For ordinary bugs, compatibility, and feature requests, use the
[GitHub issue forms](https://github.com/rS7Y/tapzap-for-linux/issues/new/choose).
Do not publish vulnerabilities, personal information, or secrets there. Use
the private route in [SECURITY.md](SECURITY.md) for sensitive reports.

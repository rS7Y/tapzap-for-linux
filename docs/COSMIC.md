# TAP ZAP for Pop!_OS 24.04 / COSMIC

Free experimental **COSMIC prototype 0.1.0**, for **amd64 (x86_64)**.
VM-tested on Pop!_OS 24.04 with COSMIC on Wayland. This package includes a
separate patched compositor and requires the **TAP ZAP COSMIC (prototype)**
login session. Stock COSMIC does not expose the prototype filter channel.
The ordinary COSMIC session and distribution compositor are retained.

## Download and install

1. Download the [DEB package](https://github.com/rS7Y/tapzap-for-linux/releases/download/cosmic-v0.1.0/tapzap-cosmic-prototype_0.1.0_amd64.deb) and [SHA256SUMS](https://github.com/rS7Y/tapzap-for-linux/releases/download/cosmic-v0.1.0/SHA256SUMS) into the same folder.
2. Open Terminal in that folder. Verify the checksum before installing:

```sh
sha256sum --check --ignore-missing SHA256SUMS &&
sudo apt install ./tapzap-cosmic-prototype_0.1.0_amd64.deb
```

The DEB checksum is `bba3956e5e8b085876fd8672eb3ca7cf24f8a1d156ab2121c54a22e56ca33a88`.
Stop if verification fails. Do not use the older ZIP installer for this package.

3. Log out. Select **TAP ZAP COSMIC (prototype)** on the login screen and sign in.
4. Open **TAP ZAP COSMIC Prototype** from Applications. Try moderate warmth
   and brightness, then check that OFF and Quit restore normal colors.

Preferences and optional launch-at-login are separate from other TAP ZAP builds.
Start with launch-at-login disabled until the first manual check passes.

## Tested scope

The Pop!_OS VM passed native installation, login/autostart, controls, OFF/Quit
restoration, two virtual displays, virtual output changes, and both s2idle and
S3/deep suspend/resume. These are virtual KMS/software-rendering checks.
Physical GPUs/panels, optical output, PWM, HDR and physical monitor hotplug
remain unvalidated; VM results do not establish hardware sleep compatibility.

The [release](https://github.com/rS7Y/tapzap-for-linux/releases/tag/cosmic-v0.1.0) includes the exact [complete source archive](https://github.com/rS7Y/tapzap-for-linux/releases/download/cosmic-v0.1.0/tapzap-cosmic-prototype-0.1.0-source.tar.gz),
licenses, build information and original validation report. A later
[RTC VM follow-up](https://github.com/rS7Y/tapzap-for-linux/blob/32502dafe05807f81c789324ef64af66b0ceff47/experiments/cosmic/vm-rtc/validation/SUMMARY.md)
records the separate lab QEMU fix and RTC wake tests with unchanged app binaries.
That QEMU patch is a lab fix, not a required change to a customer's kernel.
For source builds, use the [tagged prototype build guide](https://github.com/rS7Y/tapzap-for-linux/blob/cosmic-v0.1.0/experiments/cosmic/README.md).

## Return to ordinary COSMIC / remove

Log out and choose the ordinary COSMIC session. Remove the prototype only from
another session:

```sh
sudo apt remove tapzap-cosmic-prototype
```

Removal retains `~/.config/tapzap-cosmic-prototype/`. If launch-at-login was
enabled, remove its separate `~/.config/autostart/tapzap-cosmic-prototype.desktop`
entry after removal. Other TAP ZAP autostart entries are separate.

[Report a problem](https://github.com/rS7Y/tapzap-for-linux/issues/new/choose).
Include package version, Pop!_OS/COSMIC version, selected login session, GPU and
steps to reproduce. Review logs and remove private details before posting.

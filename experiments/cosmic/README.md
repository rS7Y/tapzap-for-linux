# TAP ZAP COSMIC prototype 0.1.0

Experimental Pop!_OS 24.04 amd64 build with a separately patched COSMIC
compositor. This does not enable the filter in stock COSMIC. Tested in a
QEMU/KVM VM with virtual KMS and software rendering. Physical GPU/panel, PWM,
optical output, HDR and physical monitor hotplug remain unvalidated.

## Install and use

Verify the download against the release's SHA256SUMS first. Then run:

```sh
sudo apt install ./tapzap-cosmic-prototype_0.1.0_amd64.deb
```

Log out. In the login screen, select **TAP ZAP COSMIC (prototype)** and sign in.
Open **TAP ZAP COSMIC Prototype** from Applications. Start with a moderate
brightness and check OFF restores the desktop. Its preferences and autostart
entry are separate from other TAP ZAP builds. The installed stock compositor
and ordinary COSMIC session are retained.

To return to stock COSMIC, log out and choose the ordinary COSMIC session.
Remove the prototype only from another session:

```sh
sudo apt remove tapzap-cosmic-prototype
```

Removal retains the prototype's settings under
`~/.config/tapzap-cosmic-prototype/`. If launch-at-login was enabled, remove its
separate `~/.config/autostart/tapzap-cosmic-prototype.desktop` entry after removal.
It does not own `~/.config/autostart/tapzap.desktop`.

## Sources and VM sleep validation

The adjacent source archive contains the exact patched COSMIC source, its
locked Cargo dependencies, the pinned public TAP ZAP source and prototype
build scripts. See its BUILD.md and validation reports for commands, hashes,
versions, sleep-mode wake method and limits. No claim of general hardware
suspend compatibility follows from the VM tests.

For a QEMU VM, place virtio-GPU behind a PCIe root port and enable
`x-pcie-pm-no-soft-reset=true` on that device. Otherwise QEMU can destroy its
resources during sleep. Use UEFI/OVMF with SMM enabled; the tested BIOS VM lost
its ACPI PM registers after S3. The VM has no CPU idle driver, so s2idle tests
use kernel trace and success-counter evidence rather than assuming its
monotonic clock stops. S2idle uses an explicit host ACPI power-button wake;
the virtual RTC alarm alone did not wake the guest. VM capture uses bounded HTTP on host loopback; a 9p
capture mount can hang across sleep. These are VM configuration requirements,
not changes to a customer's kernel or stock COSMIC package.

The control worker accounts for a new kernel-confirmed system sleep when its
monotonic lease clock advances during freezing. It credits that poll gap,
including any scheduling delay in the same gap, and retains the lease age
already consumed. Later awake stalls still expire. If the kernel sleep counter
cannot be read, the lease fails closed as before.

## Build on Pop!_OS 24.04

Install Rust 1.93.0, `build-essential`, `pkg-config`, `patch`, `python3`,
`libegl1-mesa-dev`, `libfontconfig-dev`, `libgbm-dev`, `libinput-dev`,
`libpixman-1-dev`, `libseat-dev`, `libsystemd-dev`, `libudev-dev`,
`libwayland-dev`, `libxcb1-dev`, `libxkbcommon-dev`, `libxkbcommon-x11-0`,
`libdisplay-info-dev`, `libdbus-1-dev`, `libdrm-dev`, `libx11-dev`,
`libxrandr-dev`, `libxext-dev`, `libxfixes-dev`, `libcairo2-dev`, and
`libfreetype-dev`. An existing COSMIC desktop and `/usr/bin/start-cosmic` are
required for the separate session package.

From the repository root, choose fresh output directories:

```sh
python3 experiments/cosmic/prepare.py work-cosmic
(cd work-cosmic/cosmic && CARGO_BUILD_JOBS=2 CARGO_PROFILE_DEV_DEBUG=0 \
 CARGO_INCREMENTAL=0 cargo +1.93.0 build --locked)
bash experiments/cosmic/build-tapzap.sh work-cosmic/tapzap/src/sway out-cosmic
python3 experiments/cosmic/packaging_tests.py --include out-cosmic/source
python3 experiments/cosmic/release/package.py \
 --cosmic work-cosmic/cosmic/target/debug/cosmic-comp \
 --tapzap out-cosmic/tapzap-cosmic --tapzap-source work-cosmic/tapzap \
 --output dist-cosmic
```

`sources.json` pins the upstream archives and their SHA-256 hashes. The build
stages a private source copy to isolate the autostart entry and control socket;
it does not modify that pinned input tree. The complete source release also
includes vendored Cargo dependencies for offline rebuilding. VM QA scripts are
restricted to the named disposable lab environment; edit their lab paths only
when deliberately recreating that environment.

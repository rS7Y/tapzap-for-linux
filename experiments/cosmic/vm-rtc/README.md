# QEMU RTC alarm investigation

This is an isolated QEMU 10.2.2/Q35 lab patch. It changes neither TAP ZAP nor
the customer's kernel. Keep the host emulator and other VMs intact; build and
launch a separately named emulator for the disposable Pop!_OS VM.

The original guest selects `rtc_cmos.use_acpi_alarm=Y`. A five-second RTC alarm
also fails while awake: `timeout 12 rtcwake -m on -s 5` returns 124, and neither
IRQ8 nor the ACPI RTC fixed-event counter advances. S2idle needs a fallback key.
QEMU's RTC feeds its legacy IRQ through HPET, while its system wake request is
ignored when QEMU is running. Linux s2idle leaves QEMU in that running state.
The ACPI wake notifier therefore never sets RTC_STS or raises SCI.
See the upstream [RTC timer](https://gitlab.com/qemu-project/qemu/-/blob/v10.2.2/hw/rtc/mc146818rtc.c),
[wake request](https://gitlab.com/qemu-project/qemu/-/blob/v10.2.2/system/runstate.c)
and [ACPI event handling](https://gitlab.com/qemu-project/qemu/-/blob/v10.2.2/hw/acpi/core.c).

The patch adds a separate alarm pulse after the RTC alarm status is published.
ICH9 latches RTC_STS and updates SCI, whose existing enable mask still controls
delivery. The existing S3 wake request remains intact. The added qtest requires
RTC_STS and IRQ9 when an alarm expires, and requires IRQ9 to drop after clearing
the alarm and fixed-event status. It also covers masking and enabling an already
pending alarm. The 16-test Q35 suite and the original 15-test i440fx suite were
both run against the fixed emulator. This patch is a VM experiment, not an upstream
QEMU compatibility or migration guarantee.

## Reproduce the build

Use a Linux x86_64 build environment with a C/C++ toolchain, Python venv/pip,
Meson, Ninja, pkg-config, GLib, pixman and libcurl development packages.
Fetch and verify these upstream archives before extraction:

| Source | SHA-256 |
|---|---|
| [QEMU v10.2.2](https://gitlab.com/qemu-project/qemu/-/archive/v10.2.2/qemu-v10.2.2.tar.gz) | `a047e169ddf82ee8b023b754e465ad9b2e632fa1aa52ff786eb9df72e5de7d40` |
| [libslirp](https://gitlab.freedesktop.org/slirp/libslirp/-/archive/26be815b86e8d49add8c9a8b320239b9594ff03d/libslirp-26be815b86e8d49add8c9a8b320239b9594ff03d.tar.gz) | `abd190c8213f259ab9fc1a411b8dd87c54b7aeba329d3f87f4f4aa82d921bbc9` |
| [keycodemapdb](https://gitlab.com/qemu-project/keycodemapdb/-/archive/f5772a62ec52591ff6870b7e8ef32482371f22c6/keycodemapdb-f5772a62ec52591ff6870b7e8ef32482371f22c6.tar.gz) | `d014b53382dbb17b8196ad12f50de7f20d0ef1b9f7d54b0be51a6cbb14209195` |

Extract QEMU into a fresh `qemu-source` directory, libslirp into its
`subprojects/slirp` directory and keycodemapdb into `subprojects/keycodemapdb`.
Each archive has one leading directory to strip. From `qemu-source`, apply
`patch -p1 < /path/to/qemu-10.2.2-rtc-sci.patch`, then use a separate build folder:

```sh
mkdir ../qemu-build
cd ../qemu-build
../qemu-source/configure --target-list=x86_64-softmmu \
  --without-default-features --enable-kvm --disable-tcg \
  --enable-curl --enable-slirp --enable-pixman --disable-fdt \
  --disable-docs --disable-tools --disable-werror --disable-download
ninja -j2 qemu-system-x86_64 tests/qtest/rtc-test
QTEST_QEMU_BINARY=./qemu-system-x86_64 tests/qtest/rtc-test
```

Retain the original source, patch, configure output, compiler versions, test
logs, executable hash and any libslirp runtime dependency with the lab build.
The changed upstream source files were also compared byte for byte against
the official `qemu-10.2.2.tar.xz` archive, SHA-256
`784b296ff29c1417aa72323abcb2d2ea9ab9771724f577dcd785c3b04f21e176`.

The lifecycle harness accepts `--rtc-wake` with `--s2idle` or `--suspend`.
It arms an eight-second alarm, requires a fresh successful kernel sleep, keeps
the existing image/DRM checks and clears only the alarm it armed. The host must
not inject a wake event for a passing RTC run. A fallback wake makes that run a
diagnostic, even if the guest subsequently completes its graphics checks.

On the lab host, launch the dedicated VM with:

```sh
python3 /path/to/experiments/cosmic/vm_launch_resume.py \
  --qemu /path/to/qemu-system-x86_64 --firmware-directory /path/to/firmware
```

The firmware directory must include the
installed VGA and network option ROMs as well as the ordinary QEMU data files;
Fedora stores these in separate directories. The launcher records its actual
arguments. Keep the existing UEFI/SMM and PCIe virtio-GPU configuration.

Copy the current `vm_lifecycle_tests.py` to the guest's `/home/lab/prototype/`
and `vm_monitor_connection.py` to the host's lab `monitor-connection.py` path.
Retain the existing one-shot capture server, guest session environment and
installed prototype from the Pop VM setup. On the controlling computer, run the
coordinator from this directory with a fresh evidence directory and run name:

```sh
python3 run_lifecycle.py --ssh-config /path/to/lab-ssh-config \
  --output /path/to/fresh-evidence --run pop-rtc-s2idle-before-s3 --mode s2idle
```

The SSH configuration must retain strict host verification and define only the
intended `dell` host and `pop` guest aliases. Repeat with `--mode s3`, then s2idle
again, using fresh names. [Validation](validation/SUMMARY.md) records the results.

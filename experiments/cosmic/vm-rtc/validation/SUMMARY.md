# RTC wake follow-up: completed VM validation

The remaining reproduced RTC failure was in QEMU's virtual alarm delivery.
The isolated QEMU 10.2.2/Q35 patch now passes awake alarms and both sleep modes.
TAP ZAP and its patched COSMIC compositor are unchanged from cosmic-v0.1.0.
The host's installed QEMU and the separate Mint VM were retained.

Pop!_OS 24.04, kernel 7.0.11-76070011-generic, UEFI/OVMF with SMM, virtio KMS,
llvmpipe and two 1280x800 heads were used. All final runs share boot
`f5f6e824-d131-40ed-8fc2-6d0f394d9548`. The separate emulator SHA-256 is
`20e895bc3a40778b4ba6175874e5e4df190af8e40a0bb393468d1e7d6f575224`.
See [build-info.json](build-info.json), [source inputs](qemu-source-inputs.json)
and [running native binaries](runtime-info.json).

| Run | Wake source | Sleep evidence | Result |
|---|---|---|---|
| Awake, five consecutive alarms | Guest RTC | ACPI RTC counter increases once per alarm | 5/5 |
| s2idle before S3 | Guest RTC | 6.509 s kernel machine-suspend trace | 13/13, 256 DRM allocations |
| S3/deep | Guest RTC | 7.689 s BOOTTIME minus MONOTONIC | 13/13, 256 DRM allocations |
| s2idle after S3 | Guest RTC | 7.503 s kernel machine-suspend trace | 13/13, 256 DRM allocations |

Each sleep completed one fresh kernel suspend success, ending at three.
LPC PMBASE 0x601 and ACPI control 0x80 were preserved. The host coordinator
injected zero wake events; the adjacent host.json files record that path.
Every lifecycle retains the five baseline RGB samples per head, geometry gates,
virtual output disable/enable, resize/restore, monitor disconnect/reconnect,
retained ON after wake, post-wake DRM allocation/resize, OFF, ON and quit checks.
No filter error or gamma recovery occurred. The lab unlocks the normal COSMIC
lock screen before comparing desktop pixels, without sending another ON request.

[Real autostart](autostart.json) passed without the test launching the app:
saved ON state, heartbeat, OFF, ON and quit restoration all passed. The same
compositor remained alive 120.783 seconds after the last kernel wake, with no
Power key event. The prior deep mode and disabled keyboard-wake setting were
retained, and the RTC alarm was cleared. See [stability.json](stability.json).

The original failure is retained in [original-failure.json](original-failure.json):
the five-second awake alarm timed out at 12 seconds with exit 124 and no ACPI
RTC event, while s2idle required a host key. The first added QEMU regression
failed on missing RTC_STS. Review then caught an already-pending alarm enabled
after expiry; its added regression also failed before the rising-enable fix.
The fixed Q35 RTC suite passes 16 tests, original i440fx RTC suite 15, ICH9 one,
and TCO 11. Red and green logs are adjacent. Sixteen native controller tests,
eight native bridge tests, 217 private repository tests and 35 public source
tests pass. Public source/license, syntax, format and secret checks also pass.

The emulator was compiled in the Pop guest and run on Fedora. The host loader
emits a libcurl symbol-version warning; the HTTP ISO boot and completed runtime
tests exercise that transport. The first host launch lacked a VGA ROM search
path and exited before boot; a private directory referencing the host's installed
firmware resolved it. Neither attempt is counted as a completed validation run.
This is a lab emulator patch and reproducible source, not a portable QEMU binary
release. The app DEB SHA-256 remains
`bba3956e5e8b085876fd8672eb3ca7cf24f8a1d156ab2121c54a22e56ca33a88`.

Physical GPU/panel behavior, optical output, PWM, HDR and physical monitor
hotplug remain unvalidated. This does not add the prototype channel to stock
COSMIC or establish general hardware suspend compatibility.

# Pop!_OS COSMIC prototype: suspend and release validation

Final installed native binaries were tested in a Pop!_OS 24.04 amd64 live VM
with persistence, kernel 7.0.11-76070011-generic, QEMU/KVM 10.2.2, UEFI/OVMF
with SMM, virtio virtual KMS, llvmpipe and two 1280x800 virtual displays.
This is VM evidence. Physical GPU/panel, optical output, PWM, HDR and physical
hotplug remain unvalidated. Stock COSMIC does not expose this prototype channel.

| Completed run | Wake method | Sleep evidence | Result |
|---|---|---|---|
| s2idle before S3 | Host keyboard WakeUp key | 3.786 s kernel machine-suspend trace | 13/13, 256 DRM allocations |
| S3/deep | Host QMP system_wakeup | 5.604 s BOOTTIME minus MONOTONIC | 13/13, 256 DRM allocations |
| s2idle after S3 | Host keyboard WakeUp key | 6.252 s kernel machine-suspend trace | 13/13, 256 DRM allocations |

All runs used the same boot, `7287f3f4-db1e-4156-af9f-dfd24ec0c281`.
Each completed exactly one new kernel suspend success and preserved LPC PMBASE
0x601 and ACPI control 0x80. The compositor native SHA-256 was
`65817c6e2704606683cf8bd4952ba159a76e2efe079e84dbbf8be04be5196674`;
TAP ZAP was `efaad27a1d7ab9e25019e3dd83b470cc858d693e0703f29dbc7c6cf9d8053aa2`.
Running-process and installed-file hashes were checked against the build.

Each lifecycle checks ON on both heads, output disable/enable, mode changes,
virtual monitor disconnect/reconnect, retained ON after wake, 256 DRM buffer
allocations, post-resume mode changes, OFF, ON and quit. Five visible baseline
RGB samples per head must match within two code values, with original geometry
required outside explicit resize checks. Both modes use systemctl suspend with the chosen mem_sleep state, restoring
the prior state afterward. Suspend locks COSMIC normally; the dedicated
lab session is unlocked before comparing against desktop pixels. No TAP ZAP ON
command is sent during that scene restoration. Failed lock-scene comparison
was retained as a diagnostic, not counted as a passing run.

The final package also passed a fresh real autostart check: saved ON state was
sampled before sending ON, heartbeat stayed active, OFF restored the baseline,
and quit restored it again. The test did not launch the app. Earlier native
installation/removal/reinstall tests verified the same isolated packaging layout,
no-argument GUI launch, preference retention and preservation of legacy autostart.
Final installation and stock compositor/start-cosmic hashes were checked again.
The distribution package additionally retains the original cosmic-session
copyright notice; its native binaries are byte-identical to the tested ones.

Sixteen controller tests and eight native C++/Rust bridge tests pass. Red/green
regressions cover confirmed sleep, remaining lease age, late counter publication
with both expired and nonexpired leases, ordinary expiry, old/missing counters,
heartbeat and OFF. The public source suite passes 35 tests plus source/license,
syntax, formatting and secret checks; Linux source CI passed on the release branch.

## Fixes and diagnostic history

- Put virtio-GPU behind a PCIe root port with x-pcie-pm-no-soft-reset=true.
  On the root bus its PCIe PM capability was absent; S3 destroyed GPU resources.
- Replaced BIOS with UEFI/OVMF and SMM. The BIOS guest lost its LPC ACPI registers
  after S3; the final UEFI runs preserve them and pass a subsequent s2idle cycle.
- Removed 9p capture transport. A blocked reader prevented a later userspace
  freeze. One-shot bounded HTTP captures now preserve PNG evidence while
  consuming temporary QMP transport files; the host listener is loopback-only.
- The guest has no cpuidle driver: MONOTONIC advances during s2idle. The original
  two-second owner lease expired while both peers were frozen. A new kernel
  suspend-success count now credits the worker poll gap, including scheduling
  delay in that gap, while preserving the prior lease age. Later awake gaps
  still expire; unavailable counters retain fail-closed behavior.
- Test fixes wait for actual kernel entry/exit, deliver the coordination marker
  before freezing SSH, and use the available machine_suspend trace event.
  An early asynchronous systemctl test and a delayed-host-wake timeout are
  incomplete diagnostics, not passes.
- A startup attempt exhausted host storage with retained capture retries.
  Those frames and the stopped Fedora VM were preserved externally and verified
  before relocation. A clean boot with 3.3 GB free passed startup. One capture
  service interruption also invalidated an observer attempt; it was restored
  before the completed runs. No such attempt is counted as a pass.

The virtual RTC alarm alone did not wake s2idle. An earlier ACPI power-button
wake also requested a normal desktop shutdown after 60 seconds; the journal
confirmed the key press and delayed poweroff. The final sequence uses the
dedicated WakeUp key with the lab PS/2 keyboard wake capability enabled; they do not establish RTC wake or physical
hardware suspend compatibility. The QEMU upstream description of GPU resource
loss is at https://lists.gnu.org/archive/html/qemu-devel/2024-07/msg00548.html.

The adjacent complete source archive contains the exact patched compositor,
pinned TAP ZAP input, locked vendored Cargo sources, build scripts, hashes and
full selected validation reports. VM disks, credentials and customer data are
not release assets.

The guest remained alive 154 seconds after the final wake with the same
compositor, no Power key event, and the prior keyboard-wake/mem_sleep settings
restored. The distribution DEB changes only the original cosmic-session
copyright notice and VM README; all other payload bytes/modes and DEB control
metadata match the runtime-tested DEB. See package-payload-comparison.json.

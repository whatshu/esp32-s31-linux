# Upstream rebase, 2026-10-30

Fork-only validation notes. This file is excluded from the three upstream
stacks. Raw captures and build output remain under ignored `build/` paths.

## Baseline and history

The previous baseline was `2f3eb1268edd1bb2ca002eb4be339a635003b1e4`.
The new baseline is upstream main at
`086e52a6b9ae43a3bb07beb2557ea7595df8165b`, containing 41 additional
upstream commits. The dependency chain remains main, BSP, peripheral,
application, with 11, 14 and seven commits respectively.

The old tips are retained locally under
`archive/stack-before-upstream-rebase-20261030/`. The original fork main
is unchanged. Branch tracking remains on each matching origin branch.

## Resolved integration points

- Regenerated defconfigs from fragments to retain the new common
  firewall, SFTP, mDNS, BLE and pin-control packages. The original three
  boards retain the same Buildroot option values as new upstream main.
- Kept upstream's 4096-byte final kernel-manifest sector, including the
  partition-capacity checks and packaging offsets. The native USB helper
  validates that flash erase sectors do not overlap.
- Combined upstream monitor command execution and return statuses with
  the bounded recorder and PTY console. DTR and RTS are configured before
  opening the port; a no-reset session does not flush pending input.
- Kept upstream's GPIO memory interface alongside GPIO edge interrupts.
  GPIO IRQ initialization still precedes gpiochip registration.
- Kept current upstream package documentation and added the application
  entry without restoring removed packages.
- Built the Mosaico firewall as modules. Switched only Mosaico userspace
  to size optimization after the additional upstream packages made the
  application squashfs exceed its NOR partition. No package or peripheral
  was removed to make it fit. Recompiled target packages from fresh build
  directories so old optimization flags cannot affect the result.

## Host checks

- All 36 kernel patches apply to pristine Linux 7.3 with `patch -F0`.
- Nineteen host-script tests and six tests for each firmware packer pass.
- Upstream pinctrl: 47 tests, zero failures; BLE: 43 checks, zero failures;
  Thread relay: all four host tests pass.
- PTY regression checks cover repeated successful monitor commands,
  stopping after a failed command and command timeout. All use
  `--no-reset --no-log`; no duplicate log directory is created.
- Changed Python and shell scripts pass syntax checks. Defconfig
  generation and whitespace checks pass; patch context whitespace is
  preserved.

The original-board kernels and device trees compile using all current
peripheral patches and the updated ESP-Hosted sources:

| Board | Image bytes | Coverage |
| --- | ---: | --- |
| esp32s31 | 3,915,700 | Kernel and DTB compilation |
| coreboard1 | 4,576,612 | Kernel and DTB compilation |
| korvo1 | 4,440,540 | Kernel, modules and DTB compilation |

These original-board builds establish compilation coverage only. Runtime
results on the connected Mosaico are recorded separately below.

## Mosaico build and package checks

Each layer was built and packaged with its own source revision. Target
installation directories were cleared between layers. The final BSP
image contains neither the applications nor the peripheral startup
scripts. The peripheral image contains neither application binary.

| Layer | Revision | Image bytes | Squashfs bytes |
| --- | --- | ---: | ---: |
| BSP | `685efbaa0c828fcf683679839d5d55118b3771aa` | 3,860,428 | 7,901,184 |
| Peripheral | `3d3e266cc49564aaa5f5061994f35fd8237cb3a3` | 4,289,156 | 8,990,720 |
| Application | `5cad560d6779a853692a772f945aa8a2dcc906ad` | 4,289,156 | 9,179,136 |

The kernel limit is 4,714,496 bytes, leaving its final 4096-byte sector for
the manifest. The rootfs limit is 9,895,936 bytes. All three ZIP archives
pass integrity verification. Their seven flash regions have no overlapping
erase sectors; manifest magic, size, padding and CRC agree with each Image.
The application rootfs has 716,800 bytes of partition headroom.

Local bundles are under `build/release/`, named
`esp32s31-linux-rebase-{bsp,peripheral,application}-esp-mosaico-flash.zip`.
Per-layer images and sizes are retained under `build/rebase-artifacts/`.
Compiler, packaging and range-diff logs are in `build/rebase-*.log`.

## Local environment

Buildroot and ESP-Hosted use the new upstream pins. The committed ESP-IDF
pin remains `fff9895c82d744c7237be8847347bdd1b07c6643`; the independent
local SDK checkout retains its required patches without committing that
working change. Continuous UART recording remains connected with its
15,000,000-byte limit. No device configuration or personal credentials
are included in these notes or in the stacks.

## Hardware validation after rebase

The three bundles above are tested in dependency order on CoreBoard V1.2
with CameraBoard V1.4. Each flash writes all seven NOR regions through the
native ROM USB downloader, verifies their hashes and automatically returns
to Linux. CH343 remains dedicated to the bounded UART recorder. NAND is
not erased. No playback or vibration is triggered during this pass.

### BSP

The complete BSP bundle programmed successfully and booted Linux 7.3.0 #1,
2026-10-30 15:15:38 CST. NAND UBIFS and the writable overlay mounted; zram
and NAND swap were active at priorities 100 and 10. An ordinary NAND test
file and all three checked private configuration files survived unchanged.
The firewall started successfully, including its filter and Thread NAT64
tables. Wi-Fi, SSH and a 1 MiB SSH-to-NAND transfer passed SHA256 validation;
the transfer took 2.14 seconds including SSH startup and sync. MemTotal was
12,036 KiB, with 4,412 KiB available at the recorded probe.

Framebuffer, camera and USB gadget devices were absent, and neither test
application was installed or running. The kernel was untainted, with no
panic or OOM recorded. The already documented S10usb file in the preserved
writable layer still reports a missing dwc2 module on BSP; the BSP bundle
does not contain that script.

An initial SSH failure was caused by the host and board being on different
LANs. Once the host rejoined the board's LAN, SSH and the transfer passed
without changing firmware or private configuration.

### Peripheral

The complete peripheral bundle programmed and returned automatically to
Linux 7.3.0 #1, 2026-10-30 15:12:48 CST. Storage, both swaps, firewall,
Wi-Fi, SSH, the ordinary persistent file and all three private configuration
comparisons passed. A verified 1 MiB SSH-to-NAND transfer took 7.17 seconds,
including startup and sync. MemTotal was 11,596 KiB.

The framebuffer, touch, AI key, vibrator, audio card, camera, three IIO
devices and USB role/UDC interfaces registered successfully. BMI270 and
both BMM150 devices returned samples; the second BMM150 Y sample retains
the known overflow sentinel, so no calibrated compass heading is claimed.
Neither application was installed or running.

V4L2 MMAP capture returned 60 complete VGA UYVY frames with no sequence
gaps, at 15.37 fps between completed frames. One zero-length startup frame
was flagged as an error and discarded. USB CDC transferred 262,144 bytes
in each direction with byte-for-byte equality and SHA256 verification in
0.209 seconds. The kernel remained untainted, with no panic or OOM.

An early SSH probe preceded Wi-Fi association and timed out. Retrying
after association passed. This pass leaves native Type-C connected to
the computer: physical host/HID input, touch motion, audible playback and
motor effects are not repeated here. Earlier hardware records cover those
tests; device registration alone is not a repeat of them.

### Application

The complete application bundle programmed and returned automatically to
Linux. Storage, both swaps, firewall, Wi-Fi, SSH, the persistent file and
all private configuration comparisons passed. A verified 1 MiB SSH-to-NAND
transfer took 2.59 seconds including startup and sync. Both packaged
applications were installed and remained stopped at boot. Neither their
binaries nor kernel module files were shadowed by the NAND writable layer.
Its persisted S10usb copy matches the current shipped script.

The initial camera run returned 60 complete frames but also two invalid
frames at sequences 2 and 4, producing two sequence gaps. That run failed
the strict capture check and is retained as such. Four subsequent runs
each returned 60 complete frames without sequence gaps, at 15.37–15.71
steady fps. Three discarded an invalid sequence-zero startup frame; one
had no errors. These observations do not establish that the initial
intermittent capture fault is fixed. No driver change was made in this
validation pass.

USB gadget stop/start disconnected and re-enumerated correctly. The next
CDC test verified 262,144 bytes each way in 0.217 seconds. This covers
Linux gadget operation in addition to the ROM USB programming path used
for every bundle.

All 13 LVGL pages started from the packaged binary and exited successfully
under a bounded launcher: home, display, touch, IMU, compass, camera, keys,
audio, Wi-Fi, Bluetooth, USB, storage and system. Each restored the text
console and its original keyboard mode. This checks page startup, polling
and cleanup; it does not substitute for physical touch navigation or
explicit button actions. Audio playback remained closed, with neither
playback nor motor effects armed. Both applications were stopped after
the run, and the kernel was untainted without OOM or panic.

The first standalone preview, launched through SSH, exited normally after
128 displayed frames in 28.32 seconds, discarding 40 flagged capture
errors. A second SSH run started at 9.52 fps but declined to 2–3 fps. Its
VM snapshot showed 496 KiB available, 3,541 additional file-page refaults,
352 pages scanned in direct reclaim and 278 pages reclaimed. Around 76%
of CPU time between snapshots was system time. The preview eventually stopped
normally, without OOM, after 221 frames in 48.69 seconds.

A UART-only control, with no SSH session retained during preview, displayed
187 frames in 20.96 seconds and exited normally. Five-second rates stayed
at 8.92–9.74 fps; five flagged capture errors were discarded. This is close
to the earlier preview result and shows why the SSH runs are not a neutral
performance measurement on this memory-limited board. The counters and
control support memory pressure from the additional session as an important
factor; they do not establish every source of capture errors. Low memory
headroom with concurrent SSH and intermittent capture errors remain
limitations. Raw logs and counters stay in ignored local paths.

The LVGL camera page also passed a UART-only run, restoring both console
modes after 11.02 seconds. Opening the touch device logged rate-limited
CST9220 `-EPROTO` reports. The same warning was recorded before this rebase
in `lvgl-visuals.md`; it has not been explained or fixed by these tests.
A final home-page run lasted 90.20 seconds and restored both console modes.
No physical-touch confirmation was received during that window, so touch
responsiveness is not counted as retested.

### Final state

A normal reboot of the application firmware returned to Linux, reassociated
Wi-Fi and restored SSH. NAND, swap, firewall and peripheral registration
checks passed again. The ordinary persistent test file survived and was
then removed; all three private configuration comparisons still matched.
Both demos remained stopped by default. There was no panic or OOM, and
the kernel was untainted. The connected UART recorder retained about
3.6 MB within its 15,000,000-byte limit.

The application bundle remains installed. Source tips for all three code
stacks are unchanged by hardware validation; only this fork-only test
record is committed. The writable-layer USB script on minimal BSP,
intermittent camera errors, concurrent-SSH memory pressure and pre-existing
touch protocol warnings are retained as explicit limits of these results.

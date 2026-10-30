# Python quality and legacy-board validation, 2026-10-30

Fork-only evidence; this document is not part of the upstream stack. Raw
build and console captures remain in ignored local directories.

## Reviewed source

Reviewed every Python file added or modified since upstream main
`2f3eb1268edd1bb2ca002eb4be339a635003b1e4`: both firmware packers, native USB
flashing and download-reset helpers, reset monitor, continuous UART
recorder, script tests and the LVGL example renderer. Existing upstream
Python examples and image-manifest code are unchanged.

The packers now expose focused parsing/packing functions, with command-line
handling and file I/O confined to main. They identify named vendor tables
instead of relying on the first array in a header, reject malformed
separators, expressions and out-of-range bytes, and validate before
overwriting the output. BMI270 retains its exact size and digest check.
Synthetic fixtures cover failures without copying vendor register data
into the test suite. CLI failures report useful errors without tracebacks.

The flashing helper separates USB identity validation, input loading and
programming/reset. Simulated-UART tests register cleanup immediately, so
setup failures also release processes and PTYs. The host-tool fixture and
PNG helper names were made clearer. Other reviewed scripts did not need
cosmetic rewrites.

Validation: 19 host-script tests, six SC101IOT tests and six BMI270 tests
pass. All 12 affected Python files parse successfully. The firmware
packers also run with the newly built Buildroot host interpreter. Outputs
are byte-for-byte identical to the previous packers using pinned sources:

| Artifact | Bytes | SHA256 |
| --- | ---: | --- |
| SC101IOT VGA | 408 | `1743ec3ccd8e407cef568a1e95a65e98f96c226dc7487c2e8631835c7a00c453` |
| BMI270 initialization | 8192 | `2d75e68e343a13ff99be98261dfbd99d9e8c6f267da9e3c5aeb883276ad178db` |

Rendered home, IMU and compass PNGs are unchanged.

## Legacy configuration and builds

All three original boards retain their upstream Buildroot configuration
values, partitions, loader defaults and kernel configuration. Original
board device trees and OpenSBI sources are unchanged. New SoC peripheral
nodes are disabled unless selected by the Mosaico device tree. Fixed-host
USB PHY setup and the Korvo RGB display clocks retain their original paths.
Defconfig generation passes its consistency check for all four boards.

CoreBoard1 and Korvo1 loaders and kernels compile successfully with the
current sources. All four board DTBs compile. Standalone kernel builds
use the regulatory database and already patched ESP-Hosted sources that
Buildroot normally supplies; initial missing-input failures in the manual
harness were corrected without repository changes.

| Board | Kernel Image bytes | Hardware coverage |
| --- | ---: | --- |
| esp32s31 | 3,698,900 | Full build and boot on the connected Mosaico |
| coreboard1 | 4,493,916 | Loader, kernel and DTB compilation only |
| korvo1 | 4,532,444 | Loader, kernel and DTB compilation only |

No physical CoreBoard1 or Korvo1 was available. Compilation and source
review do not establish their peripheral behavior on hardware.

## Generic esp32s31 hardware test

A fresh Buildroot output directory was built with the unchanged default
esp32s31 defconfig, then packaged and flashed through native ROM USB. No
Mosaico kernel options, board overlay or applications were enabled.
The 7,802,880-byte squashfs and kernel both fit the original partitions.
All programmed regions passed esptool verification; watchdog reset
returned to Linux automatically. CH343 remained owned by the bounded
15,000,000-byte recorder.

The running model is Espressif ESP32-S31-WROOM-3-N16R16V. UART commands,
the original NOR squashfs/UBIFS overlay, zram, the 62-line GPIO controller,
wlan0 and hci0 are present. MemTotal is 12,192 KiB. Wi-Fi scan returned
success and eight BSS entries; network names were not copied into this
document. This test did not associate using personal credentials.
No Mosaico framebuffer, camera, NAND devices or applications are exposed.

A nonsensitive marker in the original NOR writable layer survived a
normal reboot and was removed afterwards. The kernel remained untainted
and dmesg contained no WARNING, BUG, Oops, panic or Call Trace records.
The generic NOR initialization and first-boot UBI formatting were expected.
Personal Wi-Fi and SSH files were never copied into that NOR overlay or
the firmware. Mosaico NAND was not exposed to the generic kernel.

The generic configuration intentionally lacks the Mosaico reboot-mode
driver. Returning to ROM after this test requires physical BOOT/RESET;
the default configuration was not modified merely to simplify recovery.

## Stack placement

The review is folded into the original owning commits, keeping 11 BSP,
14 peripheral and seven application commits. Branch tips are:

- BSP: `3b08498b7ddafbf12f6e760b46a179a22521401a`.
- Peripheral: `36584fc5fed51d514af43533a392a3641355c08d`.
- Application: `d4da02440d8efe465fda033a1a468c55b1ed51ff`.

Dependency remains upstream main -> BSP -> peripheral -> application.
Application contains no system-script, kernel, loader or device-tree
changes relative to peripheral. Each local branch tracks its matching
fork branch. Previous tips are retained under
archive/stack-before-python-review-20261030; main is unchanged.
The local patched ESP-IDF checkout is preserved and not staged.
The three stack refs were pushed atomically to the fork using exact old-tip
leases. Existing SSH authentication was used after HTTPS lacked a credential;
no repository remote configuration was changed.

## Recovery status

The known-good dark Mosaico firmware is retained locally as a seven-image
restore bundle; every image matches its recorded SHA256. The device is
currently running generic esp32s31 after its successful reboot test.
Physical BOOT/RESET has been requested so the bundle can be restored through
native USB. NAND persistence and configuration hashes will be checked after
that restoration; no restoration is claimed yet.

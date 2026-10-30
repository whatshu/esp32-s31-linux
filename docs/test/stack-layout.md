# Local stack preparation

These notes belong to the fork. Do not include this branch in the three
upstream series. Branch publication and hardware checks are recorded in
[stack-validation.md](stack-validation.md). No PR or stack service submission
is part of this work.

The branch chain is `upstream/main -> bsp -> peripheral -> application`.
The requested original-project baseline is
`annoyedmilk/esp32-s31-linux` main at `2f3eb1268edd1bb2ca002eb4be339a635003b1e4`.
`origin/main` and local `main` retain the original fork history. The backup
`archive/esp-mosaico-before-stack-20261029` points to `2729c3b413b0912ce7b02280b14608769f52ece9`.
`rework/esp-mosaico` preserves the temporary upstream integration tree.

The series keep the generic esp32s31 board independent. Only CoreBoard V1.2
and CameraBoard V1.4 wiring are claimed. External dependencies remain Git
submodules; kernel overlays and integration patches stay in `linux/`.
Full source trees, private captures and helper scripts remain ignored in
`build/`, `vendor/` and `logs/`.

The published application series includes the manually started camera
preview with hotplug recovery. Later local commits add an LVGL peripheral
test interface; its combined camera validation is still pending. See
[lvgl-validation.md](lvgl-validation.md) for the exact revision and status.

## bsp

Base: `upstream/main`.

```text
7cc422b build: isolate the patched ESP-IDF sources in a submodule
636ddbe spi: add ESP32-S31 GPSPI masters and native memory operations
4f30e58 mtd: spinand: use the one-byte bad-block marker on GD5F1GM7UE
74d9f9c mtd: mtdswap: reserve ECC-protected marker pages when requested
080b8b4 riscv: add minimal ESP-Mosaico CoreBoard V1.2 support
be34151 buildroot: strip unused symbols from installed kernel modules
4c573b7 power: reset: support orderly ESP32-S31 ROM download reboots
891ae22 scripts: retain bounded UART logs behind a console proxy
e772163 scripts: flash Mosaico over native ROM USB and return to Linux
```

## peripheral

Base: `bsp`.

```text
f8f2b4e input: touchscreen: add the Hynitron CST9220 controller
2ad4732 fbdev: add a CO5300 QSPI framebuffer with dirty-region updates
a0d2a09 riscv: dts: enable the Mosaico display and touchscreen
51e3209 pinctrl: esp32s31: expose GPIO edge interrupts
2851d25 iio: enable Mosaico sensors, the AI key and vibration motor
87fc06f clk: esp32s31: describe AXI DMA and camera clock domains
f833749 dmaengine: add ESP32-S31 AXI receive channels
bc4142b media: i2c: add SC101IOT VGA capture mode support
4237ee0 media: add Mosaico DVP capture and removable camera management
f49ea2a clk: esp32s31: add AHB DMA and fractional I2S master clocks
e8797fd dmaengine: add ESP32-S31 AHB cyclic peripheral channels
ff3b5f3 ASoC: add the ESP32-S31 I2S CPU interface
d710c68 riscv: dts: connect Mosaico ES8311 audio and its speaker route
eb2456a usb: enable Mosaico manual OTG roles and HID input
071b6c1 riscv: reduce Mosaico zram allocation overhead
515cfcb build: refresh CI kernel sources when stack inputs change
```

## application

Base: `peripheral`.

```text
07ea4dc applications: add a reconnecting V4L2 camera preview
3135718 buildroot: package the manual Mosaico camera preview
1d95296 applications: convert camera frames in small RGB tiles
```

## Review and verification

The Standards review found an AXI DMA termination failure that could free
hardware-owned memory. The driver now quarantines active links, pins the
provider and refuses restart after a stop timeout. The camera pins the
active MMAP pages, fails the queue and requires reboot for recovery.
DMA-BUF imports are disabled because attachment lifetime handling on that
failure path is not implemented. This fault path has been code reviewed,
not induced on hardware.

The Spec review caught the old shared-IDF default, submodule detection,
optional upstream Netfilter/TUN selections, missing DT vendor prefixes
and the unignored preview binary. These were fixed in their owning commits.

Host-only checks performed after reorganization:

- All 36 kernel integration patches apply to pristine Linux 7.3 with
  zero fuzz. No full kernel tree is committed.
- BSP and peripheral Image, modules and all four DTBs build with the
  existing RV32 Buildroot toolchain. Images are 4,195,828 and 4,681,556
  bytes respectively, within the Mosaico 0x480000 kernel partition,
  including the 12-byte manifest. Image timestamps are not reproducible.
- Mosaico DT schema validation passes after adapting the panel's
  `spi-tx-bus-width` constraint to Linux 7.3's array property.
- All four generated Buildroot defconfigs match their fragments at every
  branch head. The three original board selections retain their upstream
  configuration values. The application selection resolves in Buildroot.
- The 12 UART recorder and native USB flashing helper tests pass without
  hardware. Shell syntax checks and kernel C checkpatch checks pass.
- Preview compiles on the host and RV32 with C11, `-Wall -Wextra -Werror`;
  host help/error paths and staged installation pass. It was not started
  on the board during stack preparation.
- Added files were checked for private keys and concrete Wi-Fi credentials.
  No personal filesystem archive or raw serial log is committed.

The checks above describe the initial stack preparation, which reused an
existing local toolchain and did not flash the board. Subsequent fresh
end-to-end builds and native USB hardware checks are recorded separately
in [stack-validation.md](stack-validation.md), with source revisions and
remaining failures.

Historical hardware results and unresolved issues, including preview
tearing, quiet audio, magnetic interference and the unresponsive battery
gauge, are in [esp-mosaico.md](esp-mosaico.md). Raw measurements remain at
the ignored local paths listed there. Fresh builds and hardware checks
should record their own source revisions before claiming those results.

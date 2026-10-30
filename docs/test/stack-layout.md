# Local stack preparation

These notes belong to the fork. Do not include this branch in the three
upstream series. Branch publication and hardware checks are recorded in
[stack-validation.md](stack-validation.md). No PR or stack service submission
is part of this work.

The branch chain is `upstream/main -> bsp -> peripheral -> application`.
The requested original-project baseline is
`annoyedmilk/esp32-s31-linux` main at `086e52a6b9ae43a3bb07beb2557ea7595df8165b`.
The latest rebase and build checks are in
[upstream-rebase-20261030.md](upstream-rebase-20261030.md).
`origin/main` and local `main` retain the original fork history. The backup
`archive/esp-mosaico-before-stack-20261029` points to `2729c3b413b0912ce7b02280b14608769f52ece9`.
`rework/esp-mosaico` preserves the temporary upstream integration tree.

The series keep the generic esp32s31 board independent. Only CoreBoard V1.2
and CameraBoard V1.4 wiring are claimed. External dependencies remain Git
submodules; kernel overlays and integration patches stay in `linux/`.
Full source trees, private captures and helper scripts remain ignored in
`build/`, `vendor/` and `logs/`.

The application series includes the manually started camera preview with
hotplug recovery and the LVGL peripheral test interface. Both are stopped
at boot. The full LVGL image's native USB programming and board checks are
recorded in [lvgl-validation.md](lvgl-validation.md), including exact source
revision, physical touch confirmation and remaining validation limits.
The current history consolidation and its mapping to tested revisions are in
[stack-consolidation.md](stack-consolidation.md). The subsequent host-script audit and current branch tips are in [script-portability.md](script-portability.md).

## bsp

Base: `upstream/main`.

```text
7c46cda build: isolate the patched ESP-IDF sources in a submodule
dcf5b86 spi: add ESP32-S31 GPSPI masters and native memory operations
22d7ef2 mtd: spinand: use the one-byte bad-block marker on GD5F1GM7UE
13a80b4 mtd: mtdswap: reserve ECC-protected marker pages when requested
92d7ca9 riscv: add minimal ESP-Mosaico CoreBoard V1.2 support
a2d56bf buildroot: strip unused symbols from installed kernel modules
9439422 power: reset: support orderly ESP32-S31 ROM download reboots
9dba1d2 scripts: retain bounded UART logs behind a console proxy
722a628 scripts: flash Mosaico over native ROM USB and return to Linux
8b58b90 build: refresh CI kernel sources when stack inputs change
685efba build: select host tools from the configured ESP-IDF environment
```

## peripheral

Base: `bsp`.

```text
270d691 input: touchscreen: add the Hynitron CST9220 controller
1fb2096 fbdev: add a CO5300 QSPI framebuffer with dirty-region updates
037eca6 riscv: dts: enable the Mosaico display and touchscreen
3c76c2b pinctrl: esp32s31: expose GPIO edge interrupts
1e54720 iio: enable Mosaico sensors, the AI key and vibration motor
c33d0fe clk: esp32s31: describe AXI DMA and camera clock domains
ff827df dmaengine: add ESP32-S31 AXI receive channels
00c2003 media: i2c: add SC101IOT VGA capture mode support
17754a1 media: add Mosaico DVP capture and removable camera management
4139b3e clk: esp32s31: add AHB DMA and fractional I2S master clocks
628eda9 dmaengine: add ESP32-S31 AHB cyclic peripheral channels
ed32e69 ASoC: add the ESP32-S31 I2S CPU interface
b771c45 riscv: dts: connect Mosaico ES8311 audio and its speaker route
3d3e266 usb: enable Mosaico manual OTG roles and HID input
```

## application

Base: `peripheral`.

```text
b924a2b applications: add a reconnecting V4L2 camera preview
20c6f0a buildroot: package the manual Mosaico camera preview
5fcecdb applications: pin LVGL as a source submodule
4b9a4ee applications: allow embedding the camera preview in a launcher
408d57b applications: add an on-demand LVGL peripheral test launcher
c4db4a2 buildroot: package the manual Mosaico peripheral test launcher
5cad560 applications: add reproducible LVGL interface examples
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

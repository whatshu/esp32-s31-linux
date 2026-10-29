# Local stack preparation

These notes belong to the fork. Do not include this branch in the three
upstream series. No PRs, pushes or stack service submissions were made.

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

The application series adds only the existing camera preview, with manual
startup and hotplug recovery. A future LVGL peripheral test application
belongs in a later application commit; none is implemented here.

## bsp

Base: `upstream/main`.

```text
7cc422b build: isolate the patched ESP-IDF sources in a submodule
636ddbe spi: add ESP32-S31 GPSPI masters and native memory operations
4f30e58 mtd: spinand: use the one-byte bad-block marker on GD5F1GM7UE
74d9f9c mtd: mtdswap: reserve ECC-protected marker pages when requested
6fe7b60 riscv: add minimal ESP-Mosaico CoreBoard V1.2 support
619ec3e buildroot: strip unused symbols from installed kernel modules
944df37 power: reset: support orderly ESP32-S31 ROM download reboots
b5f92f7 scripts: retain bounded UART logs behind a console proxy
e1ab74c scripts: flash Mosaico over native ROM USB and return to Linux
```

## peripheral

Base: `bsp`.

```text
0ce8cac input: touchscreen: add the Hynitron CST9220 controller
9d3c0c9 fbdev: add a CO5300 QSPI framebuffer with dirty-region updates
36da01c riscv: dts: enable the Mosaico display and touchscreen
15b4eb6 pinctrl: esp32s31: expose GPIO edge interrupts
3ade5d2 iio: enable Mosaico sensors, the AI key and vibration motor
99e3627 clk: esp32s31: describe AXI DMA and camera clock domains
3d8f19c dmaengine: add ESP32-S31 AXI receive channels
60b93e0 media: i2c: add SC101IOT VGA capture mode support
24d42b2 media: add Mosaico DVP capture and removable camera management
a6a0f36 clk: esp32s31: add AHB DMA and fractional I2S master clocks
c09534f dmaengine: add ESP32-S31 AHB cyclic peripheral channels
0a2ad76 ASoC: add the ESP32-S31 I2S CPU interface
dc117db riscv: dts: connect Mosaico ES8311 audio and its speaker route
c10c8bf usb: enable Mosaico manual OTG roles and HID input
```

## application

Base: `peripheral`.

```text
4c87ffb applications: add a reconnecting V4L2 camera preview
081d971 buildroot: package the manual Mosaico camera preview
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

These checks reuse the installed compiler, prepared ESP-Hosted sources
and firmware from the existing local build. They do not constitute a fresh
end-to-end loader, toolchain and rootfs build. No reconstructed image was
flashed. The live board, debug recorder, NAND configuration and SDK checkout
were left as they were before this task.

Historical hardware results and unresolved issues, including preview
tearing, quiet audio, magnetic interference and the unresponsive battery
gauge, are in [esp-mosaico.md](esp-mosaico.md). Raw measurements remain at
the ignored local paths listed there. Fresh builds and hardware checks
should record their own source revisions before claiming those results.

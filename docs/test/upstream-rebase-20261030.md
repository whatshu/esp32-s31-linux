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

These builds do not establish runtime behavior on hardware. The connected
Mosaico continues running its previously verified firmware; this rebase
does not flash it or start applications, audio or vibration.

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

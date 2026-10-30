# Stack hardware validation, 2026-10-29

This record belongs to the fork. Raw logs, private filesystem checksums and
firmware bundles remain under ignored `build/stack-validation/` paths.

The CH343 UART recorder stays connected at 115200 baud, bounded to 15 MB.
All programming uses the native Type-C ROM USB port, selected by USB identity,
with `reboot bootloader` and the project flashing helper. NAND is not erased.
The read-only root images contain no local overlay or personal configuration.

## Integration correction

The first full BSP image booted and mounted NAND, but its old `bt` radio
selection disagreed with the new upstream resident firmware's `bt+154`
profile. The host repeatedly requested firmware recovery and Wi-Fi did not
start. Mosaico now uses the same boot profile as the other upstream boards.
This correction is folded into the BSP board commit and inherited by the
peripheral and application stacks. Final results are recorded below after
retesting the corrected images.

## BSP

Source: `e772163cdae1b89d77ea2a45e296fd49090e51d1`. Fresh Buildroot
output with its own GCC 15.3/musl toolchain, loader and OpenSBI. Image:
4,195,828 bytes; squashfs: 8,282,112 bytes. Both fit the NOR partitions.
Full native USB programming, per-region hash verification and automatic
return to Linux passed. The running kernel was Linux 7.3.0, build #1,
2026-10-29 22:54:31 CST.

NAND UBIFS mounted, zram and NAND swap were active. A plain persistent test
file survived flashing and an ordinary reboot. The existing Wi-Fi config,
SSH authorized keys and Dropbear host-key file kept their previous hashes.
Wi-Fi, SSH and a 1 MiB SSH-to-NAND transfer with SHA256 verification passed.
Framebuffer, capture and USB gadget devices were absent, as expected for
the minimal stack; no test application was installed or running.

The preserved writable layer contains an older S10usb script. On BSP it
reports that dwc2 is not installed, without preventing boot. This is an
ordinary persisted file from bring-up, not a script shipped in the BSP image.
The validation did not delete or rewrite it.

## Peripheral

Source: `eb2456a00b935e2f6731085117b1341613ab2c87`, containing BSP.
Full build, native USB programming/verification and automatic Linux return
passed. Image: 4,681,556 bytes; squashfs: 9,310,208 bytes. Running kernel:
Linux 7.3.0 #2, 2026-10-29 22:56:12 CST. NAND, swap, Wi-Fi, SSH, persistent
file and all three configuration hashes passed. A 1 MiB SSH-to-NAND transfer
passed SHA256 verification (11.21 s including cold startup and sync).

CO5300 framebuffer, CST9220, AI key, vibrator, BMI270, both BMM150 sensors,
ES8311 ALSA card, camera and USB role/UDC interfaces were present. No speaker
playback or motor effect was triggered. BMM150 #3 Y still reports its known
magnetic overflow; no calibrated heading is claimed.

MMAP capture: 640x480 UYVY, 60 complete frames, zero errors and sequence
gaps; 14.94 fps including startup, 15.63 fps between completed frames.
USB CDC: 262,144 bytes each way, byte-for-byte equality/SHA256 verified,
0.227 s on the host. The first shell echo harness failed because this
minimal BusyBox lacks timeout; a bounded compiled echo harness passed.
A second top sample showed 97% idle after probes stopped.

Host role, physical HID input, touch motion and audible playback were not
repeated in this pass: native Type-C remains connected to the computer.
The old hardware archive records those earlier tests. The observation USB
camera currently faces the keyboard, so no visual screen confirmation is
claimed from its image. No application was installed or running at boot.

## Application: initial failure

Source before fixes: `a5146e1ede1e99836270ee289f9679c60e7078e4`.
A full image built and programmed through native USB, verified and returned
to Linux. Its kernel is the same as the peripheral image; squashfs is
9,322,496 bytes. The preview executable is installed and remains stopped
at boot. Storage, configuration hashes, Wi-Fi and the checked transfer passed.

Manual preview did not pass. Repeated runs of the original full-frame RGB
conversion were killed by the OOM killer (exit 137). There was still free
NAND swap, but zram's physical allocation exceeded its compressed payload:
about 0.3–0.4 MiB compressed data occupied roughly 1.7–2 MiB of RAM. This
observation is a diagnostic lead, not yet a confirmed complete cause.

A temporary direct-framebuffer-mapping experiment also failed and was
rejected. An eight-line conversion experiment saves about 330 KiB of
anonymous memory and passes host bounds/colour checks, but its first board
run timed out as well. UART and SSH commands stopped completing during
these experiments; no new panic trace was captured. These builds are not
accepted as a working application stack. Raw diagnostics and rejected
patches stay in ignored local build paths.

The verified BSP and peripheral heads above have been pushed to the fork.
Application changes remain local until functional validation succeeds.

## Memory correction and complete-image retest

Reducing only CONFIG_ZSMALLOC_CHAIN_SIZE from 8 to 4 made the original
full-frame preview complete 246 frames and exit normally after a bounded
run. Physical zram allocation stayed near 1 MiB. An eight-line RGB tile
then reduced application allocation from 337.5 KiB to 7.5 KiB. Its first
bounded run completed 279 frames, mostly 9–10 fps, and exited normally.
The smaller chains are a Mosaico peripheral configuration commit; tiling
is a separate application commit. Swap sizes and priorities are unchanged.

A reused BSP Buildroot output also exposed a CI cache problem: applying a
peripheral defconfig did not reapply new kernel patches, so Kconfig silently
omitted drivers. The incomplete image was rejected before flashing. The CI
script now invalidates extracted kernel/headers/Hosted inputs in the same
way as the project's container Makefile. A clean resolved configuration
was checked before packaging.

Final peripheral source: `515cfcbfd95c3acf8cc43972f8a4a024a3b8aac3`.
Complete build/program/hash verification/automatic return passed. Image:
4,681,556 bytes; squashfs: 9,310,208 bytes. NAND persistence, config hashes,
Wi-Fi, SSH and a 1 MiB verified transfer passed. Camera: 60 frames, zero
errors/gaps, 15.61 steady fps. CDC: 256 KiB in each direction, equality and
SHA256 verified, 0.192 s. A first concurrent echo launch did not reach its
ready marker; the sequential retry passed. No OOM or panic was recorded.

Final application source before LVGL:
`1d95296a25c290bc22745e178aeb4eb4bb1277d6`. Complete build/program/verification
and automatic return passed. Image: 4,681,556 bytes; squashfs: 9,322,496 bytes.
Startup/storage/configuration/network checks passed. The packaged preview
completed 283 frames in 30.70 s, around 9.6–10 fps after startup, and exited
cleanly without OOM. Four capture frames were flagged as errors and discarded.
No application ran by default. A normal reboot retained the ordinary NAND
test file and returned with the preview stopped. BSP remains unchanged at
the independently tested e772163 head. All three stack branches were pushed
to the fork; no PR was created.

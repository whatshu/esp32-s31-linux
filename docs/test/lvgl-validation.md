# LVGL application validation, 2026-10-30

Source: application `166cd8ad91699b2b406454b3dc320743d433f0eb`.
LVGL submodule: v9.4.0, `c016f72d4c125098287be5e83c0f1abed4706ee5`.
Raw output, private configuration comparisons and screenshots remain ignored
under `build/stack-validation/`. No audio playback or vibration was triggered
in these automated checks.

## Build and host checks

- C-only native and RISC-V builds with application warnings treated as errors.
- Thirteen page renders and teardown under ASan/UBSan, with leak detection.
- Synthetic input with contacts in tracking slots 5 and 15, followed by
  independent releases. No physical AI event was injected.
- Host layout inspection of the 480x480 home screen and test pages.
- Full loader/OpenSBI/kernel/rootfs build. Image: 4,681,556 bytes; squashfs:
  9,560,064 bytes. Both partition checks passed.
- Buildroot legal-info collected application and statically linked LVGL
  licenses. No application init script is installed.
- Two source reviews checked repository standards and the requested scope.
  Their findings were resolved before this revision was built.

## Programming and board checks

The complete image was programmed through native ROM USB following
`reboot bootloader` from the CH343 debug console. All flashed regions were
verified and watchdog reset returned to Linux without a physical reset.
CH343 remained dedicated to bounded serial recording.

The post-flash probe passed boot, NAND overlay and swap, device presence,
Wi-Fi/SSH, the persistent marker, private configuration hash comparisons
and a verified 1 MiB SSH-to-NAND transfer (2.03 s including startup and sync).
Both installed application executables matched the packaged hashes.
Neither application was running after boot.

The packaged camera page ran for 25 seconds, then for 40 seconds, and
exited normally with display mode restored. The observation camera captured
an actual camera image in the viewport, with the GUI header/footer present.
The 40-second run reported MemAvailable 540 KiB during sampling and
2,400 KiB after exit, against 2,196 KiB before it. This is a single run,
not a guaranteed memory floor or a new frame-rate measurement.

The user confirmed that Display/Home/Touch navigation worked. A parallel
raw evdev capture recorded two simultaneous contacts, 19 tracking IDs,
416 coordinate updates and 19 releases. The observation camera also
captured the Touch page. The initial report of unresponsive touch occurred
while the test harness had not successfully relaunched the GUI; the
confirmed running instance handled physical input.

A corrected abnormal-exit check killed the GUI child process group with
SIGKILL while retaining its supervisor. It returned in 6.216 seconds with
the expected failure status and the text console restored. A repeat after
ordinary reboot also checked keyboard mode: both forced and normal exits
restored KD_TEXT and K_UNICODE (keyboard mode 3). An earlier version of this ignored harness incorrectly depended on disabled
`CONFIG_PROC_CHILDREN`; it failed to find the child and ultimately killed
the supervisor. That harness result is rejected, not an application fault.

Eight further board runs opened IMU, Compass, Audio, Wi-Fi, USB, Storage,
System and Keys pages for four seconds each and returned status zero with
text mode restored. These were page-open/read/exit checks, not clicks on
their action buttons. In particular, Audio stayed silent and the motor was
not actuated.

An ordinary reboot passed the same persistence/configuration/device probe,
with neither application running automatically. A second verified 1 MiB
SSH-to-NAND transfer completed in 2.95 seconds including startup and sync.

## Rejected earlier run and limits

An earlier combined run used uploaded executables on RAM-backed `/tmp`.
It produced a long stall, with a delayed MemAvailable reading of 300 KiB
and repeated touchscreen `-EPROTO` messages. Bounded recovery did not
complete and a physical reset was requested. Memory pressure was a
hypothesis, not a proven root cause. The accepted comparison above uses
the packaged read-only NOR files, without RAM-backed executable copies;
no touchscreen protocol errors were captured in that comparison.

One SSH result stream was delayed despite the packaged GUI test having
already exited successfully. UART and subsequent SSH commands remained
usable. Do not interpret a missing SSH completion message alone as a
kernel hang.

These checks do not revalidate every physical peripheral action through
the new GUI. Audio and motor operation remain explicit human-triggered
checks. USB host input and camera hotplug were previously validated in
their owning stack; the GUI introduces no automatic role change at boot.
Known preview tearing, quiet audio, magnet interference and unavailable
battery telemetry remain unchanged.

The earlier three-stack programming results are recorded at their exact
revisions in [stack-validation.md](stack-validation.md). Service/build
choices are listed in [runtime-options.md](runtime-options.md); no broad
service policy was changed.

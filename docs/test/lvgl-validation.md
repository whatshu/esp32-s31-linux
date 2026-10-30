# LVGL application validation, 2026-10-30

Work in progress. Do not treat the combined camera page as accepted yet.
Raw output and screenshots remain ignored under `build/stack-validation/`.
No audio playback or vibration was triggered in the automated checks.

Source: application `166cd8ad91699b2b406454b3dc320743d433f0eb`.
LVGL submodule: v9.4.0, `c016f72d4c125098287be5e83c0f1abed4706ee5`.

Passed:

- C-only native and RISC-V builds with application warnings treated as errors.
- Thirteen page renders and teardown under ASan/UBSan, with leak detection.
- Injected input stream with two active contacts in tracking slots 5 and 15,
  then independent releases. No physical AI event was injected.
- Host layout inspection of the 480x480 home screen and test pages.
- An embedded-preview board harness preserved the caller's header/footer and
  console ownership through a 10-second camera run (65 frames, clean exit).
- Board LVGL home screen ran for eight seconds and exited with status zero.
  MemAvailable was 1,272 KiB while running, versus about 1,792 KiB before
  the upload/run. This comparison includes test-file/cache effects.
- Full loader/OpenSBI/kernel/rootfs build. Image 4,681,556 bytes; squashfs
  size is recorded in the ignored bundle manifest. Both partition checks passed.
- Package legal-info collected both application and statically included
  LVGL licenses. No application init script is installed.

Not accepted:

The first combined GUI/camera run used uploaded executables on RAM-backed
`/tmp`. It caused a long stall. A delayed diagnostic reported MemAvailable
300 KiB; SSH stopped completing and serial commands timed out. Kernel
touchscreen messages continued, with no newly captured panic or OOM kill.
This supports investigating memory pressure, but does not prove its root
cause. A physical reset was requested after bounded recovery attempts.
The next comparison must run the binaries from NAND or the packaged NOR
root, with no RAM-backed executable copies, and kill the test before
spawning diagnostic subprocesses.

Opening touch also exposed repeated `-EPROTO` reports. A stopped-input
raw report read returned all `0xff`. Physical touchscreen interaction and
the report state need rechecking after reset; the earlier bring-up's
touch verification does not substitute for this GUI test.

The previous three-stack full programming results remain valid for their
recorded revisions in [stack-validation.md](stack-validation.md). The new
LVGL image has been built and packaged but has not yet been programmed or
accepted on the board. The new application commits remain local pending
that validation. Runtime/build policy candidates are listed separately in
[runtime-options.md](runtime-options.md); no broad service policy was changed.

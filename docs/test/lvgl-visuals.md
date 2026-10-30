# Sensor visualization checks, 2026-10-30

Application source: `1849f4e2d5d1a9598433aca287ec6a81dcadb415`. This note belongs to the fork-only test
branch. Raw logs, fixture sources and screenshots stay ignored in
`build/stack-validation/`; no configuration contents are recorded here.

## Design and behavior

The home screen retains twelve manually opened tools, with warm neutral
surfaces, restrained icons and a consistent Home control. References were
[Linear's design refresh](https://linear.app/now/behind-the-latest-design-refresh)
and [Braun's design principles](https://secure-us.braun.com/en-us/design/design-and-innovation).
No website assets were copied into the application.

BMI270 acceleration drives a gravity level and pitch/roll readouts.
Angular velocity is converted from IIO radians/s to degrees/s and shown on
three signed bars spanning +/-180 degrees/s; the number remains visible
when the bar clips. This is a gravity estimate, not integrated gyro
orientation, and movement affects it.

Each BMM150 has a local XY field dial, total magnitude and per-axis values
in microtesla. These are sensor-axis directions, not calibrated north.
A missing or over-range axis is not converted into zero; an invalid XY
pair hides the needle. The tested board has an over-range Y axis on its
second BMM150, consistent with the previously recorded magnet problem.

New sensor samples are processed at most four times per second and only
on the open sensor page. Visible pixel geometry and formatted values
determine invalidation. No framebuffer-sized canvas, idle animation,
audio trigger, motor trigger or boot service was added.

## Host checks

- Native and RV32 C-only builds pass with -Wall -Wextra -Werror.
- All thirteen page renders and teardown pass ASan/UBSan with leak checks.
- Fixture IIO files check unit conversion, a level and 90-degree tilt,
  near-zero gravity, malformed axes and BMM150 INT_MIN overflow.
- Forty-eight transitions per sensor page compare incremental and full
  repaint results, including invalid axes, changing signs, clipped bars,
  inversion and disappearing data. At most 64 antialiased edge pixels
  may differ by two RGB565 channel steps; larger differences fail.
- Layouts were inspected both with missing data and populated fixtures.
  Fixture values are confined to the ignored host harness.

## Initial board runs

The uploaded executable runs from NAND rather than RAM-backed /tmp.
A 90-second IMU run, a 90-second compass run, then the optimized 60-second
IMU and 45-second compass runs returned successfully. Each restored
KD_TEXT and keyboard mode 3. The optimized IMU run reported MemAvailable
2160 KiB before and 2168 KiB after; the compass run reported 2088 and
2080 KiB. These are individual observations, not memory guarantees.

Stationary IMU sampling over /proc/stat intervals measured approximately
40.6% busy CPU before visible-change invalidation and 13.2% afterwards.
Intervals were about 12.5 and 40 seconds respectively and include the
existing system services and SSH/framebuffer-capture activity. They are
spot checks, not a controlled throughput benchmark. The compass sample crossed application exit and is not used
as an active-page CPU figure.

Framebuffer captures confirmed live values, the gravity level, local-field
needle and second-sensor over-range display on the actual board. No
speaker or vibration action was invoked. Existing camera tearing and
magnet-related heading limitations remain unchanged.

## Packaged firmware

The complete loader/OpenSBI/kernel/rootfs build passed without compiler
warnings. Image is 4,681,556 bytes and squashfs is 9,568,256 bytes; both
partition checks passed. Squashfs grew by 8,192 bytes from the previous
LVGL image. The release archive passed its integrity check.

Native ROM USB programming verified every region and automatically
returned to Linux. The post-boot probe passed NAND overlay/swap,
Wi-Fi/SSH, device presence, configuration hash comparisons and the
persistent marker. A verified 1 MiB SSH-to-NAND transfer took 2.08 seconds
including SSH startup and sync. Both applications were stopped at boot.
The installed launcher matched the staged binary's SHA256 and its license
notices were present.

Packaged IMU and Compass runs each completed for 15 seconds with status
zero and restored both console and keyboard modes. A mistakenly
overlapping compass harness launch was refused by the framebuffer lock;
that result was excluded and the page was rerun after the IMU exited.

A subsequent SSH command stream stalled while UART commands remained
responsive, similar to the earlier session noted in lvgl-validation.md.
The owned host SSH process was stopped; installed-file verification and
page checks continued through the UART proxy without resetting the board.
No cause for the stalled stream was established.

The packaged Compass run also logged CST9220 -EPROTO reports while its
input device was open. This source change does not modify that driver.
Physical navigation and movement confirmation for the final interface
are pending; page-open success is not presented as proof of touch input.

The final home screen was started manually for user inspection. No init
script was added; reboot still leaves the applications stopped. Audio and
motor actions were not triggered.

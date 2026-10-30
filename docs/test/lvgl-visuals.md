# Sensor visualization checks, 2026-10-30

Application source: `1849f4e2d5d1a9598433aca287ec6a81dcadb415`. This note belongs to the fork-only test
branch. Raw logs, fixture sources and screenshots stay ignored in
`build/stack-validation/`; no configuration contents are recorded here.

## Design and behavior

The home screen retains twelve manually opened tools, with charcoal
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
  Verification fixtures stay in the ignored host harness; the separately
  documented interface examples have their own illustrative inputs.

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
The user subsequently confirmed navigation and sensor visualizations work.
The touch driver is unchanged; that confirmation does not explain the
previously logged protocol errors.

The final home screen was started manually for user inspection. No init
script was added; reboot still leaves the applications stopped. Audio and
motor actions were not triggered.

## Motor confirmation and dark interface examples

The user initially reported no vibration from the Keys & motor page.
A targeted run of the existing application's motor action uploaded the
FF_RUMBLE effect and completed three 80 ms pulses. A temporary read-only
GPIO observer counted three rising and three falling edges on GPIO8. The
user then confirmed feeling three vibrations. No motor driver or pulse
setting was changed, and no cause for the earlier report was established.

The temporary observer could not be unloaded because the running kernel
disables module unloading. Its local payload was removed; the subsequent
firmware reboot removed the loaded observer, verified by its absent proc
entry and module-list entry. No diagnostic module, daemon
or raw log is included in the application series.

`358d618` changes the interface palette to charcoal surfaces with off-white
text and limited coral, sage and blue accents. `c29292d` adds three
480x480 PNG illustrations under `applications/mosaico-test/examples/`.
These are generated by the actual LVGL interface using explicit example
IMU and magnetic-field values, not claimed as board measurements.

The optional native renderer is disabled by default and not installed in
firmware. Its three outputs pass ASan/UBSan with leak checks, decode as
480x480 RGB PNG and remain identical across repeated regeneration. The
existing thirteen-page and incremental/full-repaint checks also pass with
the dark palette. The renderer opens no devices and invokes no hardware
action.

## Packaged dark revision

Application source: `c29292dceb2d85a3112a7f5c2adaf1936954fc2d`. The full
build and partition-size checks passed without compiler warnings. Image
remains 4,681,556 bytes and squashfs 9,568,256 bytes. The release archive
passed its integrity check. `MOSAICO_TEST_BUILD_EXAMPLES` is OFF in the
target CMake cache and no example executable is installed.

Native ROM USB programming verified all regions and automatically returned
to Linux with the UART recorder left running. The installed GUI executable
matches the staged SHA256. The post-boot UART probe passed NAND overlay,
swap, persistent marker, expected devices and unchanged private
configuration hashes. Wi-Fi was associated with its existing IPv4 address
and CDC was configured. Both applications were stopped after boot.

The bounded SSH probe timed out during connection attempts; no new SSH
transfer result is claimed for this revision. The serial console remained
responsive. An attempted single-line UART probe upload exceeded the console
input limit; the incomplete shell command was cancelled, then the existing
chunked uploader transferred and verified the probe successfully. Neither
event changed application or driver code. Raw captures remain ignored.

The packaged IMU and Compass pages each ran for eight seconds and exited
with status zero. Both restored console mode 0 and keyboard mode 3. The
post-run kernel log contains none of the checked panic, Oops, BUG or OOM
markers. This check exercised page rendering and cleanup without invoking
audio or motor actions. Temporary board-side harness files were removed.
The home screen was then started manually for inspection; no startup
policy changed.

# Hardware validation

This document records test methods, measured results and remaining checks.
Implementation and hardware interfaces belong in [internals.md](internals.md).
UART captures belong in ignored `logs/`; temporary test programs, images
and measurements belong in ignored `build/`. Reference
checkouts belong in ignored `vendor/`. Keep credentials, personal filesystem
archives and unredacted captures out of Git and release bundles.

For each hardware run, record the board revision, connected modules, running
kernel build ID, source revision and test conditions with its captures.
A local build passing checks does not establish that the same image was
flashed or tested. The records below describe the completed runs, rather
than claiming coverage of every later revision.

## ESP-Mosaico coverage

Status as of 2026-10-29, on CoreBoard V1.2. Camera results apply only to
left-slot CameraBoard V1.4 / SC101IOT. The input and sensor run used kernel
`#34`, built on 2026-10-28 at 21:02:14. USB/audio kernel `#37` was flashed
on 2026-10-29, followed by `#38` (11:35:15 CST) with the amplifier routing
correction described below.

| Path | Recorded hardware result | Remaining checks |
| --- | --- | --- |
| UART, Wi-Fi station and SSH | Console, network connection and SSH used during bring-up | AP/Enterprise and concurrent radio/storage load lack a V1.2 validation record. |
| CO5300 display | Console, RGB565, partial writes, mmap and reboot recovery checked | Preview tearing remains open; TE synchronization is not implemented. |
| CST9220 touch | Targets, swipes, holds and two-contact tracking checked | Five contacts and use by a gesture application; GPIO6 IRQ support is not implemented. |
| NOR/NAND, PSRAM and swap | File persistence, full swap-block readback and incompressible memory pressure checked | Long-duration wear and interrupted-write recovery are unverified. |
| Reboot to downloader | Ordinary reboot, ROM entry and return to Linux checked | Repeat on the next flashed image. |
| AI key and motor | Both key edges recorded; user confirmed three vibrations | Long holds, rapid presses and motor cancellation on close need dedicated checks. |
| BMI270 | Initialization, temperature reads, stationary and motion response checked | Mounting axes, calibration and physical accuracy are unverified. |
| Two BMM150 sensors | Both probe; five magnetic axes respond | `0x12` Y overflows in this magnetic assembly; six-axis magnetic measurements and heading are unverified. |
| Native Type-C USB | Updated image registers the UDC and loads `g_serial` without the FIFO warning | Host enumeration and bidirectional transfers remain unverified. |
| ES8311 / I2S / DMA | User heard the AI-triggered 0.5 s, 440 Hz tone at BSP 50%; amplifier-off routing checked | Output was quiet. Recording and other rates remain unverified. |
| Camera | VGA MMAP capture and physical reconnect with the temporary preview checked | DMA-BUF import, boot without the module and repeated hotplug cycles remain unverified. |
| Bluetooth | Shared radio firmware provides the controller | V1.2 BLE and BR/EDR scan, connection and data transfer lack a hardware validation record. |
| Power management | Hardware charging is reported by the user | Linux shutdown on V1.2 and power consumption are unverified. |
| BQ27220 | BSP-equivalent requests at `0x55` time out | Support is deferred; no node or driver is enabled. |

Sensor interrupt buffers, a magnetic contact classifier,
additional camera modes, simultaneous audio capture/playback,
IEEE 802.15.4 and Linux NOR XiP are not implemented. They are outside the
current test coverage rather than tests awaiting a pass. The SAM8108 latch
and charge LEDs remain hardware-controlled; Linux uses the existing SBI
shutdown path. Programmable battery monitoring is blocked by the gauge's
missing response.

The current pass covers user-triggered speaker playback and USB host/device
roles, including keyboard and mouse input. Reliability and stress
checks listed above are not part of this pass. Retain UART recording and
do not start audible playback without a physical user key press.

## UART recording

`scripts/serial-record.py` owns the physical console UART and records received
bytes before forwarding them to a pseudo-terminal. It sends no commands or
reset pulses on startup. It sets DTR and RTS inactive before opening the port;
some USB bridges can still glitch control lines when opened. The recorder
reconnects after a USB disconnect. Use a stable `/dev/serial/by-id/` path on
Linux so it reconnects to the board rather than another serial device.

Start it with `make record SERIAL_PORT=PORT`, or keep it running independently
of the terminal on a Linux host:

```sh
systemd-run --user --unit=esp-mosaico-serial \
    --property=Restart=on-failure --property=RestartSec=2 --property=UMask=0077 \
    --working-directory="$PWD" /usr/bin/python3 "$PWD/scripts/serial-record.py" \
    --port PORT --baud 115200 --log-dir "$PWD/logs/serial"
```

The host Python needs pyserial. This transient user service survives closing
the terminal; it is not installed as a host boot service. Its physical port
must not be opened by another reader. `make console` connects through
`logs/serial/console` without resetting the board or creating a second log;
Ctrl-] closes only the console, leaving recording active. Other command tools
must use that proxy too. Only one console client should read it at a time.

Raw output rotates through `uart.log`, `uart.log.1` and `uart.log.2`, each at
most 5,000,000 bytes. The total never exceeds 15,000,000 bytes; rotation removes
the oldest file first. Read `.2`, then `.1`, then the current file for time
order. Output is unbuffered to the host filesystem; this does not guarantee
survival of a host power failure. A slow or absent console can lose proxy
output but cannot block or truncate the recorded UART stream. `status.json`
reports connectivity, the PID and any proxy drops. Opening the proxy does not
replay the entire log. USB disconnection, a stopped recorder, UART overruns
and output emitted before recording starts can still leave gaps.

Logs have mode 0600 in a private directory and `logs/serial/` is ignored by
Git. They can contain shell output and credentials: do not publish or commit
captures without redacting them. The recorder stores no separate transmit log;
commands echoed by the board still appear in received output.

For ESP-Mosaico flashing, send `reboot bootloader` through the proxy, then
use the native Type-C ROM USB port with `scripts/flash-esp-mosaico.py`.
Keep CH343 recording at 115200 throughout entry, flashing and reboot; do
not give the debug UART to esptool. A foreground recorder stops with Ctrl-C
or SIGTERM.
After a panic, keep power applied and stop issuing commands while preserving
the logs. The current kernel does not enable pstore, KGDB or Magic SysRq; a
reset loses its volatile log and register state. UART recording retains only
what the board actually emitted and the host received. Reproduce only after
capturing the first exception, registers, call trace and running build ID.

The recorder's rotation, bidirectional proxy, duplicate-owner rejection,
slow-console handling and reconnect behavior can be checked without a board:

```sh
python3 -m unittest discover -s scripts/tests -p 'test_serial_record.py' -v
```

## Display

The framebuffer console, RGB565 color bars through `/dev/fb0`, partial
writes, mmap updates and display recovery after Linux reboot have been
checked on the board. After the BBPLL correction, 90 synchronous full-frame
updates measured 11.132–11.229 FPS at 40 MHz. Camera capture confirmed the
test bars were displayed. A five-second idle sample used about 0.8% CPU;
the touch device was closed during that sample.

**Known issue (2026-10-28): camera preview tearing.** Moving objects show
severe tearing in the temporary preview on CoreBoard V1.2 with CameraBoard
V1.4. The program converts 640x480 UYVY capture to a centered 480x360
RGB565 image. Preview runs at approximately 10 FPS, with each display
update taking about 63 ms. The 360-line image is sent as 45 blocks of
eight lines while the panel continues scanning its display RAM.

The official
[BSP enables TE synchronization on GPIO43](https://github.com/esp-mosaico/esp-mosaico-bsp/blob/7cbfb390638dcb93522fc3cde094b2d43cf907c2/components/esp-mosaico-bsp/onboard/display.c#L398).
The Linux panel driver and device tree do not implement that path.
`fsync()` waits for deferred framebuffer work to finish; it does not
synchronize updates with panel scanning. Missing scan synchronization and
the long transfer time are the leading explanation, pending a controlled
comparison. Buffer ownership was reviewed without finding premature
reuse, but raw capture and displayed frames have not yet been compared to
exclude a capture-side contribution. The issue remains open; no driver or
preview changes were made during this investigation.

## Touch

Validation on 2026-10-28 used the kernel at `94e431d`, temporary fbdev
targets and evdev event capture. The controller reported project
`0x6854`, which requires each report to be acknowledged.

| Check | Result |
| --- | --- |
| Center and four corner targets | All five contacts were within a 40-pixel radius of the drawn targets. These were manual alignment checks, not precision calibration. |
| Horizontal and vertical swipes | Coordinate direction matched the displayed guides. |
| Single-contact hold | A 1.560 s hold produced one activation and one release. |
| Two contacts | Independent slots tracked both moving and stationary fingers; a stationary overlap lasted 4.480 s. |
| Release state | All 30 activations had matching releases. At the end, no tracking slots were active and `BTN_TOUCH` was clear. |
| Event integrity | Across 280 event frames, no coordinates were out of range, no `SYN_DROPPED` occurred and no I2C report failures were logged. |

Linux suppresses unchanged input values. A test of stationary contacts
must include the time until release, rather than requiring repeated
coordinate events. The temporary visual test initially missed this
interval; replaying the captured slot states confirmed the holds without
a driver change. Five simultaneous contacts and gesture recognition
above evdev have not been tested.

## Storage

The earlier source-rate model overclocked transfers: requesting 40 MHz
returned a shifted NAND ID. Reading the same ID with corrected dividers
returned `c8 91`. Changing only the master-to-HS ratio did not fix the ID;
both the source rate and divider constraints were required. GPSPI2 uses
the same corrected clock model.

Validation uses incompressible data rather than zero-filled files.
The initial generic SPI path measured 0.183 MiB/s write with fsync and
0.255 MiB/s cold read with byte verification. Native memory operations
and bounded status polling removed the allocation/phase overhead and
tick-sized waits. Earlier tests labelled 20 MHz predate the BBPLL
correction and must not be treated as calibrated frequency comparisons.

On 2026-10-28, the final kernel at the corrected 40 MHz passed:

| Check | Result |
| --- | --- |
| 4 MiB incompressible UBIFS write with fsync, three runs | 1.422–1.627 MiB/s; median 1.435 MiB/s |
| Cold read and byte verification of each file | 2.267–2.289 MiB/s; median 2.275 MiB/s |
| Raw 2 MiB MTD read | 0.50 s elapsed, 0.46 s kernel CPU |
| Direct swap-block I/O, with swap disabled | Two complete 28,644 KiB overwrite/readback passes, exercising garbage collection |
| 16 MiB incompressible memory pressure | Forward and reverse byte verification passed; NAND swap reached 14,572 KiB |
| ECC and bad blocks after testing and reset | Zero uncorrectable errors and zero bad blocks on both partitions |
| Existing configuration files across firmware update and reset | Hashes unchanged; contents stayed on NAND |

The memory-pressure test took 72.114 s, with 1,404 major page faults,
0.551 s user CPU and 11.661 s system CPU in the test process.
The file benchmark includes data generation and verification: process
CPU time was 0.874–0.928 s for write and 1.716–1.722 s for read. It
excludes background UBI work. PIO remains CPU-bound during reads; these
results do not imply DMA or wire-rate throughput.

Memory-pressure testing exposed both issues above. With 6 MiB zram,
incompressible data exhausted PSRAM before NAND swap was used. With
2 MiB zram and the original mtdswap layout, 16 MiB of test data reached
NAND but failed on read with `-EBADMSG`. A 512 KiB direct block write/read
reproduced that failure without memory pressure. Reserving marker pages
made the direct test pass with no new ECC failures. The final full-capacity
and memory-pressure tests confirm the reservation also works across
garbage collection. Reboot activated 2,044 KiB zram and 28,644 KiB NAND
swap, with zero uncorrectable errors. A new 64 KiB random file also
retained its hash after a synchronized reboot. The writable filesystem
partition is unaffected by the swap layout change.

After a flash, check the NAND upper mount, both swap priorities and the
MTD ECC/bad-block counters. Repeat incompressible write/fsync, cold read
and byte comparison, then apply memory pressure and verify the data
again. Reboot and recheck swap availability: this exercises the OOB
marker regression. Verify ordinary writable-file persistence and the
display and touch paths too. Never unbind GPSPI3 while UBI or NAND swap
is using it. Test programs, captures and migration archives stay in
ignored build output.

## Reboot to downloader

The [download-mode interface](internals.md#reboot-into-the-rom-downloader)
is checked with ordinary reboot, rejection of an invalid `next_mode`,
entry into ROM download, an esptool connection and successful return to
Linux. The UART captures are kept in ignored build output.

## AI button, vibration and sensors

Validation on 2026-10-29 with kernel `#34` captured an AI press and release
as `KEY_PROG1` values 1 and 0, about 200 ms apart. The GPIO7 interrupt count
increased from 0 to 2. A second press/release, about 240 ms apart, increased
the count to 4 and triggered the motor test. These checks exercise both
interrupt edges; long holds, rapid repeated presses and the newer image
still need verification.

Validation on 2026-10-28 loaded the Bosch initialization image and read
all six BMI270 motion axes and temperature. Both BMM150 devices probe
and return samples. **Known limitation:** address `0x12` repeatedly
returns `-2147483648` for Y. This is the driver's overflow sentinel,
not a usable magnetic-field measurement. The other five magnetic axes
return finite values. The source of that overflow has not been
determined, and compass accuracy is not validated. On 2026-10-29 the user
confirmed three separate vibrations from three 150 ms `FF_RUMBLE` effects.
The temporary test stopped each effect and closed the device after the
sequence; motor amplitude and current were not measured. A five-second
idle sample used approximately 0.6% CPU with the preview stopped and
input devices closed.

A stationary check on 2026-10-29 collected 20 sets of all 13 raw sensor
channels at 5 Hz without read errors. The BMI270 acceleration vector had
a mean magnitude of 9.83 m/s² using its reported scale; mean angular rates
were within 0.0025 rad/s of zero. A separate three-minute, 900-set capture
also had no read errors, but every `0x12` Y sample was the overflow
sentinel. Each set used sequential sysfs reads, taking 5 to 8 ms; these
are functional checks, not synchronized scans or a calibration.

A subsequent AI-triggered, 60-second capture recorded 300 sets during
user-operated forward/back and left/right tilts, horizontal X/Y movement,
and rotation about the gravity axis. All three accelerometer and gyroscope
axes responded, then settled when the device was put down. The final
five-second mean acceleration magnitude was 9.83 m/s². There were no read
errors. All three `0x11` magnetic axes and the `0x12` X/Z axes changed
during motion, but all 300 `0x12` Y readings remained overflow samples.
This validates motion response, not axis mounting, absolute accuracy or
compass heading.

The upstream Y compensation path returns this sentinel when the signed
raw Y register value is `-4096`. Section 4.6.6 of the Bosch
[BMM150 datasheet](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmm150-ds001.pdf)
defines that raw value as an X/Y measurement overflow. The software must
retain it as invalid data rather than scale it into a magnetic field.
The tested enclosure has rear permanent magnets, as confirmed by the
user. Their positions relative to the sensors stay fixed during whole-body
rotation. The valid magnetic readings also have a large fixed component:
the `0x12` X axis remained around -1040 to -1090 µT and Z around 580 to
640 µT during the motion test. Magnetic influence from the assembly is
therefore a plausible explanation for the saturation; the measurements
do not establish a faulty sensor or a compensation bug.

## Native Type-C USB

On 2026-10-29, kernel #39 and the updated rootfs passed a complete native
USB flash and boot cycle. CH343 remained the debug console at 115200 baud
with the same recorder PID throughout. After `reboot bootloader`, ROM
appeared as `303a:0020`; esptool reported USB-OTG and verified all seven
written regions. The flashing helper cleared FORCE_DOWNLOAD_BOOT and
triggered the watchdog on that same connection, returned success and
Linux booted without pressing RESET. NAND overlay and both swap devices
mounted normally.

Linux then automatically enumerated as `0525:a4a7` CDC ACM, with UDC
`configured` at high speed. A temporary raw echo process on
`/dev/ttyGS0` returned 262,144 bytes in each direction in 0.192 seconds;
all bytes matched, including all 256 byte values. This is a functional
loopback check, not a bulk-throughput benchmark. The echo process was
stopped after the test. Repeated init start, stop/start and restart also
restored enumeration without UDC start/stop warnings. Keyboard/mouse
hardware validation is still pending: the first device-to-host switch with
a mouse attached stopped responding without an Oops or panic in the UART
log. Removing the adapter did not restore the console; RESET did. The
role-switch path is under investigation and host support is not yet
validated.

The earlier two-process flash/reset helper verified flash but could fail
while reconnecting to the active USB stub, even though the board reset.
The current helper uses one connection through verification and reset;
it never resets after a failed write or verification. Host-side script
tests cover rejected USB identities, image parsing, overlapping erase
sectors and reset ordering.

Keep the external UART connected. V1.2 does not route HUSB320 role signals
to the SoC, so select the USB data role explicitly. The default is device
mode; HUSB320 controls the power direction independently.

For a keyboard or mouse, connect a Type-C OTG adapter and select host mode:

```sh
for role in /sys/class/usb_role/*/role; do echo host > "$role"; done
cat /proc/bus/input/devices
evtest
```

Require USB enumeration, a bound generic HID driver and actual keyboard
key-down/key-up or mouse motion/button events. The keyboard also feeds the
panel's `tty1` console. Mouse applications can use evdev or `/dev/input/mice`;
the text console does not provide a graphical pointer.

Before connecting a data-capable Type-C cable to a computer, select device
mode. The expected gadget is CDC ACM on `/dev/ttyGS0`:

```sh
for role in /sys/class/usb_role/*/role; do echo device > "$role"; done
for d in /sys/class/udc/*; do
    echo "$d: $(cat "$d/state")"
done
```

Require PC enumeration, `configured` UDC state and a bidirectional byte
comparison. Do not infer success from UDC probe alone. Check the kernel log
and verify that `reboot bootloader`, native USB flashing and return to Linux
work with USB enabled, while CH343 recording remains active. Verify the
ROM USB identity separately from Linux CDC ACM; a successful UART flash
does not validate this path. `S10usb stop` disconnects the gadget without
unloading the controller or changing its role.

### Build and runtime record

The 2026-10-28 USB/audio build passed compilation, zero-fuzz patch
application, new binding validation and strict checkpatch. The image was
flashed on 2026-10-29: kernels `#37` and `#38` register the DWC2 UDC and load
`g_serial` without the earlier FIFO warning. PC enumeration and data
transfer remain unverified. An earlier experimental USB role switch left
the board unresponsive; software UART and ROM probes could not recover it.

The subsequent dual-role/HID build, kernel `#39`, boots and loads DWC2,
`usbhid`, `hid_generic`, `mousedev` and `g_serial`. Its 4,681,556-byte kernel
and 9,203,712-byte squashfs fit the NOR partitions. All 31 kernel patches
apply at zero fuzz, the USB DT bindings and strict checkpatch pass, and
stripping the 53 installed modules preserves their allocated sections
and relocations. This image was written through CH343 before the flashing
workflow was corrected. Native USB flashing and host HID input still need
physical verification; the current UDC state is `not attached`.

The native USB flashing helper rejects CH343, Serial/JTAG, Linux gadget,
unidentified and unrelated USB ports before invoking esptool. Host-side
tests cover this rejection, ROM USB selection through a symlink and leaving
the downloader available after a failed write. These checks do not replace
an actual ROM USB flash followed by a Linux boot with continuous UART logs.

On 2026-10-29 the user reset the unresponsive board. Its retained boot
messages identify kernel `#34` from 2026-10-28 21:02:14 and a DWC2 WARNING
at 9.925 seconds: `insufficient fifo memory` during `g_serial` loading.
Linux remains responsive after that warning, with `panic_on_warn=0` and
`panic_on_oops=0`. This is not evidence of the earlier panic's cause; its
original exception was not captured. Continuous host UART recording is
now enabled.

A subsequent report of a trace remaining on the LCD was checked through
the external camera and `/dev/vcs1`. The screen's addresses and timestamps
matched that same boot-time USB warning. UART commands still completed,
and a fresh `dmesg` contained no newer Oops or panic.

## Audio

Discover the card and controls before setting gain or starting recording:

```sh
cat /proc/asound/cards
arecord -l
aplay -l
amixer -c 0 scontrols
```

A capture-only transport check, once the card has probed, is:

```sh
arecord -D hw:0,0 -t raw -f S16_LE -c 2 -r 16000 -d 3 /tmp/microphone.raw
wc -c /tmp/microphone.raw
```

The expected payload is 192000 bytes. File size alone does not validate the
microphone signal: also check samples, elapsed time, DMA errors and xruns.
Input routing and gains depend on the enumerated codec controls. Speaker
testing requires a physical user key press. No boot service plays sound.

### Speaker bring-up (2026-10-29)

On kernel `#37`, the temporary AI-key test submitted 8000 stereo S16_LE
frames at 16 kHz and drained in 0.513 s without a DMA error. The user
reported no sound after two key presses; the one-shot program logged only
the first playback. Successful DMA completion does not establish audible
output.

Silent register probes found that the generic amplifier's unused right
output remained a DAPM endpoint, keeping GPIO45 high even with `Speaker
Switch` disabled. Kernel `#38` routes both logical amplifier paths through
the controlled mono speaker endpoint. With the switch off, GPIO45 now
reads low during and after an 8000-frame all-zero DMA check (0.512 s).
Pad sampling observed BCLK, WS and MCLK transitions; changing the GPIO
output-enable bits did not change them, so no pinctrl change was made.
Codec register readback and these pad samples do not validate the analog
output or all I2S timing requirements.

The revised temporary program waits for a fresh AI press, streams one
second of zeros with the speaker disabled to establish clocks, then plays
one 0.5 s, 440 Hz stereo tone with short amplitude ramps. It disables the
speaker afterward and waits for another press. Its volume follows the
[official codec library's default curve](https://github.com/espressif/esp-adf/blob/release/v2.x/components/esp_codec_dev/esp_codec_dev.c):
50% maps to -25 dB, with the BSP's 3.3 V / 5 V hardware-gain compensation
giving ES8311 DAC register value 148 (-21.5 dB). The initial test used raw
128 (-31.5 dB), which is half the register range but differs from BSP 50%.
The user confirmed audible, quiet output on this setup. They noted that the
initial test might also have been audible at low volume, so its reported
silence is not a confirmed amplifier failure. The program was stopped after
confirmation; the speaker switch is off and GPIO45 reads low.

## Battery gauge

**Known issue (2026-10-28): BQ27220 does not respond at I2C address
`0x55` on the tested CoreBoard V1.2.** The internal battery is connected
and USB power is attached; the user reports that the battery is charging.
The cause of the missing response has not been established.

The official [BSP battery initialization](https://github.com/esp-mosaico/esp-mosaico-bsp/blob/7cbfb390638dcb93522fc3cde094b2d43cf907c2/components/esp-mosaico-bsp/onboard/battery.c)
uses the shared 400 kHz I2C bus on GPIO56 (SDA) and GPIO3 (SCL). Its
[BQ27220 component](https://github.com/espressif/esp-iot-solution/blob/1608d9c922c992986c285fd753ef15ecbbad3cde/components/sensors/battery_fuel_gauge/bq27220/bq27220.c)
first requests the device number: write little-endian `0x0001` to control
register `0x00`, wait 15 ms, then read two bytes from MAC data register
`0x40`. The expected device number is `0x0220`.

The equivalent Linux transactions were repeated on the board:

```sh
i2ctransfer -y 0 w3@0x55 0x00 0x01 0x00
usleep 15000
i2ctransfer -y 0 w1@0x55 0x40 r2
```

Both the initial command write and the ID read return `Operation timed
out`. A direct two-byte voltage read from register `0x08` also times out.
On the same bus, control reads return ES8311 ID bytes `0x83`, `0x11` and
BMI270 chip ID `0x24`. Those successful reads confirm that the shared bus
can communicate with other devices; they do not establish why the gauge
fails.

No BQ27220 device-tree node, driver extension or automatic probe is
enabled. The gauge was not unsealed, reset or given a battery profile.
The BSP's 80 mAh profile is not applied to this board's 65 mAh battery.
Revisit support only after device-number reads succeed on the hardware.

## Camera

Validation on 2026-10-28 used a temporary V4L2 MMAP capture program on the
CoreBoard V1.2 and CameraBoard V1.4. A 60-frame run had no error frames;
a 300-frame run had one initial short frame and then 300 complete frames
without sequence gaps, at approximately 15 fps. A saved UYVY frame was
converted and visually checked. Unbinding the module driver during
capture woke the application with a poll error; capture while absent
failed with `ENOLINK`, and rebinding restored a 60-frame capture. These
checks exercise sensor removal and recreation in software.

The user also physically unplugged and reinserted the left module while a
temporary preview with reconnection handling was running, and confirmed
that the live image recovered automatically. That application check does
not establish repeated-cycle reliability or electrical behavior during
connector insertion. Boot with the module absent and DMA-BUF import remain
unverified; the completed capture tests used MMAP buffers.

## Local evidence

Raw evidence is intentionally untracked. In the bring-up workspace,
`build/display-debug/` contains display measurements and images,
`build/resource-audit/` contains storage regressions,
`build/reboot-audit/` contains downloader tests, and `build/camera-audit/`
contains capture and reconnection evidence. `build/peripheral-audit/`
contains the input and sensor runs; `triggered-motion.csv` and
`triggered-summary.json` describe the actual 300-set movement capture.
The earlier 900-set run was not the requested movement sequence.
`build/panic-audit/20261029-091238/` contains the screen/virtual-console
comparison and retained kernel log.

These local paths are evidence locations, not committed test fixtures or
required build inputs. New runs should keep their own build IDs and results
rather than overwrite the meaning of the earlier records.

# Runtime and build options for review

This is a fork-only decision list, not a change to the default service
policy. The three stacks retain their tested defaults. New applications
have no init script and are started explicitly.

| Component | Current behavior | Possible runtime switch | Build-time option and likely benefit |
| --- | --- | --- | --- |
| LVGL test interface | Stopped at boot; `mosaico-test` runs in the foreground. Test resources close on page exit. | Start/exit the application; choose individual test pages. | Omit `BR2_PACKAGE_MOSAICO_TEST` to save executable/license space. No idle CPU benefit when already stopped. |
| Camera preview | Stopped at boot; capture buffers and DMA are allocated only during a preview. | Stop preview and release its queue. | Omit `BR2_PACKAGE_CAMERA_PREVIEW` for image space; omit the camera driver family for a camera-free image. |
| Camera module detection | S08camera loads modules; an in-kernel delayed worker polls the left module EEPROM every 250 ms. | Add an explicit load/unload or discovery-enable switch. Current `S08camera stop` does not unload anything. Disabling discovery loses automatic detection until re-enabled. | Disable camera/sensor/AXI-DMA options together to reduce kernel/module space. Loading less saves resident memory; compiling out already-unloaded code mainly saves flash. |
| BMI270 and BMM150 | S09peripherals loads IIO modules. Reads are requested by userspace; upstream drivers contain runtime PM support. | Add module loading on demand and dependency-aware unloading. Verify actual runtime PM state before claiming a power saving. | Omit the corresponding IIO modules and BMI270 firmware. |
| AI key and vibrator | Modules load at boot. AI is interrupt-driven; force-feedback effects run only on request. | Keep AI available for control; load/unload the vibrator separately if desired. Current init-script stop is a no-op. | Remove either driver for a narrower hardware profile. Little idle CPU work is expected from an unused interrupt-driven key. |
| Audio | S12audio loads ALSA, I2S/AHB-DMA, codec, card and amplifier modules. No PCM starts at boot; DAPM controls the amplifier. | Load on demand and unload once all streams close. Keep amplifier-off cleanup. Current `S12audio stop` does not unload modules. | Omit the ALSA/codec/I2S stack and utilities to save image space and loaded module memory. |
| USB | S10usb loads DWC2, gadget serial and HID modules. Data role is manual; default device role provides CDC. | Select device/host/none; a gadget stop only disconnects it and retains modules. A real unload switch must coordinate gadget/HID users first. | Device-only or host-only DWC2 configurations can reduce code, at the cost of the other runtime role. ROM USB programming is provided by ROM, not Linux gadget code. |
| Display and touch | Built-in framebuffer/text console; dirty-region transfers. Touch polls at 20 ms only while an input consumer is open. | Backlight/display blanking and input-consumer lifetime; an optional larger touch poll interval trades latency for work. | A display-free profile can remove FB/VT/touch and framebuffer allocation. It also loses the LCD console and GUI. The minimal BSP already demonstrates such a profile. |
| Wi-Fi | wpa_supplicant, wpa_cli and DHCP provide networking used for SSH and transfers. | Stop/restart Wi-Fi explicitly when network access is dispensable. | A radio/network-free image saves code, but is a different product configuration. Do not disable it during remote testing. |
| Bluetooth | dbus/bluetoothd start lazily through bluetoothctl; a stored pairing can start them at boot. The GUI's explicit controller check uses this existing path. | Stop Bluetooth services when no other client needs them, or decide whether paired-device autostart is desired. | Omit BlueZ utilities; disabling the resident radio service is a separate loader/kernel protocol choice. |
| Logging and cron | Standard syslogd/klogd and crond follow the existing image policy. | Individually stop/restart if no consumer needs them. | Remove corresponding userspace features for a reduced image. Preserve useful crash evidence. |
| NAND, overlay and swap | UBI/UBIFS provide writable files; zram precedes NAND swap. Their kernel workers service storage. | Tune swap policy only with measurements. Do not kill storage workers. | These are required for this storage layout; removing them is not an application optimization. |
| Host UART recorder | Separate host service, bounded to 15 MB, CH343 debug only. | Keep running during development. | Not part of the target image and consumes no target application RAM. |

No memory or frame-rate saving is promised from these candidates without
an A/B measurement. Idle modules, active kernel polling and userspace
daemons have different costs; a kernel worker name alone does not identify
the subsystem doing work.

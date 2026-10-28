# ESP32-S31 internals for Linux developers

This document tells how Linux runs on the ESP32-S31, and why the port has
its current form. It is for developers who know the RISC-V Linux kernel and
want to understand this SoC. Unless another board is named, facts are from
chip revision v0.0 on the ESP32-S31-Korvo-1 board. JTAG measurements gave
most of them, because Espressif does not publish a technical reference
manual for this chip yet.
The port has four builds: `esp32s31` for the ESP32-S31-WROOM-3-N16R16V
module alone, `coreboard1` for the ESP32-S31-Function-CoreBoard-1,
`korvo1` for the Korvo-1, and `esp-mosaico` for the ESP-Mosaico CoreBoard
V1.2.

The ESP-IDF register headers (`components/soc/esp32s31/register/soc/`) are
the best public reference for addresses and fields.

## The SoC from the view of Linux

| Item | Value |
| --- | --- |
| Harts | 2 × RV32 at 320 MHz: `rv32imafc` + Zba/Zbb/Zbs, Zc*, vendor extensions. The kernel is compiled with Zba/Zbb/Zbs (`patches/0020`). |
| Privilege modes | M, S, U. Sv32 MMU. |
| FPU | F only, no D. Linux supports an FPU only with D, so the FPU stays off. Floating point works in software (soft-float userspace), only slower. See "The FPU". |
| I-cache | 32 KiB for each hart, 2-way, 64-byte lines: 256 sets, address bits 13:6 select the set. See "The instruction cache". |
| D-cache | 64 KiB for the two harts, 2-way, 64-byte lines |
| Cache maintenance | No Zicbom. A cache controller with a "sync engine" (MMIO). |
| Interrupts | CLIC only. The standard `sie`/`sip` CSRs do not exist. |
| Internal SRAM | 512 KiB at `0x2F000000`. The harts access it without the I-cache and the D-cache. |
| PSRAM | 16 MiB octal, cached. The loader maps it at `0x50000000` with the MMU of the PSRAM controller. Linux uses it as RAM. An uncached alias is at `0xC0000000`. |
| Flash | 16 MB NOR, mapped by the MMU of the flash controller |

## Boot chain

```text
ROM -> ESP-IDF 2nd stage -> loader (hart 0) -> OpenSBI (hart 1, M) -> Linux (hart 1, S)
```

1. The ESP-IDF second-stage bootloader starts the loader application
   (`bootloader/main/`) on hart 0.
2. The loader starts PSRAM, and on the Korvo-1 the LCD
   (`board_korvo1.c`). It copies two flash partitions
   to PSRAM and checks them: the kernel `Image` (size and CRC32 from a
   manifest at the end of the kernel partition) and OpenSBI `fw_jump`. It
   starts Wi-Fi and Bluetooth.
3. The loader writes the D-cache back, sets the PMA of hart 1 for PSRAM and
   releases hart 1 at the OpenSBI entry. Then it locks hart 0 out of PSRAM
   and the DMA pool with PMP.
4. Hart 0 stays in the loader. The loader is now the ESP-Hosted Wi-Fi
   firmware.
5. OpenSBI sets up the CLIC, the timer, PMP, APM and the SBI extensions.
   Then it jumps to the kernel in S-mode.
6. The kernel has a built-in DTB (`esp32s31.dts`,
   `esp32s31_coreboard1.dts`, `esp32s31_korvo1.dts` or
   `esp32s31_esp_mosaico.dts`). On the Korvo-1, the root is the ext4
   partition `/dev/mmcblk0p2` on the SD card, on the
   Function-CoreBoard-1 p2 of the USB stick with the disk signature
   `0x45533331` (`root=PARTUUID=45533331-02` and `rootwait`). On the
   esp32s31 and esp-mosaico builds, the root is the squashfs in the flash,
   with a UBIFS overlay.

### Memory map

| Address | Size | Use |
| --- | --- | --- |
| `0x2F040000` | 56 KiB | Coherent DMA pool for Linux (`shared-dma-pool`) |
| `0x2F04E000` | 8 KiB | Trap code of Linux (`CONFIG_RISCV_TRAP_SRAM`), see "The instruction cache" |
| `0x2F050000` | 52 KiB | Shared block: requests, flash mailbox, ESP-Hosted rings |
| `0x2F079000` | 4 KiB | LCD DMA descriptor ring (Korvo-1) |
| `0x50000000` | 15 MiB - 4 KiB | Linux RAM, from the kernel image |
| `0x50EFF000` | 4 KiB | Guard page below OpenSBI (reserved, no-map, see Protection) |
| `0x50F00000` | 192 KiB | OpenSBI (reserved, no-map): 128 KiB code, 64 KiB data, heap and stack |
| `0x50F30000` | 64 KiB | Linux RAM |
| `0x50F40000` | 768 KiB | LCD frame buffer (Korvo-1: reserved, no-map, uncached; other boards: Linux RAM) |

The loader (`main.c`, `display.c`) and OpenSBI (`platform.c`) keep their
own copies of these numbers. The device trees must agree with them.

### Internal SRAM

The HP SRAM is 512 KiB (`0x2F000000`-`0x2F080000`). Measured on the
Korvo-1 with Wi-Fi and Bluetooth, at v0.4.0 (the numbers change with the
firmware):

| Use | Size |
| --- | --- |
| Loader and ESP-Hosted firmware, static (`.text`, `.data`, `.bss`) | 165 KiB |
| Linux: DMA pool, trap code and shared block (`0x2F040000`-`0x2F05D000`) | 116 KiB |
| LCD descriptor ring and reserved blocks | 8 KiB |
| ESP-IDF heap in HP SRAM | 211 KiB |

The heap also has 32 KiB of LP SRAM. Of the controllers, Bluetooth takes the
most: 42 KiB of code in SRAM and 61 KiB of heap. 100 KiB of heap are free
when hart 1 starts. With 32 dynamic Wi-Fi buffers in each direction, the
minimum was 35.9 KiB during iperf3 with a BLE scan and a BR/EDR inquiry.

## Why Linux uses only hart 1

The Wi-Fi and Bluetooth radios need the ESP-IDF libraries. These libraries
are binary, run in M-mode and use FreeRTOS. So hart 0 keeps a small
ESP-IDF application that owns the radio, the ESP-Hosted firmware, and
Linux talks to it through shared SRAM (see "Wi-Fi"). The price is a
uniprocessor Linux.

## Interrupts: the CLIC

The hart has only a CLIC (Core-Local Interrupt Controller). There is no
PLIC, and the S-mode interrupt CSRs (`sie`, `sip`) are illegal
instructions. The Interrupt Matrix connects the SoC interrupt sources to
the 32 external CLIC inputs (IDs 16 to 47).

Each hart sees its own CLIC registers at the same address. The registers of
the other hart are at `+0x10000`.

| Window | Address |
| --- | --- |
| M-mode configuration (`cliccfg`) | `0x10800000` |
| M-mode per-input registers (IP, IE, ATTR, CTL: 4 bytes per ID) | `0x10801000` |
| S-mode configuration | `0x10A00000` |
| S-mode per-input registers | `0x10A01000` |

The traps that cost the most time:

- **An input is visible to S-mode only when M-mode gives it.** The MODE
  field of `clicintattr` must be S. Otherwise the S-mode window reads 0 and
  ignores writes, with no fault. OpenSBI gives inputs 16 to 47 to S-mode.
- **`cliccfg` is per hart.** Write the full value. The Linux hart does not
  run the ESP-IDF start code. The measured layout is `mnlbits` [3:0],
  `nmbits` [5:4], `snlbits` [19:16], `unlbits` [27:24]. The ESP-IDF header
  `clic_reg.h` shows an older layout. OpenSBI writes `0x23`: three machine
  level bits, `nmbits` = 2 (the MODE field selects M, S or U), and no
  supervisor level bits, so all S-mode inputs have level 255.
- **`sret` restores the interrupt level from `scause.spil`, but only when
  `scause` bit 31 is set.** With an exception-type `scause`, the level stays
  where it is. `mret` from S-mode does not trap on this revision. It changes
  only the machine level and leaves `sintstatus.SIL` high, which masks all
  S-mode interrupts.
- **The level stack and the scheduler.** Only `sret` lowers the level. If a
  handler schedules another task while its level is still high, that task
  runs with interrupts masked. The stock `handle_exception` (with the hunks
  of `linux/patches/0001`) does an `sret` to itself before `do_irq()`, with
  `spil` = 0, so that all handlers run at the base level.
- **`scause` holds SPP and SPIE.** In CLIC mode, these bits of `scause` are
  aliases of the same bits in `sstatus`. A write of `scause` changes them:
  with 0, `sret` returns in user mode.
- **Exceptions come to the same vector.** In CLIC mode, `stvec` catches
  exceptions and interrupts, so `handle_exception` is 64-byte aligned. It
  masks the CLIC bits out of `scause` before the usual exception handlers.

Driver: `drivers/irqchip/irq-esp32s31-clic.c`. Interrupt specifiers have
three cells: `<CLIC ID, matrix source, type>`. The driver writes the
Interrupt Matrix route itself.

### Timer and IPI

The machine timer is CLINT-like at `0x10000000` (`mtime` at `+0xBFF8`,
`mtimecmp` at `+0x4000`). A match is CLIC input 7 (M-mode). OpenSBI takes
it and sets input 5, which is the S-mode timer ID. `MIP.STIP` and
`MIP.SSIP` are not writable, so OpenSBI uses platform hooks for them
(`opensbi/patches/0005`, `0006`).

The `time` CSR is in the hardware. A read from user mode takes approximately
2 CPU cycles and does not trap. `mtime` has the same rate when the hart is
idle and when it is busy: -17 ppm and +10 ppm against a host clock, with a
resolution of approximately 30 ppm. It also counts in WFI when
`CPU_WAIT_MODE_FORCE_ON` (`0x20586118`, bit 0) is cleared.

A uniprocessor kernel still needs a self-interrupt for `irq_work`. Without
one, the work waits for the next tick, and with `NO_HZ_IDLE` the kernel can
deadlock when the only task that can run waits for the work (Tiny SRCU
does this when it replaces the boot console). CLIC input 1 accepts IP
writes but does not arbitrate on this revision. So the driver uses external
input 16 as a software interrupt (`patches/0005`).

## Cache and DMA

This is the part that is most different from other RISC-V SoCs.

- **No coherent bus master.** SD, USB, LCD and the GDMA do not snoop the
  D-cache.
- **No Zicbom, and Sv32 has no cacheability bits.** So Linux cannot make a
  usual mapping uncached. The hardware has two other ways. PSRAM has a
  cache-bypass alias at `0xC0000000` (the address plus `0x70000000`). It
  needs no PMA entry and works from S-mode, and the port does not use it.
  And bit 27 of a PMA cfg makes a range uncached at its usual address. The
  PMA entry with the lowest index has priority. The port uses this for the
  frame buffer. An uncached word read takes approximately 120 CPU cycles,
  and a write approximately 50.
- **Two rules for uncached memory.** A store through one address of a word
  is visible to a load through its other address only after a `fence`. And
  when a range becomes uncached, its lines must not be in the cache: the
  write-back of an old dirty line overwrites newer uncached data. The loader
  writes back and invalidates the full cache before the handoff.
- **The cache controller has a sync engine.** It writes back or invalidates
  an address range, one operation at a time, through global registers. Two
  masters that use it at the same time corrupt each other. OpenSBI programs
  it for Linux. The firmware on hart 0 also uses it, through the ROM, when
  ESP-IDF invalidates the flash window after a program or erase for Linux.
  So the two take a lock word in the shared block for each operation
  (`ESP_SHM_CACHE_LOCK_OFF`); the firmware wraps the two ROM functions with
  `--wrap`. The lock covers only the short engine operation, never the
  program or erase. Linux asks through a vendor SBI extension
  (`0x09000612`, functions: write back all, invalidate, write back,
  write back and invalidate, I-cache sync). See
  `drivers/cache/esp32s31-cache.c`. An
  erratum: a write-back can lose part of the range, so OpenSBI runs it
  twice, as the ESP-IDF ROM patch does. A test of 360 write-backs did not
  show the loss, but ESP-IDF marks it as necessary for this chip.
- **Coherent allocations come from SRAM.** The CPU accesses SRAM without
  the D-cache. The 56 KiB `shared-dma-pool` at `0x2F040000` is the default
  for `dma_alloc_coherent()`.
- **Streaming DMA uses the sync engine** through
  `riscv_noncoherent_register_cache_ops()`. `setup_arch()` enables
  non-coherent DMA early (`patches/0006`), because slab gets its alignment
  from `dma_get_cache_alignment()` before the initcalls.
- **The I-cache refills from PSRAM, not from the D-cache, and `fence.i` does
  not invalidate it.** New code (module load, `exec`, JIT) can be in a dirty
  D-cache line. The I-cache also keeps the old line until other code replaces
  it. Measured from user space: a write, a flush with `fence.i` and a call of
  the same address gave the old code in 292 of 300 runs. So
  `local_flush_icache_all()` asks OpenSBI to write the D-cache back and to
  invalidate the I-cache of the hart (`patches/0009`, vendor SBI function 4).
  For a new user code page, `flush_icache_pte()` gives only the range of the
  folio (same patch). A test that changes the code in a file page, maps it
  again and calls it gets the new code. Without a new mapping (no sync), it
  gets the old code.
- **The frame buffer is uncached.** PSRAM is write-back. The LCD DMA reads
  the frame buffer from PSRAM with no cache maintenance, so OpenSBI makes
  that range uncached with two PMA entries. Write-through for all of PSRAM
  (bit 26 of the PMA cfg) also gives a correct picture, but then Linux is 2
  to 10 times slower.

### Drivers that needed a change for this

- **SD (`dw_mmc`).** The PIO FIFO port does not work on this silicon. The
  driver must use the internal DMA (IDMAC). Its descriptor ring comes from
  the coherent DMA pool in internal SRAM, so the stock driver works.
- **USB (DWC2).** Buffer DMA works through the sync engine. Descriptor DMA
  stays off. The reset FIFO sizes are larger than the 896-word FIFO RAM, so
  the driver sets smaller ones (`patches/0004`). The DWC2 register window
  must include the FIFO windows at `0x1000 × (channel + 1)`.

## The instruction cache

Each hart has a private I-cache of 32 KiB: 2 ways of 16 KiB, 64-byte lines,
thus 256 sets. Address bits 13:6 select the set. Three sources agree: the
datasheet (section 4.1.3.4), `MAX_L1_ICACHE_SIZE` in the ROM header
`rom/cache.h`, and the reset value of `L1_ICACHE_CACHESIZE_CONF` (32K).
A measurement on hart 1 also agrees (below). The ESP-IDF option
`CONFIG_CACHE_L1_ICACHE_SIZE` gives 16 KiB, which is wrong for this chip.

- **Three lines in one set.** The cache has only 2 ways. When three lines
  that a path runs each time have the same set, they evict each other each
  time. A syscall uses approximately 24 lines in 5 places of the kernel
  (`arch/`, `kernel/`, `lib/`, the noinstr text and the entry code). A small
  change elsewhere in the kernel moves these places against each other.
  In one build, `handle_exception+0x80`, `task_active_pid_ns` and the
  out-of-line `arch_exit_to_user_mode_prepare` had set 15: a `getpid()`
  took 2.6 µs instead of 1.2 µs, with 3.2 line fills for each call instead
  of 0.08. A refill from PSRAM takes approximately 450 ns.
- **The counters.** The cache controller counts hits, misses and fills
  for each bus (`0x2C000180`: enable and clear, `0x2C000194` and up: IBUS1,
  the I-cache of hart 1; `0x2C0001D8` and up: DBUS1). S-mode can read them.
  OpenSBI or `/dev/mem` can sample them without a change to the kernel image.
- **SRAM.** The harts fetch internal SRAM without the I-cache, as fast as a
  cache hit: 229 ns for a block of 61 instructions from PSRAM (a hit) and
  from SRAM. Code in SRAM uses no line of the I-cache. A fetch loop of hart 1
  in SRAM did not slow the Wi-Fi of hart 0 (iperf3 RX 35.0 against 34.6
  Mbit/s with the same loop in PSRAM).
- **The trap code in SRAM** (`CONFIG_RISCV_TRAP_SRAM`, `patches/0018`). The
  entry, irqentry, noinstr and cpuidle text (4.6 KiB) is linked for the
  virtual address `0xFFC00000` (the last PGD entry) and loaded in the image
  before `.text`. `setup_vm()` copies it to `0x2F04E000` with the MMU off
  and maps it in `early_pg_dir`; `setup_vm_final()` maps it in
  `swapper_pg_dir` and frees the load pages. Thus the trap path does not
  depend on the layout of the kernel. With all other code at the same
  address, a pipe round trip between two processes took 136 µs, against
  174 µs with the trap code in PSRAM. Kallsyms does not cover this range.

## Page blocks

Without huge pages, the page block order is `PAGE_BLOCK_MAX_ORDER`, 10 by
default: 4 MiB blocks, and 16 MiB has only 4. Page cache, slab and stacks
then share 2 or 3 blocks, and each migrate-type fallback counts the free
pages of a whole block, 1024 `struct page`. During a disk read, this took
45% of the CPU time. The port uses order 3 (32 KiB, `PAGE_ALLOC_COSTLY_ORDER`):
the disk read went from 9 to 19 MB/s.

## The FPU

The FPU has F (single precision) but not D. A kernel patch can save the F
registers and let user programs use F: a single-precision benchmark ran 37
times faster. But the userspace does almost no single-precision math. A
scan of all programs and libraries of the rootfs found calls to the soft-float
helpers for single precision only in `libgcc_s` itself and 36 in
`libstdc++`. Lua, MicroPython and libm use double precision, which F does
not help. Thus the FPU stays off.

## Protection

- **PMP.** OpenSBI gives its memory (`0x50F00000`, 192 KiB) no S/U access.
  PMP entries must not overlap on this CPU, even for a short time.
- **APM.** `HP_APM` checks the accesses of the bus masters, also DMA to
  PSRAM. M-mode is TEE mode, and the DMA masters are REE mode. OpenSBI
  gives its memory to TEE mode only and locks the regions. A blocked access
  returns 0, and the APM records it, with no bus error. PMP does not cover the
  uncached alias of the OpenSBI memory (`0xC0F00000`). There, an S-mode
  read also returns 0 and a write is lost. A read that hits the D-cache is
  not checked: the APM sees only bus accesses, so PMP must protect the
  cached addresses. `CPU_APM` checks only CPU accesses, and `HP_MEM_APM`
  covers only SRAM.
- **The word below an APM region.** `HP_APM` puts the last word below the
  start of a region into that region. Thus `0x50EFFFFC` is TEE only: a DMA
  write to it is lost, and an S-mode cache line fill gets wrong data for
  it. The device tree reserves the page `0x50EFF000` as no-map, so Linux
  never uses it. In the same way, `0x50F2FFFC` is in region 2, and DMA can
  write it. Thus OpenSBI must end 4 bytes below `0x50F30000`.
  `esp32s31_early_init()` checks this.
- **`TEE_SUPERVISOR_PRIV_SEL`** (`0x20504200`) is 0 after the boot, and
  S-mode runs with 0. When it is set for a hart, the S-mode accesses of that
  hart use the M-mode (TEE) path, and the APM protection above is gone.
- **The APM registers themselves** (`0x20504000`) are M-mode only, through
  an OpenSBI domain region.
- **Hart 0.** It runs ESP-IDF in M-mode, where only a locked PMP entry
  applies. After the loader starts hart 1, it locks hart 0 out of PSRAM,
  of the uncached PSRAM alias and of its SRAM window (PMP entries 13 to 15, no
  access). The lock stays until the next reset. Hart 0 can still write the
  rest of SRAM and all peripherals.

## Radios

The radios are ESP-Hosted (`external/esp-hosted-linux`). The loader includes
the ESP-Hosted firmware (`bootloader/components/esp_hosted/`). ESP-Hosted
supplies its own ESP32-S31 Wi-Fi libraries with the full-MAC hooks
(`esp/esp_driver/lib/esp32s31`), and the component links these in place of
the libraries of ESP-IDF. The kernel has the ESP-Hosted host driver
`esp32_shm` built in, a cfg80211 full-MAC driver. A Buildroot linux
extension (`br2-external/linux/`) copies it from the submodule into
`drivers/net/wireless/espressif/esp_hosted`, next to the shared-memory
transport (`linux/drivers/net/wireless/espressif/esp_hosted/esp_shm.c`).
The firmware runs the 802.11 MAC. `wpa_supplicant` and `hostapd` on Linux
run the authentication and the key handshakes. ESP-Hosted has no
transport for a host on the same chip, so the two sides use the frames of
the USB transport in shared SRAM. The layout is in
`shared/esp32s31-hosted-shm.h`, which the loader and the driver include.

- Two byte rings in SRAM at `0x2F050000`: 16 KiB to the firmware and 32 KiB
  to Linux. Each record is a 32-bit length and one ESP-Hosted frame. Each
  ring has one producer and one consumer, so no lock is necessary. The harts
  access SRAM without the D-cache.
- Two cross-core interrupts are the doorbells. Linux sets `FROM_CPU_1`, and
  the firmware sets `FROM_CPU_2`. `FROM_CPU_0` is not usable: the ESP-IDF
  cross-core handler on hart 0 clears it.
- A request word in the control block carries the requests to the
  firmware: a new boot event from Linux, and poweroff from OpenSBI. At
  poweroff, the firmware stops Wi-Fi and puts the chip into deep sleep
  without a wake-up source.
- A producer rings only when the consumer has read all frames, because only
  then the consumer can wait. Each side stores its index, then does a full
  fence, then loads the index of the other side. Thus one of the two sides
  always sees the other, and no frame waits for a doorbell that does not
  come. At full load, Linux gets one interrupt for approximately 5 frames.
- Linux writes a frame into the ring in `ndo_start_xmit`, and uses a worker
  only when the ring is full. When 64 frames wait, it stops the net queue.
  At full iperf3 TCP load, Linux has almost no idle time: the copy from
  SRAM, the RX worker and the network stack use the hart. The release
  notes give the throughput of each release.
- The host driver pads the payload header (offset 12 to 15 bytes) so that
  the header starts aligned in the headroom
  (`br2-external/package/esp-hosted-linux/0008-*`).
  Before, it copied each TX frame into a new skb: 15% of the CPU time.
- GRO through `gro_cells` gave no gain: the RX work gives one frame at a
  time, and each `local_bh_enable()` runs the poll for that frame only.
- A restart of the firmware also restarts Linux. Thus when the host driver
  opens or closes the data path, the firmware only sends its boot event
  again. Over USB, close restarts the firmware. Here the firmware keeps its
  Wi-Fi state. The fatal error paths of the firmware still call
  `esp_restart()`, which restarts the full chip.
- When Wi-Fi stops, the firmware drops the events that are still in its
  event loop. The host waited 5 s for such a disconnect event and then asked
  for a firmware restart. A patch of the host driver stops that wait when
  `DEINIT` succeeds (`br2-external/package/esp-hosted-linux/0003-*`).
- The loader starts Bluetooth before hart 1, as Wi-Fi: a radio start can
  access the flash. Linux then asks for the same service
  (`esp32_shm.radio_service=bt`), which changes nothing.
- The port does not use the IEEE 802.15.4 radio. Its RCP took SRAM from
  the Wi-Fi buffers, and its coexistence scheme halved the Wi-Fi
  throughput.
- The Bluetooth controller reports "BR/EDR Not Supported" in its LMP
  features, although it has BR/EDR. A host patch clears the bit
  (`0006-*`).
- The ESP-Hosted Wi-Fi libraries need `rom.patch` of ESP-Hosted in ESP-IDF.
  Without it, the ROM handles the received EAPOL frames, and the 4-way
  handshake fails. `scripts/patch-esp-idf.sh` applies it as a local
  commit, with the fixes from newer ESP-IDF in `bootloader/idf-patches`.
  The backports are only fixes that do not need newer Wi-Fi or Bluetooth
  libraries. The others wait until ESP-Hosted moves to a newer ESP-IDF.
- With WPA3-SAE and protected management frames, the first connection
  after a reset can fail. The access point does not answer the first SAE
  commit, and after approximately 4 s the closed Wi-Fi library on hart 0
  stops (`auth timeout`). The second try connects. After `reboot`,
  `wpa_supplicant` sends a deauthentication, and the first try connects.
  Thus the access point probably keeps the protected association of the
  board until its SA Query fails.

## Display and touch

### CO5300 QSPI display

On ESP-Mosaico CoreBoard V1.2, Linux drives the 480x480 CO5300 panel through
GPSPI2. The drivers are `linux/drivers/spi/spi-esp32s31.c` and
`linux/drivers/video/fbdev/co5300fb.c`. The framebuffer is native-endian
RGB565; the display driver sends each pixel's high byte first. Commands
use one wire and pixel data uses four. The board assigns an 80 MHz SPI
core clock and requests 40 MHz QSPI. The board BSP also uses 40 MHz QSPI.

The display driver tracks dirty pairs of scan lines and combines up to
eight adjacent lines per transfer. A blinking console cursor therefore
does not resend the entire frame. Writes, fbdev drawing operations and
deferred mmap updates all record damage. Transfers use PIO and a staging
buffer, with a 20 ms deferred update delay.

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

### CST9220 touch

The CST9220 driver is `linux/drivers/input/touchscreen/cst9220.c`. On
CoreBoard V1.2 it uses the shared 400 kHz I2C bus, address `0x5a`, and
reports coordinates from 0 to 479 on both axes through evdev. GPIO6 is the
interrupt line. The touch driver retains its validated polling path at a
requested interval of 20 ms while open; the AI button's GPIO IRQ support
does not change the touch report protocol or timing.

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

## The flash root without a card

The ESP32-S31-WROOM-3-N16R16V alone has no card. Linux has no driver for the
SPI flash, and it must not have one: hart 0 runs its code from the same
flash, through the cache. So hart 0 maps the flash for Linux and writes it
for Linux, through a mailbox in the shared block (`ESP_SHM_FLASH`), and
Linux sees a standard MTD device.

- **The window.** The firmware gives Linux the `rootfs` partition and, if
  present, the adjacent `data` partition, and nothing else. Each board's
  device tree describes the same layout as `fixed-partitions` under the
  `flash` node. ESP-Mosaico exposes only its read-only NOR rootfs.
- **Reads.** At start, the firmware maps the window in the flash cache
  (`spi_flash_mmap`) and writes the address to the mailbox. Linux maps it
  write-back and reads it with `memcpy`, without hart 0: 15.5 MB/s. A read
  through the mailbox gave 3.9 MB/s, because it needed two cross-core
  interrupts and a stall for each 2 KiB.
- **Programs and erases.** Program (at most 2 KiB, in one 4 KiB sector)
  and erase (one 64 KiB block). Linux writes the address, the length, the
  data and then the command, and rings. The firmware (`shm_flash.c`) writes
  the status, clears the command and rings back. The driver
  (`esp_shm_flash.c`) sleeps until then.
- **Flash and cache.** Without flash auto-suspend, a program or erase of
  ESP-IDF freezes the cache of the two harts, and Linux runs from PSRAM
  through that cache. With `CONFIG_SPI_FLASH_AUTO_SUSPEND`, the cache stays
  on, and each cache miss to the flash suspends the operation. Linux runs
  on, and the firmware erases a 64 KiB block in one operation.
- **Bus timeouts.** A cache miss of hart 1 to PSRAM can wait behind the
  suspends longer than the core bus timeout (`HP_SYSTEM_CORE_IBUS/DBUS_TIMEOUT_CONF`,
  65535 cycles at most). The bus then gives an error, and Linux gets an
  access fault on a correct address (`CORE1_DBUS_TIMEOUT` in
  `HP_SYSTEM_CORE_TIMEOUT_INT_RAW`, seen with JTAG). The firmware turns
  both timeouts off before it starts hart 1.
- **Coherency.** After a program or erase, ESP-IDF invalidates the cache
  lines of each mapped page that it changed. The two harts share the data
  cache, so Linux reads the new data. The driver holds its lock in reads
  too, so that a read does not see half of a program.
- **The root.** A squashfs on `mtdblock0` (`rootfs`, 7.5 MiB), and UBIFS
  on `mtd1` (`data`, 2.4375 MiB) as an overlayfs over it
  (`/sbin/overlay-init`, as in OpenWrt). UBI attaches an
  erased partition and formats it. The upper layer is UBIFS and not jffs2,
  because jffs2 has no `RENAME_WHITEOUT`: overlayfs then cannot make a
  directory where a lower directory was deleted (`EINVAL`).
- **Compression.** The squashfs is zstd at the highest level that
  Buildroot gives (`BR2_TARGET_ROOTFS_SQUASHFS_EXTREME_COMP`), with 32 KiB
  blocks. gzip does not fit with BlueZ. xz is smaller, but the kernel
  decompresses it more slowly on this core. Smaller blocks decompress less
  data for each page fault.
- **The erase block.** The flash erases 4 KiB sectors, but the MTD device
  shows 64 KiB erase blocks: UBIFS needs a logical erase block of at least
  15 KiB, and UBI takes 128 bytes of each block for its headers.

## ESP-Mosaico NAND storage

CoreBoard V1.2 has a GD25Q128EWIGR 16 MiB boot NOR and a separate
GD5F1GM7UEYIGR 128 MiB SPI NAND. The NOR holds the loader, OpenSBI, kernel
and compressed read-only system. The NAND on GPSPI3 supplies a 96 MiB
`nand-data` partition and a 32 MiB `nand-swap` partition. Usable space is
smaller after bad-block reserves, UBI headers, filesystem metadata and
swap wear-levelling overhead.

The board follows the official BSP wiring: CLK20, D21, Q22, CS23, HOLD24
and WP25. The BSP's `onboard/nand_flash.c` releases HOLD and WP before
reading the ID and enabling QE, then uses quad I/O and ESP-IDF DMA.
Its defaults are 40 MHz, a 4 KiB maximum transfer and a queue of ten
transactions. These are transport settings, not filesystem throughput.
The BSP's `spi_nand_flash` 1.3.0 dependency sends a whole cache read or
program load as one transaction. Its GigaDevice table delays 25 us before
read-status checks and 380 us before program-status checks. Linux retains
its SPI-NAND operation timing, timeout and ECC handling; those mechanisms
already implement the same page/cache sequence. The ESP-IDF component's
Dhara metadata layout is not used for Linux UBI or mtdswap.
Linux's SPI-NAND core already handles QE, ECC and the chip's cache/program
commands. The GPSPI driver preserves high idle levels on the data pins
before QE is enabled, matching the BSP's bootstrap requirement. Clearing
those levels leaves HOLD asserted and causes probe timeouts.

GPSPI3 uses hardware CS and the driver's native `spi-mem` operations.
Command, address and dummy cycles use their hardware phases; a page's
data continues through the 64-byte FIFO with CS held between chunks.
This removes the generic path's temporary message allocation and
separate transfers for each phase. CS hold timing matches the ESP-IDF
master's one-clock setting. Clock divisors are cached. GPSPI2
continues to use the display's GPIO CS path. The current Linux driver
uses PIO; ESP-IDF's DMA support cannot be reused without a Linux DMA
controller driver and validated channel, descriptor and cache handling.

GPSPI source selector 2 is the 480 MHz BBPLL, not CPLL / 2.
The device tree supplies BBPLL separately to the clock provider. As in
ESP-IDF, the master divider stays at two, and the high-speed clock is
limited to 160 MHz. A requested 80 MHz master clock therefore uses
BBPLL / 3 / 2; the SPI counter divides it by two for 40 MHz on the wire.
The earlier 160 MHz source model overclocked transfers: requesting
40 MHz returned a shifted NAND ID. Reading the same ID with the corrected
dividers returns `c8 91`. Changing only the master-to-HS ratio did not
fix the ID; both the source rate and divider constraints are required.
GPSPI2 uses the same corrected clock model.

The controller also provides native status polling. This kernel uses
100 Hz ticks without high-resolution timers: a short `usleep_range()`
can last a whole tick. The generic status loop therefore became slower
after command transfers were shortened enough to observe NAND busy.
Poll actively for at most 1 ms, preserving the SPI-NAND timeout; slow
erases and devices that stay busy then sleep. This matches the official
component's short-delay strategy without busy-waiting through an erase.

GD5F1GM7UE reserves only the first OOB byte for its bad-block marker.
The next byte is free for filesystem or swap metadata. The kernel patch
`0024-mtd-spinand-use-one-byte-BBM-on-GD5F1GM7UE.patch` makes bad-block
checks and marking follow that layout for this part. Otherwise, mtdswap's
valid `95 20` erase-count marker at OOB offset 1 makes every used swap
erase block look bad after reboot. A raw read distinguishes this case:
`ff 95 20` starts with a good block marker. No erase is needed to recover
from that false classification.

`nand-storage init ERASE [overlay.tar]` is an explicit destructive setup
command. It checks partition names, types, sizes and whether they are in
use, then creates a UBI `data` volume with `upper` and `work` directories.
An optional whole-overlay archive is extracted with xattrs and numeric
ownership. The readiness marker is written last. Early init never formats
storage: it mounts the initialized NAND upper layer over the NOR root,
or falls back to the read-only NOR system on failure. Wi-Fi settings,
SSH keys and all other configuration remain ordinary files in that
upper layer. Keep personal overlays and migration archives out of Git
and release images.

`S05swap.sh` starts zram at priority 100. The common default is 6 MiB;
Mosaico's `/etc/default/swap` sets 2 MiB so incompressible pages cannot
consume almost all PSRAM before lower-priority NAND swap is reached.
A zram memory limit is not a spill policy: rejected writes report I/O
errors. Bound its logical size instead and let swap priorities choose
NAND after it fills.

`S06nand-swap.sh` resolves `nand-swap` by its label and loads `mtdswap`
at priority 10. The MTD translation layer handles erases, bad blocks and
wear levelling and supplies its own swap header. It enables
`reserve_marker_pages=1`: the first two NAND pages of each eraseblock
contain clean/dirty OOB markers and are excluded from swap data. On-die
ECC protects these markers; programming swap data over them would try
to replace already programmed parity bits and cause uncorrectable reads.
The reservation rounds up to a system page and reduces the advertised
capacity and free-page accounting. Other mtdswap users keep the default
layout. The resulting 32 MiB physical partition provides 28,644 KiB of
swap, after page reservations and translation spares. Do not run
`mkswap` on a raw `mtdblock` device or use a UBIFS swap file.

### Storage validation

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

### Reboot into the ROM downloader

On ESP-Mosaico, `reboot bootloader` requests an orderly shutdown through
BusyBox init. Its reboot applet only sends a signal to PID 1, so the board's
`/sbin/reboot` wrapper first writes `bootloader` to
`/sys/bus/platform/devices/20700008.reboot-mode/next_mode`. The kernel keeps
that choice in RAM until its reboot notifier sets
`LP_SYSTEM_REG_FORCE_DOWNLOAD_BOOT` (`0x20700008`, bit 2). OpenSBI then
performs the normal system reset. No temporary module or flash write is
needed. Ordinary `reboot` selects `normal`; poweroff does not apply the
saved choice. Direct callers of Linux's `RESTART2` API can also pass
`bootloader` without the wrapper.

Once the UART reports `waiting for download`, close the serial monitor and
flash with the tested esptool version, bypassing unavailable RTS/DTR lines:

```sh
esptool --chip esp32s31 -p PORT -b 921600 --before no-reset \
    --after no-reset write-flash @flash_args
python3 scripts/reset-from-bootloader.py --port PORT --baud 921600
```

Use the release bundle's `flash_args`, or generate it with
`make flash-args BOARD=esp-mosaico`. Run the helper in the Python
environment containing the tested esptool. It clears only the
forced-download bit and resets through the watchdog, preserving the other
LP_SYSTEM bits. Reconnect the UART console at 115200 baud. Keep 921600 for
the reset command when the flasher left the UART at that rate; a fresh ROM
session starts at 115200. A reset without clearing the forced-download bit
returns to the ROM downloader.

Validation checks ordinary reboot, rejection of an invalid `next_mode`,
entry into ROM download, an esptool connection and successful return to
Linux. The temporary UART logs stay in ignored build output.

### Execution and memory

Linux continues to execute from PSRAM. Both harts are already in use:
hart 0 runs the radio firmware, with code mapped from NOR, and hart 1
runs Linux. Internal SRAM holds firmware state, stacks, shared-memory
queues, the DMA pool and Linux trap code. The rest remains firmware heap;
giving it to Linux would overwrite allocations used by the radios.
The device tree exposes the 16 MiB PSRAM except for the documented
OpenSBI and guard reservations. Zram compresses cold pages in that same
RAM; NAND swap adds backing capacity, not PSRAM bandwidth.

The GD25Q128E supports hardware XiP, but this Linux 7.3 RISC-V port has no
`CONFIG_XIP_KERNEL` implementation. A Linux XiP port would require split
NOR/RAM linking, early data relocation, changes to physical/virtual
address translation and boot mappings, loader/OpenSBI handoff changes,
and validation of flash mappings, cache maintenance and instruction
patching while hart 0 uses the same NOR. This is substantial architecture
work, so it is intentionally deferred. SPI NAND requires page reads into
its internal cache and cannot serve as a CPU instruction mapping.
Squashfs already keeps compressed executable files in NOR; its RAM page
cache is reclaimable.

Reference documents are GigaDevice's GD5F1GM7UE Rev.1.6MT, section 12.4
and the recommended spare-area table (bad-block marker and OOB), and
GD25Q128E Rev.1.4 (XiP). The official implementation used for comparison is
[ESP-Mosaico BSP NAND initialization](https://github.com/esp-mosaico/esp-mosaico-bsp/blob/7cbfb390638dcb93522fc3cde094b2d43cf907c2/components/esp-mosaico-bsp/onboard/nand_flash.c),
along with its `include/bsp/nand_flash.h` defaults and the pinned
[SPI NAND transaction implementation](https://github.com/espressif/idf-extra-components/blob/0fe2ac6c6fea758108bf1f5ef2b3bf99ce879db1/spi_nand_flash/src/spi_nand_oper.c).
Other references are ESP-IDF's
[ESP32-S31 clock source rates](https://github.com/espressif/esp-idf/blob/fff9895c82d744c7237be8847347bdd1b07c6643/components/esp_hw_support/port/esp32s31/esp_clk_tree.c),
its ESP32-S31 GPSPI HAL and [SPI master documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s31/api-reference/peripherals/spi_master.html).
Linux 7.3's SPI-NAND and RISC-V implementations supply the kernel-side
contracts. Reference checkouts and downloaded datasheets stay in ignored
`vendor/` or `build/` directories.

## ESP-Mosaico on-board peripherals

The CoreBoard V1.2 wiring follows the official
[schematic](https://dl.espressif.com/AE/Mosaico/SCH_SCH_ESP-Mosaico_CoreBoard_V1_2_1_2026-09-24.pdf)
and the [BSP pin definitions](https://github.com/esp-mosaico/esp-mosaico-bsp/blob/7cbfb390638dcb93522fc3cde094b2d43cf907c2/components/esp-mosaico-bsp/include/bsp/esp_mosaico.h).
The sensors use I2C0 on GPIO56/GPIO3 at 400 kHz. Their supplies are the
always-on MCU 3.3 V rail, upstream of the GPIO60 peripheral switch. The
motor uses VDD directly through its GPIO8-controlled MOSFET; its supply
does not have software voltage control.

| Device | Connection | Linux interface |
| --- | --- | --- |
| BMI270 accelerometer and gyroscope | I2C `0x69` | `bmi270_i2c`, IIO acceleration, angular velocity and temperature |
| BMM150 #2 and #3 | I2C `0x11` and `0x12` | `bmc150_magn_i2c`, IIO magnetic field |
| AI button | GPIO7, active low, internal pull-up | `gpio-keys`, evdev `KEY_PROG1`, both-edge IRQ |
| Vibration motor | GPIO8, active high | `gpio-vibra`, evdev `FF_RUMBLE` |

`S09peripherals` loads the upstream drivers after the writable root has
mounted. The `bmi270-firmware` package extracts the standard 8192-byte
initialization image from the pinned Bosch SensorAPI source, checks its
SHA256 and installs it as `/lib/firmware/bmi270-init-data.fw`, together
with its BSD-3-Clause license. There is no firmware download at boot.
The BMM150 I2C patch preserves the client name when the non-deprecated
device-tree compatible has no legacy I2C ID, so its IIO name is available
for device discovery.

Find IIO devices by their name and their parent I2C address; device
numbers depend on probe order. For example:

```sh
for d in /sys/bus/iio/devices/iio:device*; do
    echo "$d: $(cat "$d/name")"
    readlink -f "$d"
done
```

Read the BMI270's `in_accel_{x,y,z}_raw`, `in_anglvel_{x,y,z}_raw` and
`in_temp_raw` attributes, or a BMM150's `in_magn_{x,y,z}_raw`. Apply the
corresponding scale and temperature offset attributes from that device;
do not assume that raw values share units. Mounting matrices have not
been calibrated to the display orientation. Shared sensor interrupts on
GPIO2 are not configured in this integration. The sensor drivers provide
direct sysfs reads, without a background userspace sampler or
hardware-triggered sensor buffers.

Use `evtest` and select `keys` to observe the AI button. The GPIO driver
supports rising, falling, both-edge and level interrupts through GPIO_INTR0,
routed to the Linux hart. The button uses both edges and 50 ms software
debounce, without periodic polling. POWER/RESET/BOOT retain their hardware
functions. The motor accepts timed `FF_RUMBLE` effects through the
standard evdev API. Any nonzero magnitude switches it on; this GPIO
implementation does not provide proportional intensity control. It is
off at probe and stops when the input device closes. Boot does not play
an effect.

The 2026-10-28 local build registers the GPIO7 AI key through gpio-keys.
An earlier on-device kernel exposed the interrupt in /proc/interrupts;
physical press/release events and the final image still need verification.

Validation on 2026-10-28 loaded the Bosch initialization image and read
all six BMI270 motion axes and temperature. Both BMM150 devices probe
and return samples. **Known limitation:** address `0x12` repeatedly
returns `-2147483648` for Y. This is the driver's overflow sentinel,
not a usable magnetic-field measurement. The other five magnetic axes
return finite values. The source of that overflow has not been
determined, and compass accuracy is not validated. A temporary evdev
program submitted and stopped a 150 ms motor effect successfully; that
checks the software path, not a measured motor amplitude. A five-second
idle sample used approximately 0.6% CPU with the preview stopped and
input devices closed.

Several power parts operate autonomously. The HUSB320 has its SCL/OUT2
and SDA/OUT1 pins unconnected in this schematic, so no I2C Type-C driver
is instantiated. TP4057 controls charging and the red/green charge LED
without MCU status signals. TLV62569, SY7088 and HE9073A33 provide the
fixed power rails; their presence does not imply a programmable bus
interface. The SAM8108 power latch remains hardware-controlled; Linux
keeps the existing SBI shutdown path. ES8311 responds at `0x19`, but
an ESP32-S31 I2S/ALSA path and speaker playback remain unimplemented.
No speaker playback was performed during this work.

### BQ27220 battery gauge

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

## ESP-Mosaico camera

CoreBoard V1.2's left connector accepts CameraBoard V1.4 with an SC101IOT
sensor. The module has its own 20 MHz crystal. Linux does not generate
XCLK or drive the connector's unused sensor reset line.

| Signal | CoreBoard GPIO or I2C address |
| --- | --- |
| DVP D0..D7 | 16, 15, 33, 4, 14, 12, 18, 13 |
| PCLK, HREF, VSYNC | 17, 19, 55 |
| Sensor PWDN, active high | 48 |
| Flash control, open drain, high means off | 34 |
| I2C1 SDA, SCL, at 100 kHz | 0, 1 |
| Left module descriptor EEPROM | 0x50 |
| SC101IOT SCCB | 0x68; chip ID 0xda4a |

GPIO14 is DVP D4 and must not be driven as an EEPROM address pin. The
loader releases the native USB Serial/JTAG pads because GPIO33/34 are
needed by the camera. UART0 remains the console and download connection.
The shared 3.3 V rail also powers the display and touch controller, so
removing or stopping the sensor does not switch that rail off.

The `esp-mosaico-camera-module` driver polls the EEPROM every 250 ms. It
checks the ESP camera descriptor and all three CRC16 fields, then requires
three matching reads before creating the SC101IOT I2C client. Three missed
or invalid reads remove the client. The disabled sensor node in the device
tree describes the wiring without probing a missing module at boot.

The receiver uses the V4L2 asynchronous subdevice interface. Sensor detach
stops capture, synchronizes DMA callbacks, returns queued buffers with an
error and wakes the application. The capture node stays registered while
the module is absent; input enumeration reports no signal and STREAMON
returns `ENOLINK`. After reconnection, applications must stop and restart
their stream, or close and reopen the device. A two-second watchdog also
stops a stream that no longer produces frames.

`esp32s31-camera` exposes one capture mode: 640x480 UYVY, nominally 15 fps,
with MMAP or DMA-BUF streaming. AXI receive DMA writes directly into
scatter-gather PSRAM pages; coherent SRAM holds only the descriptor links.
The queue uses two to four buffers. Each buffer allocates 618496 bytes,
including a page-aligned tail beyond the 614400-byte image for samples at
the VSYNC boundary. No frame-sized staging copy is used. Short or failed
frames carry `V4L2_BUF_FLAG_ERROR`; applications should discard them,
including partial frames while the sensor starts. Controls and other
resolutions are not implemented.

The Buildroot `sc101iot-firmware` package fetches the pinned official
[ESP video components mode table](https://github.com/espressif/esp-video-components/blob/cd6194c97c43d6997eee13faf9e4c2ac4cae0c26/esp_cam_sensor/sensors/sc101iot/private_include/sc101iot_dvp_8bit_20Minput_640x480_yuv422_uyvy_15fps.h)
and packs it into `/lib/firmware/sc101iot/vga-uyvy.bin`. The Apache-2.0 mode
data remains separate from the GPL kernel driver. The implementation also
follows the official
[Mosaico BSP camera fixes](https://github.com/esp-mosaico/esp-mosaico-bsp/tree/7cbfb390638dcb93522fc3cde094b2d43cf907c2/components/mosaico_module_camera/patches):
zero SC101IOT PCLK delay and a receiver FIFO reset before each frame.

Validation on 2026-10-28 used a temporary V4L2 MMAP capture program on the
CoreBoard V1.2 and CameraBoard V1.4. A 60-frame run had no error frames;
a 300-frame run had one initial short frame and then 300 complete frames
without sequence gaps, at approximately 15 fps. A saved UYVY frame was
converted and visually checked. Unbinding the module driver during
capture woke the application with a poll error; capture while absent
failed with `ENOLINK`, and rebinding restored a 60-frame capture. These
checks exercise sensor removal and recreation in software. Physical
unplug/replug, boot with the module absent and electrical behavior during
connector insertion still need hardware validation. Captures and temporary
test programs stay in ignored `build/` directories.

## Build configurations

The four builds share one Buildroot base
(`br2-external/board/esp32s31/defconfig/common.fragment`), and
`scripts/mkdefconfigs.sh` merges it with the fragment of the root and the
fragment of the board. The module alone and ESP-Mosaico have their roots
in the flash (`flashroot.fragment`). The Korvo-1 and the
Function-CoreBoard-1 have it on a disk, the card or a USB stick
(`diskroot.fragment`). A setting is in one fragment only, and `make check`
fails when a defconfig in `br2-external/configs/` differs from its fragments.

The table compares the original three builds. ESP-Mosaico uses a NOR
system with a NAND upper layer, the CO5300 display and CST9220 touch
drivers, `evtest`, GNU tar with xattrs and the NAND setup tools.

| | Module alone | Function-CoreBoard-1 | Korvo-1 |
| --- | --- | --- | --- |
| Base | Kernel, toolchain, BusyBox, Dropbear, OpenSSL, `wpa_supplicant` (WPA3, PEAP/TTLS/TLS, AP mode), curl, iperf3, libgpiod, BlueZ (`bluetoothd`, `bluetoothctl`), Lua | The same | The same |
| Root | squashfs root in the flash, UBIFS overlay, the MTD tools | ext4 root on the USB stick | ext4 root on the card |
| Board parts | | Ethernet, USB, the LED | LCD, touch, the card, USB, the LED |
| Tools | The base only | `hostapd`, the classic BlueZ tools, bash, vim, gdb, strace, tcpdump, links, MicroPython, htop, filesystem tools, tmux, nano, nnn, ncdu, tree, bc, sqlite3, ethtool | `hostapd`, the classic BlueZ tools, bash, vim, gdb, strace, tcpdump, links, MicroPython, htop, filesystem tools, tmux, nano, nnn, ncdu, tree, bc, sqlite3, `evtest`, `gpm`, a tmux desktop on the LCD, lynx, irssi |
| Swap | zram only | zram, then p3 of the stick | zram, then p3 of the card |

The Korvo-1 and the Function-CoreBoard-1 have a disk with much space, and
the module alone has only its flash. The base keeps the network, security
and Bluetooth stack the same on all boards.

## Silicon traps

| Trap | Effect |
| --- | --- |
| `mret` from S-mode does not trap | It changes M-mode state from S-mode. |
| CLIC input not given to S-mode | S-mode window reads 0, writes are lost, no fault. |
| `sret` without the full `scause` | `SIL` stays high, all S-mode interrupts masked. |
| SD PIO FIFO | Does not work. Use IDMAC. |
| Cache write-back erratum | A single write-back can lose data. Run it twice. |
| `fence.i` | Does not invalidate the I-cache. Use the sync engine. |
| Sync engine range | It does not round the size up to a line. Align address and size. |
| `HP_APM` region start | The last word below a region belongs to the region. Keep the page below it out of use. |
| `cliccfg` layout | Not the layout in the ESP-IDF header. See "Interrupts". |
| PMP entries overlap | Fault, even for a short time during an update. |
| PMA entries overlap | No fault. The entry with the lowest index is used. |
| FPU `EXT_ILL` | Cannot identify FLW/FSW. |
| GPIO33/34 | LCD data pins and also the USB Serial/JTAG pins. |
| Flash program or erase | Freezes the cache of the two harts. Use auto-suspend. |
| Core bus timeout | Fires behind flash suspends: access fault on PSRAM. Turn it off. |
| DMA burst to PSRAM | More than 16 beats gives an MSPI error that stops hart 0. The GMAC uses PBL 16 without the x8. |
| I-cache 2-way | Three hot lines in one set miss each time. A kernel change can make syscalls 2 times slower. |
| ESP-IDF I-cache size | `CONFIG_CACHE_L1_ICACHE_SIZE` says 16 KiB. The hardware has 32 KiB. |
| TRNG after the 2nd stage | ESP-IDF v6.1 stops it. The loader starts it again for `esp_random()` and the Linux hwrng. Its noise source does not use the RF. |
| USB Serial/JTAG without a host | `TX_FREE` stays clear, and the console slows to approximately 130 B/s. OpenSBI stops the copy after one timeout. |
| ROM stack of hart 1 | The ROM start code of hart 1 uses 1 KiB below `0x2F07E600`. ESP-IDF gives this area to its heap, so the loader reserves it. |

## Where to start reading

| To understand | Read |
| --- | --- |
| The handoff | `bootloader/main/main.c` |
| M-mode setup | `opensbi/platform/esp32s31/platform.c` |
| Interrupt entry | `handle_exception` in `arch/riscv/kernel/entry.S`, `linux/patches/0001` |
| Interrupt controller | `linux/drivers/irqchip/irq-esp32s31-clic.c` |
| DMA coherency | `linux/drivers/cache/esp32s31-cache.c`, `linux/patches/0006` |
| Wi-Fi transport | `shared/esp32s31-hosted-shm.h` |
| The flash root | `bootloader/components/esp_hosted/shm_flash.c`, `linux/drivers/net/wireless/espressif/esp_hosted/esp_shm_flash.c` |
| The boards | `bootloader/main/board_*.c`, `br2-external/board/esp32s31/defconfig/*.fragment` (`make defconfigs` makes `br2-external/configs/*_defconfig`) |
| Pins and GPIO | `linux/drivers/pinctrl/pinctrl-esp32s31.c` |
| Clocks and resets | `linux/drivers/clk/clk-esp32s31.c` |
| Device tree | `linux/arch/riscv/boot/dts/espressif/esp32s31.dtsi` |

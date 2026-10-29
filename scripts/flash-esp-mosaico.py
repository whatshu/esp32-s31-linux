#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Flash ESP-Mosaico through ROM USB while keeping the debug UART open."""

import argparse
from pathlib import Path
import subprocess
import sys

from esptool.targets.esp32s31 import ESP32S31ROM
from serial.tools import list_ports


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="ESP32-S31 ROM USB CDC port")
    parser.add_argument("--baud", type=int, default=921600)
    parser.add_argument("images", nargs="+", help="offset/file pairs or @flash_args")
    args = parser.parse_args()
    if args.baud <= 0:
        parser.error("--baud must be positive")

    # Both a USB UART bridge and the Linux gadget can appear as ttyACM ports.
    # Check the USB identity before opening anything or sending reset signals.
    path = Path(args.port).resolve()
    port = next(
        (p for p in list_ports.comports() if Path(p.device).resolve() == path), None
    )
    if port is None or (port.vid, port.pid) != (
        ESP32S31ROM.ESPRESSIF_VID, ESP32S31ROM.IMAGE_CHIP_ID
    ):
        parser.error(
            "--port must identify the ESP32-S31 ROM USB downloader; "
            "connect the native Type-C port and run 'reboot bootloader' "
            "from the CH343 debug console first"
        )

    try:
        subprocess.run(
            [
                sys.executable, "-m", "esptool", "--chip", "esp32s31",
                "--port", args.port, "--baud", str(args.baud),
                "--before", "no-reset", "--after", "no-reset",
                "write-flash", *args.images,
            ],
            check=True,
        )
        # A normal reset would re-enter ROM while FORCE_DOWNLOAD_BOOT is set.
        subprocess.run(
            [
                sys.executable, str(Path(__file__).with_name("reset-from-bootloader.py")),
                "--port", args.port, "--baud", str(args.baud),
            ],
            check=True,
        )
    except subprocess.CalledProcessError as error:
        return error.returncode
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

# SPDX-License-Identifier: MIT
"""Keep the debug UART and unrelated USB devices out of the flashing path."""

import contextlib
import importlib.util
import io
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

SCRIPT = Path(__file__).resolve().parents[1] / "flash-esp-mosaico.py"
SPEC = importlib.util.spec_from_file_location("flash_esp_mosaico", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class FlashTests(unittest.TestCase):
    def invoke(self, port, devices, runner):
        with (
            patch.object(sys, "argv", [str(SCRIPT), "--port", port, "@flash_args"]),
            patch.object(MODULE.list_ports, "comports", return_value=devices),
            patch.object(MODULE.subprocess, "run", runner),
        ):
            return MODULE.main()

    def test_refuses_other_ports_without_starting_esptool(self):
        # CH343, Linux g_serial, Serial/JTAG and a keyboard are all different
        # from the native Type-C ROM downloader, despite similar device names.
        for identity in ((0x1A86, 0x55D3), (0x0525, 0xA4A7),
                         (0x303A, 0x1001), (0x4F57, 0x4110), (None, None)):
            with self.subTest(identity=identity):
                device = SimpleNamespace(
                    device="/dev/ttyACM1", vid=identity[0], pid=identity[1]
                )
                runner = Mock()
                with (
                    contextlib.redirect_stderr(io.StringIO()),
                    self.assertRaises(SystemExit) as error,
                ):
                    self.invoke(device.device, [device], runner)
                self.assertEqual(error.exception.code, 2)
                runner.assert_not_called()

    def test_accepts_rom_usb_by_symlink_and_resets_after_flashing(self):
        with tempfile.TemporaryDirectory() as name:
            device = Path(name) / "ttyACM2"
            device.touch()
            alias = Path(name) / "rom-usb"
            alias.symlink_to(device)
            usb = SimpleNamespace(device=str(device), vid=0x303A, pid=0x0020)
            runner = Mock()
            self.assertEqual(self.invoke(str(alias), [usb], runner), 0)
            flash, reset = (call.args[0] for call in runner.call_args_list)
            self.assertEqual(flash[flash.index("--port") + 1], str(alias))
            self.assertEqual(flash[flash.index("--before") + 1], "no-reset")
            self.assertEqual(flash[flash.index("--after") + 1], "no-reset")
            self.assertEqual(flash[-2:], ["write-flash", "@flash_args"])
            self.assertEqual(Path(reset[1]).name, "reset-from-bootloader.py")
            self.assertEqual(reset[reset.index("--port") + 1], str(alias))

    def test_failed_flash_leaves_downloader_available_for_retry(self):
        usb = SimpleNamespace(device="/dev/ttyACM2", vid=0x303A, pid=0x0020)
        runner = Mock(side_effect=subprocess.CalledProcessError(1, "esptool"))
        self.assertEqual(self.invoke(usb.device, [usb], runner), 1)
        self.assertEqual(runner.call_count, 1)


if __name__ == "__main__":
    unittest.main()

#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Pack the pinned vendor VGA mode into the SC101IOT driver's register format."""

import pathlib
import re
import struct
import sys

source = pathlib.Path(sys.argv[1]).read_text()
source = re.sub(r"/\*.*?\*/|//[^\n]*", "", source, flags=re.S)
body = source.split("[] = {", 1)[1].rsplit("};", 1)[0]
body = body.replace("SC101IOT_YUV422_FMT_UYVY", "0x02")
entries = re.findall(r"\{\s*(0x[0-9a-fA-F]+)\s*,\s*(0x[0-9a-fA-F]+)\s*\}", body)
remaining = re.sub(r"\{\s*0x[0-9a-fA-F]+\s*,\s*0x[0-9a-fA-F]+\s*\}\s*,?", "", body)
if remaining.strip() or not 1 <= len(entries) <= 512:
    raise SystemExit("Unexpected vendor register table")
payload = bytes(int(value, 16) for pair in entries for value in pair)
header = struct.pack("<4sHHI", b"SC10", 1, len(entries), 0)
pathlib.Path(sys.argv[2]).write_bytes(header + payload)

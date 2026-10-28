#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Extract the standard BMI270 image from the pinned Bosch SensorAPI."""

import hashlib
import pathlib
import re
import sys

source = pathlib.Path(sys.argv[1]).read_text()
table = re.search(
    r"const uint8_t bmi270_config_file\[\] = \{(.*?)\};", source, re.S
)
if table is None:
    raise SystemExit("BMI270 initialization table not found")
body = re.sub(r"/\*.*?\*/|//[^\n]*", "", table[1], flags=re.S)
if re.sub(r"0x[0-9a-fA-F]{2}|[\s,]", "", body):
    raise SystemExit("Unexpected BMI270 initialization data")
image = bytes(
    int(value, 16) for value in re.findall(r"0x[0-9a-fA-F]{2}", body)
)
expected = "2d75e68e343a13ff99be98261dfbd99d9e8c6f267da9e3c5aeb883276ad178db"
if len(image) != 8192 or hashlib.sha256(image).hexdigest() != expected:
    raise SystemExit(
        "BMI270 initialization image does not match the pinned version"
    )
pathlib.Path(sys.argv[2]).write_bytes(image)

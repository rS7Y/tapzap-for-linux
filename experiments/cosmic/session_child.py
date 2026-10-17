#!/usr/bin/env python3
"""Record only the nested session's display coordinates, then keep kiosk alive."""
import json
import os
from pathlib import Path
import sys
import time

Path(sys.argv[1]).write_text(json.dumps({key: os.environ.get(key) for key in (
    "DISPLAY", "WAYLAND_DISPLAY", "XDG_RUNTIME_DIR")}, indent=2))
while True:
    time.sleep(60)

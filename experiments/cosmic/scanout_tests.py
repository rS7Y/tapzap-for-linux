#!/usr/bin/env python3
"""Replay the existing tests using QEMU's virtual scanout instead of Xvfb.

Run inside the disposable guest. QMP must be accessible only from that guest
and host loopback; its screenshot directory is shared read-only into the guest.
The VM's graphical VT must be active. This is virtual, not optical, capture.
"""
import argparse
import io
import json
from pathlib import Path
import runpy
import socket
import sys
import urllib.request
from PIL import Image, ImageGrab


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qmp-host", required=True)
    parser.add_argument("--qmp-port", type=int, required=True)
    parser.add_argument("--host-output", required=True,
                        help="Absolute screenshot filename on the QEMU host")
    transport = parser.add_mutually_exclusive_group(required=True)
    transport.add_argument("--guest-input", type=Path,
                        help="The same file through the guest's read-only share")
    transport.add_argument("--capture-url", help="Read-only host-loopback HTTP URL for the completed dump")
    parser.add_argument("test", choices=("render", "gui", "login"))
    parser.add_argument("arguments", nargs=argparse.REMAINDER)
    args = parser.parse_args()

    def capture(**_):
        with socket.create_connection((args.qmp_host, args.qmp_port), timeout=5) as connection:
            with connection.makefile("rwb", buffering=0) as stream:
                greeting = json.loads(stream.readline(65536))
                if "QMP" not in greeting:
                    raise RuntimeError("Expected a QMP greeting")
                commands = [{"execute": "qmp_capabilities"},
                            {"execute": "screendump", "arguments": {"filename": args.host_output}}]
                for command in commands:
                    stream.write(json.dumps(command).encode() + b"\n")
                    while True:
                        response = json.loads(stream.readline(65536))
                        if "error" in response:
                            raise RuntimeError(response["error"])
                        if "return" in response:
                            break
        if args.capture_url:
            with urllib.request.urlopen(args.capture_url, timeout=5) as reply:
                source = io.BytesIO(reply.read())
        else:
            source = args.guest_input
        with Image.open(source) as image:
            return image.convert("RGB")

    # Both original harnesses route every pixel read through this one function.
    # No COSMIC screencopy API is used, so the RGB effect cannot be bypassed.
    ImageGrab.grab = capture
    test = Path(__file__).with_name(args.test + "_tests.py")
    arguments = args.arguments[1:] if args.arguments[:1] == ["--"] else args.arguments
    if args.test == "render":
        arguments += ["--capture-description", "QEMU virtual KMS scanout after compositor postprocessing"]
    elif args.test == "gui":
        arguments += ["--ui-transport", "XWayland inside full COSMIC session; QEMU virtual KMS capture"]
    sys.argv = [str(test)] + arguments
    runpy.run_path(str(test), run_name="__main__")


if __name__ == "__main__":
    main()

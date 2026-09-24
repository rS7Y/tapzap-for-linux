#!/usr/bin/env python3
# Copyright (C) 2026 Tap Zap.
# SPDX-License-Identifier: GPL-3.0-or-later
"""Generate a disposable SDR ICC/VCGT proof; does not change display settings."""
import argparse
import ctypes as C
import ctypes.util
import math
from pathlib import Path


def generate(destination, gains, baseline=None):
    if len(gains) != 3 or any(not math.isfinite(x) or not 0 <= x <= 1 for x in gains):
        raise ValueError('Three finite RGB gains in [0, 1] are required')
    lib = C.CDLL(ctypes.util.find_library('lcms2') or 'liblcms2.so.2')
    lib.cmsCreate_sRGBProfile.restype = C.c_void_p
    lib.cmsOpenProfileFromFile.argtypes = [C.c_char_p, C.c_char_p]
    lib.cmsOpenProfileFromFile.restype = C.c_void_p
    lib.cmsReadTag.argtypes = [C.c_void_p, C.c_uint32]
    lib.cmsReadTag.restype = C.c_void_p
    lib.cmsIsTag.argtypes = [C.c_void_p, C.c_uint32]
    lib.cmsEvalToneCurve16.argtypes = [C.c_void_p, C.c_uint16]
    lib.cmsEvalToneCurve16.restype = C.c_uint16
    lib.cmsBuildTabulatedToneCurve16.argtypes = [C.c_void_p, C.c_uint32, C.POINTER(C.c_uint16)]
    lib.cmsBuildTabulatedToneCurve16.restype = C.c_void_p
    lib.cmsWriteTag.argtypes = [C.c_void_p, C.c_uint32, C.c_void_p]
    lib.cmsFreeToneCurve.argtypes = [C.c_void_p]
    lib.cmsSaveProfileToFile.argtypes = [C.c_void_p, C.c_char_p]
    lib.cmsCloseProfile.argtypes = [C.c_void_p]
    profile = (lib.cmsOpenProfileFromFile(str(baseline).encode(), b'r') if baseline
               else lib.cmsCreate_sRGBProfile())
    if not profile:
        raise RuntimeError('Cannot open/create baseline ICC profile')
    signature = lambda name: int.from_bytes(name.encode('ascii'), 'big')
    try:
        # KWin gives MHC2 precedence over VCGT. Never silently produce a no-op.
        if lib.cmsIsTag(profile, signature('MHC2')):
            raise RuntimeError('MHC2 baseline requires separate support')
        vcgt = lib.cmsReadTag(profile, signature('vcgt'))
        curves = C.cast(vcgt, C.POINTER(C.c_void_p)) if vcgt else None
        values = []
        for channel, gain in enumerate(gains):
            for index in range(256):
                original = (lib.cmsEvalToneCurve16(curves[channel], index * 257)
                            if curves and curves[channel] else index * 257)
                values.append(round(original * gain))
        # Use the typed writer: replacing a decoded tag with cmsWriteRawTag can
        # leave LittleCMS's decoded-tag ownership inconsistent on profile close.
        replacement = (C.c_void_p * 3)()
        try:
            for channel in range(3):
                samples = (C.c_uint16 * 256)(*values[channel * 256:(channel + 1) * 256])
                replacement[channel] = lib.cmsBuildTabulatedToneCurve16(None, 256, samples)
                if not replacement[channel]:
                    raise RuntimeError('Cannot allocate calibration curve')
            if not lib.cmsWriteTag(profile, signature('vcgt'), replacement):
                raise RuntimeError('Cannot write calibration curves')
        finally:
            for curve in replacement:
                if curve:
                    lib.cmsFreeToneCurve(curve)
        if not lib.cmsSaveProfileToFile(profile, str(destination).encode()):
            raise RuntimeError('Cannot save test profile')
    finally:
        lib.cmsCloseProfile(profile)
    # Read it back through LittleCMS rather than trusting the serialization.
    profile = lib.cmsOpenProfileFromFile(str(destination).encode(), b'r')
    if not profile:
        raise RuntimeError('Generated ICC profile failed readback')
    try:
        curves = C.cast(lib.cmsReadTag(profile, signature('vcgt')), C.POINTER(C.c_void_p))
        if not curves:
            raise RuntimeError('Generated ICC profile has no readable VCGT')
        for channel in range(3):
            for index in range(256):
                actual = lib.cmsEvalToneCurve16(curves[channel], index * 257)
                if actual != values[channel * 256 + index]:
                    raise RuntimeError('Generated curve did not survive ICC readback')
    finally:
        lib.cmsCloseProfile(profile)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('destination', type=Path)
    parser.add_argument('gains', type=float, nargs=3, metavar='GAIN')
    parser.add_argument('--baseline', type=Path)
    args = parser.parse_args()
    generate(args.destination, args.gains, args.baseline)
    print('PASS: generated ICC file and verified all 768 calibration entries')

#!/usr/bin/env python3
"""
depth_bias.py — Apply a depth bias to a DEPTH24_STENCIL8 texture.

The DEPTH24_STENCIL8 format packs 24-bit depth and 8-bit stencil into a single
32-bit unsigned integer per pixel (GL_UNSIGNED_INT_24_8).  This script unpacks
the depth channel, adds a floating-point bias, clamps to [0, 1], repacks the
pixel, and writes the result back out.

Usage:
    python depth_bias.py -bias 0.001 -size 2048 4096 input.bin output.bin
    python depth_bias.py -bias -0.0005 -width 1024 -height 1024 depth.raw out.raw
    python depth_bias.py -bias 0.0 --dump input.bin 64 64   # passthrough + stats

Supported input: raw binary (GL_DEPTH_STENCIL / UNSIGNED_INT_24_8 byte order).
"""

import argparse
import struct
import sys
from pathlib import Path

# GL_UNSIGNED_INT_24_8 packs depth in the high 24 bits.
_DEPTH_MAX_INT = (1 << 24) - 1  # 16,777,215


def unpack_depth_stencil(pixel: int):
    """Return (depth_float, stencil_byte) from a packed UINT_24_8 value."""
    depth = (pixel >> 8) & _DEPTH_MAX_INT
    stencil = pixel & 0xFF
    return depth / _DEPTH_MAX_INT, stencil


def pack_depth_stencil(depth_float: float, stencil: int) -> int:
    """Pack a float depth and byte stencil back into a UINT_24_8 value."""
    depth_int = int(round(depth_float * _DEPTH_MAX_INT))
    if depth_int < 0:
        depth_int = 0
    if depth_int > _DEPTH_MAX_INT:
        depth_int = _DEPTH_MAX_INT
    return (depth_int << 8) | (stencil & 0xFF)


def apply_bias(data: bytes, bias: float, quiet: bool = False):
    """Unpack, bias, clamp, repack; return (bytes, stats_dict)."""
    elem_size = 4  # uint32
    if len(data) % elem_size != 0:
        raise ValueError(
            f"file size {len(data)} is not a multiple of {elem_size} bytes "
            f"(expected DEPTH24_STENCIL8 = 4 bytes / pixel)"
        )

    pixel_count = len(data) // elem_size
    pixels = struct.unpack(f">{pixel_count}I", data)

    depth_min = 1.0
    depth_max = 0.0
    clamp_lo = 0
    clamp_hi = 0
    out = bytearray(len(data))

    for i, p in enumerate(pixels):
        d, s = unpack_depth_stencil(p)
        depth_min = min(depth_min, d)
        depth_max = max(depth_max, d)

        d += bias
        if d < 0.0:
            d = 0.0
            clamp_lo += 1
        elif d > 1.0:
            d = 1.0
            clamp_hi += 1

        struct.pack_into(">I", out, i * elem_size, pack_depth_stencil(d, s))

    if not quiet:
        print(f"Pixels      : {pixel_count}")
        print(f"Depth range : [{depth_min:.6f}, {depth_max:.6f}] (in)")
        rng = (depth_min + bias, depth_max + bias)
        print(f"  after bias: [{max(0, rng[0]):.6f}, {min(1, rng[1]):.6f}]")
        if clamp_lo:
            print(f"Clamped to 0: {clamp_lo} pixels")
        if clamp_hi:
            print(f"Clamped to 1: {clamp_hi} pixels")

    return bytes(out)


# ---------------------------------------------------------------------------
# dump mode — print stats without writing
# ---------------------------------------------------------------------------
def dump_stats(data: bytes):
    elem_size = 4
    if len(data) % elem_size != 0:
        raise ValueError(f"file size {len(data)} not multiple of 4")
    pixel_count = len(data) // elem_size
    pixels = struct.unpack(f">{pixel_count}I", data)

    depth_min = 1.0
    depth_max = 0.0
    depth_sum = 0.0
    nonzero = 0
    stencil_vals = set()

    for p in pixels:
        d, s = unpack_depth_stencil(p)
        if d < depth_min:
            depth_min = d
        if d > depth_max:
            depth_max = d
        depth_sum += d
        if d > 0.0:
            nonzero += 1
        stencil_vals.add(s)

    print(f"Pixels      : {pixel_count}")
    print(f"Depth range : [{depth_min:.6f}, {depth_max:.6f}]")
    print(f"Depth mean  : {depth_sum / pixel_count:.6f}")
    print(f"Depth > 0   : {nonzero} / {pixel_count}")
    print(f"Stencil vals: {len(stencil_vals)} unique "
          f"(min={min(stencil_vals)}, max={max(stencil_vals)})")


# ---------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(
        description="Apply depth bias to a DEPTH24_STENCIL8 texture"
    )
    parser.add_argument(
        "-bias", type=float, default=0.0,
        help="Bias added to depth values (default: 0.0)"
    )
    parser.add_argument(
        "-size", nargs=2, type=int, metavar=("W", "H"),
        help="Texture dimensions (width height)"
    )
    parser.add_argument(
        "-width", type=int, help="Texture width"
    )
    parser.add_argument(
        "-height", type=int, help="Texture height"
    )
    parser.add_argument(
        "--dump", action="store_true",
        help="Print depth/stencil stats only, no output file"
    )
    parser.add_argument(
        "-q", "--quiet", action="store_true",
        help="Suppress extra output"
    )
    parser.add_argument(
        "input", help="Input raw binary file (DEPTH24_STENCIL8)"
    )
    parser.add_argument(
        "output", nargs="?", help="Output file (omit with --dump)"
    )

    args = parser.parse_args()

    # ---- resolve dimensions ----
    if args.size:
        w, h = args.size
    elif args.width and args.height:
        w, h = args.width, args.height
    else:
        parser.error("specify dimensions with -size W H or -width W -height H")

    expected_bytes = w * h * 4

    # ---- read input ----
    with open(args.input, "rb") as f:
        data = f.read()

    if len(data) != expected_bytes:
        print(
            f"Warning: file size {len(data)} != {expected_bytes} "
            f"({w}x{h}x4 bytes).  Using actual file size.",
            file=sys.stderr,
        )

    # ---- dump mode ----
    if args.dump:
        dump_stats(data)
        return

    if not args.output:
        parser.error("output file required (or use --dump)")

    # ---- apply bias ----
    result = apply_bias(data, args.bias, args.quiet)

    # ---- write output ----
    Path(args.output).write_bytes(result)
    if not args.quiet:
        print(f"Wrote: {args.output}")


if __name__ == "__main__":
    main()

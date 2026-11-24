#!/usr/bin/env python3
"""Set this lab's QTI encoder linear-format default; never install anything."""
import argparse
import hashlib
from pathlib import Path

ORIGINAL_SHA256 = "e5afb835a68b970cab4e0cb9197607b6fabbeeca5f5c83cbda9138add5549d0c"
OFFSET = 0x7CC6C
BEFORE = bytes.fromhex("09 00 b0 12")  # mov w9, #0x7fffffff
AFTER = bytes.fromhex("29 00 80 52")   # mov w9, #1


def patch(data: bytes) -> bytes:
    if hashlib.sha256(data).hexdigest() != ORIGINAL_SHA256:
        raise ValueError("Unknown input SHA256; refusing a vendor ABI mismatch")
    if data[:6] != b"\x7fELF\x02\x01" or data[18:20] != b"\xb7\x00":
        raise ValueError("Expected a little-endian AArch64 ELF")
    if data[OFFSET:OFFSET + 4] != BEFORE:
        raise ValueError("Unexpected instruction at the encoder default")
    if not data.startswith(b"qti-ext-enc-linear-color-format\0", 0x2A828):
        raise ValueError("Encoder parameter name anchor mismatch")
    # The constructor at 0x7c960 builds vendor parameter 0xc039, assigns
    # the value at C2Param + 8 (0x7cc70), and names it at 0x7ce00.
    # Change its initial INT_MAX (automatic layout) to 1 (linear layout).
    # The setter and explicit client values, including 0, are unchanged.
    if data[0x7CC70:0x7CC74] != bytes.fromhex("09 09 00 b9"):
        raise ValueError("C2Param value store anchor mismatch")
    return data[:OFFSET] + AFTER + data[OFFSET + 4:]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    if args.input.resolve() == args.output.resolve():
        parser.error("Use a separate output; retain the original")
    try:
        result = patch(args.input.read_bytes())
    except ValueError as error:
        parser.error(str(error))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(result)
    print(f"SHA256={hashlib.sha256(result).hexdigest()} OUTPUT={args.output}")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Offline patch for ONE verified QTI library. Never writes to the input."""
import argparse
import hashlib
import json
import struct
from pathlib import Path

INPUT = "3e4cef7a63b47c18bb1ba8283a08fdbd150a99194138e92ead3cede662c9098b"
OUTPUT = "7ba792e5becada349ca60216468b9b444c99c531ce7915db0a387e7aefb10337"

def require(condition, message):
    if not condition:
        raise ValueError(message)

def branch(pc, target, link=False):
    delta = target - pc
    require(delta % 4 == 0 and -(1 << 27) <= delta < (1 << 27), "invalid AArch64 branch")
    return struct.pack("<I", (0x94000000 if link else 0x14000000) | ((delta // 4) & 0x3ffffff))

def patch(data):
    require(hashlib.sha256(data).hexdigest() == INPUT, "input SHA256 does not match; unsupported ROM/library")
    b = bytearray(data)
    require(b[:6] == b"\x7fELF\x02\x01" and struct.unpack_from("<H", b, 18)[0] == 183, "not AArch64 ELF64 LE")
    require(b[0x12780:0x12788].hex() == "b40080526c000014", "unexpected return branch")
    require(b[0x17860:0x1786c] == bytes(12), "trampoline area is not empty")
    b[0x12784:0x12788] = branch(0x12784, 0x17860)
    b[0x17860:0x1786c] = struct.pack("<I", 0xaa1303e0) + branch(0x17864, 0x17170, True) + branch(0x17868, 0x12934)
    phoff = struct.unpack_from("<Q", b, 32)[0]
    size, count = struct.unpack_from("<HH", b, 54)
    changed = 0
    for i in range(count):
        off = phoff + i * size
        typ, flags = struct.unpack_from("<II", b, off)
        fileoff, vaddr, _, filesz, memsz, _ = struct.unpack_from("<QQQQQQ", b, off + 8)
        if typ == 1 and flags == 5:
            require(fileoff == vaddr == 0xa000 and filesz == memsz == 0xd860, "unexpected executable LOAD")
            struct.pack_into("<QQ", b, off + 32, 0xd86c, 0xd86c)
            changed += 1
    require(changed == 1, "expected one executable LOAD")
    require(hashlib.sha256(b).hexdigest() == OUTPUT, "patched output SHA256 mismatch")
    return bytes(b)

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("input", type=Path)
    ap.add_argument("output", type=Path)
    args = ap.parse_args()
    require(args.input.resolve() != args.output.resolve(), "input and output must be different")
    result = patch(args.input.read_bytes())
    with args.output.open("xb") as f:
        f.write(result)
    print(json.dumps({"input_sha256": INPUT, "output_sha256": OUTPUT, "output": str(args.output)}, indent=2))

if __name__ == "__main__":
    main()

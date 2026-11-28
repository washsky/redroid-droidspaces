#!/usr/bin/env python3
"""Host regression checks for compressed image transfers; no phone needed."""
import gzip
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='r13-gzip-test-') as d:
    d = Path(d)
    exe = d / 'gzip-to-sparse'
    subprocess.run(['cc', '-O2', '-Wall', '-Wextra', '-Werror',
                    str(repo / 'src/release/gzip-to-sparse.c'), '-lz', '-o', str(exe)], check=True)
    raw = bytes(2**21) + os.urandom(16384) + bytes(2**21) + b'end'
    compressed = gzip.compress(raw)
    def decode(name, content, size):
        return subprocess.run([str(exe), str(d / name), str(size)], input=content,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    p = decode('good.img', compressed, len(raw))
    assert p.returncode == 0, p.stderr
    image = d / 'good.img'
    assert hashlib.sha256(image.read_bytes()).digest() == hashlib.sha256(raw).digest()
    assert image.stat().st_blocks * 512 < len(raw) // 2
    print('PASS exact bytes and sparse holes')
    old = image.read_bytes()
    assert decode('good.img', compressed, len(raw)).returncode != 0
    assert image.read_bytes() == old
    print('PASS existing output is preserved')
    for name, data, size in [('crc.img', compressed[:-8] + bytes(8), len(raw)),
                              ('plain.img', raw, len(raw)),
                              ('truncated.img', compressed[:-5], len(raw)),
                              ('oversize.img', compressed, len(raw) - 1),
                              ('undersize.img', compressed, len(raw) + 1)]:
        assert decode(name, data, size).returncode != 0
        assert not (d / name).exists()
    print('PASS corrupt, truncated, overrun and underrun inputs removed')

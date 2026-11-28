#!/usr/bin/env python3
"""Download and SHA256-check release assets. No GitHub login or phone access."""
import argparse
import hashlib
import os
from pathlib import Path
import re
import tempfile
import urllib.parse
import urllib.request


def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for data in iter(lambda: f.read(1024**2), b''):
            h.update(data)
    return h.hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--profile', choices=['base', 'sm8475'], default='sm8475',
                   help='base: original ARM64 inputs; sm8475: validated hardware integration')
    p.add_argument('--tag', help='override the fixed tag selected by --profile')
    p.add_argument('--directory', type=Path)
    p.add_argument('--image', choices=['original', 'optin', 'all'], default='original',
                   help='base profile image selection; original is the software-first starting input')
    p.add_argument('--tools-only', action='store_true', help='skip the compressed rootfs image')
    a = p.parse_args()
    a.tag = a.tag or ('v2026.11.28-arm64-base' if a.profile == 'base' else 'v2026.11.28-arm64')
    a.directory = a.directory or Path('out/prebuilt-base-20261128' if a.profile == 'base' else 'out/prebuilt-20261128')
    if not re.fullmatch(r'[A-Za-z0-9._-]+', a.tag):
        p.error('invalid tag')
    base = 'https://github.com/washsky/redroid-droidspaces/releases/download/' + a.tag + '/'
    request = urllib.request.Request(base + 'SHA256SUMS', headers={'User-Agent': 'redroid-droidspaces-prebuilt'})
    with urllib.request.urlopen(request, timeout=60) as r:
        text = r.read(1024**2).decode('ascii')
    entries = {}
    for line in text.splitlines():
        match = re.fullmatch(r'([0-9a-f]{64})  ([A-Za-z0-9][A-Za-z0-9._-]*)', line)
        if not match:
            raise ValueError('unsafe or malformed SHA256SUMS entry')
        digest, name = match.groups()
        if name in entries:
            raise ValueError('duplicate asset')
        entries[name] = digest
    if not entries:
        raise ValueError('empty SHA256SUMS')
    a.directory.mkdir(parents=True, exist_ok=True)
    for name, digest in entries.items():
        if a.tools_only and name.endswith('.img.zst'):
            continue
        if a.profile == 'base' and name.endswith('.img.zst') and a.image != 'all':
            selected = 'source-original' if a.image == 'original' else 'property-optin-original'
            if selected not in name:
                continue
        target = a.directory / name
        if target.is_symlink():
            raise ValueError('refusing symlink output: ' + name)
        if target.exists():
            if sha(target) != digest:
                raise ValueError('existing file differs; move it aside first: ' + name)
            print('OK existing', name, flush=True)
            continue
        fd, temp = tempfile.mkstemp(prefix='.' + name + '.', suffix='.part', dir=a.directory)
        try:
            h = hashlib.sha256(); size = 0
            req = urllib.request.Request(base + urllib.parse.quote(name), headers={'User-Agent': 'redroid-droidspaces-prebuilt'})
            with os.fdopen(fd, 'wb') as out, urllib.request.urlopen(req, timeout=120) as r:
                while data := r.read(1024**2):
                    size += len(data)
                    if size >= 2 * 1024**3:
                        raise ValueError('unexpectedly large release asset')
                    h.update(data); out.write(data)
                    if size % (64 * 1024**2) == 0:
                        print(name, size // 1024**2, 'MiB', flush=True)
                out.flush(); os.fsync(out.fileno())
            if h.hexdigest() != digest:
                raise ValueError('download checksum differs: ' + name)
            # Do not replace an output created concurrently by another process.
            os.link(temp, target); os.unlink(temp)
            print('OK downloaded', name, size, flush=True)
        finally:
            Path(temp).unlink(missing_ok=True)
    sums = a.directory / 'SHA256SUMS'
    if sums.is_symlink():
        raise ValueError('refusing checksum symlink')
    if sums.exists() and sums.read_text() != text:
        raise ValueError('existing SHA256SUMS differs')
    sums.write_text(text)
    print('All selected assets verified. No image was installed or booted.')


if __name__ == '__main__':
    main()

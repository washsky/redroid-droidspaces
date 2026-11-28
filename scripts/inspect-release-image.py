#!/usr/bin/env python3
"""Read-only Linux inspection of a pristine release image (requires sudo)."""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import subprocess
import tempfile


def sha(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for b in iter(lambda: f.read(8 * 1024 * 1024), b''):
            h.update(b)
    return h.hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('image', type=Path)
    p.add_argument('--output-dir', type=Path, required=True)
    a = p.parse_args()
    if os.geteuid():
        p.error('requires root for a private read-only loop mount')
    image = a.image.resolve(strict=True)
    out = a.output_dir.absolute(); out.mkdir(parents=True, exist_ok=True)
    names = ['rootfs-files.sha256', 'rootfs-metadata.json', 'rootfs-xattrs.tsv', 'image-manifest.json']
    if any((out / n).exists() for n in names):
        p.error('refusing to replace existing manifests')
    libc = ctypes.CDLL(None, use_errno=True)
    if libc.unshare(0x00020000):
        raise OSError(ctypes.get_errno(), 'unshare mount namespace')
    subprocess.run(['mount', '--make-rprivate', '/'], check=True)
    entries, hashes, attrs = [], {}, []
    with tempfile.TemporaryDirectory(prefix='.r13-inspect-', dir=out.parent) as td:
        root = Path(td)
        subprocess.run(['mount', '-t', 'ext4', '-o', 'loop,ro,noload', str(image), td], check=True)
        try:
            for name in ['data', 'cache', 'metadata', 'dev', 'proc', 'run', 'sys', 'tmp']:
                if list((root / name).iterdir()):
                    raise ValueError(name + ' contains state')
            for directory, dirs, files in os.walk(root, followlinks=False):
                dirs.sort(); files.sort()
                for name in dirs + files:
                    path = Path(directory) / name; relative = str(path.relative_to(root))
                    s = path.lstat()
                    e = {'path': relative, 'mode': oct(stat.S_IMODE(s.st_mode)), 'uid': s.st_uid, 'gid': s.st_gid}
                    if path.is_symlink():
                        e.update(type='symlink', target=os.readlink(path))
                    elif stat.S_ISDIR(s.st_mode):
                        e['type'] = 'directory'
                    elif stat.S_ISREG(s.st_mode):
                        e.update(type='file', size=s.st_size, sha256=sha(path)); hashes[relative] = e['sha256']
                        if s.st_size < 1024**2 and re.search(br'-----BEGIN (?:[A-Z0-9 ]+ )?PRIVATE KEY-----', path.read_bytes()):
                            raise ValueError('private key in image: ' + relative)
                    else:
                        raise ValueError('unexpected special file: ' + relative)
                    entries.append(e)
                    for attr in sorted(os.listxattr(path, follow_symlinks=False)):
                        attrs.append(relative + '\t' + attr + '\t' + os.getxattr(path, attr, follow_symlinks=False).hex())
            profile = json.loads((root / 'system/etc/arm13-release.json').read_text())
            # Compare against the original, published observed core inputs.
            repo = Path(__file__).resolve().parents[1]
            expected = json.loads((repo / 'artifacts/hashes.json').read_text())['device_files_sha256']
            for path, digest in expected.items():
                if hashes.get(path.lstrip('/')) != digest:
                    raise ValueError('core input differs from recorded baseline: ' + path)
        finally:
            subprocess.run(['umount', td], check=True)
    (out / names[0]).write_text(''.join(digest + '  ' + path + '\n' for path, digest in sorted(hashes.items())))
    (out / names[1]).write_text(json.dumps(entries, ensure_ascii=False, indent=2) + '\n')
    (out / names[2]).write_text('\n'.join(attrs) + '\n')
    result = {'schema': 1, 'image': image.name, 'image_size_bytes': image.stat().st_size,
              'image_sha256': sha(image), 'regular_files': len(hashes), 'metadata_entries': len(entries),
              'xattr_entries': len(attrs), 'userdata_entries': 0, 'profile': profile,
              'validation': 'pristine filesystem and recorded core hashes; runtime results recorded separately'}
    (out / names[3]).write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    for name in names:
        os.chown(out / name, int(os.environ.get('SUDO_UID', '0')), int(os.environ.get('SUDO_GID', '0')))
    print(json.dumps({k: v for k, v in result.items() if k != 'profile'}, ensure_ascii=False), flush=True)


if __name__ == '__main__':
    main()

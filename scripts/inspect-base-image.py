#!/usr/bin/env python3
"""Read-only inventory of a historical base image, including nonempty initial data.

Unlike inspect-release-image.py this does not require a clean current-device
profile or compare old files against newer binaries. Run with sudo on Linux.
"""
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
    with path.open('rb') as f:
        for block in iter(lambda: f.read(4*1024*1024), b''):
            h.update(block)
    return h.hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('image', type=Path)
    p.add_argument('--directory', type=Path, required=True)
    p.add_argument('--prefix', required=True)
    a = p.parse_args()
    if os.geteuid() or not re.fullmatch(r'[a-z0-9-]+', a.prefix):
        p.error('requires root and a simple lowercase prefix')
    a.image = a.image.resolve(strict=True)
    a.directory = a.directory.absolute()
    libc = ctypes.CDLL(None, use_errno=True)
    if libc.unshare(0x00020000):
        raise OSError(ctypes.get_errno(), 'unshare mount namespace')
    subprocess.run(['mount', '--make-rprivate', '/'], check=True)
    a.directory.mkdir(parents=True, exist_ok=True)
    outputs = [a.directory/(a.prefix+s) for s in ['-files.sha256', '-metadata.json', '-xattrs.tsv', '-inspection.json']]
    if any(p.exists() for p in outputs):
        raise SystemExit('refusing to replace an existing inventory')
    check = subprocess.run(['/usr/sbin/e2fsck', '-fn', str(a.image)], capture_output=True, text=True)
    rows = []; sums = []; xattrs = []; data = []; problems = []; props = {}
    private_key = re.compile(rb'-----BEGIN (?:OPENSSH |RSA |EC |DSA |ENCRYPTED )?PRIVATE KEY-----\r?\n[A-Za-z0-9+/=]{40}')
    with tempfile.TemporaryDirectory(prefix='inspect-arm64-base-') as temp:
        root = Path(temp)
        subprocess.run(['mount', '-t', 'ext4', '-o', 'loop,ro,noload', str(a.image), str(root)], check=True)
        try:
            for path in sorted(root.rglob('*')):
                s = path.lstat(); rel = path.relative_to(root).as_posix()
                kind = 'file' if stat.S_ISREG(s.st_mode) else ('directory' if stat.S_ISDIR(s.st_mode) else ('symlink' if stat.S_ISLNK(s.st_mode) else 'special'))
                item = {'path': rel, 'type': kind, 'uid': s.st_uid, 'gid': s.st_gid, 'mode': oct(stat.S_IMODE(s.st_mode)), 'bytes': s.st_size}
                if kind == 'symlink':
                    item['target'] = os.readlink(path)
                if kind == 'file':
                    item['sha256'] = sha(path); sums.append(item['sha256']+'  '+rel+'\n')
                    previous = b''
                    with path.open('rb') as f:
                        for block in iter(lambda: f.read(1024*1024), b''):
                            if private_key.search(previous+block):
                                problems.append('private key material: '+rel); break
                            previous = block[-256:]
                    if rel.startswith('data/'):
                        data.append({'path': rel, 'bytes': s.st_size, 'sha256': item['sha256']})
                    if path.name in {'adbkey', 'adb_keys', 'magisk.db', 'accounts.db', 'accounts_ce.db', 'accounts_de.db', 'WifiConfigStore.xml'}:
                        problems.append('authorization/account path: '+rel)
                for name in sorted(os.listxattr(path, follow_symlinks=False)):
                    value = os.getxattr(path, name, follow_symlinks=False)
                    xattrs.append(rel+'\t'+name+'\t'+value.hex()+'\n')
                rows.append(item)
            for rel in ['system/build.prop', 'vendor/build.prop']:
                if (root/rel).is_file():
                    props[rel] = {k:v for k,v in (line.split('=',1) for line in (root/rel).read_text().splitlines() if line and not line.startswith('#') and '=' in line)}
        finally:
            subprocess.run(['umount', str(root)], check=True)
    report = {'schema':1, 'image_size_bytes':a.image.stat().st_size, 'image_sha256':sha(a.image),
              'files':len(sums), 'metadata_entries':len(rows), 'xattrs':len(xattrs),
              'userdata_empty':not data, 'initial_data_files':data, 'sensitive_findings':problems,
              'e2fsck_exit':check.returncode, 'e2fsck_output':(check.stdout+check.stderr).replace(str(a.image), a.image.name),
              'build_properties':props, 'original_bytes_preserved':True,
              'runtime_validation':'Not booted by this inspection; no cross-device result inferred.'}
    for path, text in zip(outputs, [''.join(sums), json.dumps(rows,indent=2)+'\n', ''.join(xattrs), json.dumps(report,indent=2)+'\n']):
        path.write_text(text)
        os.chown(path, int(os.environ.get('SUDO_UID','0')), int(os.environ.get('SUDO_GID','0')))
    print(json.dumps({k:report[k] for k in ['image_size_bytes','image_sha256','files','xattrs','userdata_empty','sensitive_findings','e2fsck_exit']}), flush=True)
    if problems:
        raise SystemExit('sensitive material found; do not publish')


if __name__ == '__main__':
    main()

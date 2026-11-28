#!/usr/bin/env python3
"""Assemble a new ext4 from an existing root skeleton and a static-only export.

Run with sudo on Linux. The script creates its own private mount namespace and
never mounts an input read-write. This is an image builder, not a phone deployer.
"""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import subprocess
import tempfile


def run(*args):
    subprocess.run([str(x) for x in args], check=True)


def sha(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for data in iter(lambda: f.read(8 * 1024 * 1024), b''):
            h.update(data)
    return h.hexdigest()


def child(root, name):
    p = PurePosixPath(name)
    if p.is_absolute() or '..' in p.parts or not p.parts:
        raise ValueError('unsafe relative path: ' + name)
    target = root.joinpath(*p.parts)
    for parent in list(target.parents):
        if parent == root:
            break
        if parent.is_symlink():
            raise ValueError('symlink parent: ' + name)
    return target


def install(source, dest, mode):
    dest.parent.mkdir(parents=True, exist_ok=True)
    temp = dest.with_name(dest.name + '.release-new')
    if temp.exists():
        raise ValueError('temporary destination exists')
    shutil.copyfile(source, temp)
    os.chown(temp, 0, 0)
    os.chmod(temp, mode)
    os.replace(temp, dest)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--base-image', required=True, type=Path)
    p.add_argument('--static-archive', required=True, type=Path)
    p.add_argument('--metadata', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    p.add_argument('--tools-dir', required=True, type=Path)
    p.add_argument('--manager-apk', required=True, type=Path)
    p.add_argument('--manifest', required=True, type=Path)
    p.add_argument('--size-gib', type=int, default=12)
    a = p.parse_args()
    if os.geteuid() or not 8 <= a.size_gib <= 64:
        p.error('requires root and a size between 8 and 64 GiB')
    for k in ['base_image', 'static_archive', 'metadata', 'tools_dir', 'manager_apk', 'manifest']:
        setattr(a, k, getattr(a, k).resolve(strict=True))
    a.output = a.output.absolute()
    if a.output.exists():
        p.error('refusing to replace an existing output')
    libc = ctypes.CDLL(None, use_errno=True)
    if libc.unshare(0x00020000) != 0:  # CLONE_NEWNS
        raise OSError(ctypes.get_errno(), 'unshare mount namespace')
    run('mount', '--make-rprivate', '/')
    a.output.parent.mkdir(parents=True, exist_ok=True)
    with a.output.open('xb') as f:
        f.truncate(a.size_gib * 1024**3)
    # New e2fsprogs defaults may be newer than the Android 5.10 host kernel.
    run('/usr/sbin/mkfs.ext4', '-F', '-q', '-b', '4096', '-m', '0', '-L', 'R13ARM64REL',
        '-O', '^orphan_file,^metadata_csum_seed', '-E', 'lazy_itable_init=0,lazy_journal_init=0', a.output)
    repo = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix='.r13-build-', dir=a.output.parent) as td:
        base, root = Path(td) / 'base', Path(td) / 'root'
        base.mkdir(); root.mkdir()
        mounted = []
        try:
            run('mount', '-t', 'ext4', '-o', 'loop,ro,noload', a.base_image, base); mounted.append(base)
            run('mount', '-t', 'ext4', '-o', 'loop,rw', a.output, root); mounted.append(root)
            root_stat = base.stat()
            os.chown(root, root_stat.st_uid, root_stat.st_gid)
            os.chmod(root, stat.S_IMODE(root_stat.st_mode))
            for name in os.listxattr(base):
                os.setxattr(root, name, os.getxattr(base, name))
            # Only root-level directory metadata and known AOSP symlinks. No
            # old data/cache/metadata/dev/run/proc contents enter the new image.
            links = {'bin', 'bugreports', 'd', 'etc', 'init', 'product', 'sdcard', 'system_ext'}
            for entry in base.iterdir():
                s = entry.lstat(); dest = root / entry.name
                if stat.S_ISDIR(s.st_mode):
                    dest.mkdir(exist_ok=True)
                    os.chown(dest, s.st_uid, s.st_gid)
                    os.chmod(dest, stat.S_IMODE(s.st_mode))
                    for name in os.listxattr(entry, follow_symlinks=False):
                        os.setxattr(dest, name, os.getxattr(entry, name, follow_symlinks=False), follow_symlinks=False)
                elif stat.S_ISLNK(s.st_mode) and entry.name in links:
                    dest.symlink_to(os.readlink(entry)); os.lchown(dest, s.st_uid, s.st_gid)
            # init is also in the static tar; remove just this skeleton symlink.
            (root / 'init').unlink(missing_ok=True)
            run('tar', '--zstd', '-xf', a.static_archive, '-C', root, '--numeric-owner', '--same-owner', '--same-permissions')
            count = 0
            for line in a.metadata.read_text().splitlines():
                if line.startswith('#stat\t'):
                    continue
                relative, name, value = line.split('\t')
                target = child(root, relative)
                os.setxattr(target, name, bytes.fromhex(value), follow_symlinks=False)
                if os.getxattr(target, name, follow_symlinks=False) != bytes.fromhex(value):
                    raise ValueError('xattr mismatch: ' + relative)
                count += 1
            # These are the runtime-address-aware controllers from the pinned
            # source. All core and device libraries keep their exported bytes.
            for name in ['arm13-proxyctl', 'arm13-bootctl']:
                install(a.tools_dir / name, root / 'vendor/bin' / name, 0o755)
            install(repo / 'config/bin/arm13-release-setup', root / 'vendor/bin/arm13-release-setup', 0o755)
            install(repo / 'config/init/000-arm13-release.rc', root / 'vendor/etc/init/000-arm13-release.rc', 0o644)
            install(a.manager_apk, root / 'system/app/UIDNetworkManager/UIDNetworkManager.apk', 0o644)
            install(a.manifest, root / 'system/etc/arm13-release.json', 0o644)
            os.chmod(root / 'system/app/UIDNetworkManager', 0o755)
            system_label = os.getxattr(root / 'system/app', 'security.selinux')
            vendor_label = os.getxattr(root / 'vendor/bin/arm13-netbroker', 'security.selinux')
            vendor_config_label = os.getxattr(root / 'vendor/etc/init', 'security.selinux')
            for relative in ['system/app/UIDNetworkManager', 'system/app/UIDNetworkManager/UIDNetworkManager.apk', 'system/etc/arm13-release.json']:
                os.setxattr(root / relative, 'security.selinux', system_label, follow_symlinks=False)
            for relative in ['vendor/bin/arm13-proxyctl', 'vendor/bin/arm13-bootctl', 'vendor/bin/arm13-release-setup']:
                os.setxattr(root / relative, 'security.selinux', vendor_label, follow_symlinks=False)
            os.setxattr(root / 'vendor/etc/init/000-arm13-release.rc', 'security.selinux', vendor_config_label, follow_symlinks=False)
            assert not list((root / 'data').iterdir()), 'data must remain empty'
            for name in ['cache', 'metadata', 'dev', 'proc', 'run', 'sys', 'tmp']:
                assert not list((root / name).iterdir()), name + ' must remain empty'
            os.sync()
            print(json.dumps({'xattrs_restored': count, 'size_bytes': a.output.stat().st_size,
                              'core_init_sha256': sha(root / 'system/bin/init'),
                              'cap_init_sha256': sha(root / 'init.arm13-caps'),
                              'data_entries': 0}), flush=True)
        finally:
            for mount in reversed(mounted):
                run('umount', mount)
    run('/usr/sbin/e2fsck', '-fn', a.output)
    os.chown(a.output, int(os.environ.get('SUDO_UID', '0')), int(os.environ.get('SUDO_GID', '0')))
    print('Image SHA256:', sha(a.output), flush=True)


if __name__ == '__main__':
    main()

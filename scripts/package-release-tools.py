#!/usr/bin/env python3
"""Package explicitly selected ARM64 helpers; never collect private/out recursively."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import subprocess
import tarfile
import tempfile


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    root = Path(__file__).resolve().parents[1]
    # name: (source, installed path in a compatible rootfs; empty for diagnostics)
    binaries = {
        'arm13-netbroker': ('src/netmanager/netbroker.c', '/vendor/bin/arm13-netbroker'),
        'arm13-netctl': ('src/netmanager/netctl.c', '/vendor/bin/arm13-netctl'),
        'arm13-proxy-relay': ('src/netmanager/proxy-relay.c', '/vendor/bin/arm13-proxy-relay'),
        'arm13-proxyctl': ('src/netmanager/proxyctl.c', '/vendor/bin/arm13-proxyctl'),
        'arm13-bootctl': ('src/netmanager/bootctl.c', '/vendor/bin/arm13-bootctl'),
        'arm13-cap-init': ('src/root/cap-init.c', '/init.arm13-caps'),
        'arm13-su-login': ('src/root/magisk-login-compat.c', '/vendor/bin/arm13-su-login'),
        'arm13-magisk-daemon-guard': ('src/root/magisk-daemon-guard.c', '/vendor/bin/arm13-magisk-daemon-guard'),
        'arm13-magisk-module-mounts': ('src/root/magisk-module-mounts.c', '/vendor/bin/arm13-magisk-module-mounts'),
        'libarm13_socketbroker.so': ('src/netmanager/preload.c', '/system/lib64/libarm13_socketbroker.so'),
        'libmagisk_container_context.so': ('src/root/magisk-context-guard.c', '/system/lib64/libmagisk_container_context.so'),
        'libarm13_codec_vulkan.so': ('src/surface/codec-vulkan-select.c', '/vendor/lib64/arm13-codec-compat/libarm13_codec_vulkan.so'),
        'libarm13_linear_window.so': ('src/surface/linear-window.c', '/vendor/lib64/arm13-codec-compat/libarm13_linear_window.so'),
        'arm13-binderfs-add': ('src/probes/binderfs-add.c', ''),
        'arm13-export-static-xattrs': ('src/release/export-xattrs.c', ''),
        'arm13-gzip-to-sparse': ('src/release/gzip-to-sparse.c', ''),
        'arm13-ahb-is-supported': ('src/gralloc/ahb-is-supported.c', ''),
        'arm13-find-gralloc-leak': ('src/gralloc/find-gralloc-leak.c', ''),
        'arm13-isolated-gralloc-probe': ('src/gralloc/isolated-probe.c', ''),
        'arm13-gralloc-lock-read': ('src/probes/read-gralloc-mutex.c', ''),
        'arm13-zygisk-peer-negative': ('src/probes/zygisk/peer-negative.c', ''),
        'arm13-fd-transport': ('src/probes/zygisk/fd-transport.c', ''),
        'arm13-zygisk-probe.zip': ('src/probes/zygisk/module.cpp', ''),
    }
    payload = {}
    manifest = []
    for name, (source, install) in binaries.items():
        path = root / 'out/native' / name
        folder = 'lib' if name.endswith('.so') else ('probes' if not install and name != 'arm13-gzip-to-sparse' and name != 'arm13-binderfs-add' and name != 'arm13-export-static-xattrs' else 'bin')
        rel = folder + '/' + name
        payload[rel] = (path.read_bytes(), 0o644 if name.endswith(('.so', '.zip')) else 0o755)
        builder = 'scripts/build-zygisk-probe.sh' if 'zygisk' in name or name == 'arm13-fd-transport' else 'scripts/build-native.sh'
        manifest.append({'file': rel, 'sha256': digest(path), 'bytes': path.stat().st_size,
                         'source': source, 'source_sha256': digest(root/source),
                         'build_script': builder, 'build_script_sha256': digest(root/builder),
                         'compatible_rootfs_install_reference': install or None,
                         'installation_automatic': False})
    # Explicit public config and source/header trees; no local lab.env or build output.
    for folder in ('config', 'LICENSES', 'src'):
        for path in sorted((root/folder).rglob('*')):
            if path.is_file():
                rel = path.relative_to(root).as_posix()
                payload[rel] = (path.read_bytes(), 0o755 if folder == 'config' and path.parent.name == 'bin' else 0o644)
    for path in ('scripts/build-native.sh', 'scripts/build-zygisk-probe.sh', 'THIRD_PARTY.md'):
        payload[path] = ((root/path).read_bytes(), 0o755 if path.endswith('.sh') else 0o644)
    metadata = {'schema': 1, 'architecture': 'AArch64', 'android_api': 33, 'ndk': 'r29',
                'release_ref': 'v2026.11.28-arm64', 'binaries': manifest,
                'scope': 'Source-built helpers; excludes proprietary QTI binaries. ABI/runtime prerequisites still apply. Runtime libraries in the image may be older validated builds; compare rootfs-files.sha256 before replacing anything.'}
    payload['manifest.json'] = ((json.dumps(metadata, indent=2, ensure_ascii=False)+'\n').encode(), 0o644)
    readme = '''# ARM64 prebuilt helpers

For Android13/API33 ARM64. This is not an installer. Use the full image for the
validated current-device combination. Read docs/PREBUILT_RELEASES.md in the repo.

manifest.json maps every binary to source, build-script hashes and an installation
REFERENCE. Never overwrite an active ELF/library or another instance's config.
Private QTI binaries are in the device-specific image, not this tools archive.
Source and headers are included; licenses retain their original terms.
Diagnostics require appropriate target/root permissions; they are not all needed
for normal operation. The Zygisk probe ZIP is optional and not preinstalled.
bin/arm13-gzip-to-sparse creates only new files and rejects size/CRC mismatches.
The binder helper requires the real BinderFS control and three unused names.
'''
    payload['README.md'] = (readme.encode(), 0o644)
    sums = ''.join(hashlib.sha256(data).hexdigest()+'  '+name+'\n' for name,(data,_) in sorted(payload.items()))
    payload['SHA256SUMS'] = (sums.encode(), 0o644)
    a.output.parent.mkdir(parents=True, exist_ok=True)
    if a.output.exists():
        raise SystemExit('output exists; refusing to overwrite')
    with tempfile.TemporaryDirectory(prefix='arm13-tools-') as temp:
        raw = Path(temp)/'tools.tar'
        with tarfile.open(raw, 'w') as tar:
            for name, (data, mode) in sorted(payload.items()):
                info = tarfile.TarInfo('arm64-tools/'+name)
                info.size = len(data); info.mode = mode; info.uid = info.gid = 0
                info.uname = info.gname = 'root'; info.mtime = 1795780800
                tar.addfile(info, io.BytesIO(data))
        subprocess.run(['zstd', '-T2', '-10', str(raw), '-o', str(a.output)], check=True)
    print(json.dumps({'files': len(payload), 'bytes': a.output.stat().st_size, 'sha256': digest(a.output)}))


if __name__ == '__main__':
    main()

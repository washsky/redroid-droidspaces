#!/usr/bin/env python3
"""Package hash-pinned AOSP compatibility files without touching a phone."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import subprocess
import tarfile
import tempfile


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--input', type=Path, required=True, help='root containing system/lib64 and vendor/bin outputs')
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    root = Path(__file__).resolve().parents[1]
    values = [
        ('system/lib64/libandroid_runtime.so', '41ebfbeb2634b53fc40482d237452538bdfd662f503c965a5e20b94265895b15', '8c1a525238306509bdb33f7cb5d2960da0cfad03d1b731c1b045a01752a24b99', 'patches/aosp/frameworks__base.patch', 0o644),
        ('vendor/bin/ipconfigstore', '6cc88412039ab4ebc0ddb66411d49eaa38cdaea81aac1378c0b86ce9c9ec3108', '762338667e513ae7b09708ff7f9cda68162370ba7f47580a3ca0726c777ceb66', 'patches/aosp/vendor__redroid.patch', 0o755),
        ('system/apex/com.android.runtime/bin/linker64', 'c948b3d0b8dd152adbdabd515b69f15c622a16ff2b1d59766386777de98d1031', 'd329f60ff9aa2558c5e4ef8381b1b1a33287d6f0e57159c4cb32a002ebfd2101', 'patches/aosp/bionic.patch', 0o755),
        ('system/apex/com.android.runtime/bin/linkerconfig', '4a896447054431add8d31839a89125d5e115d00b15989b529761f1a51321fa64', '5598fa2fe475a999b6b2f844f66beb84f0ad64783a0d4822eb170c14e461054c', 'patches/aosp/bionic.patch', 0o755),
        ('system/apex/com.android.runtime/lib64/bionic/libc.so', '115175233eb452be1b420c976b5b6a6f69901e576ab4f8a4fd067ab3d02eab63', 'df30f385690e8ab6d2f9b14672383b342295d3cbf61e6fb9a7c76449a1ebd977', 'patches/aosp/bionic.patch', 0o644),
        ('system/bin/bootstrap/linker64', '814dcdcad569d9b7e40a65be8bda30e79a3a5d9af8af3641bf2b337a58010f21', 'a9f8dc6bee40b32629a33b22369da95ee92b91385d859770ec0d8cb060419e73', 'patches/aosp/bionic.patch', 0o755),
        ('system/bin/bootstrap/linkerconfig', '81c2d0ce9675063b755298ed11b57cd58dc897a94c644dda07deeca447cf8dd1', '879ff4c8dc5b63d523fb447c9ddb02410c3247ef43933666a9c62ab8ead7f025', 'patches/aosp/bionic.patch', 0o755),
        ('system/bin/snapuserd', '4b01432e4c6005cae75cc3b981be415b1c098488ad43ab748b93f0a4b1fc8f62', 'b6c51dbfc4fe314669055df770d377a26d855e06f166a7b533761eb58739c74a', 'patches/aosp/bionic.patch', 0o755),
        ('system/lib64/bootstrap/libc.so', '115175233eb452be1b420c976b5b6a6f69901e576ab4f8a4fd067ab3d02eab63', 'df30f385690e8ab6d2f9b14672383b342295d3cbf61e6fb9a7c76449a1ebd977', 'patches/aosp/bionic.patch', 0o644),
    ]
    payload = {}; entries = []
    for rel, expected, old, patch, mode in values:
        content = (a.input/rel).read_bytes()
        if hashlib.sha256(content).hexdigest() != expected:
            raise SystemExit('unsupported fix input: '+rel)
        if content[:4] != b'\x7fELF' or int.from_bytes(content[18:20], 'little') != 183:
            raise SystemExit('not an AArch64 ELF: '+rel)
        payload['rootfs/'+rel] = (content, mode)
        entries.append({'path':'/'+rel, 'sha256':expected, 'known_original_sha256':old,
                        'owner':'0:0', 'mode':oct(mode), 'source_patch':patch,
                        'source_patch_sha256':hashlib.sha256((root/patch).read_bytes()).hexdigest(),
                        'preserve_existing_xattrs':True,
                        'origin':'historical opt-in image; also matches validated integration' if patch.endswith('/bionic.patch') else 'Ubuntu20 current AOSP output; also matches validated integration',
                        'source_note':'Bionic compatibility transition and dependent relinks; patch is a reference to the recorded source changes, not proof of an exact historical full-tree build.' if patch.endswith('/bionic.patch') else 'Recorded source patch reference.'})
    for rel in ['patches/aosp/frameworks__base.patch', 'patches/aosp/vendor__redroid.patch', 'patches/aosp/bionic.patch', 'patches/aosp/source-patches.json', 'aosp-manifest-pinned.xml']:
        payload[rel] = ((root/rel).read_bytes(), 0o644)
    payload['LICENSES/Apache-2.0.txt'] = ((root/'LICENSES/scrcpy-LICENSE').read_bytes(), 0o644)
    manifest = {'schema':1, 'architecture':'AArch64', 'android_api':33,
                'origin':'Two Ubuntu20 AOSP output fixes plus seven historical opt-in Bionic/dependent files; all hashes match the existing validated integration',
                'standalone_base_combination_boot_tested':False, 'arbitrary_processors_verified':False,
                'device_private_libraries_included':False, 'automatic_install':False, 'files':entries}
    payload['manifest.json'] = ((json.dumps(manifest,indent=2)+'\n').encode(), 0o644)
    readme = '''# Portable AOSP compatibility files for the historical ARM64 base

These nine AArch64/API33 files are not QTI/GPU libraries. Seven form the observed
source-original -> property-opt-in Bionic/linker/dependent binary transition;
the other two fix Zygote runtime and DHCP. All match the validated integration.
The newly assembled base+fixes
combination was NOT boot-tested in this publication run. Other devices must
complete docs/porting/SOFTWARE_FIRST.md. There is no automatic installer.

Keep original images unchanged. Make a new OFFLINE working copy; replay/check
its ext4 journal first. In a private mount namespace verify the backing image,
Android13/arm64 build and known_original_sha256 (or already-final sha256) of ALL
selected destination files. Copy the transition as a set: a marker alone cannot
replace the changed libc/linker files. Only then copy rootfs/ files, preserving
existing xattrs, setting uid/gid0 and the listed modes. Record final
hashes, sync/unmount and fsck the working copy. Do not overwrite an active ELF,
the host Android or a different Android/ABI tree.

The original source image also needs the recorded property-label opt-in marker
for that compatibility branch. New ipconfigstore requires the container init
to choose ro.boot.redroid_net_mode=dhcp BEFORE invoking it. Do not copy all
hardware-era init settings blindly: keep the software Codec/GPU baseline.
No Magisk, UID broker, hardware driver or arbitrary platform compatibility is
silently added by these files. The full pinned source/patch history is in
the repository; legacy rootfs wrapper source gaps remain documented.
'''
    payload['README.md'] = (readme.encode(), 0o644)
    sums = ''.join(hashlib.sha256(data).hexdigest()+'  '+rel+'\n' for rel,(data,_) in sorted(payload.items()))
    payload['SHA256SUMS'] = (sums.encode(), 0o644)
    if a.output.exists():
        raise SystemExit('refusing existing output')
    a.output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='arm13-core-package-') as td:
        raw = Path(td)/'core.tar'
        with tarfile.open(raw,'w') as t:
            for rel,(data,mode) in sorted(payload.items()):
                info = tarfile.TarInfo('portable-core/'+rel);info.size=len(data)
                info.uid=info.gid=0;info.uname=info.gname='root';info.mode=mode;info.mtime=1795780800
                t.addfile(info,io.BytesIO(data))
        subprocess.run(['zstd','-T2','-10',str(raw),'-o',str(a.output)],check=True)
    print('Packaged nine hash-verified AOSP compatibility files; no image was modified.')


if __name__ == '__main__':
    main()

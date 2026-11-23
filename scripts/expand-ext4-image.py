#!/usr/bin/env python3
"""Offline grow of a regular ext4 image with a new verified full backup."""
import argparse
import hashlib
import os
import shutil
import subprocess
from pathlib import Path

def digest(p):
    h=hashlib.sha256()
    with p.open("rb") as f:
        for block in iter(lambda:f.read(8*1024*1024),b""):
            h.update(block)
    return h.hexdigest()

def offline(image):
    target=str(image.resolve())
    for f in Path("/sys/block").glob("loop*/loop/backing_file"):
        try:
            backing=f.read_text().strip()
        except OSError:
            continue
        if backing and str(Path(backing).resolve())==target:
            raise SystemExit("Image is attached to "+str(f.parent.parent)+"; stop/unmount the target first")
    mounts=Path("/proc/self/mountinfo")
    if mounts.exists() and target in mounts.read_text():
        raise SystemExit("Image appears in mountinfo; refusing")

def binary(name):
    found=shutil.which(name)
    if found:return found
    for folder in ["/usr/sbin","/sbin","/system/bin"]:
        candidate=Path(folder)/name
        if candidate.is_file() and os.access(candidate,os.X_OK):return str(candidate)
    raise SystemExit("Missing "+name)

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("image",type=Path)
    ap.add_argument("--add-gib",type=int,required=True)
    ap.add_argument("--backup",type=Path,required=True)
    a=ap.parse_args()
    if a.image.is_symlink() or not a.image.is_file() or a.add_gib<=0:
        raise SystemExit("Need a regular image file and a positive growth")
    if a.backup.exists() or a.backup.is_symlink() or a.backup.resolve()==a.image.resolve():
        raise SystemExit("Backup must be a new independent path")
    fsck=binary("e2fsck");resize=binary("resize2fs")
    offline(a.image)
    old=a.image.stat().st_size
    if shutil.disk_usage(a.backup.parent).free < old + 128*1024*1024:
        raise SystemExit("Insufficient space for a full backup plus metadata margin")
    with a.image.open("rb") as src,a.backup.open("xb") as dst:
        shutil.copyfileobj(src,dst,8*1024*1024);dst.flush();os.fsync(dst.fileno())
    before=digest(a.image)
    if digest(a.backup)!=before:raise SystemExit("Backup SHA256 mismatch; image unchanged")
    offline(a.image)
    rc=subprocess.run([fsck,"-f","-p",str(a.image)]).returncode
    if rc not in (0,1):raise SystemExit("e2fsck failed; backup retained")
    new=old+a.add_gib*1024**3
    with a.image.open("r+b") as f:f.truncate(new)
    subprocess.run([resize,str(a.image)],check=True)
    subprocess.run([fsck,"-f","-n",str(a.image)],check=True)
    print("backup_sha256="+before)
    print("old_bytes="+str(old)+" new_bytes="+str(new))
    print("Update only the target config sparse_image_size_gb; then boot and validate df/read-write.")

if __name__=="__main__":
    main()

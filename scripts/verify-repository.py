#!/usr/bin/env python3
"""Validate local documentation links, public-file hygiene and checksum manifest."""
import hashlib
import json
import re
from pathlib import Path
r=Path(__file__).resolve().parents[1]
errors=[]
for p in r.rglob("*"):
    if not p.is_file() or ".git" in p.parts or "out" in p.relative_to(r).parts:continue
    rel=str(p.relative_to(r))
    if p.suffix in {".img",".apk",".so",".ko",".pk8",".pem",".key",".jks",".keystore"}:
        errors.append("unexpected binary/key: "+rel)
    if p.suffix==".md":
        text=p.read_text()
        for target in re.findall(r'\]\(([^)]+)\)',text):
            target=target.split("#",1)[0]
            if not target or re.match(r'^[a-z]+:',target):continue
            if not (p.parent/target).exists():errors.append(rel+": missing link "+target)
manifest=r/"artifacts/repository-files.sha256"
if manifest.exists():
    for line in manifest.read_text().splitlines():
        sha,name=line.split("  ",1)
        p=r/name
        if not p.is_file() or hashlib.sha256(p.read_bytes()).hexdigest()!=sha:errors.append("checksum mismatch: "+name)
if errors:raise SystemExit("\n".join(errors))
print("repository checks passed")

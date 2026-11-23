#!/usr/bin/env python3
"""Apply recorded diffs to their exact AOSP project HEADs; no repo sync/reset."""
import argparse
import json
import subprocess
from pathlib import Path

def git(root, *args):
    return subprocess.run(["git", "-C", str(root), *args], check=True, capture_output=True, text=True).stdout.strip()

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("aosp", type=Path)
    ap.add_argument("--check", action="store_true", help="validate without applying")
    args = ap.parse_args()
    patches = Path(__file__).resolve().parents[1] / "patches/aosp"
    entries = json.loads((patches / "source-patches.json").read_text())
    pending = []
    for e in entries:
        project = args.aosp / e["project"]
        if git(project, "rev-parse", "HEAD") != e["base"]:
            raise SystemExit("Wrong HEAD: " + e["project"])
        p = patches / e["patch"]
        # Permit a repeat check on a tree whose recorded patch is already applied.
        reverse = subprocess.run(["git", "-C", str(project), "apply", "--reverse", "--check", str(p)], capture_output=True)
        if reverse.returncode == 0:
            print("already applied:", e["project"])
            continue
        git(project, "apply", "--check", str(p))
        pending.append((project, p))
    if args.check:
        print("validated patches:", len(entries))
        return
    for project, patchfile in pending:
        git(project, "apply", str(patchfile))
        print("applied:", project.name)

if __name__ == "__main__":
    main()

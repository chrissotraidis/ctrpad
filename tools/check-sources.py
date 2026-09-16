#!/usr/bin/env python3
"""Verify vendored source trees, including modes, without Git or network access."""
import hashlib
import json
from pathlib import Path
import os
import sys

ROOT = Path(__file__).resolve().parents[1]


def object_hash(kind, data):
    return hashlib.sha1(kind.encode() + b' ' + str(len(data)).encode() + b'\0' + data).digest()


def tree_hash(directory):
    entries = []
    for path in directory.iterdir():
        if path.is_symlink():
            mode, digest = b'120000', object_hash('blob', os.fsencode(os.readlink(path)))
        elif path.is_dir():
            mode, digest = b'40000', tree_hash(path)
        else:
            mode = b'100755' if path.stat().st_mode & 0o111 else b'100644'
            digest = object_hash('blob', path.read_bytes())
        name = os.fsencode(path.name)
        entries.append((name + (b'/' if mode == b'40000' else b''), mode + b' ' + name + b'\0' + digest))
    return object_hash('tree', b''.join(entry for _, entry in sorted(entries)))


def main():
    lock = json.loads((ROOT / 'sources.lock.json').read_text())
    failed = False
    for component in lock['vendored']:
        actual = tree_hash(ROOT / component['path']).hex()
        if actual != component['tree']:
            print(f"MISMATCH {component['path']}: {actual} != {component['tree']}", file=sys.stderr)
            failed = True
        else:
            print(f"OK {component['path']} {actual}")
    return int(failed)


if __name__ == '__main__':
    sys.exit(main())

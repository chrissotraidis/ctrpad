#!/usr/bin/env python3
"""Validate synthetic XA manifests through the application's production reader."""
from pathlib import Path
import struct
import subprocess
import sys
import tempfile


def manifest(positions=0, tracks=0, songs=0, first=0, size=0x44):
    data = bytearray(max(size, 0x44))
    for offset, value in [(0, 0x464E4958), (4, 102), (8, 3),
                          (12, positions), (16, tracks), (0x2C, songs), (0x38, first)]:
        struct.pack_into('<I', data, offset, value)
    return data[:size]


with tempfile.TemporaryDirectory(prefix='ctrpad-xa-test-') as tmp:
    root = Path(tmp)
    assets = root / 'assets'
    for name in ['BIGFILE.BIG', 'SOUNDS/KART.HWL', 'TEST.STR', 'XA/MUSIC/S00.XA']:
        path = assets / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b'synthetic fixture, not game data')
    cases = [
        ('empty tables', manifest(), True),
        ('one song', manifest(1, 1, 1, size=0x4C), True),
        ('truncated header', manifest(size=0x43), False),
        ('missing position', manifest(1), False),
        ('missing track', manifest(0, 1), False),
        ('truncated track', manifest(1, 1, 1, size=0x4B), False),
        ('position multiplication wrap', manifest(0x40000000), False),
        ('track multiplication wrap', manifest(0, 0x40000000), False),
        ('maximum positions', manifest(0xFFFFFFFF), False),
        ('maximum tracks', manifest(0, 0xFFFFFFFF), False),
        ('song outside table', manifest(0, 1, 1, 1, 0x48), False),
    ]
    for name, content, valid in cases:
        (assets / 'XA/ENG.XNF').write_bytes(content)
        result = subprocess.run([sys.argv[1], '--self-test-assets', str(root)], capture_output=True, text=True)
        assert result.returncode == (0 if valid else 1), (name, result.stdout, result.stderr)
    (assets / 'XA/ENG.XNF').write_bytes(manifest(1, 1, 1, size=0x4C))
    for missing in ['TEST.STR', 'XA/ENG.XNF', 'XA/MUSIC/S00.XA']:
        path = assets / missing
        content = path.read_bytes()
        path.unlink()
        result = subprocess.run([sys.argv[1], '--self-test-assets', str(root)], capture_output=True, text=True)
        assert result.returncode == 1, (missing, result.stdout, result.stderr)
        path.write_bytes(content)
print('XA manifest checks passed: 11 table cases and 3 required-media failures.')

#!/usr/bin/env python3
"""Check real dependency edits are rejected in a restored tree without .git."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='ctrpad-source-lock-') as tmp:
    restored = Path(tmp)
    for name in ['externals/SDL', 'include/psn00bsdk']:
        shutil.copytree(root / name, restored / name)
    (restored / 'tools').mkdir()
    shutil.copy2(root / 'tools/check-sources.py', restored / 'tools/check-sources.py')
    shutil.copy2(root / 'sources.lock.json', restored / 'sources.lock.json')
    def check(expected):
        result = subprocess.run([sys.executable, str(restored/'tools/check-sources.py')], capture_output=True)
        assert result.returncode == expected, result.stdout + result.stderr
    check(0)
    license_file = restored / 'externals/SDL/LICENSE.txt'
    original = license_file.read_bytes()
    mode = license_file.stat().st_mode
    license_file.write_bytes(original + b'changed\n')
    check(1)
    license_file.write_bytes(original)
    license_file.chmod(mode | 0o111)
    check(1)
    license_file.chmod(mode)
    extra = restored / 'externals/SDL/unreviewed.c'
    extra.write_text('/* unreviewed */\n')
    check(1)
    extra.unlink()
    license_file.unlink()
    check(1)
    license_file.write_bytes(original)
    license_file.chmod(mode)
    check(0)
print('Source lock tests passed: offline restore, changed bytes/modes, extra/missing files.')

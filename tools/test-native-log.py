#!/usr/bin/env python3
"""Exercise real logger files in a disposable directory; no app/user data needed."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='ctrpad-log-test-') as tmp:
    work = Path(tmp)
    harness = work / 'test.c'
    harness.write_text(r'''
#include "platform/native_log.h"
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
    char payload[4096];
    memset(payload, 'x', sizeof(payload));
    payload[4094] = '\n'; payload[4095] = 0;
    if (!Platform_LogSetPath(argv[1])) return 1;
    Platform_LogInit("test");
    Platform_LogInit("test"); /* must not rotate/leak a live stream */
    Platform_Log("test breadcrumb\n");
    if (argc > 2 && strcmp(argv[2], "large") == 0)
        for (int i = 0; i < 2500; ++i) Platform_Log("%s", payload);
    Platform_LogFlush();
    if (argc > 2 && strcmp(argv[2], "interrupted") == 0) _Exit(0);
    Platform_LogShutdown();
    Platform_LogShutdown();
    return 0;
}
''')
    exe = work / 'test'
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror', '-pthread', '-I', str(root/'include'),
                    str(harness), str(root/'platform/native_log.c'), '-o', str(exe)], check=True)
    log = work / 'session.log'
    def run(mode='normal'):
        subprocess.run([str(exe), str(log), mode], stdout=subprocess.DEVNULL, check=True)
        return log.read_text()
    assert 'previous=unavailable' in run()
    assert 'previous=clean' in run('interrupted')
    assert 'previous=interrupted-or-legacy' in run()
    assert 'continuing current session' in run('large')
    assert 'clean_shutdown=yes' in log.read_text()
    assert 'previous=clean' in run()
    for _ in range(5): run()
    assert len(list(work.glob('session.log*'))) == 5
    assert all(p.stat().st_size < 8*1024*1024 + 8192 for p in work.glob('session.log*'))
print('Logger tests passed: clean/interrupted status, duplicate init/shutdown, size rotation, retention.')

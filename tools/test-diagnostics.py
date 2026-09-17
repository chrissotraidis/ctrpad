#!/usr/bin/env python3
"""Exercise the actual Apple export builder with synthetic logs, never user data."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='ctrpad-diagnostics-') as tmp:
    work = Path(tmp)
    harness = work / 'test.m'
    harness.write_text(r'''
#import "native_diagnostics.h"
#include "platform/native_log.h"
#include <assert.h>
#include <pthread.h>
static void *writer(void *unused) {
    for (int i=0; i<2000; i++) Platform_Log("[CTR Game] tick=%d\n", i);
    return NULL;
}
int main(int argc, char **argv) { @autoreleasepool {
    assert(Platform_LogSetPath(argv[1]));
    Platform_LogInit("export-test");
    Platform_Log("[CTR Game] level=3 mode=2\n");
    Platform_Log("Disc=/private/var/mobile/My Game.bin\n");
    Platform_Log("user@example.com\n");
    Platform_Log("C:\\Users\\Secret\\Game.bin\n");
    Platform_Log("id=12345678-abcd-abcd-abcd-123456789abc\n");
    NSString *report=CTRPadDiagnosticsReport(@{@"Build":@3});
    assert([report containsString:@"level=3 mode=2"]);
    assert([report containsString:@"Build: 3"]);
    for (NSString *secret in @[@"My Game", @"user@example", @"Secret", @"12345678-abcd"])
        assert(![report containsString:secret]);
    assert([report containsString:@"Unavailable or empty"]);
    Platform_LogShutdown(); Platform_LogInit("new-session");
    report=CTRPadDiagnosticsReport(@{});
    assert([report containsString:@"level=3 mode=2"]);
    assert([report containsString:@"previous=clean"]);
    char buffer[8192];
    assert(Platform_LogCopySegment(-1,buffer,sizeof buffer)==0);
    assert(Platform_LogCopySegment(5,buffer,sizeof buffer)==0);
    assert(Platform_LogCopySegment(0,NULL,sizeof buffer)==0);
    pthread_t thread; assert(pthread_create(&thread,NULL,writer,NULL)==0);
    for(int i=0;i<20;i++) assert(Platform_LogCopySegment(0,buffer,sizeof buffer)<=sizeof buffer);
    pthread_join(thread,NULL);
    size_t n=Platform_LogCopySegment(0,buffer,sizeof buffer);
    assert(n==sizeof buffer);
    NSString *snapshot=[[NSString alloc] initWithBytes:buffer length:n encoding:NSUTF8StringEncoding];
    assert([snapshot containsString:@"previous=clean"]);
    assert([snapshot containsString:@"middle omitted"]);
    assert([snapshot containsString:@"tick=1999"]);
    Platform_LogShutdown();
} return 0; }
''')
    subprocess.run(['clang','-std=c11','-I',str(root/'include'),'-c',str(root/'platform/native_log.c'),'-o',str(work/'log.o')],check=True)
    subprocess.run(['clang','-fobjc-arc','-fblocks','-framework','Foundation','-I',str(root/'include'),'-I',str(root/'platform/apple'),str(harness),str(root/'platform/apple/native_diagnostics.m'),str(work/'log.o'),'-o',str(work/'test')],check=True)
    subprocess.run([str(work/'test'),str(work/'test.log')],check=True,stdout=subprocess.DEVNULL)
print('Diagnostics tests passed: redaction, metadata, previous logs, missing logs, bounded header/tail, concurrent writes.')

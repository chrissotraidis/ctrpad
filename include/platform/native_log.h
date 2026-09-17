#ifndef NATIVE_LOG_H
#define NATIVE_LOG_H

#include <macros.h>
#include <stddef.h>

int Platform_LogSetPath(const char *path);
const char *Platform_LogGetPath(void);
const char *Platform_LogGetArchivePath(void);
int Platform_LogIsOpen(void);
void Platform_LogInit(const char *appName);
void Platform_LogShutdown(void);
void Platform_LogFlush(void);
/* Bounded, rotation-safe snapshot: segment 0=current, 1..4=older.
 * Returns bytes copied (not NUL terminated), or 0 when unavailable. */
size_t Platform_LogCopySegment(int segment, char *buffer, size_t capacity);
void Platform_Log(const char *fmt, ...) CTR_PRINTF_FORMAT(1, 2);
void Platform_LogWarn(const char *fmt, ...) CTR_PRINTF_FORMAT(1, 2);
void Platform_LogError(const char *fmt, ...) CTR_PRINTF_FORMAT(1, 2);

#endif

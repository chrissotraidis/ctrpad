#include "platform/native_log.h"

#include <macros.h>

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include "platform/native_win32.h"
#endif

#ifdef _WIN32
static SRWLOCK s_logLock = SRWLOCK_INIT;
static void Platform_LogLock(void) { AcquireSRWLockExclusive(&s_logLock); }
static void Platform_LogUnlock(void) { ReleaseSRWLockExclusive(&s_logLock); }
#else
#include <pthread.h>
static pthread_mutex_t s_logLock = PTHREAD_MUTEX_INITIALIZER;
static void Platform_LogLock(void) { pthread_mutex_lock(&s_logLock); }
static void Platform_LogUnlock(void) { pthread_mutex_unlock(&s_logLock); }
#endif

global_variable FILE *s_logStream = NULL;
global_variable char s_logPath[512]; // TODO(aalhendi): yeah this is an issue waiting to happen. w/e
global_variable char s_logArchivePath[512];
global_variable uint64_t s_logStartMilliseconds;

#define NATIVE_LOG_ARCHIVE_COUNT 4
#define NATIVE_LOG_MAX_BYTES (8L * 1024L * 1024L)

// Inspect only the bounded tail; missing shutdown means interrupted, not proven crash.
internal const char *Platform_LogPreviousStatus(void)
{
	char tail[256];
	FILE *previous = fopen(s_logPath, "rb");
	long size;
	size_t count;
	if (previous == NULL) return "unavailable";
	if (fseek(previous, 0, SEEK_END) != 0 || (size = ftell(previous)) < 0)
	{
		fclose(previous);
		return "unknown";
	}
	if (fseek(previous, size > 255 ? size - 255 : 0, SEEK_SET) != 0)
	{
		fclose(previous);
		return "unknown";
	}
	count = fread(tail, 1, sizeof(tail) - 1, previous);
	tail[count] = 0;
	fclose(previous);
	return strstr(tail, "[INFO] [CTR Session] clean_shutdown=yes\n") != NULL ? "clean" : "interrupted-or-legacy";
}

internal uint64_t Platform_LogWallMilliseconds(void)
{
	struct timespec wallTime;

	if (timespec_get(&wallTime, TIME_UTC) != TIME_UTC)
	{
		return 0;
	}

	return (uint64_t)wallTime.tv_sec * 1000u + (uint64_t)wallTime.tv_nsec / 1000000u;
}

internal void Platform_LogFormatWallTime(uint64_t wallMilliseconds, char *dst, size_t dstSize)
{
	time_t seconds = (time_t)(wallMilliseconds / 1000u);
	struct tm utcTime;
	int valid;

#if defined(_WIN32)
	valid = gmtime_s(&utcTime, &seconds) == 0;
#else
	valid = gmtime_r(&seconds, &utcTime) != NULL;
#endif
	if (!valid || (strftime(dst, dstSize, "%Y-%m-%dT%H:%M:%S", &utcTime) == 0))
	{
		snprintf(dst, dstSize, "unknown-time");
		return;
	}

	{
		size_t length = strlen(dst);
		if (length < dstSize)
		{
			snprintf(dst + length, dstSize - length, ".%03lluZ", (unsigned long long)(wallMilliseconds % 1000u));
		}
	}
}

internal int Platform_LogBuildArchivePath(int archiveIndex, char *dst, size_t dstSize)
{
	int written = snprintf(dst, dstSize, "%s.%d", s_logPath, archiveIndex);
	return (written >= 0) && ((size_t)written < dstSize);
}

internal void Platform_LogRotateExisting(void)
{
	int archiveIndex;

	s_logArchivePath[0] = '\0';
	for (archiveIndex = NATIVE_LOG_ARCHIVE_COUNT; archiveIndex >= 1; archiveIndex--)
	{
		char sourcePath[512];
		char destinationPath[512];
		const char *source;

		if (!Platform_LogBuildArchivePath(archiveIndex, destinationPath, sizeof(destinationPath)))
		{
			fprintf(stderr, "[CTR Log] archive path is too long for '%s'\n", s_logPath);
			return;
		}
		if (archiveIndex == 1)
		{
			source = s_logPath;
		}
		else
		{
			if (!Platform_LogBuildArchivePath(archiveIndex - 1, sourcePath, sizeof(sourcePath)))
			{
				fprintf(stderr, "[CTR Log] archive path is too long for '%s'\n", s_logPath);
				return;
			}
			source = sourcePath;
		}

		// Windows rename does not replace an existing destination. Removing an
		// archive is safe here because the next-newer complete session replaces it.
		remove(destinationPath);
		if (rename(source, destinationPath) == 0)
		{
			if (archiveIndex == 1)
			{
				snprintf(s_logArchivePath, sizeof(s_logArchivePath), "%s", destinationPath);
			}
		}
	}
}

internal void Platform_LogWrite(FILE *consoleStream, const char *level, const char *text)
{
	FILE *stream = (consoleStream != NULL) ? consoleStream : stdout;

#ifdef _WIN32
	OutputDebugStringA(text);
#endif

	Platform_LogLock();
	fputs(text, stream);

	if (s_logStream != NULL)
	{
		// Keep five bounded segments, including long-running sessions.
		if (ftell(s_logStream) >= NATIVE_LOG_MAX_BYTES)
		{
			fclose(s_logStream);
			s_logStream = NULL;
			Platform_LogRotateExisting();
			s_logStream = fopen(s_logPath, "wb");
			if (s_logStream == NULL)
			{
				fputs("[CTR Log] cannot reopen rotated log\n", stderr);
				Platform_LogUnlock();
				return;
			}
			fputs("[CTR Log] continuing current session after size rotation\n", s_logStream);
		}
		char wallTime[40];
		uint64_t wallMilliseconds = Platform_LogWallMilliseconds();
		uint64_t elapsedMilliseconds = wallMilliseconds >= s_logStartMilliseconds ? wallMilliseconds - s_logStartMilliseconds : 0;

		Platform_LogFormatWallTime(wallMilliseconds, wallTime, sizeof(wallTime));
		fprintf(s_logStream, "[%s +%llu.%03llus] [%s] ", wallTime,
		        (unsigned long long)(elapsedMilliseconds / 1000u),
		        (unsigned long long)(elapsedMilliseconds % 1000u), level);
		fputs(text, s_logStream);
		fflush(s_logStream);
	}
	Platform_LogUnlock();
}

internal void Platform_LogV(FILE *consoleStream, const char *level, const char *fmt, va_list args)
{
	char text[4096];
	int written = vsnprintf(text, sizeof(text), fmt, args);

	if (written < 0)
	{
		return;
	}

	text[sizeof(text) - 1] = '\0';
	Platform_LogWrite(consoleStream, level, text);
}

int Platform_LogSetPath(const char *path)
{
	int written;

	if (s_logStream != NULL)
	{
		return 0;
	}

	if ((path == NULL) || (path[0] == '\0'))
	{
		s_logPath[0] = '\0';
		s_logArchivePath[0] = '\0';
		return 1;
	}

	written = snprintf(s_logPath, sizeof(s_logPath), "%s", path);
	if ((written < 0) || ((size_t)written >= sizeof(s_logPath)))
	{
		s_logPath[0] = '\0';
		s_logArchivePath[0] = '\0';
		fprintf(stderr, "[CTR Native] Error: log path is too long\n");
		return 0;
	}

	return 1;
}

const char *Platform_LogGetPath(void)
{
	return s_logPath;
}

const char *Platform_LogGetArchivePath(void)
{
	return s_logArchivePath;
}

int Platform_LogIsOpen(void)
{
	return s_logStream != NULL;
}

void Platform_LogInit(const char *appName)
{
	const char *previousStatus;
	if (s_logStream != NULL) return;
	if (s_logPath[0] == '\0')
	{
		int written = snprintf(s_logPath, sizeof(s_logPath), "%s.log", appName);

		if ((written < 0) || ((size_t)written >= sizeof(s_logPath)))
		{
			fprintf(stderr, "[CTR Native] Error: log filename is too long\n");
			s_logPath[0] = '\0';
			return;
		}
	}

	previousStatus = Platform_LogPreviousStatus();
	Platform_LogRotateExisting();
	s_logStream = fopen(s_logPath, "wb");

	if (s_logStream == NULL)
	{
		fprintf(stderr, "[CTR Native] Error: cannot create log file '%s'\n", s_logPath);
		return;
	}

	s_logStartMilliseconds = Platform_LogWallMilliseconds();
	Platform_Log("[CTR Session] previous=%s max_segment_bytes=%ld\n", previousStatus, NATIVE_LOG_MAX_BYTES);
	Platform_Log("[CTR Log] session opened path=%s previous=%s archives=%d\n", s_logPath,
	             s_logArchivePath[0] != '\0' ? s_logArchivePath : "none", NATIVE_LOG_ARCHIVE_COUNT);
}

void Platform_LogShutdown(void)
{
	if (s_logStream == NULL) return;
	Platform_Log("[CTR Session] clean_shutdown=yes\n");

	Platform_LogLock();
	if (s_logStream != NULL)
	{
		fclose(s_logStream);
	}

	s_logStream = NULL;
	Platform_LogUnlock();
}

void Platform_LogFlush(void)
{
	Platform_LogLock();
	if (s_logStream != NULL)
	{
		fflush(s_logStream);
	}
	Platform_LogUnlock();
}

void Platform_Log(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	Platform_LogV(stdout, "INFO", fmt, args);
	va_end(args);
}

void Platform_LogWarn(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	Platform_LogV(stdout, "WARN", fmt, args);
	va_end(args);
}

void Platform_LogError(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	Platform_LogV(stderr, "ERROR", fmt, args);
	va_end(args);
}

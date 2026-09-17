#import "native_diagnostics.h"
#include "platform/native_log.h"

NSString *CTRPadDiagnosticsRedact(NSString *text)
{
    // Strip entire path-bearing lines, rather than guessing where a path ends.
    // Paths can contain spaces, game names, sandbox IDs, or external volumes.
    NSMutableString *safe = [NSMutableString string];
    NSRegularExpression *privateValue = [NSRegularExpression regularExpressionWithPattern:
        @"(?i)(?:/[a-z]|file:|[a-z]:\\\\|[a-z0-9._%+-]+@[a-z0-9.-]+\\.[a-z]{2,}|[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}|token[=:]|password[=:])"
        options:0 error:nil];
    [text enumerateLinesUsingBlock:^(NSString *line, BOOL *stop) {
        if ([privateValue firstMatchInString:line options:0 range:NSMakeRange(0, line.length)])
            [safe appendString:@"[line omitted: path or potentially identifying value]\n"];
        else [safe appendFormat:@"%@\n", line];
    }];
    return safe;
}

NSString *CTRPadDiagnosticsReport(NSDictionary<NSString *, id> *metadata)
{
    NSMutableString *report = [NSMutableString stringWithString:
        @"CTRPad diagnostic report v1\nReview before sharing. No automatic upload.\n"
         "Includes current log and up to four older segments (128 KiB each).\n"
         "Long segments retain their header and tail. Older segments may be from another build.\n"
         "An interrupted session does not by itself prove a crash.\n"
         "This is not an Apple crash report or a save backup.\n\n"];
    for (NSString *key in [[metadata allKeys] sortedArrayUsingSelector:@selector(compare:)])
        [report appendString:CTRPadDiagnosticsRedact([NSString stringWithFormat:@"%@: %@", key, metadata[key]])];
    char *buffer = malloc(128 * 1024);
    if (!buffer) return nil;
    for (int segment = 0; segment <= 4; segment++)
    {
        size_t count = Platform_LogCopySegment(segment, buffer, 128 * 1024);
        [report appendFormat:@"\n--- %@ ---\n", segment == 0 ? @"Current log" : [NSString stringWithFormat:@"Older log %d", segment]];
        if (!count) [report appendString:@"Unavailable or empty.\n"];
        else
        {
            // Truncation may split a UTF-8 character. Latin-1 fallback retains
            // the diagnostic bytes without dropping the whole segment.
            NSString *text = [[NSString alloc] initWithBytes:buffer length:count encoding:NSUTF8StringEncoding];
            if (!text) text = [[NSString alloc] initWithBytes:buffer length:count encoding:NSISOLatin1StringEncoding];
            [report appendString:CTRPadDiagnosticsRedact(text)];
        }
    }
    free(buffer);
    return report;
}

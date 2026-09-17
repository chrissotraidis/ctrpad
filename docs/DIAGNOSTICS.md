# Reporting CTRPad failures

CTRPad keeps `Crash Team Racing.log` plus up to four older segments (`.1` is
newest). macOS stores them under
`~/Library/Application Support/chrissotraidis/CTRPad/`; iPhone/iPad keeps
them in the app's private Application Support directory. Developer collection
is documented in [Install iOS](INSTALL-IOS.md#run-the-physical-device-acceptance-campaign).
On iPhone/iPad, use **Options → Export diagnostic logs → Save to Files** to
export a reviewed report. Each raw log segment is limited to approximately
8 MiB (plus the final log line); rotation also occurs at launch. Copy the files
soon after a problem, before repeated launches or long sessions replace them.

v0.1.2 includes:

- Full source commit for Git builds, dirty build label, platform and SDL
  version at startup (extracted archives use their known source identity);
  existing renderer, audio, controller, import and lifecycle messages remain.
- Previous-session status: `clean`, `interrupted-or-legacy`, `unknown`, or
  `unavailable`. An interrupted session may be a force quit, OS termination,
  old build or crash; this is not a crash diagnosis.
- `[CTR Game]` context on level/mode changes and every 300 logic calls: frame,
  level, mode, player count and particle/oscillator pool counts.
- `[CTR Particle] invalid_pool` before dereferencing invalid particle or
  oscillator links. It records the operation and game context, flushes the log,
  then aborts rather than writing through corrupt pointers. It does not catch
  every memory error, repair corruption or silently discard particles.

Raw logs can include local file paths; review and redact personal paths and
identifiers before sharing. The iOS report exporter filters path and identifying-
pattern lines, but its text should still be reviewed. Nothing uploads automatically.

For a report, include version/source, platform/OS, track/mode/player count,
what happened immediately before failure, and reviewed log excerpts from the
current and previous segment. For a macOS crash, also include the reviewed
`.ips` report from Console or `~/Library/Logs/DiagnosticReports`. Preserve
thread backtraces, exception details and CTRPad's binary UUID; remove personal
paths and incident/device identifiers. On iOS, use Settings → Privacy &
Security → Analytics & Improvements → Analytics Data when a report exists.
Never share the imported disc, extracted assets, saves or signing files.

## Issue #36: Tiger Temple

The report matches the public v0.1.1 macOS executable UUID
`91F56AB5-8D0D-3039-9721-D20097E36A5B`. Its `Particle_UpdateList + 932`
instruction is `str xzr, [x8, #8]`, in inlined oscillator destruction/free-list
insertion. The oscillator link is invalid. This establishes the fault boundary,
not which earlier operation corrupted it. The new checks distinguish update,
destruction and free-head failures before an unsafe access. v0.1.2 also corrects the oscillator allocation from the original 24-byte stride
to the host structure size (32 bytes on 64-bit builds). Confirmation against the
reporter's original reproduction is still needed; the issue remains open.

## iOS: exporting diagnostics

In v0.1.2, open **Options → Export diagnostic logs → Save to Files**. After an unexpected exit, reopen the app and export before repeatedly relaunching: retention is limited to the current log and four older segments. Review the text, then attach it to the CTRPad issue with the action/track that triggered the problem. Nothing is uploaded automatically.

The report includes app version/build/full source commit, OS and hardware model, thermal state, control/display settings, and up to 128 KiB from each of five log segments. Large segments keep their header and tail; the omitted middle is labelled. Snapshot reads hold the logger lock, preventing rotation from invalidating the read. Path, email, UUID and credential-pattern lines are omitted. Saves, disc images, preferences files, signing material and device names/identifiers are never collected. Review remains important because logs are free-form text.

This export is not an Apple crash report. For a native stack trace, also retain the matching CTRPad `.ips` report from Settings → Privacy & Security → Analytics & Improvements → Analytics Data when available. A missing clean-shutdown marker indicates interruption, not necessarily a crash.

The options menu groups diagnostics, touch controls and display settings. Touch polish changes strokes, corners, shadows and pressed-state emphasis; bindings, hit areas, layouts, gas latch and drift behavior remain unchanged. iOS uses a generic empty blue kart icon with no characters. Build 3 was a private test candidate. Public v0.1.2 includes these features plus the build-4 Options and default-layout improvements.

Host export checks: `python3 tools/test-diagnostics.py` (macOS/Foundation). Logger checks: `python3 tools/test-native-log.py`.

During build-3 qualification, the new checks exposed an undersized native oscillator pool. The allocator now uses the host structure size; see [candidate validation](source-maintenance/2026-09-17-ios-test.md). This fix still needs confirmation against the original reporter’s crash.

Build 4 moves diagnostics below touch controls. Done stays at the top while the settings scroll. The game and audio pause while Options or the layout editor is open, then resume on exit. The new iPad default uses the owner's simplified left-steering layout, Large size and approximately 53% opacity; existing custom layouts take precedence.

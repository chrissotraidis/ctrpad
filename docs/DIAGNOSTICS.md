# Reporting CTRPad failures

CTRPad keeps `Crash Team Racing.log` plus up to four older segments (`.1` is
newest). macOS stores them under
`~/Library/Application Support/chrissotraidis/CTRPad/`; iPhone/iPad keeps
them in the app's private Application Support directory. Developer collection
is documented in [Install iOS](INSTALL-IOS.md#run-the-physical-device-acceptance-campaign).
Files exposes the imported disc, not these private logs; there is no in-app
log export yet. Each new segment is limited to approximately
8 MiB (plus the final log line); rotation also occurs at launch. Copy the files
soon after a problem, before repeated launches or long sessions replace them.

The maintenance candidate adds:

- Full source commit, dirty build label, platform and SDL version at startup;
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

These changes are not in public v0.1.1. Existing logs include local file paths;
review and redact personal paths and identifiers before sharing. No automatic
upload or new reporting UI is included. KartPad's richer report-export UI and
redaction are not claimed here.

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
destruction and free-head failures before an unsafe access. Reproduction with
the diagnostic candidate, ideally compared with v0.1.1 under the same steps,
is still required; this change does not claim the track crash is fixed.

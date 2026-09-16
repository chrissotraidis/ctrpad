# Contributing to CTRPad

Thank you for helping improve CTRPad. The project welcomes focused fixes,
portable platform work, documentation corrections, tests, and carefully scoped
Apple usability improvements.

## Before starting

1. Read the relevant sections of `README.md` and `docs/DECISIONS.md`.
2. Check `docs/ROADMAP.md` and `docs/parity/README.md` for known boundaries.
3. Keep changes small enough to review and validate on every affected target.
4. Open an issue before beginning a large renderer, memory-model, serialization,
   or platform-architecture rewrite.

## Build and test

For the primary Apple host test path:

```sh
cmake --preset macos-arm64-app
cmake --build --preset macos-arm64-app
ctest --preset macos-arm64-app --output-on-failure
```

For Apple-mobile compilation:

```sh
cmake --preset ios-simulator-arm64
cmake --build --preset ios-simulator-arm64

cmake --preset ios-device-arm64
cmake --build --preset ios-device-arm64
```

Run the strongest relevant checks for your change and report exactly what was
and was not tested. A successful build is not physical-device, controller,
touch, audio, performance, or multiplayer proof.

For dependency changes, follow [source maintenance](docs/source-maintenance/README.md).
Run `python3 tools/check-sources.py` and `python3 tools/test-native-log.py` for
source/package or logger changes.

## Pull requests

A useful pull request explains:

- the problem and root cause;
- the smallest change that addresses it;
- the user-visible effect;
- the exact validation commands and results; and
- any remaining platform or physical-hardware checks.

Do not mix unrelated cleanup into a functional change. Preserve the existing
portable/native boundary and keep PlayStation-shaped serialized data separate
from native host pointers.

## No game data or credentials

Never commit or attach:

- disc images, CUE files, extracted assets, audio, video, textures, or saves;
- crash reports or logs containing private user paths without redaction;
- provisioning profiles, certificates, private keys, or signing credentials;
- generated applications, IPAs, ZIPs, build trees, or runtime containers.

Use only your own legally acquired compatible retail image for local testing.
Do not ask maintainers or issue reporters to share game data.

## Reporting bugs

Include:

- platform and OS version;
- device or Mac model when relevant;
- the full 40-character source commit;
- build/install route;
- concise reproduction steps;
- expected and observed behavior; and
- a short, reviewed excerpt from the rotating CTRPad log.

Do not attach the retail image, extracted files, memory-card saves, or an
unredacted device-acceptance evidence directory.

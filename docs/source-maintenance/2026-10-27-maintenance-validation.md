# Local maintenance validation — October 27, 2026

Implemented the focused changes from the [update review](2026-10-27-update-review.md)
on `codex/docs-and-update-review`, using the existing checkout. Nothing was
pushed, tagged, released, or uploaded. Version remains 0.2.3 (build 8).

| Local commit | Change |
|---|---|
| `2bc7e77fdb` | Defined GTE scaling and unsigned flag constants; weapon-allocation failure handling with missile-counter rollback and partial-shield cleanup; XA table bounds checks; corrected mixed-face indices in the full-dynamic renderer. Includes regression coverage. |
| `d4b36ec591` | SDL 3.4.18, source lock and notices. Applies the selected upstream delta while preserving both UIKit patches and inherited import omissions. |
| `62eb51be6d` | All three checkout actions pinned to v7.0.1 commit `3d3c42e5aac5ba805825da76410c181273ba90b1`. Full history, read-only permissions, and build-only behavior retained. |

The complete build/package validation used clean source
`62eb51be6d7521522574d60bf43b20590f2d2f50`, before this documentation-only record.

## Results

| Check | Result |
|---|---|
| macOS ARM64 build and CTest | 31/31 passed with SDL 3.4.18. Includes renderer pixel, input, lifecycle, audio, storage, particle-pool, signing-helper, and identity checks. |
| Fresh Debug build with AddressSanitizer | 31/31 passed in `build-macos-maintenance-asan`. App and vendored C sources built with ASan; this is host regression coverage, not a gameplay soak. |
| GTE UBSan regression | 120 GPL, 96 MVMVA, and 8 RTPS/RTPT cases passed. Checks negative accumulator/translation scaling and saturation; not a hardware-accuracy oracle for every flag. |
| Weapon allocation regression | 50 cases passed using real instance/thread/object pools: missing models, exhausted pools, missile-counter rollback, partial shield cleanup, existing-child preservation, successful missiles/bombs/warpballs/shields, and pool restoration. |
| XA manifest regression | 11 synthetic table cases plus 3 missing-media cases passed. Covers count multiplication wrap, maximum counts, truncation, song bounds and valid tables. Missing movies, manifests and referenced XA streams still fail validation. |
| Full-dynamic renderer regression | Three actual GT4 packets checked for the expected mixed-face coordinates and shared edges. |
| iOS device and Simulator builds | Both ARM64 builds passed with Xcode 27.1. Existing caches contained an old source-commit override; it was cleared so the current checkout supplies its verified identity. |
| iPad Simulator startup/resume | A fresh iOS 27.0 iPad Pro Simulator reached the disc-selection screen. The guarded installer verified the installed executable against its signed staged copy. Launching Settings and then CTRPad returned to the same import screen and the same app process. No retail disc was imported. |
| Logger and diagnostics | Rotation, retention, interrupted/clean sessions, redaction, bounded exports, missing logs and concurrent reads passed. |
| Dependency lock | Both trees passed. Offline restore, changed bytes/modes, extra files and missing files were tested. SDL's locked tree is `0587efab2a78ddc4abcf71a4411f637029319951`. |
| Local packaging | Mac ZIP, unsigned device IPA, and corresponding-source archive passed their packagers. Exact clean source identity and retail/signing-data exclusion verified; Mac staged signature verification passed. |
| PadMint audits | Local Mac ZIP, unsigned IPA, and `padmint.json` passed `python3 -m padmint audit` from the local PadMint checkout. This was not the empty-cache player-check release gate. |
| Workflow | `actionlint .github/workflows/apple-packages.yml` passed. The v7.0.1 commit was verified against GitHub's tag. No hosted run was dispatched. |

The first full-dynamic test fixture lacked scratchpad initialization and GPU
token registration. Those fixture errors were corrected before the passing
runs; production rendering did not require either change.

## Scope and remaining validation

The GPU environment/clear-order and `SetGeomScreen` changes identified for
separate review were not imported. Neither were the broad upstream matching
rewrite, the already-addressed clock/battle fixes, or optional-media startup.

Physical device gameplay, completed races, low-memory gameplay, rotation and
Options/audio interaction during play, split-screen behavior, and the specific
Xbox/8BitDo/Joy-Con/Steam Controller hardware fixes remain unverified. Simulator
startup and the software tests do not establish those results. Windows/Linux
builds and the actual GitHub-hosted checkout action were not exercised here.

An eventual release still requires PadMint's current-main `player-check.sh`
before tagging and after publication, plus audits of the recipe and checksum.
Public app binaries remain prohibited. The 0.2.3 published release notes were
left unchanged; their proposed correction remains in the review.

## Local outputs and reproduction

Private outputs remain under ignored `dist/`:

- `CTRPad-maintenance-local-macos.zip`
- `CTRPad-maintenance-local-ios.ipa`
- `CTRPad-source-62eb51be6d75.tar.gz`
- `maintenance-validation/` contains command logs, Simulator screenshots and
  the SDL import file list. These local logs can contain host paths; do not
  publish them without review.

The temporary SDL research repository and fresh test Simulator were removed
after preserving the results. No extra project checkout or worktree was made.

Core repeatable checks:

```sh
cmake --preset macos-arm64-app
cmake --build --preset macos-arm64-app
ctest --preset macos-arm64-app --output-on-failure
python3 tools/test-native-log.py
python3 tools/test-diagnostics.py
python3 tools/test-source-lock.py
actionlint .github/workflows/apple-packages.yml
```

For sanitizer coverage, configure the same preset with a separate build
directory, `CMAKE_BUILD_TYPE=Debug`,
`CMAKE_C_FLAGS="-fsanitize=address -fno-omit-frame-pointer"`, and
`CMAKE_EXE_LINKER_FLAGS=-fsanitize=address`; build and run CTest in that directory.
The standalone GTE test also enables UBSan on supported Clang/GCC host builds.

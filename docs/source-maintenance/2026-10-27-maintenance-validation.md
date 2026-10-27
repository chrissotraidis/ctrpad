# Local maintenance validation — October 27, 2026

Implemented the focused changes from the [update review](2026-10-27-update-review.md)
on `codex/docs-and-update-review`, using the existing checkout. The initial
local validation used version 0.2.3 (build 8). The work subsequently merged
and shipped as **0.2.4 (build 9)**; release and final-device results are recorded
[below](#release-and-final-device-checks).

| Local commit | Change |
|---|---|
| `2bc7e77fdb` | Defined GTE scaling and unsigned flag constants; weapon-allocation failure handling with missile-counter rollback and partial-shield cleanup; XA table bounds checks; corrected mixed-face indices in the full-dynamic renderer. Includes regression coverage. |
| `d4b36ec591` | SDL 3.4.18, source lock and notices. Applies the selected upstream delta while preserving both UIKit patches and inherited import omissions. |
| `62eb51be6d` | All three checkout actions pinned to v7.0.1 commit `3d3c42e5aac5ba805825da76410c181273ba90b1`. Full history, read-only permissions, and build-only behavior retained. |
| `1b2cc58e63` | Size replacement UIKit views to the current window bounds, correcting portrait-sized rendering and clipped controls during a landscape cold launch. |

The complete build/package validation used clean source
`62eb51be6d7521522574d60bf43b20590f2d2f50`. Subsequent physical-iPad testing
exposed a layout defect, corrected and installed from clean source
`1b2cc58e63`. The [iPad correction record](2026-10-27-ipad-layout-fix.md)
documents its separate build, preservation, visual and owner-test evidence.

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
| Physical iPad layout and rotation | The corrected build cold-launched with the proper 2732×2048 landscape surface. Device screenshots confirmed the game picture and controls fit the display; the owner then rotated to portrait and back and confirmed both stayed correctly positioned. |
| Physical iPad update preservation | Signed preflight and in-place installation passed. A fresh backup and before/after SHA-256 comparison confirmed game data, saves and preferences unchanged. Only iPadOS launch-screen snapshot caches were discarded during the update. |
| Logger and diagnostics | Rotation, retention, interrupted/clean sessions, redaction, bounded exports, missing logs and concurrent reads passed. |
| Dependency lock | Both trees passed. Offline restore, changed bytes/modes, extra files and missing files were tested. The initial SDL import tree was `0587efab2a78ddc4abcf71a4411f637029319951`; the reviewed UIKit correction advances it to `da0ace7b54deef5b1fd7e4d1d16e922f1eb095d0`, with lock tests passing again. |
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

Physical iPad launch, layout, rotation and update-data preservation passed as
recorded above. Completed races, low-memory gameplay, Options/audio
interaction during play, split-screen behavior, and the specific
Xbox/8BitDo/Joy-Con/Steam Controller hardware fixes remain unverified. Simulator
startup and the software tests do not establish those results. Windows/Linux
builds remain unverified. The GitHub-hosted Apple/source workflow subsequently
passed on both the PR and release commit, including the updated checkout action.

The required PadMint current-main player checks before tagging and after
publication, plus recipe/checksum audits, passed for 0.2.4 as recorded below.
Future releases require their own checks. Public app binaries remain prohibited.
The 0.2.3 published release notes were
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

## Final local review before push

After the owner's successful physical-iPad rotation test, the complete local
change set was reviewed again against refreshed `origin/main` (`7db56dab306c`).
No blocking findings were identified. The focused engine changes, allocation
cleanup, bounds arithmetic, test wiring, UIKit replacement-view sizing,
documentation and workflow changes were checked together.

All 183 files in the retained SDL upstream import plan were compared against
upstream commit `829a65d769d935c4852f8159e964312c0957260a`, allowing the
documented line-ending/trailing-whitespace normalization; none differed.
The separate local UIKit correction and the current dependency lock were
reviewed explicitly.

The existing macOS regression build passed all 31 CTests again; desktop runtime
sources had not changed since its build. Logger, diagnostics, source-lock
mutation tests, workflow lint and diff whitespace checks also passed again.
The complete pending file set contains no app package, disc, save,
provisioning profile or private-key material, and `public_binaries` remains
false. Private device evidence stays ignored.

This review supports pushing the source branch for hosted CI. It does not
replace the remaining gameplay/controller checks or release gates above.
No push, hosted workflow, tag or release was performed during this review.

## Release and final device checks

[PR #48](https://github.com/chrissotraidis/ctrpad/pull/48) merged as
`7cdf9ef067511fc0fbc3f8dacb9f83e6e732a115`.
[CTRPad 0.2.4](https://github.com/chrissotraidis/ctrpad/releases/tag/v0.2.4)
tags that commit and sets `version.json` to 0.2.4, build 9.

- All three jobs (iOS, macOS and corresponding source) passed in
  [PR CI](https://github.com/chrissotraidis/ctrpad/actions/runs/40459016393) and
  [main CI](https://github.com/chrissotraidis/ctrpad/actions/runs/40459299289).
- PadMint's `scripts/player-check.sh` passed for both iOS and macOS before
  tagging with the clean checkout, then again after publication using the
  public release without a checkout argument. All four runs used Apple's
  Python 3.9 and separate empty PadMint folders, from PadMint main
  `0d0596303c3b0adf2b78f771f29def357ef608c4`. Package hashes, 0.2.4/build-9
  metadata and source revisions were verified.
- The recipe and checksum passed both `python3 -m padmint audit` and the
  private release gate. Downloaded public files matched the audited local
  files and checksum. The release contains only `CTRPad-v0.2.4-padmint.json`
  and `SHA256SUMS`; the workflow uploaded no app artifacts.
- The final release build was signed and update-installed on the physical
  iPad. Installed version, startup, running process and correct landscape
  layout passed verification. A fresh backup and before/after hashes confirmed
  game data, saves and preferences were preserved. See the
  [final install record](2026-10-27-ipad-layout-fix.md#final-024-release-installation)
  for the exact device-test scope.

Private release checks and personal build outputs remain under ignored
`dist/release-0.2.4/`. The four temporary player-check folders were removed
after their package/run evidence was preserved; no extra project worktree
remains from this release. The final installation does not extend acceptance
to completed races, extended gameplay or untested controllers.

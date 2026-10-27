# Dependency and upstream review — October 27, 2026

Reviewed CTRPad `7db56dab306c3de46bc38e5e11946d533f549b89` (0.2.3,
build 8), CTR Native master `3f5f6b8b28cee1d3501c420e83c11585505b4e2b`,
SDL 3.4.10 through 3.4.18, and actions/checkout 7.0.1. This is a source
review and recommendation, not an engine upgrade or gameplay acceptance.

The recommended engine fixes, SDL update, and checkout-action update were
subsequently implemented in separate local commits. See the
[implementation and validation record](2026-10-27-maintenance-validation.md).
The assessment below records the pre-change findings.

The local main branch was fast-forwarded to GitHub's main. Documentation now
records the Tiger Temple reporter's confirmation, consistently describes
personal builds and recipe-only releases, and uses PadMint's current
`#github-cloud-build-no-mac-needed` guide anchor. Dated campaign records remain
historical evidence.

## Recommended order

1. Bring in the geometry arithmetic fixes and their regression coverage.
2. Adapt weapon-allocation failure handling and XA table bounds checks.
3. Bring in the small clipped-primitives fix with renderer validation.
4. Upgrade SDL separately, preserving CTRPad's UIKit patches.
5. Update the checkout action in its own CI change.

Do not merge all of CTR Native master. It has 110 commits after the recorded
foundation and changes 375 files in `game/`, `include/`, and `platform/` alone
(71,978 insertions, 50,476 deletions). Much of this is PlayStation byte-matching
work, including changed layouts and compiler contracts. Each native fix needs
comparison against CTRPad's ARM64 and Apple changes.

## Engine candidates

| Upstream change | Evidence in CTRPad | Recommendation and validation |
|---|---|---|
| [Signed GTE accumulator/translation scaling](https://github.com/CTR-tools/ctr-native/commit/429169bdace9c3c599402835b639b0456cd819bd), plus the RTPS arithmetic from [48d4c04a6b](https://github.com/CTR-tools/ctr-native/commit/48d4c04a6b7da95fd9dbdd4d5389b17648bd5a6e) | `platform/native_gte_core.c` still left-shifts potentially negative accumulators and translations. UBSan reproduces the fault; see below. | Bring in the multiplication-based scaling, make flag-bit constants unsigned, and adapt the upstream regression. Run the existing GTE/parity checks and UBSan, then validate rendering. |
| [Weapon allocation failures](https://github.com/CTR-tools/ctr-native/commit/bf2ed389c5d9ce536f376c9cd128144f309bcaf2) | `VehPickupItem_ShootNow` still dereferences unchecked missile/bomb, mine, shield and warpball births. It even retains a comment deferring the low-memory fallback. CTRPad's branches differ from upstream. | Adapt the guards, missile-counter rollback, and partial shield cleanup. Test injected failures, preservation of existing child threads, and normal successful births before gameplay checks. Do not copy the whole upstream function. |
| XA manifest bounds in [99dd3fad6f](https://github.com/CTR-tools/ctr-native/commit/99dd3fad6f3f715848938c819673701febdea3bb) | `NativeAssets_ValidateXA` multiplies untrusted counts into 32-bit offsets before proving they fit the buffer. The existing end-offset test does not catch every multiplication wrap. | Bring in the division-before-multiplication bounds checks with malformed and truncated manifest tests. Keep CTRPad's required-media validation; accepting missing audio/video is a separate product change. |
| [Sporadically clipped primitives](https://github.com/CTR-tools/ctr-native/commit/b37cd1b7e03184f8203c85415376a6e8b97c03f9) | `Ovr226_800a15d4_FullDynamicHelperSlot0` still uses the two pre-fix face helpers. The mixed-face table and preserve-slot helper needed by the fix already exist. | Bring in the focused two-call change. Check emitted face/slot data, existing renderer tests, and Mac/iOS visual behavior. Source review establishes applicability, not a reproduced visual symptom. |
| GPU environment and geometry register behavior in [48d4c04a6b](https://github.com/CTR-tools/ctr-native/commit/48d4c04a6b7da95fd9dbdd4d5389b17648bd5a6e) | `SetDrawEnv` still emits five words without the mask reset/background TILE. `SetGeomScreen` writes the low half-register. | Review as a separate renderer change. CTRPad has custom GL/GLES clear handling and native packet links; verify packet capacity, clear ordering, offsets, split-screen, and register semantics before adopting. |
| [Clock item and empty driver slots](https://github.com/CTR-tools/ctr-native/commit/c7bc76896d872467426e6aa7dc7e18156e5b125f) | CTRPad checks `victim == 0` before writing `clockFlash`. | Already addressed locally; no import needed. |
| [Battle menu native tracker access](https://github.com/CTR-tools/ctr-native/commit/59bcb19eda20c04a0ce9264605da97f445ff14b3) | CTRPad uses `gGT->originalEventTime` directly rather than reconstructing an address from a 32-bit page. | The offending upstream expression is absent; no import needed. This does not certify multiplayer acceptance. |
| [Font argument widths](https://github.com/CTR-tools/ctr-native/commit/ff705b75f345d8a2e37b7c38dc1dc729da3af1f8) | CTRPad's wrapper already casts coordinates and flags to `s16` before forwarding them. | Defer the signature and call-site cleanup unless pursuing the retail compiler contract; no demonstrated native behavior fix here. |
| [Overlay-local assembly labels](https://github.com/CTR-tools/ctr-native/commit/424c50e5255b2cece50aa9cb261cda8d49043a88) | The change concerns non-native assembly and matching-tool bindings. | Defer for this Apple maintenance pass. |

## GTE experiment

The upstream `tests/gte_gpl_shift_regression.c` at `429169bdac` was compiled
against CTRPad's current geometry core using Apple Clang, C17, `-O2`,
`-fsanitize=undefined`, and `-fno-sanitize-recover=all`.

The test's `b8` field was adapted to `uint8_t`, because CTRPad does not define
that upstream alias. The first baseline failure was an existing signed
`1 << 31` flag expression at `native_gte_core.c:142`. Changing only those flag
expressions to `1u << 31` in a scratch baseline allowed the test to reach the
target fault: a left shift of negative value `-5659` at line 87.

A second scratch copy used the same unsigned flag constants plus multiplication
by 4096 for the accumulator and translation scaling from the upstream fixes.
It passed 120 GPL cases and 96 MVMVA cases under UBSan. The RTPS scaling was
changed in that scratch copy but is not directly covered by those 216 cases.
No runtime source in the repository was changed. This experiment does not
establish hardware-accurate overflow flags, complete UB coverage, or gameplay
acceptance.

## Dependencies and CI

### SDL: recommend 3.4.18 in a separate update

The [3.4.10 → 3.4.18 comparison](https://github.com/libsdl-org/SDL/compare/release-3.4.10...release-3.4.18)
contains 241 commits and 184 changed files. Relevant stable-release fixes:

- [3.4.12](https://github.com/libsdl-org/SDL/releases/tag/release-3.4.12):
  flipped Xbox 360 controller axes on macOS.
- [3.4.14](https://github.com/libsdl-org/SDL/releases/tag/release-3.4.14):
  duplicate 8BitDo Pro 3 controllers on macOS and clipboard access on older Macs.
- [3.4.16](https://github.com/libsdl-org/SDL/releases/tag/release-3.4.16):
  Joy-Con calibration and Mac windows remaining after SDL shutdown.
- [3.4.18](https://github.com/libsdl-org/SDL/releases/tag/release-3.4.18):
  duplicate Steam Controllers on macOS 27 and additional controller recognition.

No `src/video/uikit` files changed in that upstream range. Preserve CTRPad's
root-view-controller lifecycle handling in `SDL_uikitview.m` and both
`NSRunLoopCommonModes` changes in `SDL_uikitviewcontroller.m`. The vendored
tree also has inherited import omissions and normalized line endings; replacing
it wholesale with a tarball would obscure that provenance. Review the selected
delta, then update the tag/commit/tree in `sources.lock.json` deliberately.

Validate Mac, device and Simulator builds, source-lock checks, controller
mapping/hotplug, rotation, background/resume, and audio while interacting with
Options. Controller-specific fixes need the matching physical controllers.

### actions/checkout: recommend 7.0.1

All three jobs currently use v4. [Version 7.0.1](https://github.com/actions/checkout/releases/tag/v7.0.1)
includes dependency updates and safer fork-PR handling. The
[action's documentation](https://github.com/actions/checkout#whats-new)
records the Node 24 runner requirement inherited from v5 and the credential
storage change in v6. CTRPad uses GitHub-hosted runners, ordinary `pull_request`
events, and `fetch-depth: 0`; retain full history for source identity and do not
enable unsafe PR checkout. Run all three jobs after updating. No runtime
dependency upgrade is needed for this change.

### Other inherited components

PSn00bSDK is a header subset with no linked `libpsn00b`; PsyCross/Psy-X code is
maintained inside the app sources. No standalone version bump is justified by
this review. Both locked vendored trees pass `tools/check-sources.py`.

## Release-note follow-up

The published 0.2.3 notes still describe only local Mac builds. Suggested
replacement for their introductory build guidance:

> CTRPad 0.2.3 for iPhone, iPad and Apple Silicon Mac. This release contains
> only the PadMint recipe and its checksum. Build your personal copy with
> PadMint on an Apple Silicon Mac, or use
> [GitHub Cloud Build](https://github.com/chrissotraidis/padmint#github-cloud-build-no-mac-needed)
> for iPhone and iPad without owning a Mac. Choose your own compatible Crash
> Team Racing disc image in the installed app and keep your app build private.

Put the Cloud Build link before the numbered local-Mac steps and label those
steps **Build on your own Mac**. Published release notes were not edited as
part of this local documentation change.

Any eventual game release still requires PadMint's current-main player check
before tagging and after publication, plus audits of both release files.

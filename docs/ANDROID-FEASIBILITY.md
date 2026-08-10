# CTRPad Android feasibility

- **Assessment date:** 2026-08-10
- **CTRPad source assessed:** `ef9c0059950b1c6cd78458612b3608e22bb987ed`
- **Assessment type:** source and architecture review; no Android APK was built or run

## Executive verdict

CTRPad is a good technical candidate for an Android port. It is not an engine
rewrite, and it should be materially smaller than the completed Apple-port
campaign. The native game, 64-bit conversion, SDL audio and controller paths,
storage primitives, save system, and shared OpenGL ES 3 renderer are all useful
Android foundations.

An Android version that merely compiles and boots with a controller is a small
to medium project. An Android version that behaves like CTRPad on iPhone is a
medium to large platform port because the polished mobile experience is
implemented in UIKit and must be recreated for Android.

Estimated effort for one experienced native Android/C/graphics engineer working
full time:

| Outcome | Estimated effort | What it proves |
|---|---:|---|
| ARM64 Android proof | 1-2 weeks | APK installs, GLES initializes, retail boot reaches a race with a controller |
| Usable Android alpha | 3-6 weeks | Import, private storage, audio, controller, saves, lifecycle, and a fixed touch layout work on one device |
| iPhone-experience parity | 10-16 weeks | Full racing controls, settings, editable layouts, accessibility, import/reselection, and update-safe persistence |
| Public release quality | 12-20 weeks | Multi-device validation, packaging, documentation, clean-room proof, and release safeguards |

These are engineering ranges, not delivery promises. A developer learning the
Android NDK, JNI, SDL's Android shell, or mobile graphics during the work should
plan for roughly four to seven calendar months for release quality.

## What “same as iPhone” means

This assessment treats Android parity as the complete user experience, not just
a successful native build. A parity claim requires:

- a standard installable Android package with no retail data bundled;
- first-run selection and validation of the user's NTSC-U `SCUS_944.26` raw
  `MODE2/2352` BIN;
- atomic, non-destructive disc replacement;
- private logs, settings, memory cards, and imported game data that survive an
  in-place app update;
- coherent GLES rendering and native audio;
- SDL controller hot-plug, mapping, axes, and rumble where the device supports
  them;
- the simplified horizontal racing controls and the classic circular controls;
- Gas lock, Drift Toggle, Boost Tap, menu gestures, Start, Select, Brake, Item,
  View, and complete simultaneous multi-touch;
- touch visibility, handedness, size, opacity, internal resolution, layout
  editing, reset, and independently stored phone/tablet control profiles;
- safe-area and orientation behavior across phones and tablets;
- TalkBack-appropriate labels, actions, focus, and state announcements;
- held-input cleanup, audio suspension, save durability, and timing rebasing
  across background, foreground, interruption, rotation, and termination; and
- physical-device evidence for complete racing, drift boosts, audio, saves,
  lifecycle, performance, and thermals.

Anything less can be a useful proof or alpha, but should not be described as
equivalent to the iPhone release.

## Why CTRPad is a strong candidate

### The difficult game conversion is already complete

CTR Native originally depended on 32-bit host pointers and PSX-shaped runtime
layouts. CTRPad separated guest references from native pointers, repaired
serialized and scratch layouts, and established cross-width replay and state
evidence. Android therefore starts from a checked 64-bit codebase rather than
from the old 32-bit port.

That distinction is decisive. The original Apple campaign had to solve the
game's LP64 correctness before any mobile product work was meaningful. A new
Android target should be able to reuse that result for `arm64-v8a`.

### Android is already recognized by the renderer configuration

`CMakeLists.txt` enables `CTR_NATIVE_RENDERER_GLES` by default for `ANDROID` and
`IOS`. The shared renderer requests an OpenGL ES 3.0 context and validates the
entry points it consumes. It does not require the optional
`GL_EXT_shader_framebuffer_fetch` extension: when unavailable, CTRPad uses its
two-pass fallback.

This makes Android GPU bring-up a validation and compatibility task rather than
a new renderer project. The renderer still must be exercised on actual Android
drivers; Apple GLES success is not proof of Adreno, Mali, PowerVR, or emulator
behavior.

### SDL already owns most portable host services

CTRPad vendors SDL3 and uses it for:

- window and OpenGL context creation;
- lifecycle events;
- audio output;
- keyboard and game-controller discovery and input;
- controller hot-plug and haptics;
- timing and performance counters; and
- preference and user-folder discovery where enabled by the platform layer.

SDL supplies an Android Activity, Java/JNI bridge, Gradle skeleton, and NDK
build guidance. CTRPad should adapt that supported path rather than inventing a
new Android runtime shell.

### The touch-to-game seam is already narrow

The game does not depend directly on UIKit. The Apple overlay publishes button
and analog state through `Platform_InputTouchButton` and
`Platform_InputTouchLeftStick`. An Android overlay can feed the same native
transport through a small JNI boundary. Controller, keyboard, and touch can
then compose into the primary PlayStation pad without changing gameplay code.

### An older Android fork reduces uncertainty, but is not a port base

`Simon358/ctr-native-android` has an Android branch at `34648097d` with a
Gradle project, SDLActivity subclass, shared-library target, Android logging,
and an early GLES path. It demonstrates that the older native port could enter
the Android toolchain.

It remains a 32-bit `armeabi-v7a` prototype. It predates CTRPad's LP64,
renderer, storage, lifecycle, touch, save-safety, packaging, and evidence work.
Use it only as a source of small Android-specific reference snippets. Do not
merge it wholesale or replace CTRPad's current shared renderer with it.

## Work that can be reused

| Existing CTRPad area | Expected Android reuse | Notes |
|---|---|---|
| Game source and PSX facade | Very high | No Android-specific gameplay fork should be created |
| ARM64/LP64 memory model | Very high | Target `arm64-v8a` first |
| Shared GLES 3 renderer | High | Requires Android driver and surface validation |
| Native audio mixer | High | SDL should provide the device backend; route and interruption behavior still need tests |
| Controller input | High | SDL mappings and hot-plug are portable; Android button labels and rumble vary by device |
| Touch input transport | High | Reuse the C button/analog seam, not the UIKit views |
| Disc validation and asset loading | High | Reuse validation after Android copies a selected URI into private storage |
| Memory cards and atomic writes | High | Place them under Android private app storage |
| Lifecycle reducer | Medium to high | SDL events exist, but Android Activity/surface behavior needs proof |
| Replay, state, renderer, and storage tests | High | Add Android-capable gates where execution is practical |
| Release safeguards | Medium | Concepts transfer; APK/AAB contents and signing need Android-specific audits |

## Work that must be implemented for Android

### 1. Android project and native build

Create a small `platform/android/` application shell based on SDL's supported
Android project structure:

- Gradle project, manifest, resources, adaptive icon, and package metadata;
- NDK/CMake integration;
- `arm64-v8a` shared game library and SDL shared library loading;
- Android-aware SDL main entry point;
- GLES 3.0 feature declaration;
- landscape/fullscreen and display-cutout policy;
- version/source-identity metadata; and
- debug and release APK production, followed later by AAB support if needed.

The current CMake target is always an executable and `main.c` marks every
non-iOS target as `SDL_MAIN_HANDLED`. SDL's Android path expects a shared native
library and SDL-owned Java/JNI startup. These are bounded build and entry-point
changes, not gameplay changes.

### 2. Android storage and retail-disc import

The current sandbox branch in `NativeStorage_Init` is Apple-only. Android must
be treated as a sandboxed app with explicit locations for:

- private writable state and logs;
- private imported retail data;
- temporary staging; and
- optional user-visible exports, if later added.

Use Android's Storage Access Framework with `ACTION_OPEN_DOCUMENT`. The selected
item is a content URI, not a normal filesystem path. The Android layer should
stream the selected file into a private staging file, run CTRPad's existing
sector/region/completeness validation, then atomically replace the installed
copy only after validation succeeds.

Do not request broad legacy external-storage permissions. Do not run the game
directly from a provider URI or rely on persistent provider availability.

The importer must preserve the existing contract:

1. a cancelled import changes nothing;
2. a malformed, wrong-region, incomplete, cooked, compressed, or otherwise
   unsupported image changes nothing;
3. a valid replacement is durable before the old image is removed;
4. an interrupted import can be detected and recovered; and
5. app updates preserve the imported image, saves, settings, and logs.

### 3. Touch overlay and options

This is the largest product workstream. The current iPhone/iPad implementation
is approximately 2,000 lines of UIKit behavior plus its native input seam.

The lowest-risk Android design is a thin Android View overlay above SDL's game
surface. Implement the controls in Kotlin or Java and call the existing native
touch functions through JNI. This preserves the proven game/input boundary and
does not disturb the working iOS overlay.

The first implementation should reproduce behavior rather than translate UIKit
classes line by line. Required Android concepts include:

- stable pointer-ID ownership for genuine multi-touch;
- a pass-through overlay that does not block the game surface outside controls;
- normalized phone/tablet and left/right layout profiles;
- system-bar, display-cutout, hinge, and safe-inset clamping;
- saved control positions and independent size multipliers;
- lifecycle cancellation of every held or latched source;
- Android haptic feedback for Gas lock and drift state;
- an options sheet or screen that remains usable on compact phones; and
- TalkBack actions and state announcements corresponding to the iOS controls.

Avoid using raw screen pixels as the persistence format. Store normalized
positions inside the current safe content rectangle so layouts survive density,
resolution, inset, and orientation changes.

### 4. Lifecycle, timing, and surfaces

Android normally runs the native game loop on SDL's native thread, unlike the
iOS CADisplayLink path. CTRPad should initially retain the ordinary `CTR_Main`
loop and let SDL translate Activity events into the existing lifecycle reducer.

The port must nevertheless verify:

- Activity pause/resume and process recreation;
- EGL surface loss and recreation;
- background audio suspension and clean resume;
- timing rebase without an artificial multi-second VBlank jump;
- low-memory notification handling;
- held-input cleanup before the app loses CPU time;
- rotation/configuration changes without duplicate native runtimes; and
- deterministic shutdown without relying on desktop process behavior.

### 5. Rendering and presentation validation

The renderer requires GLES 3.0. The Android manifest should declare that exact
requirement, and context creation should fail with a useful diagnostic rather
than silently attempting ES 2.

Validation must cover both renderer routes:

- devices exposing `GL_EXT_shader_framebuffer_fetch`; and
- devices using CTRPad's two-pass fallback.

At minimum, test a recent Pixel-class device and a recent Samsung-class device
to cover two common GPU/driver families. Emulator output is useful for
automation but does not replace physical cadence, thermal, touch, or audio
evidence.

Do not begin with Vulkan. A Vulkan backend would greatly increase scope without
first establishing that the existing GLES path is insufficient.

### 6. Audio, controllers, and haptics

SDL should make initial audio and controller bring-up straightforward, but
Android devices vary in buffer sizing, sample-rate conversion, Bluetooth
latency, controller mappings, and vibration capability.

Require evidence for:

- music, voice, XA, effects, panning, and reverb;
- no repeated underruns during a complete race;
- speaker, wired, and Bluetooth route changes where available;
- controller connection before launch and hot-plug during play;
- correct PlayStation-shaped face/shoulder mappings across representative
  Xbox, PlayStation, and generic Android controllers; and
- graceful behavior when rumble is absent or rejected.

### 7. Packaging, CI, and release safeguards

Add Android-specific equivalents of CTRPad's Apple release gates:

- clean `arm64-v8a` build from the asserted source commit;
- reproducible version/source metadata;
- APK/AAB inventory and native-library architecture checks;
- confirmation that no retail image, save, log, signing material, or local
  reference data entered the package;
- signing verification for release artifacts;
- SHA-256 sidecars for directly distributed APKs;
- clean-room source build documentation;
- in-place update and data-preservation proof; and
- CI that builds the retail-free Android package.

Google Play publication is a separate product and policy milestone. It should
not be allowed to block a private or community sideloaded preview, but any store
submission must use the then-current target SDK, signing, listing, content, and
policy requirements.

## Recommended implementation sequence

### Phase A: bounded technical proof

**Expected duration:** 1-2 weeks.

1. Add the Android Gradle/SDLActivity shell and `arm64-v8a` native target.
2. Compile CTRPad without adding touch UI or a document picker.
3. Install a debug APK on one physical Android device.
4. Place a user-owned validated BIN into private app storage using development
   tooling only.
5. Prove title, menu, one complete race, audio, controller input, memory-card
   save, cold relaunch, and background/resume.
6. Capture the GLES vendor/version/extensions, framebuffer-fetch route, frame
   statistics, audio diagnostics, and app log.

**Go/no-go gate:** proceed only if a complete controller-driven race is coherent
and stable on physical ARM64 hardware without evidence of a fundamental GLES,
timing, audio, or LP64 fault.

### Phase B: usable Android alpha

**Expected cumulative duration:** 3-6 weeks.

1. Add sandboxed Android storage and Storage Access Framework import.
2. Prove invalid/cancelled imports preserve a working image.
3. Add a fixed simplified racing-control layout and complete multi-touch.
4. Add phone/tablet safe-inset behavior and essential options.
5. Prove saves and imported data survive an in-place update.

**Gate:** a new user can install the APK, supply their own valid image, complete
a race using touch, save, relaunch, and continue without development tools.

### Phase C: iPhone-experience parity

**Expected cumulative duration:** 10-16 weeks.

1. Add classic controls, Gas lock, drift behavior, haptics, and all buttons.
2. Add handedness, size, opacity, visibility, resolution, layout editing, reset,
   and independent profiles.
3. Add disc reselection and interrupted-import recovery.
4. Complete TalkBack semantics and compact-phone options behavior.
5. Validate lifecycle, controller coexistence, audio routes, and long-session
   performance on the target device matrix.

**Gate:** every item in “What same as iPhone means” has direct Android evidence
or is explicitly documented as unsupported.

### Phase D: public release

**Expected cumulative duration:** 12-20 weeks.

1. Add clean-room build and CI evidence.
2. Audit retail-data exclusion and source/license materials.
3. Produce and verify the release APK, checksums, installation guide, and update
   preservation instructions.
4. Repeat physical acceptance with the exact release artifact.
5. Consider AAB/Play publication only after the sideloaded package is complete.

## Acceptance matrix

Compilation, installation, a live process, and gameplay acceptance are separate
claims. An Android release should record them separately.

| Gate | Minimum evidence |
|---|---|
| Build | Clean `arm64-v8a` configure/link from an asserted commit |
| Package | APK inventory, ABI, manifest, signature, retail-data exclusion, checksum |
| Install | In-place update succeeds on a named physical device |
| Launch | Installed package starts and the native runtime remains live |
| Import | Valid image accepted; invalid/cancelled replacement preserves prior image |
| Rendering | Coherent title/menu/race pixels and recorded GLES route on physical GPUs |
| Touch | Complete touch-only race and repeated three-boost drift chains |
| Controller | Cold-connect and hot-plug mapping, axes, pause, and supported rumble |
| Audio | Music, voice, effects, route changes, and underrun evidence |
| Persistence | Memory card, settings, layout, and imported image survive cold relaunch and update |
| Lifecycle | Background/foreground, interruption, surface recreation, and held-input reset |
| Performance | Frame statistics, thermal behavior, and sustained race on representative devices |
| Accessibility | TalkBack traversal, labels, state, actions, and editor/options usability |

## Principal risks

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| Android GLES driver differences expose a renderer fault | Medium | High | Phase A physical GPU proof; retain and test the two-pass fallback |
| Touch parity is underestimated as “just buttons” | High | High | Treat the complete iOS behavior list as a product specification |
| Content URIs are treated as normal file paths | Medium | High | Stage through Android streams into private storage before validation/use |
| Activity or EGL surface recreation creates duplicate/stale native state | Medium | High | Keep one SDL-owned runtime and test lifecycle transitions early |
| Audio buffer choices work on one device and underrun elsewhere | Medium | Medium | Record device formats/buffers and test representative hardware/routes |
| Old Android fork is merged and regresses current CTRPad architecture | Medium | High | Cherry-pick concepts only; keep the current LP64/GLES/storage/test foundations |
| Device-matrix work expands without a support boundary | High | Medium | Start with ARM64, GLES 3, and a documented representative device set |
| Store work distracts from a playable APK | Medium | Medium | Ship and validate a sideloaded preview before considering Play publication |
| Public packaging accidentally contains protected or private data | Low | Critical | Extend package audits and verify from a clean corresponding-source tree |

## Scope controls

The first Android release should deliberately avoid:

- a Vulkan renderer;
- an Android-wide engine rewrite;
- replacing SDL with GameActivity or a custom native lifecycle before evidence
  shows SDLActivity is insufficient;
- supporting 32-bit ABIs;
- broad external-storage permissions;
- direct execution from arbitrary document-provider URIs;
- redesigning the working iOS overlay into a shared UI framework;
- committing or bundling retail media; and
- claiming broad Android compatibility from one emulator or one phone.

These exclusions keep the work focused on the smallest existing seams and
preserve the proven Apple implementation.

## Final recommendation

Proceed with Phase A if there is enough community demand to justify one or two
focused engineering weeks. CTRPad's completed ARM64 conversion and shared GLES
renderer make it one of the stronger candidates for an Android port.

Do not commit immediately to an “identical to iPhone” public release. Use the
controller-driven ARM64 proof as the first investment gate, then fund the
Android importer and touch experience only after real hardware confirms the
portable core.

The likely outcome is favorable: the game itself should transfer, and the main
cost is recreating the mobile product layer carefully. The honest planning
label is **medium to large platform port**, not “small build change” and not
“enormous engine rewrite.”

## Evidence and references

Repository evidence:

- [`CMakeLists.txt`](../CMakeLists.txt) — Android GLES default, SDL build, target
  type, platform sources, and tests.
- [`main.c`](../main.c) — entry point, platform selection, startup, import, and
  iOS-specific display-loop boundary.
- [`platform/native_renderer.c`](../platform/native_renderer.c) — GLES 3
  contract, optional framebuffer fetch, fallback, and presentation.
- [`platform/native_storage.c`](../platform/native_storage.c) — current Apple
  sandbox versus desktop storage split.
- [`platform/native_input.c`](../platform/native_input.c) — SDL controller and
  portable touch-input transport.
- [`platform/apple/native_ios_touch.m`](../platform/apple/native_ios_touch.m) —
  current mobile controls, settings, editing, accessibility, and lifecycle
  behavior to reproduce.
- [`platform/apple/native_ios_import.m`](../platform/apple/native_ios_import.m)
  — current staged and validated user-file import contract.
- [`ref/README.md`](../ref/README.md) — provenance and limits of the historical
  Android fork.
- [`docs/history/README.md`](history/README.md) — completed Apple-port phase map
  and evidence boundary.

External primary references:

- [SDL3 Android README](https://wiki.libsdl.org/SDL3/README-android)
- [Android Storage Access Framework](https://developer.android.com/training/data-storage/shared/documents-files)
- [Android OpenGL ES support and manifest declarations](https://developer.android.com/develop/ui/views/graphics/opengl/about-opengl)
- [Google Play target API requirements](https://developer.android.com/google/play/requirements/target-sdk)
- [Google Play 64-bit support requirements](https://developer.android.com/google/play/requirements/64-bit)

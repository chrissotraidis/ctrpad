# Build, Sign, and Sideload CTRPad on iPhone or iPad

CTRPad is GPL-3.0 software. It does not include Crash Team Racing, a disc
image, extracted retail data, saves, an Apple certificate, a private key, or a
provisioning profile. Obtain the source corresponding to this build from
<https://github.com/chrissotraidis/ctrpad>. The app reports its exact source
commit in its build identity; rebuild that commit to reproduce or modify it.

This document is Installation Information for the sideloaded build. It is not
legal advice and does not grant rights to retail game data or Sony/Naughty Dog
marks. Use only your own compatible NTSC-U retail disc image.

Tagged [GitHub Releases](https://github.com/chrissotraidis/ctrpad/releases)
provide a retail-free unsigned IPA and its exact corresponding source. You can
also build the app from this repository. In either case, sign the IPA with your
own Apple ID; one device build supports both iPhone and iPad.

## Choose an installation path

| Goal | Recommended path |
|---|---|
| Play on your iPhone or iPad with a personal Apple ID | Build the unsigned IPA, then import it into a compatible sideloading tool such as AltStore Classic so that tool can sign it for your device. |
| Install directly as an Apple developer | Build the device app and run `package-ios.sh` with your development identity, matching profile, and device UDID. |
| Develop or inspect the UI without signing hardware | Build the Simulator target and use `tools/install-ios-simulator.sh`. |

The simplest source-build sequence is:

```sh
brew install cmake ninja
git clone https://github.com/chrissotraidis/ctrpad.git
cd ctrpad
cmake --preset ios-device-arm64
cmake --build --preset ios-device-arm64
./package-ios.sh
```

The generated IPA is unsigned and cannot be installed until your chosen tool
applies an identity and provisioning profile that authorize the target device.
Continue below for the exact signing and verification details.

## Requirements

The packaging and structured device-evidence tools require Python 3.

- an ARM64 Mac with the current Xcode command-line tools;
- CMake 3.20 or newer and Ninja;
- an iPhone or iPad running iOS/iPadOS 15 or newer;
- Developer Mode enabled on the device for development-signed apps;
- for direct signing, an Apple signing identity in the login keychain and a
  non-expired iOS provisioning profile that authorizes the bundle ID and
  target device; or
- a compatible user-side IPA re-signing tool such as AltStore Classic.

Apple explains that third-party iOS/iPadOS code needs a provisioning profile,
that the profile belongs at `CTRPad.app/embedded.mobileprovision`, and that the
profile constrains the signer, App ID, devices, lifetime, and entitlements in
[TN3125](https://developer.apple.com/documentation/technotes/tn3125-inside-code-signing-provisioning-profiles?changes=_5).
Current iOS releases require signatures with DER entitlements; see Apple's
[latest code-signature format](https://developer.apple.com/documentation/xcode/using-the-latest-code-signature-format).

## Build and produce an unsigned IPA

From a clean source checkout:

```sh
cmake --preset ios-device-arm64
cmake --build --preset ios-device-arm64
./package-ios.sh
```

Or let the packager run both build commands:

```sh
./package-ios.sh --build
```

The result is written under ignored `dist/` with the 12-character source prefix
in its filename and a SHA-256 sidecar. The script
requires a thin ARM64 device executable, an iOS load command, the legal/source
installation resources, and the standard `Payload/CTRPad.app` IPA layout. It
fails if it sees known retail-media extensions or runtime save directories.
It also refuses a dirty checkout and requires the bundle's full
`CTRNativeSourceCommit` plus clean 12-character `CTRNativeBuildIdentity` to
match the checkout before packaging.

Package the exact corresponding source from the same clean committed checkout:

```sh
./package-source.sh
```

This writes `dist/CTRPad-source-<commit>.tar.gz` and its SHA-256 sidecar. The
source packager includes the complete tracked source and build inputs, vendored
SDL,
build/package scripts, GPL license, notices, modification history and this
Installation Information. It fails if tracked changes are present or if the
archive contains retail/runtime data, an IPA, a profile, a certificate or a
private-key-like file. Publish this archive alongside the matching signed or
unsigned IPA. The app's `CTRNativeSourceCommit` identifies all 40 Git
characters; the shorter build identity and filenames are display prefixes
only. For a release build from an extracted archive, pass the full commit
through the packager so CMake embeds the same exact identity:

```sh
./package-ios.sh --build \
  --source-commit FULL_40_CHARACTER_SOURCE_COMMIT
```

Keeping the generated `CTRPad-source-<12-character-commit>` root still gives
ordinary non-release builds a readable short identity. A renamed tree may be
configured directly with
`-DCTR_NATIVE_SOURCE_COMMIT=<full-or-12-character-commit>`, but IPA packaging
requires the full 40-character value.

To use a different App ID, configure and verify it explicitly:

```sh
./package-ios.sh --build \
  --bundle-id com.example.yourname.ctrpad
```

The unsigned IPA has no executable authorization and cannot be launched as-is.
A compatible re-signing tool must apply the user's own identity and profile.
AltStore's official documentation describes Classic as a user-side IPA
sideloading system and records current account, expiry, active-app, and
Developer Mode constraints:
<https://faq.altstore.io/altstore-classic/your-altstore> and
<https://faq.altstore.io/altstore-classic/how-to-install-altstore-macos>.
Follow that project's current import/install UI; do not give Apple credentials
to CTRPad or store them in this repository.

## Produce a manually signed IPA

If Xcode has already created or downloaded a compatible development/ad-hoc
profile and its identity is present in the keychain:

```sh
security find-identity -v -p codesigning

./package-ios.sh --build \
  --bundle-id com.example.yourname.ctrpad \
  --identity "Apple Development: Your Name (TEAMID)" \
  --profile /absolute/path/to/CTRPad.mobileprovision \
  --device YOUR_DEVICE_UDID
```

For an identity held in an already unlocked non-default keychain, add
`--keychain /absolute/path/to/signing.keychain-db`. The packager then limits
identity discovery and signing to that keychain; it does not modify the user's
default keychain or search list.

The script cryptographically verifies the profile CMS, validates its signer
chain and purpose, and pins its root to an Apple Root CA in the macOS system
root keychain before reading authorization values. It rejects
expiration/platform/App-ID/optional-device mismatches, constructs only CTRPad's
minimal application/team/keychain entitlements in Apple's
`<App ID prefix>.<bundle ID>` form, requests DER entitlements, and signs. It
then verifies the app's code-signing chain against an Apple root, requires its
leaf certificate to appear in the profile's `DeveloperCertificates`, reads the
final entitlements back, and binds the exact App ID prefix/bundle ID, team,
sole keychain group and optional `get-task-allow` to the profile. CTRPad's
signature is limited to those minimal keys; unexpected service entitlements
fail. Public trust evidence stays in temporary staging; the private key never
leaves the keychain. A decodable CMS or user-trusted self-signed root is not
accepted as Apple authorization.

CTRPad uses no Apple service capability such as Game Center, iCloud, push, an
app group, or background execution. If a later modification adds one, do not
use this minimal entitlement set unchanged: enable the capability in the App
ID, regenerate the profile, and update the signing workflow deliberately.

## Install a signed app directly

Unpack the signed IPA and install its app bundle with Xcode's device tool:

```sh
mkdir -p /private/tmp/ctrpad-install
ditto -x -k dist/CTRPad-0.1.0-1-SOURCEPREFIX-signed.ipa /private/tmp/ctrpad-install
xcrun devicectl list devices
xcrun devicectl device install app \
  --device YOUR_DEVICE_ID \
  /private/tmp/ctrpad-install/Payload/CTRPad.app
```

The connected device must be unlocked, trusted, in Developer Mode, and covered
by the embedded profile. Do not use the example temporary path as a permanent
backup. Apple documents registered-device development/distribution at
<https://developer.apple.com/documentation/Xcode/distributing-your-app-to-registered-devices>.

## Run the physical-device acceptance campaign

For the release campaign, prefer the repository helper to separate offline
signature/profile proof from device mutation and to retain versioned
`devicectl` JSON rather than scraping its human-formatted output.

First validate the signed IPA without contacting an iPad:

```sh
./tools/ios-device-campaign.sh preflight \
  --ipa dist/CTRPad-0.1.0-1-SOURCEPREFIX-signed.ipa \
  --source-commit FULL_40_CHARACTER_SOURCE_COMMIT \
  --device-udid YOUR_PHYSICAL_IPAD_UDID \
  --evidence-dir dist/device-acceptance-preflight
```

The preflight requires a standard one-app IPA, valid sidecar when present,
thin ARM64 `IOS` executable, strict non-ad-hoc signature, embedded non-expired
iOS profile, cryptographically valid profile CMS, Apple-root-pinned profile and
app certificate chains, provisioning-profile signer purpose, exact App
ID prefix/bundle/team, profile-authorized sole keychain group, minimal signed
entitlement allowlist, target UDID authorization, leaf
signing-certificate membership in the profile, distribution resources and
retail/runtime-data exclusion. It does not prove that installation or launch
works. Raw profile/device trust evidence belongs only in the ignored evidence
directory.

With the iPad unlocked, trusted and in Developer Mode, update-install and
launch without uninstalling:

```sh
./tools/ios-device-campaign.sh prepare \
  --ipa dist/CTRPad-0.1.0-1-SOURCEPREFIX-signed.ipa \
  --source-commit FULL_40_CHARACTER_SOURCE_COMMIT \
  --device-udid YOUR_PHYSICAL_IPAD_UDID \
  --evidence-dir dist/device-acceptance-initial
```

The helper saves `devicectl` 518-compatible versioned JSON and log files for
device discovery, details, installation, installed-app lookup and launch. It
structurally verifies the exact command type and success envelope, then requires
an explicit positive integer JSON schema version, the expected installed
bundle/version/build and launched `CTRPad` process.
Bundle text elsewhere in a failed or empty response is not sufficient. The
verified remote app URL and positive process identifier are retained in the
ignored manifest. It hashes the signed packaged executable but does not claim
it can read back the installed executable from iPadOS. It never calls
`uninstall` and never copies the retail image to or from the Mac.
The preflight/prepare manifests bind the same full source commit; collection
refuses a prepare manifest without that verified identity.

After the human touch/race/lifecycle test, collect CTRPad's rotating logs,
diagnostics and saves:

```sh
./tools/ios-device-campaign.sh collect \
  --device-udid YOUR_PHYSICAL_IPAD_UDID \
  --bundle-id io.github.chrissotraidis.ctrpad \
  --evidence-dir dist/device-acceptance-initial
```

Collection reads only CTRPad's Application Support domain. It deliberately
does not copy `Documents/CTRPad`, where the 605 MB retail image lives. Raw
evidence contains a device identifier and user save data, so the tool permits
in-repository output only under a gitignored path such as `dist/`; review and
redact it before sharing. Collection first requires the successful matching
prepare manifest and exact device/bundle values, then writes targeted-fault,
FPS, frame-distribution, device-state, campaign-event and save-hash summaries
beside the versioned CoreDevice files. `frame-stats-rows.txt` retains each
120-frame mean/median/nearest-rank p95/p99/maximum wall-cadence window, while
`device-state-rows.txt` retains startup and change events for thermal state,
Low Power Mode and battery context. Zero rows mean missing evidence, not a
pass. Collection also requires an exact installed-app result, records its
observed version/build, and validates the `devicectl.device.copy.from` success
envelope before claiming collection success. Copy
`docs/templates/IOS-DEVICE-ACCEPTANCE.md` into a dated `docs/parity/` report and
fill every result from observation. The logs measure sustained cadence and
system thermal/power states but cannot certify human touch ergonomics, a
complete race, audio quality, enclosure temperature, energy consumption, Files
behavior or update persistence automatically.

## Install an exact Simulator build

Simulator installation does not use an Apple development certificate or a
provisioning profile. Build the thin ARM64 Simulator product, boot exactly one
target Simulator, and use the repository helper:

```sh
cmake --preset ios-simulator-arm64
cmake --build --preset ios-simulator-arm64

./tools/install-ios-simulator.sh \
  --device YOUR_BOOTED_SIMULATOR_UDID \
  --launch
```

The helper refuses zero or multiple booted Simulators, validates the ARM64
`IOSSIMULATOR` load command, copies the app into an isolated temporary
directory, applies and strictly verifies an ad-hoc Simulator-only signature,
then performs an update install. It resolves the installed application
container and requires its executable SHA-256 to equal the signed staged
executable. This prevents an older installed bundle from being mistaken for
the current source product and leaves the possibly stale build-tree signature
untouched.

For a validation Simulator that already contains the retail image and a
slot-zero save, request the slower preservation proof:

```sh
./tools/install-ios-simulator.sh \
  --device YOUR_BOOTED_SIMULATOR_UDID \
  --verify-persistence \
  --launch
```

That mode requires both files to exist before the update and compares each
file's inode, size and SHA-256 before and after installation. It can take
several minutes for a full retail image on a slow Simulator host. The helper
does not boot or shut down devices, delete an app, modify a runtime container,
or create a physical-device signature.

## Import the retail image and verify persistence

1. Launch CTRPad. If no valid image is installed, the native Files chooser
   appears.
2. Select your own NTSC-U, single-track raw MODE2/2352 BIN image. CTRPad stages,
   validates, and installs it in the app's Documents container; the image is
   never part of the IPA.
3. Start the game with the top-center **START / PAUSE** touch control, create or
   update a memory-card save, background and relaunch the app, and verify the
   profile remains visible.
4. Before installing a replacement build, preserve the same bundle ID and use
   an update install. Deleting the app deletes its container, retail import,
   and saves unless the sideload tool separately backs them up.

For source-build details, evidence boundaries, modifications, and known
limitations, read `README.md`, `docs/ROADMAP.md`, `docs/DECISIONS.md`,
`docs/history/ENGINEERING-JOURNAL.md`, and the reports under `docs/parity/` in
the corresponding source checkout. Physical-device performance, touch feel,
full-race completion, drift/boost ergonomics, and final user-owned signing must
be verified on the actual target iPad; Simulator evidence does not replace
those gates.

# Install CTRPad on macOS

> [!IMPORTANT]
> **Build your own copy with [PadMint](https://github.com/chrissotraidis/padmint/releases/latest).**
> CTRPad releases publish no app, only the recipe PadMint follows.

CTRPad is a native Apple Silicon application for macOS 11 or newer. The app
contains no Crash Team Racing data. On first launch it asks you to choose your
own compatible NTSC-U single-track raw MODE2/2352 BIN.

Build `CTRPad.app` with PadMint, which saves a ZIP of the app in the folder
you choose, or locally using the short sequence below. Earlier releases with
prebuilt Mac ZIPs are retired.

## Quick local build

```sh
brew install cmake ninja
git clone https://github.com/chrissotraidis/ctrpad.git
cd ctrpad
cmake --preset macos-arm64-app
cmake --build --preset macos-arm64-app
ctest --preset macos-arm64-app --output-on-failure
open build-macos-arm64-app/CTRPad.app
```

## Install a PadMint or packaged build

1. Build the ZIP with PadMint, or package the app locally as described below.
2. Open the archive and move `CTRPad.app` to Applications.
3. Open CTRPad.
4. Choose your own compatible retail BIN when prompted.

A Developer ID-signed and notarized release opens normally. An ad-hoc-signed
package is not notarized; on first launch, Control-click the app,
choose **Open**, and confirm macOS's prompt. Do not disable Gatekeeper
system-wide.

CTRPad validates the selected image before starting. CUE files, compressed
archives, cooked 2048-byte ISOs, other regions, and incomplete images are not
accepted. The app remembers the selected file's external path and does not
copy it into `CTRPad.app`. Keep the BIN at that location.

To deliberately choose a different image from Terminal:

```sh
/Applications/CTRPad.app/Contents/MacOS/CTRPad --choose-disc
```

## Controls

The window is resizable and preserves the game's 4:3 presentation. Use `F11`
or `Option-Return` to enter or leave fullscreen.

| PlayStation input | Keyboard | Mouse |
|---|---|---|
| D-pad / steer | `W` `A` `S` `D` or arrow keys | — |
| Triangle / View / Back | Escape or `Z` | — |
| Square / Brake | `E` or `X` | Right button |
| Cross / Gas | Either Shift key | Left button |
| Circle / Item | Space or `V` | Middle button |
| L1 / R1 drift | `Q` / `R` | Mouse 4 / Mouse 5 |
| L2 / R2 | Control keys | — |
| Start / Pause | `P` or Return | — |
| Select | Tab | — |

Click the **•••** button at the top right of the game window to open CTRPad's
native Mac Options panel:

- **Display** selects 1×, 2×, 3×, or 4× internal geometry resolution. Apple
  Silicon Macs default to 4×; changes apply immediately and persist.
- **Keyboard** changes either binding for every PlayStation input or restores
  the racing-first defaults. Press Backspace while capturing to clear a key.
- **Controller** shows the shared controller layout and whether macOS currently
  detects a compatible gamepad.

Keyboard remapping changes only CTRPad's host aliases, not any control setting
stored by the retail game. Display scaling preserves the original textures,
logical VRAM effects, aspect ratio, and retail game state.

Standard SDL-compatible Bluetooth and USB controllers are supported alongside
keyboard and mouse input. macOS, iOS, and iPadOS use the same standard SDL
gamepad-to-PlayStation mapping.

## Saves, logs, and preferences

CTRPad keeps writable data outside the application bundle under:

```text
~/Library/Application Support/chrissotraidis/CTRPad/
```

That directory contains memory-card saves and `Crash Team Racing.log`. App
updates do not need to replace it. The remembered disc path and custom keyboard
bindings are macOS app preferences, not copies of the game.

## Build locally

Install Xcode or its command-line tools, CMake 3.20 or newer, and Ninja:

```sh
cmake --preset macos-arm64-app
cmake --build --preset macos-arm64-app
ctest --preset macos-arm64-app
```

The app is written to `build-macos-arm64-app/CTRPad.app` and ad-hoc signed for
local execution.

Create a retail-free distributable archive from a clean checkout:

```sh
./package-macos.sh --build
```

Maintainers with a Developer ID Application identity and notarytool profile
can produce a hardened, notarized archive:

```sh
./package-macos.sh --build \
  --identity "Developer ID Application: Your Name (TEAMID)" \
  --notary-profile CTRPad-Notary
```

The package script verifies the bundle architecture, metadata, icon,
signature, source identity, required legal resources, and absence of retail or
runtime data before writing the ZIP and SHA-256 sidecar under ignored `dist/`.

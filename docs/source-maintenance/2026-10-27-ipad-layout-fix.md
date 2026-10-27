# iPad landscape cold-launch layout correction

The owner reported the game image displaced toward the lower left, with the
right controls clipped, after installing the local maintenance build. The same
fault reproduced on the physical iPad with source `216c7ba21d17`.
The device was in landscape, but the game logged a 1024×1366-point,
2048×2732-pixel portrait surface.

## Cause and correction

CTRPad's existing SDL UIKit patch preserves the root controller while swapping
its generic view for the OpenGL view. It directly attaches the replacement
view to the window to avoid unbalanced appearance transitions. That path did
not explicitly size the replacement. Its initial frame could retain portrait
orientation even though the actual window was landscape; the renderer and
touch overlay then inherited the incorrect dimensions.

Local commit `1b2cc58e63` sets the replacement view's frame to the current
UIWindow bounds before direct attachment, including the reverse path when
restoring an older view. The existing flexible-width/height autoresizing mask
continues to handle subsequent resizing. It retains the balanced controller
lifecycle and requires no renderer viewport compensation or forced rotation.
The reviewed SDL tree is now `da0ace7b54deef5b1fd7e4d1d16e922f1eb095d0`.

## Verification

- iOS device and ARM64 Simulator builds passed.
- Source-tree verification and offline source-lock mutation tests passed.
- Signed package and physical-device preflight passed for clean source
  `1b2cc58e63`; the installed application, team and keychain identities match
  the prior app.
- The same physical iPad cold-launched in landscape with the corrected
  1366×1024-point, 2732×2048-pixel surface. Device screenshots show the legal
  screen and subsequent rendered intro filling the display, with the right
  controls and Options button inside its bounds.
- A fresh Documents/Library backup preceded the in-place update. SHA-256
  comparison before launch confirmed game data, saves, preferences and logs
  unchanged. iPadOS discarded nine launch-screen snapshot cache files; those
  are retained in the local backup and are not game data.
- Startup diagnostics contained no ERROR or FATAL entries.
- After installation, the owner physically rotated the iPad to portrait and
  back to landscape and confirmed that the picture and controls stayed
  correctly positioned. This closes the reported layout and rotation defect
  on the tested iPad; it is not a completed-race or controller acceptance result.

The device orientation command returned success for a portrait request, but a
subsequent query and screenshot still showed landscape. That command was not
evidence of a successful rotation test; the owner's subsequent physical test
above supplies that confirmation. Live window resizing, Options interaction
and completed gameplay remain separate acceptance checks. The Simulator was
built, not used as runtime evidence for this correction.

Private backups, signed package, device logs and screenshots remain under
ignored `dist/ipad-maintenance-install.v0Xhf1/layout-fix/`. No binary, retail
material, save or signing data was added to Git. Nothing was pushed or
published during that initial test; its installed version was 0.2.3 (build 8).

## Final 0.2.4 release installation

After [PR #48](https://github.com/chrissotraidis/ctrpad/pull/48) merged and
[0.2.4 was published](https://github.com/chrissotraidis/ctrpad/releases/tag/v0.2.4),
the final release build was installed on the same physical iPad on October 27,
2026. The installed version is **0.2.4 (build 9)** from release commit
`7cdf9ef067511fc0fbc3f8dacb9f83e6e732a115`.

The app came from the successful post-publication PadMint iOS player check.
Its package hash, version and full source identity were verified before
signing; the existing application, team and keychain identities matched.
Signing verification and device preflight passed before the in-place update.

CTRPad was stopped and a fresh Documents/Library backup was taken. Comparing
SHA-256 manifests before the first launch confirmed the imported game data,
saves and preferences were unchanged. There were no unexpected differences
among the 24 original files; iPadOS discarded eight launch-screen snapshot
cache files during the update. The backup remains private and local.

Device evidence confirmed the installed 0.2.4/build-9 identity, successful
launch and a running process afterward. A physical-device screenshot showed
the picture and controls correctly positioned in landscape; the startup log
reported 1366×1024 points and 2732×2048 pixels, with no ERROR or FATAL entries.
The owner's successful portrait-to-landscape rotation check above was on the
preceding build containing the same layout fix; it was not repeated as a new
owner test of the final release package. Completed-race, extended gameplay
and controller-specific acceptance remain open.

Final-install evidence is under ignored `dist/ipad-final-024-3y2vav9m/`.
No private package, screenshot, game data, save or signing material is tracked
or publicly attached. See the [release checks](2026-10-27-maintenance-validation.md#release-and-final-device-checks)
for hosted CI, publication audits and fresh-player build results.

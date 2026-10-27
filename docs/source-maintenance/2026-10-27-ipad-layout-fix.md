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

The device orientation command returned success for a portrait request, but a
subsequent query and screenshot still showed landscape. This is not evidence
of a successful rotation test. Physical portrait/landscape rotation, live
window resizing, Options interaction and completed gameplay remain separate
acceptance checks. The Simulator was built, not used as runtime evidence for
this correction.

Private backups, signed package, device logs and screenshots remain under
ignored `dist/ipad-maintenance-install.v0Xhf1/layout-fix/`. No binary, retail
material, save or signing data was added to Git. Nothing was pushed or
published; the installed version remains 0.2.3 (build 8).

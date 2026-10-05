# Agent instructions

## Releases publish the recipe only

CTRPad apps are compiled from the CTR-ModSDK decompilation, so every app is a
personal build. Players build their own copy with PadMint
(https://github.com/chrissotraidis/padmint) and choose their own disc in the app.

- A release contains `CTRPad-vX.Y.Z-padmint.json` (a copy of `padmint.json`)
  and `SHA256SUMS`, nothing else. Never upload, attach, or link an IPA, macOS
  ZIP, APK, or other app build anywhere public: releases, workflow artifacts,
  issues, Discord, or a sideloading source.
- `padmint.json` must keep `"publication": {"public_binaries": false}`.
- `version.json` is the single version. Bump it for each release; CMake reads it.
- Before publishing, every release file must pass `python3 -m padmint audit <file>`
  from the PadMint checkout. A failure is a stop, not a note.
- Never commit or attach disc images, extracted game data, saves, or signing material.

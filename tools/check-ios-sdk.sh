#!/bin/sh
# PadMint's first iPhone/iPad step: stop early, in plain words, when Xcode's iOS
# platform is missing, instead of failing later inside CMake.
if xcrun --sdk iphoneos --show-sdk-path >/dev/null 2>&1; then
    echo "Xcode iOS SDK: $(xcrun --sdk iphoneos --show-sdk-version 2>/dev/null)"
    exit 0
fi
cat >&2 <<'MSG'
CTRPad for iPhone and iPad needs Xcode's iOS platform, and this Mac doesn't have it.
Open Xcode, choose Settings > Components, add iOS, wait for the download to finish,
then run PadMint again. (Building for this Mac doesn't need it.)
MSG
exit 1

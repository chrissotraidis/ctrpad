#!/bin/sh

set -eu

fail() {
    echo "package-macos: $*" >&2
    exit 1
}

usage() {
    cat <<'EOF'
Usage: ./package-macos.sh [--build] [--identity NAME] [--notary-profile NAME]

Creates a retail-free Apple Silicon CTRPad.app ZIP under dist/.
Without --identity the staged app is ad-hoc signed. --notary-profile requires
a Developer ID Application identity and submits with xcrun notarytool.
EOF
}

build_app=0
signing_identity=""
notary_profile=""

while [ "$#" -gt 0 ]; do
    case "$1" in
        --build)
            build_app=1
            shift
            ;;
        --identity)
            [ "$#" -ge 2 ] || fail "--identity requires a value"
            signing_identity=$2
            shift 2
            ;;
        --notary-profile)
            [ "$#" -ge 2 ] || fail "--notary-profile requires a value"
            notary_profile=$2
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            fail "unknown argument: $1"
            ;;
    esac
done

[ "$(uname -s)" = "Darwin" ] || fail "macOS is required"
[ -z "$notary_profile" ] || [ -n "$signing_identity" ] || \
    fail "--notary-profile requires --identity"

repo_root=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
cd "$repo_root"
python3 "$repo_root/tools/check-sources.py"

git diff --quiet && git diff --cached --quiet || fail "tracked checkout must be clean"
source_commit=$(git rev-parse HEAD)
case "$source_commit" in
    [0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]*) ;;
    *) fail "could not determine source commit" ;;
esac

if [ "$build_app" -eq 1 ]; then
    cmake --preset macos-arm64-app
    cmake --build --preset macos-arm64-app
fi

source_app="$repo_root/build-macos-arm64-app/CTRPad.app"
[ -x "$source_app/Contents/MacOS/CTRPad" ] || \
    fail "missing app; run with --build or build the macos-arm64-app preset"

stage_root=$(mktemp -d "${TMPDIR:-/tmp}/ctrpad-macos-package.XXXXXX")
cleanup() {
    case "$stage_root" in
        "${TMPDIR:-/tmp}"/ctrpad-macos-package.*)
            [ ! -d "$stage_root" ] || rm -rf -- "$stage_root"
            ;;
        *)
            echo "package-macos: refusing to remove unexpected staging path: $stage_root" >&2
            ;;
    esac
}
trap cleanup EXIT HUP INT TERM
stage_app="$stage_root/CTRPad.app"
ditto "$source_app" "$stage_app"

info_plist="$stage_app/Contents/Info.plist"
executable="$stage_app/Contents/MacOS/CTRPad"
[ -f "$info_plist" ] || fail "bundle has no Info.plist"
[ "$(plutil -extract CFBundlePackageType raw -o - "$info_plist")" = "APPL" ] || \
    fail "bundle package type is not APPL"
[ "$(plutil -extract CFBundleIdentifier raw -o - "$info_plist")" = "io.github.chrissotraidis.ctrpad" ] || \
    fail "unexpected bundle identifier"
[ "$(plutil -extract CTRNativeSourceCommit raw -o - "$info_plist")" = "$source_commit" ] || \
    fail "bundle source identity does not match checkout"
build_identity=$(plutil -extract CTRNativeBuildIdentity raw -o - "$info_plist")
case "$build_identity" in
    *-dirty|unknown*) fail "bundle has a dirty or unknown build identity" ;;
esac
[ -f "$stage_app/Contents/Resources/CTRPad.icns" ] || fail "bundle has no app icon"
[ -f "$stage_app/Contents/Resources/LICENSE" ] || fail "bundle has no GPL license"
[ -f "$stage_app/Contents/Resources/THIRD_PARTY_NOTICES.md" ] || fail "bundle has no third-party notices"
[ -f "$stage_app/Contents/Resources/INSTALL-MACOS.md" ] || fail "bundle has no macOS installation guide"

architectures=$(lipo -archs "$executable")
[ "$architectures" = "arm64" ] || fail "expected thin arm64 executable, got: $architectures"

if find "$stage_app" -type f \( -iname '*.bin' -o -iname '*.cue' -o -iname '*.iso' \
    -o -iname '*.chd' -o -iname '*.pbp' -o -iname '*.mcr' -o -iname '*.mcd' \) \
    -print -quit | grep -q .; then
    fail "bundle contains prohibited retail or runtime data"
fi

if [ -n "$signing_identity" ]; then
    codesign --force --options runtime --timestamp --sign "$signing_identity" "$stage_app"
else
    codesign --force --sign - --identifier io.github.chrissotraidis.ctrpad "$stage_app"
fi
codesign --verify --deep --strict --verbose=2 "$stage_app"

version=$(plutil -extract CFBundleShortVersionString raw -o - "$info_plist")
short_commit=$(printf '%s' "$source_commit" | cut -c1-12)
mkdir -p "$repo_root/dist"
archive="$repo_root/dist/CTRPad-macOS-arm64-${version}-${short_commit}.zip"
rm -f "$archive" "$archive.sha256"
ditto -c -k --sequesterRsrc --keepParent "$stage_app" "$archive"

if [ -n "$notary_profile" ]; then
    xcrun notarytool submit "$archive" --keychain-profile "$notary_profile" --wait
    xcrun stapler staple "$stage_app"
    xcrun stapler validate "$stage_app"
    rm -f "$archive"
    ditto -c -k --sequesterRsrc --keepParent "$stage_app" "$archive"
fi

archive_hash=$(shasum -a 256 "$archive" | cut -d ' ' -f 1)
printf '%s  %s\n' "$archive_hash" "$(basename "$archive")" > "$archive.sha256"
(
    cd "$repo_root/dist"
    shasum -a 256 -c "$(basename "$archive").sha256" >/dev/null
)
echo "macOS package: $archive"
echo "SHA-256: $archive_hash"

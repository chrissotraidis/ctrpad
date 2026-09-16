#!/usr/bin/env bash
# Create and verify a deterministic GPL corresponding-source archive.

set -euo pipefail

usage() {
    cat <<'EOF'
Usage: ./package-source.sh [options]

Options:
  --ref REF       Git commit/tag to package (default: HEAD).
  --output PATH   Output .tar.gz path
                  (default: dist/CTRPad-source-<commit>.tar.gz).
  -h, --help      Show this help.

The archive is produced only from a committed, clean tracked tree. Ignored and
untracked retail media, saves, build output, profiles, and keys are never read.
The same Git commit produces a byte-identical gzip stream on this toolchain.
EOF
}

fail() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

require_command() {
    command -v "$1" >/dev/null 2>&1 || fail "required command not found: $1"
}

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$script_dir"
source_ref="HEAD"
output_path=""

while (($#)); do
    case "$1" in
        --ref|--output)
            (($# >= 2)) || fail "$1 requires a value"
            case "$1" in
                --ref) source_ref="$2" ;;
                --output) output_path="$2" ;;
            esac
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            fail "unknown option: $1"
            ;;
    esac
done

for command_name in python3 basename cut dirname git grep gzip head mkdir mktemp mv rm shasum tar tr wc; do
    require_command "$command_name"
done

git -C "$repo_root" rev-parse --is-inside-work-tree >/dev/null 2>&1 || \
    fail "package-source.sh must run from its Git checkout"

tracked_status="$(git -C "$repo_root" status --porcelain=v1 --untracked-files=no)"
[[ -z "$tracked_status" ]] || \
    fail "tracked changes are present; commit them before packaging corresponding source"

source_commit="$(git -C "$repo_root" rev-parse --verify "${source_ref}^{commit}" 2>/dev/null)" || \
    fail "not a Git commit or tag: $source_ref"
short_commit="${source_commit:0:12}"
archive_root="CTRPad-source-${short_commit}"

if [[ -z "$output_path" ]]; then
    output_path="$repo_root/dist/${archive_root}.tar.gz"
elif [[ "$output_path" != /* ]]; then
    output_path="$repo_root/$output_path"
fi

output_dir="$(dirname "$output_path")"
mkdir -p "$output_dir"
output_dir="$(cd "$output_dir" && pwd)"
output_path="$output_dir/$(basename "$output_path")"
[[ "$output_path" == *.tar.gz ]] || fail "output path must end in .tar.gz"
[[ ! -e "$output_path" && ! -e "$output_path.sha256" ]] || \
    fail "output already exists; choose a new --output path: $output_path"

tmp_dir="$(mktemp -d "${TMPDIR:-/tmp}/ctrpad-source-package.XXXXXX")"
cleanup() {
    case "$tmp_dir" in
        "${TMPDIR:-/tmp}"/ctrpad-source-package.*) rm -rf "$tmp_dir" ;;
        *) printf 'WARNING: refusing to remove unexpected temporary path: %s\n' "$tmp_dir" >&2 ;;
    esac
}
trap cleanup EXIT INT TERM

archive_tar="$tmp_dir/${archive_root}.tar"
archive_list="$tmp_dir/archive-members.txt"
archive_gzip="$tmp_dir/${archive_root}.tar.gz"
archive_sidecar="$tmp_dir/${archive_root}.tar.gz.sha256"

git -C "$repo_root" archive --format=tar --prefix="${archive_root}/" \
    "$source_commit" >"$archive_tar"
tar -tf "$archive_tar" >"$archive_list"

for required_file in \
    .gitignore \
    CMakeLists.txt \
    CMakePresets.json \
    CONTRIBUTING.md \
    LICENSE \
    README.md \
    RIGHTS_AND_LICENSES.md \
    THIRD_PARTY_NOTICES.md \
    build.sh \
    docs/DECISIONS.md \
    docs/INSTALL-IOS.md \
    docs/INSTALL-MACOS.md \
    docs/ROADMAP.md \
    docs/templates/IOS-DEVICE-ACCEPTANCE.md \
    docs/history/ENGINEERING-JOURNAL.md \
    docs/history/PROGRESS-LOG.md \
    externals/SDL/LICENSE.txt \
    main.c \
    package-ios.sh \
    package-macos.sh \
    package-source.sh \
    tools/install-ios-simulator.sh \
    tools/ios-device-campaign.sh \
    tools/test-devicectl-json.sh \
    tools/test-ios-build-identity.sh \
    tools/test-ios-entitlement-binding.sh \
    tools/verify-devicectl-json.sh \
    tools/verify-ios-build-identity.sh \
    tools/verify-ios-entitlement-binding.sh \
    tools/verify-ios-signing-trust.sh; do
    grep -Fxq "${archive_root}/${required_file}" "$archive_list" || \
        fail "source archive is missing required file: $required_file"
done

for required_tree in docs/history externals/SDL game include platform tools; do
    grep -q "^${archive_root}/${required_tree}/" "$archive_list" || \
        fail "source archive is missing required tree: $required_tree"
done

retail_match="$(grep -Ei \
    '/(ctr-u\.bin|[^/]+\.(bin|img|iso|cue|ccd|sub|big|hwl|xa|str|tim|sav|mcr))$' \
    "$archive_list" | head -1 || true)"
[[ -z "$retail_match" ]] || fail "retail/runtime-like file found in source archive: $retail_match"

private_match="$(grep -Ei \
    '/([^/]+\.(ipa|mobileprovision|provisionprofile|p12|p8|pfx|cer|crt|der|pem|key)|id_rsa|id_ed25519|\.env)$' \
    "$archive_list" | head -1 || true)"
[[ -z "$private_match" ]] || fail "credential/package-like file found in source archive: $private_match"

generated_match="$(grep -E \
    "^${archive_root}/(build(/|$)|build-[^/]+/|dist/|assets/|debug/|memcards/|\.git/)" \
    "$archive_list" | head -1 || true)"
[[ -z "$generated_match" ]] || fail "generated/runtime path found in source archive: $generated_match"

mkdir "$tmp_dir/verify"
tar -xf "$archive_tar" -C "$tmp_dir/verify"
if [[ -f "$tmp_dir/verify/$archive_root/sources.lock.json" || -f "$tmp_dir/verify/$archive_root/tools/check-sources.py" ]]; then
    python3 "$tmp_dir/verify/$archive_root/tools/check-sources.py"
fi

gzip -n -9 -c "$archive_tar" >"$archive_gzip"
gzip -t "$archive_gzip"
tar -tzf "$archive_gzip" >/dev/null

member_count="$(wc -l <"$archive_list" | tr -d '[:space:]')"
archive_hash="$(shasum -a 256 "$archive_gzip" | cut -d ' ' -f 1)"
[[ "$archive_hash" =~ ^[0-9a-f]{64}$ ]] || fail "could not hash completed source archive"
printf '%s  %s\n' "$archive_hash" "$(basename "$output_path")" >"$archive_sidecar"

mv "$archive_gzip" "$output_path"
mv "$archive_sidecar" "$output_path.sha256"
(
    cd "$output_dir"
    shasum -a 256 -c "$(basename "$output_path").sha256" >/dev/null
)

printf 'Wrote %s\n' "$output_path"
printf 'Wrote %s\n' "$output_path.sha256"
printf 'Source commit: %s\n' "$source_commit"
printf 'Archive members: %s\n' "$member_count"
printf 'Retail media, runtime state, packages, profiles, and keys: excluded\n'

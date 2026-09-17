#!/usr/bin/env bash
# Structurally verify versioned devicectl JSON evidence for the iPad campaign.

set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
  ./tools/verify-devicectl-json.sh envelope \
    --json PATH --command-type TYPE --output PATH
  ./tools/verify-devicectl-json.sh install \
    --json PATH --bundle-id ID --output PATH
  ./tools/verify-devicectl-json.sh installed-app \
    --json PATH --bundle-id ID [--version VERSION --build BUILD] --output PATH
  ./tools/verify-devicectl-json.sh launch \
    --json PATH --bundle-id ID --executable NAME --output PATH

All modes require a successful, error-free, versioned devicectl envelope with
the expected commandType. Specialized modes additionally validate the exact
installed bundle or launched process rather than searching arbitrary JSON text.
EOF
}

fail() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

require_command() {
    command -v "$1" >/dev/null 2>&1 || fail "required command not found: $1"
}

# Older macOS plutil versions do not accept JSON input consistently. Parse
# devicectl's actual JSON with Python; preserve the existing strict field checks.
json_query() {
    python3 - "$json_path" "$1" "${2:-raw}" <<'PYQUERY'
import json
import sys
try:
    with open(sys.argv[1]) as stream:
        value = json.load(stream)
    for key in sys.argv[2].split('.'):
        value = value[int(key)] if isinstance(value, list) else value[key]
    if sys.argv[3] == 'type':
        names = {str: 'string', int: 'integer', float: 'real', bool: 'boolean',
                 list: 'array', dict: 'dictionary', type(None): 'null'}
        print(names[type(value)])
    elif isinstance(value, (str, int, float)) and not isinstance(value, bool):
        print(value)
    else:
        sys.exit(1)
except (OSError, ValueError, KeyError, IndexError, TypeError):
    sys.exit(1)
PYQUERY
}

extract_required() {
    local key_path="$1"
    local label="$2"
    local value
    if ! value="$(json_query "$key_path" 2>/dev/null)"; then
        fail "devicectl JSON is missing $label at $key_path"
    fi
    [[ -n "$value" ]] || fail "devicectl JSON contains an empty $label"
    printf '%s\n' "$value"
}

mode="${1:-}"
if [[ -z "$mode" ]]; then
    usage
    exit 1
fi
if [[ "$mode" == "-h" || "$mode" == "--help" ]]; then
    usage
    exit 0
fi
case "$mode" in
    envelope|install|installed-app|launch) ;;
    *) fail "unknown mode: $mode" ;;
esac
shift

requested_json_path=""
requested_output_path=""
expected_command_type=""
bundle_id=""
bundle_version=""
build_version=""
executable_name=""
while (($#)); do
    case "$1" in
        --json|--output|--command-type|--bundle-id|--version|--build|--executable)
            (($# >= 2)) || fail "$1 requires a value"
            case "$1" in
                --json) requested_json_path="$2" ;;
                --output) requested_output_path="$2" ;;
                --command-type) expected_command_type="$2" ;;
                --bundle-id) bundle_id="$2" ;;
                --version) bundle_version="$2" ;;
                --build) build_version="$2" ;;
                --executable) executable_name="$2" ;;
            esac
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *) fail "unknown option: $1" ;;
    esac
done

for command_name in awk basename dirname git python3 shasum uname; do
    require_command "$command_name"
done
[[ "$(uname -s)" == "Darwin" ]] || fail "devicectl JSON verification requires macOS"
[[ -f "$requested_json_path" ]] || fail "devicectl JSON not found: $requested_json_path"
[[ -n "$requested_output_path" ]] || fail "--output is required"

json_parent="$(cd "$(dirname "$requested_json_path")" && pwd -P)"
json_path="$json_parent/$(basename "$requested_json_path")"
output_parent="$(dirname "$requested_output_path")"
[[ -d "$output_parent" ]] || fail "output parent directory not found: $output_parent"
output_parent="$(cd "$output_parent" && pwd -P)"
output_path="$output_parent/$(basename "$requested_output_path")"
[[ ! -e "$output_path" ]] || fail "output path already exists: $output_path"

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
repo_root="$(cd "$script_dir/.." && pwd -P)"
case "$output_path" in
    "$repo_root"|"$repo_root"/*)
        git -C "$repo_root" check-ignore -q -- "$output_path" || \
            fail "in-repository devicectl verification output must be gitignored: $output_path"
        ;;
esac

python3 -c 'import json, sys; json.load(open(sys.argv[1]))' "$json_path"
if json_query error type >/dev/null 2>&1; then
    fail "devicectl JSON contains an error object"
fi

case "$mode" in
    envelope)
        [[ "$expected_command_type" =~ ^devicectl\.[A-Za-z0-9.]+$ ]] || \
            fail "--command-type is required for envelope mode"
        ;;
    install)
        expected_command_type='devicectl.device.install.app'
        ;;
    installed-app)
        expected_command_type='devicectl.device.info.apps'
        ;;
    launch)
        expected_command_type='devicectl.device.process.launch'
        ;;
esac

actual_command_type="$(extract_required info.commandType 'command type')"
[[ "$actual_command_type" == "$expected_command_type" ]] || \
    fail "devicectl command type $actual_command_type does not equal $expected_command_type"
outcome="$(extract_required info.outcome outcome)"
[[ "$outcome" == "success" ]] || fail "devicectl outcome is not success: $outcome"
devicectl_version="$(extract_required info.version 'devicectl version')"
[[ "$devicectl_version" =~ ^[0-9]+([.][0-9]+)*$ ]] || \
    fail "devicectl version is malformed: $devicectl_version"

json_version="$(extract_required info.jsonVersion 'JSON schema version')"
[[ "$(json_query info.jsonVersion type)" == "integer" ]] || \
    fail "devicectl jsonVersion is not an integer"
[[ "$json_version" =~ ^[1-9][0-9]*$ ]] || \
    fail "devicectl jsonVersion is not positive: $json_version"

validated_bundle_id="not-applicable"
validated_version="not-applicable"
validated_build="not-applicable"
validated_url="not-applicable"
validated_process_id="not-applicable"

case "$mode" in
    envelope)
        ;;
    install)
        [[ "$bundle_id" =~ ^[A-Za-z0-9.-]+$ ]] || \
            fail "--bundle-id is required for install mode"
        installed_bundle_id="$(extract_required \
            result.installedApplications.0.bundleID 'installed bundle ID')"
        [[ "$installed_bundle_id" == "$bundle_id" ]] || \
            fail "install result bundle ID $installed_bundle_id does not equal $bundle_id"
        if json_query result.installedApplications.1.bundleID >/dev/null 2>&1; then
            fail "install result contains more than one installed application"
        fi
        installation_url="$(extract_required \
            result.installedApplications.0.installationURL 'installation URL')"
        [[ "$installation_url" == file://*/CTRPad.app/ ]] || \
            fail "install result URL is not a CTRPad.app bundle URL: $installation_url"
        validated_bundle_id="$installed_bundle_id"
        validated_url="$installation_url"
        ;;
    installed-app)
        [[ "$bundle_id" =~ ^[A-Za-z0-9.-]+$ ]] || \
            fail "--bundle-id is required for installed-app mode"
        [[ -n "$bundle_version" || -z "$build_version" ]] || \
            fail "--version and --build must be supplied together"
        [[ -n "$build_version" || -z "$bundle_version" ]] || \
            fail "--version and --build must be supplied together"
        matching_bundle_id="$(extract_required \
            result.matchingBundleIdentifier 'matching bundle identifier')"
        [[ "$matching_bundle_id" == "$bundle_id" ]] || \
            fail "query bundle ID $matching_bundle_id does not equal $bundle_id"
        installed_bundle_id="$(extract_required \
            result.apps.0.bundleIdentifier 'installed app bundle identifier')"
        [[ "$installed_bundle_id" == "$bundle_id" ]] || \
            fail "installed app bundle ID $installed_bundle_id does not equal $bundle_id"
        if json_query result.apps.1.bundleIdentifier >/dev/null 2>&1; then
            fail "installed-app result contains more than one exact bundle match"
        fi
        installed_version="$(extract_required result.apps.0.version 'installed app version')"
        installed_build="$(extract_required result.apps.0.bundleVersion 'installed app build')"
        if [[ -n "$bundle_version" ]]; then
            [[ "$installed_version" == "$bundle_version" ]] || \
                fail "installed app version $installed_version does not equal $bundle_version"
            [[ "$installed_build" == "$build_version" ]] || \
                fail "installed app build $installed_build does not equal $build_version"
        fi
        installed_url="$(extract_required result.apps.0.url 'installed app URL')"
        [[ "$installed_url" == file://*/CTRPad.app/ ]] || \
            fail "installed app URL is not a CTRPad.app bundle URL: $installed_url"
        validated_bundle_id="$installed_bundle_id"
        validated_version="$installed_version"
        validated_build="$installed_build"
        validated_url="$installed_url"
        ;;
    launch)
        [[ "$bundle_id" =~ ^[A-Za-z0-9.-]+$ ]] || \
            fail "--bundle-id is required for launch mode"
        [[ "$executable_name" =~ ^[A-Za-z0-9._-]+$ ]] || \
            fail "--executable is required for launch mode"
        process_id="$(extract_required result.process.processIdentifier 'process identifier')"
        [[ "$(json_query result.process.processIdentifier type)" == "integer" ]] || \
            fail "launch process identifier is not an integer"
        [[ "$process_id" =~ ^[1-9][0-9]*$ ]] || \
            fail "launch process identifier is not positive: $process_id"
        process_executable="$(extract_required result.process.executable 'process executable URL')"
        [[ "$process_executable" == file://*/CTRPad.app/"$executable_name" ]] || \
            fail "launched executable is not CTRPad.app/$executable_name: $process_executable"
        validated_bundle_id="$bundle_id"
        validated_url="$process_executable"
        validated_process_id="$process_id"
        ;;
esac

json_sha256="$(shasum -a 256 "$json_path" | awk '{print $1}')"
{
    printf 'TYPE=devicectl-json-verification\n'
    printf 'VALIDATION_MODE=%s\n' "$mode"
    printf 'VALIDATION_STATUS=verified\n'
    printf 'COMMAND_TYPE=%s\n' "$actual_command_type"
    printf 'OUTCOME=%s\n' "$outcome"
    printf 'DEVICECTL_VERSION=%s\n' "$devicectl_version"
    printf 'JSON_VERSION=%s\n' "$json_version"
    printf 'JSON_SHA256=%s\n' "$json_sha256"
    printf 'BUNDLE_ID=%s\n' "$validated_bundle_id"
    printf 'VERSION=%s\n' "$validated_version"
    printf 'BUILD=%s\n' "$validated_build"
    printf 'REMOTE_URL=%s\n' "$validated_url"
    printf 'PROCESS_IDENTIFIER=%s\n' "$validated_process_id"
} >"$output_path"

printf 'DEVICECTL_JSON_VERIFIED=%s\n' "$output_path"

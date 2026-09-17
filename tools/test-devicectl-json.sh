#!/usr/bin/env bash
# Test structured devicectl success and false-positive rejection paths.

set -Eeuo pipefail
trap 'printf "SELF-TEST ERROR: line %s: %s\n" "$LINENO" "$BASH_COMMAND" >&2' ERR

fail() {
    printf 'SELF-TEST ERROR: %s\n' "$*" >&2
    exit 1
}

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
verify_tool="$script_dir/verify-devicectl-json.sh"
[[ -x "$verify_tool" ]] || fail "devicectl verifier is not executable: $verify_tool"
plist_buddy='/usr/libexec/PlistBuddy'
[[ -x "$plist_buddy" ]] || fail "PlistBuddy is unavailable"

test_tmp_dir="$(mktemp -d "${TMPDIR:-/tmp}/ctrpad-devicectl-test.XXXXXX")"
cleanup() {
    case "$test_tmp_dir" in
        "${TMPDIR:-/tmp}"/ctrpad-devicectl-test.*) rm -rf "$test_tmp_dir" ;;
        *) printf 'WARNING: refusing unexpected cleanup path: %s\n' "$test_tmp_dir" >&2 ;;
    esac
}
trap cleanup EXIT INT TERM

create_base() {
    local path="$1"
    local command_type="$2"
    local outcome="${3:-success}"
    plutil -create xml1 "$path"
    "$plist_buddy" -c 'Add :info dict' "$path"
    "$plist_buddy" -c "Add :info:commandType string $command_type" "$path"
    "$plist_buddy" -c "Add :info:outcome string $outcome" "$path"
    "$plist_buddy" -c 'Add :info:version string 518.33' "$path"
    "$plist_buddy" -c 'Add :info:jsonVersion integer 3' "$path"
}

to_json() {
    plutil -convert json "$1"
}

edit_json() {
    python3 - "$@" <<'PYEDIT'
import json
import sys
path, action, key = sys.argv[1:4]
with open(path) as stream:
    data = json.load(stream)
if action == 'set-info':
    data['info'][key] = sys.argv[4]
elif action == 'remove-info':
    del data['info'][key]
elif action == 'append-app':
    data['result']['apps'].append({'bundleIdentifier': key})
elif action == 'set-version':
    data['result']['apps'][0]['version'] = key
with open(path, 'w') as stream:
    json.dump(data, stream)
PYEDIT
}

create_installed_app() {
    local path="$1"
    local app_bundle="${2:-io.github.chrissotraidis.ctrpad}"
    local include_app="${3:-1}"
    create_base "$path" devicectl.device.info.apps
    "$plist_buddy" -c 'Add :result dict' "$path"
    "$plist_buddy" -c 'Add :result:matchingBundleIdentifier string io.github.chrissotraidis.ctrpad' "$path"
    "$plist_buddy" -c 'Add :result:apps array' "$path"
    if [[ "$include_app" == "1" ]]; then
        "$plist_buddy" -c 'Add :result:apps:0 dict' "$path"
        "$plist_buddy" -c "Add :result:apps:0:bundleIdentifier string $app_bundle" "$path"
        "$plist_buddy" -c 'Add :result:apps:0:bundleVersion string 1' "$path"
        "$plist_buddy" -c 'Add :result:apps:0:version string 0.1.0' "$path"
        "$plist_buddy" -c 'Add :result:apps:0:url string file:///private/var/containers/Bundle/Application/SYNTHETIC/CTRPad.app/' "$path"
    fi
    to_json "$path"
}

create_install() {
    local path="$1"
    local installed_bundle="${2:-io.github.chrissotraidis.ctrpad}"
    create_base "$path" devicectl.device.install.app
    "$plist_buddy" -c 'Add :result dict' "$path"
    "$plist_buddy" -c 'Add :result:installedApplications array' "$path"
    "$plist_buddy" -c 'Add :result:installedApplications:0 dict' "$path"
    "$plist_buddy" -c "Add :result:installedApplications:0:bundleID string $installed_bundle" "$path"
    "$plist_buddy" -c 'Add :result:installedApplications:0:installationURL string file:///private/var/containers/Bundle/Application/SYNTHETIC/CTRPad.app/' "$path"
    to_json "$path"
}

create_launch() {
    local path="$1"
    local process_id="${2:-14306}"
    local process_url="${3:-file:///private/var/containers/Bundle/Application/SYNTHETIC/CTRPad.app/CTRPad}"
    create_base "$path" devicectl.device.process.launch
    "$plist_buddy" -c 'Add :result dict' "$path"
    "$plist_buddy" -c 'Add :result:process dict' "$path"
    "$plist_buddy" -c "Add :result:process:processIdentifier integer $process_id" "$path"
    "$plist_buddy" -c "Add :result:process:executable string $process_url" "$path"
    to_json "$path"
}

expect_success() {
    local label="$1"
    shift
    local output="$test_tmp_dir/$label.manifest"
    if ! "$verify_tool" "$@" --output "$output" >"$test_tmp_dir/$label.stdout" 2>"$test_tmp_dir/$label.stderr"; then
        cat "$test_tmp_dir/$label.stdout" "$test_tmp_dir/$label.stderr" >&2
        fail "$label verifier unexpectedly failed"
    fi
    grep -Fxq 'VALIDATION_STATUS=verified' "$output" || \
        fail "$label did not write a verified manifest"
}

expect_failure() {
    local label="$1"
    local expected="$2"
    shift 2
    local output="$test_tmp_dir/$label.manifest"
    if "$verify_tool" "$@" --output "$output" \
        >"$test_tmp_dir/$label.stdout" 2>"$test_tmp_dir/$label.stderr"; then
        fail "$label unexpectedly passed"
    fi
    grep -Fq "$expected" "$test_tmp_dir/$label.stderr" || \
        fail "$label did not fail at the expected boundary"
    [[ ! -e "$output" ]] || fail "$label wrote a success manifest on failure"
}

envelope_json="$test_tmp_dir/envelope.json"
create_base "$envelope_json" devicectl.list.devices
to_json "$envelope_json"
installed_json="$test_tmp_dir/installed.json"
create_installed_app "$installed_json"
install_json="$test_tmp_dir/install.json"
create_install "$install_json"
launch_json="$test_tmp_dir/launch.json"
create_launch "$launch_json"

expect_success envelope envelope --json "$envelope_json" \
    --command-type devicectl.list.devices
expect_success installed installed-app --json "$installed_json" \
    --bundle-id io.github.chrissotraidis.ctrpad --version 0.1.0 --build 1
expect_success install install --json "$install_json" \
    --bundle-id io.github.chrissotraidis.ctrpad
expect_success launch launch --json "$launch_json" \
    --bundle-id io.github.chrissotraidis.ctrpad --executable CTRPad

failed_with_text="$test_tmp_dir/failed-with-text.json"
create_base "$failed_with_text" devicectl.device.info.apps failed
"$plist_buddy" -c 'Add :error dict' "$failed_with_text"
"$plist_buddy" -c 'Add :error:description string io.github.chrissotraidis.ctrpad' "$failed_with_text"
to_json "$failed_with_text"
expect_failure failed-with-text 'contains an error object' installed-app \
    --json "$failed_with_text" --bundle-id io.github.chrissotraidis.ctrpad \
    --version 0.1.0 --build 1

empty_apps="$test_tmp_dir/empty-apps.json"
create_installed_app "$empty_apps" io.github.chrissotraidis.ctrpad 0
expect_failure empty-apps 'missing installed app bundle identifier' installed-app \
    --json "$empty_apps" --bundle-id io.github.chrissotraidis.ctrpad \
    --version 0.1.0 --build 1

wrong_command="$test_tmp_dir/wrong-command.json"
cp "$installed_json" "$wrong_command"
edit_json "$wrong_command" set-info commandType devicectl.list.devices
expect_failure wrong-command 'command type devicectl.list.devices' installed-app \
    --json "$wrong_command" --bundle-id io.github.chrissotraidis.ctrpad \
    --version 0.1.0 --build 1

missing_json_version="$test_tmp_dir/missing-json-version.json"
cp "$envelope_json" "$missing_json_version"
edit_json "$missing_json_version" remove-info jsonVersion
expect_failure missing-json-version 'missing JSON schema version' envelope \
    --json "$missing_json_version" --command-type devicectl.list.devices

wrong_bundle="$test_tmp_dir/wrong-bundle.json"
create_installed_app "$wrong_bundle" io.github.chrissotraidis.other
expect_failure wrong-bundle 'installed app bundle ID' installed-app \
    --json "$wrong_bundle" --bundle-id io.github.chrissotraidis.ctrpad \
    --version 0.1.0 --build 1

second_app="$test_tmp_dir/second-app.json"
cp "$installed_json" "$second_app"
edit_json "$second_app" append-app io.github.chrissotraidis.ctrpad
expect_failure second-app 'more than one exact bundle match' installed-app \
    --json "$second_app" --bundle-id io.github.chrissotraidis.ctrpad \
    --version 0.1.0 --build 1

wrong_version="$test_tmp_dir/wrong-version.json"
cp "$installed_json" "$wrong_version"
edit_json "$wrong_version" set-version 9.9.9
expect_failure wrong-version 'installed app version 9.9.9' installed-app \
    --json "$wrong_version" --bundle-id io.github.chrissotraidis.ctrpad \
    --version 0.1.0 --build 1

wrong_install_bundle="$test_tmp_dir/wrong-install-bundle.json"
create_install "$wrong_install_bundle" io.github.chrissotraidis.other
expect_failure wrong-install-bundle 'install result bundle ID' install \
    --json "$wrong_install_bundle" --bundle-id io.github.chrissotraidis.ctrpad

zero_pid="$test_tmp_dir/zero-pid.json"
create_launch "$zero_pid" 0
expect_failure zero-pid 'process identifier is not positive' launch \
    --json "$zero_pid" --bundle-id io.github.chrissotraidis.ctrpad \
    --executable CTRPad

wrong_executable="$test_tmp_dir/wrong-executable.json"
create_launch "$wrong_executable" 14306 \
    file:///private/var/containers/Bundle/Application/SYNTHETIC/Other.app/Other
expect_failure wrong-executable 'launched executable is not CTRPad.app/CTRPad' launch \
    --json "$wrong_executable" --bundle-id io.github.chrissotraidis.ctrpad \
    --executable CTRPad

printf 'DEVICECTL_JSON_SELF_TEST=passed positives=4 negatives=10\n'

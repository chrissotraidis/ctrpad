# Maintenance and diagnostics qualification — 17 September 2026

Scope: retained-source maintenance, source delivery and diagnostics for issue
[#36](https://github.com/chrissotraidis/ctrpad/issues/36). No upstream version
upgrade, source-tree migration, device installation or binary publication.

## Source and baseline

- Starting source: `d1b084b79cdda54166486e390937aff842439ce1`.
- Maintenance: `eaafce2ebd866bb1ca9e4fe7ca657f65e47d1db7`.
- Diagnostics / tested clean candidate:
  `0c299e80e843c1add1648905a0f55d1adf31155e`.
- Subsequent qualification documentation and device-verifier/test portability
  changes do not change executable source.
- CTR Native base remains `2df55dc5ad7d28e2712fc3453cd5bda7b737206e`;
  SDL remains 3.4.10, tracked tree `4a9dba870b17c7216710860a8a5540efb7dfee90`;
  PSn00bSDK subset tree `b63d699547c9d7af0e09aa85e2683c2acc879b0f`.

The complete original checkout was backed up outside the repository, restored
separately, and compared using file SHA-256, modes and symlink targets: 4,518
files matched. This includes Git history, ignored nested sources and original
local app artifacts. Both copies are on the same disk, so this is rollback
protection, not independent disaster recovery. App plist/signature identities
and the verification manifest are retained privately. No device/container was
accessed or changed.

## Published v0.1.1

All six downloaded release assets match GitHub's published SHA-256 digests.
The primary asset hashes are:

| Asset | SHA-256 |
|---|---|
| `CTRPad-source-d095cadfb62d.tar.gz` | `741d6aea71d7ad3808c22058e1dfc41c9e690ada61bb26ba709c279755442f18` |
| `CTRPad-macOS-arm64-0.1.1-d095cadfb62d.zip` | `6ade037692201f1b7bb9b9e94a2228972751e79df7cdc26e76dd52a13d25a7e4` |
| `CTRPad-0.1.1-2-d095cadfb62d-unsigned.ipa` | `e782a32e5a5eabcb0c9e425586b81eef5001928bd4b04f294f9e5d1cf629698c` |

The source archive was extracted outside any Git worktree, built with the
macOS app preset and passed all 26 original CTests. A local repackage contains
identical contents, modes and links for all 3,314 archive members. Its gzip
bytes differ from the published archive across toolchains; no byte-identical
cross-toolchain claim is made.

The published Mac bundle passes `codesign --verify --deep --strict`, is ad-hoc
signed, and has UUID `91F56AB5-8D0D-3039-9721-D20097E36A5B`, matching issue #36.
The unsigned IPA has neither a provisioning profile nor a `_CodeSignature`
resource tree. Its identity is `io.github.chrissotraidis.ctrpad`, version 0.1.1,
build 2, source `d095cadfb62d4bddc5738f731104f00c81d3ec19`.

## Clean candidate results

Host: Apple Silicon, Xcode 26.6 (17F113), CMake 4.4.2, Ninja.

- `./package-macos.sh --build`: pass; ad-hoc ARM64 app ZIP, identity/notices
  and signature checks pass. `ctest --preset macos-arm64-app`: 27/27 pass.
- `./package-ios.sh --build`: pass; unsigned ARM64 iPhone/iPad IPA, iOS 15
  minimum, full clean source identity and retail-free package audit pass.
- Simulator configure/build: pass. No Simulator installation or gameplay
  acceptance is claimed.
- `python3 tools/test-native-log.py`: pass; real log files exercise clean and
  interrupted sessions, duplicate init/shutdown, size rotation and retention.
- `python3 tools/test-source-lock.py`: pass; a restored tree without Git
  accepts exact sources and rejects modified bytes/modes, extra/missing files.
- Particle CTest: valid oscillator chain and real two-particle destruction
  restore the pools; malformed alignment, one-past pointers, invalid links,
  cycles and zero strides are rejected without dereferencing invalid memory.
- Two `package-source.sh` runs from the same clean commit produce identical
  bytes. The 3,321-member archive was extracted into a new directory and its
  checker, configure, build and 27 CTests passed with macOS `sandbox-exec`
  denying all network access. No private input or Git reference clone used.

Local qualification artifacts only; version 0.1.1/build 2 is deliberately
unchanged because no new release is being published:

| Asset | SHA-256 |
|---|---|
| `CTRPad-source-0c299e80e843.tar.gz` | `f093e1d9bb1485828a0dca5c6d18191bb7ff2184c3f3221c9d72d1470b6c4659` |
| `CTRPad-macOS-arm64-0.1.1-0c299e80e843.zip` | `2eb87e1173bdb5ba5ebd7b5b21bec8c7f647147fa02ca0ee766889f2417ab93f` |
| `CTRPad-0.1.1-2-0c299e80e843-unsigned.ipa` | `8a616723f3a257d2da28b9556f9ccba79308791a7f808980683d9430c2dcb3ba` |

## Rollback rehearsal

In a disposable worktree at the tested candidate:

```sh
git revert --no-commit 0c299e80e843c1add1648905a0f55d1adf31155e eaafce2ebd866bb1ca9e4fe7ca657f65e47d1db7
git diff --cached d1b084b79cdda54166486e390937aff842439ce1 --exit-code
git write-tree
```

The diff is empty and the restored tree is
`87cc673cd23581e5e8b250dfd4b2ac5a0f5e9c51`, exactly the starting source tree.
This did not reset main, the working candidate or any device.

## CI portability follow-up

Enabling the full suite on GitHub's macOS 15 runner exposed an existing
`plutil` JSON-input failure in the device-evidence verifier. It passed on the
newer local macOS host. The verifier now uses Python's JSON parser for field
lookup and retains all existing command, outcome, schema, bundle, version and
process checks. Fixture edits also use a JSON parser instead of asking older
`plutil` to modify JSON. The four positive and ten negative cases pass locally;
CI verifies them on macOS 15. Test failures now report the failing command and
verifier output rather than exiting silently.

## Remaining boundaries

PR review/merge is pending. Existing public releases remain unchanged.
Tiger Temple's corruption source is not proven or repaired: diagnostics now
identify the invalid pool operation before the unsafe write. Reporter/device
reproduction remains open. There is no new in-app report export, crash signal
handler or automatic upload. Windows/Linux were not built on this Apple host.
Successful host tests and package audits do not establish physical gameplay,
controller, audio or performance acceptance. Existing game-material rights
boundaries remain as documented; no third-party license has been changed and
no general rights clearance is claimed.

# CTRPad source maintenance

CTRPad already maintains ordinary tracked source, with CTR Native/CTR-ModSDK
history retained in Git. There is no central patch/preparation stack to migrate,
no submodule, and no proprietary binary engine to turn into a source fork.
Normal builds compile `game/`, `include/`, `platform/` and `externals/SDL`
directly. Do not replace the app repository or replay patches during builds.

## Baseline and component ownership

- Foundation: [CTR-tools/ctr-native](https://github.com/CTR-tools/ctr-native),
  `2df55dc5ad7d28e2712fc3453cd5bda7b737206e` (0.1.0-beta.7.1).
  This is the verified common ancestor, not a newly selected upstream upgrade.
  The app commit selects all engine, facade, and Apple platform modifications.
  CTR-ModSDK history is inherited through that foundation. GPL-3.0 text and
  component notices remain intact; see [rights](../../RIGHTS_AND_LICENSES.md).
- SDL 3.4.10: upstream tag commit
  `8e37db5e797b6167f3a00d697d816a684bd259c7`, tracked at `externals/SDL`.
  The exact selected tree is in [sources.lock.json](../../sources.lock.json).
  CTR Native's import `0aba5644385f05910bd772c94d308fab0d1f0c22`
  already normalized line endings and omitted upstream packaging/generated
  files. It is not a byte-identical SDL checkout. CTRPad's additional UIKit
  changes are ordinary commits `6b268157888fbe66b8a4910ae6bf02db6b095d2b`
  and `253b8f505509acb7fb82b4b10e7aeb39a2174e73`: balanced view transitions
  and display-link common run-loop modes. Zlib license; no version upgrade.
- PSn00bSDK: the tracked header subset at `include/psn00bsdk`, inherited at
  the CTR Native base above, with its own exact tree in the lock. MPL-2.0
  notices remain in the headers and `LICENSE.md`. No `libpsn00b` is linked.
  An independent original PSn00bSDK revision has not been established.
- PsyCross/Psy-X: MIT-derived facade/runtime code in `include/psx`,
  `include/platform`, and `platform`, inherited through CTR Native's history.
  Its exact maintained revision is the app commit, not a fictitious separate
  dependency pin. The full MIT notice is in `THIRD_PARTY_NOTICES.md`.
  GL loader and other per-file notices remain with their source.

There are no new dependency forks. GitHub identifies CTRPad as a standalone
app repository; that does not erase its retained upstream history or establish
rights to third-party game material. The documented GPL and component licenses
are source terms, not a general clearance for copyrighted game material.

## Compare and update

Work on a branch and keep platform changes separate. To inspect upstream
without changing the selected engine version:

```sh
git fetch https://github.com/CTR-tools/ctr-native.git master
git merge-base HEAD FETCH_HEAD
git log --oneline 2df55dc5ad7d28e2712fc3453cd5bda7b737206e..HEAD -- game include platform
git diff 2df55dc5ad7d28e2712fc3453cd5bda7b737206e HEAD -- game include platform
# Existing Apple changes relative to the inherited SDL tree:
git diff 2df55dc5ad7d28e2712fc3453cd5bda7b737206e HEAD -- externals/SDL
python3 tools/check-sources.py
```

An upstream version update is separate work. Review selected commits, retain
attribution and history, test affected platforms, then deliberately update the
lock's tree identity with `git rev-parse HEAD:externals/SDL` after committing
reviewed SDL changes. Do not automatically regenerate the lock to hide a
mismatch. The checker reads working files and executable modes, so it also
rejects local edits or extra files in these maintained dependency directories.
Use out-of-source build directories.

App-specific crashes and uncertain graphics problems belong on CTRPad's issue
tracker. A proposed upstream contribution should include the base, focused
delta, reproduction and logs; contacting upstream is a separate decision.

## Build and source delivery

Apple packages ship for macOS ARM64 and iOS/iPadOS ARM64; Simulator is a
separate development path. Windows x86/MSVC/MinGW and Linux i686 presets use
the same tracked engine and SDL; they are not current Apple release assets.
No runtime disc image is needed to compile or run retail-free self-tests.
Actual gameplay requires the user's compatible disc.

Packaging requires Python 3 for the portable dependency checker. Both Apple
packagers verify the locked working trees. `package-source.sh --ref COMMIT`
packages committed files, restores its own archive into a temporary directory,
and verifies its selected vendored trees there. Historical refs predating the
lock still package without retroactively changing their contents. The complete
tracked SDL tree and header subset are included; GitHub submodule omission is
not applicable here. Ignored reference clones, game data, saves, builds and
signing inputs are excluded.

```sh
./package-source.sh
# In a separate directory, preserve the archive's source-root name:
tar -xzf CTRPad-source-COMMIT.tar.gz
cd CTRPad-source-COMMIT
python3 tools/check-sources.py
cmake --preset macos-arm64-app
cmake --build --preset macos-arm64-app
ctest --preset macos-arm64-app
```

An extracted tree needs no network fetch or private reference checkout. For a
renamed source root, pass the verified full commit using
`-DCTR_NATIVE_SOURCE_COMMIT=COMMIT`. Source archive reproducibility means
byte-identical source packages on the same toolchain; signed app packages and
binaries built with different Apple SDKs are not promised to be byte-identical.

## September 2026 qualification and rollback

Starting public release: [v0.1.1](https://github.com/chrissotraidis/ctrpad/releases/tag/v0.1.1),
commit `d095cadfb62d4bddc5738f731104f00c81d3ec19`, version 0.1.1/build 2.
Source at start: `d1b084b79cdda54166486e390937aff842439ce1` (README-only
change after release). The maintenance and diagnostics work does not publish
or replace a binary release.

The complete starting checkout, Git history, ignored nested sources and local
artifacts were copied outside the checkout and restored separately. A private
manifest verified SHA-256, modes and symlinks for 4,518 files. Both copies are
on the same physical disk. No device was installed, uninstalled or reset.
Preserve the public packages and user's entire data container for any later
installation; match bundle and signer before an in-place update.

Source rollback is a separate checkout, never a reset of a working product:

```sh
git worktree add --detach /absolute/new/rollback-ctrpad d095cadfb62d4bddc5738f731104f00c81d3ec19
```

Keep private backup manifests, crash reports, signing identity and local paths
out of public source. Qualification results are recorded in
[2026-09-17 validation](2026-09-17-validation.md).

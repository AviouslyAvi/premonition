# Ad-hoc signing — quick reference

Ad-hoc signing is free, requires no Apple account, and is good enough
for personal use and beta testers willing to run one `xattr` command.
It **cannot** be notarized — for public distribution, switch to
Developer ID + notarytool.

## Sign the two plugin bundles

```sh
codesign --force --deep --sign - \
  "$HOME/Library/Audio/Plug-Ins/VST3/Premonition.vst3"

codesign --force --deep --sign - \
  "$HOME/Library/Audio/Plug-Ins/Components/Premonition.component"
```

## Verify

```sh
codesign --verify --verbose=2 \
  "$HOME/Library/Audio/Plug-Ins/VST3/Premonition.vst3"

codesign --verify --verbose=2 \
  "$HOME/Library/Audio/Plug-Ins/Components/Premonition.component"
```

Expected output: `valid on disk` + `satisfies its Designated Requirement`.

## Build installer + DMG

```sh
./scripts/release-adhoc.sh              # defaults to version 1.0.0
./scripts/release-adhoc.sh 1.2.3        # custom version
```

Output: `dist/Premonition-<version>.dmg` containing the signed `.pkg`
and `INSTALL.txt`.

## What the release script does

1. Ad-hoc signs the VST3 and AU bundles in `~/Library/Audio/Plug-Ins/`.
2. Builds two component pkgs (`pkgbuild`) with install locations
   `/Library/Audio/Plug-Ins/VST3` and `/Library/Audio/Plug-Ins/Components`.
3. Merges them into a single `Premonition-Installer.pkg` with
   `productbuild`.
4. Stages the pkg + `INSTALL.txt` and wraps them in a compressed DMG.

## Gatekeeper note for users

Ad-hoc signed plugins will trip Gatekeeper on first launch on any
machine other than the one that signed them. `INSTALL.txt` (bundled in
the DMG) tells the user how to clear the quarantine flag.

# Releasing Premonition (ad-hoc signed DMG)

Full pipeline from source → signed `.pkg` → distributable `.dmg`.
Designed to be runnable by a fresh agent or developer with zero prior
context. Three commands total.

## Prerequisites (one-time)

- Xcode installed (`xcode-select -p` should point to `/Applications/Xcode.app/...`).
- If `xcodebuild` fails with a `CoreSimulator.framework` / `IDESimulatorFoundation`
  plug-in error, run once:
  ```sh
  sudo xcodebuild -runFirstLaunch
  ```

## Step 1 — Build the plugins

From the repo root:

```sh
cd Premonition/projects

xcodebuild -project Premonition-macOS.xcodeproj \
           -target VST3 -configuration Release build

xcodebuild -project Premonition-macOS.xcodeproj \
           -target AU   -configuration Release build
```

The Xcode project's post-build step copies the bundles to:

- `~/Library/Audio/Plug-Ins/VST3/Premonition.vst3`
- `~/Library/Audio/Plug-Ins/Components/Premonition.component`

Those two paths are the inputs to the release script — don't move them.

## Step 2 — Sign + package + DMG

From the repo root:

```sh
./scripts/release-adhoc.sh 1.0.0
```

(Pass whatever version string you want. Defaults to `1.0.0`.)

The script:
1. Ad-hoc signs both bundles with `codesign --force --deep --sign -`.
2. Builds two component pkgs (`pkgbuild`) pointing at
   `/Library/Audio/Plug-Ins/VST3` and `/Library/Audio/Plug-Ins/Components`.
3. Merges them into `Premonition-Installer.pkg` (`productbuild`).
4. Wraps the pkg + `INSTALL.txt` in a compressed DMG.

## Step 3 — Ship

Output: `dist/Premonition-<version>.dmg`.

Upload wherever (website, Gumroad, etc.). The DMG contains everything
a user needs, including Gatekeeper-bypass instructions in `INSTALL.txt`.

## Files in this directory

| File | Purpose |
|------|---------|
| `release-adhoc.sh` | The build-and-ship script (Step 2). |
| `INSTALL.txt` | End-user install guide, bundled inside every DMG. |
| `SIGNING.md` | Reference for the raw `codesign` commands and limitations. |
| `README.md` | This file. |

## Notes for future-you (or a future agent)

- **Ad-hoc signing is free but not notarized.** Users on machines other
  than the signing machine will hit a Gatekeeper warning on first load.
  `INSTALL.txt` tells them how to clear it. For public distribution,
  switch to Developer ID + `notarytool` — see `SIGNING.md`.
- **Identifiers are hardcoded** (`com.lumorait.premonition.*`). Edit
  `release-adhoc.sh` if forking.
- **CLAP is not included.** The iPlug2 macOS project has a CLAP target,
  but this pipeline currently ships VST3 + AU only. Add a third
  `pkgbuild` block + another `pkg-ref` in `distribution.xml` if needed.

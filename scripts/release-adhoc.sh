#!/usr/bin/env bash
# Ad-hoc sign Premonition VST3 + AU, wrap them in a .pkg, wrap that in a .dmg.
# Usage:  ./scripts/release-adhoc.sh [version]
set -euo pipefail

VERSION="${1:-1.0.0}"
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="$REPO_ROOT/dist"
STAGE="$(mktemp -d -t premonition-release)"
trap 'rm -rf "$STAGE"' EXIT

VST3_SRC="$HOME/Library/Audio/Plug-Ins/VST3/Premonition.vst3"
AU_SRC="$HOME/Library/Audio/Plug-Ins/Components/Premonition.component"

if [[ ! -d "$VST3_SRC" || ! -d "$AU_SRC" ]]; then
  echo "ERROR: expected plugin bundles at:" >&2
  echo "  $VST3_SRC" >&2
  echo "  $AU_SRC"   >&2
  echo "Build the VST3 and AU targets first." >&2
  exit 1
fi

mkdir -p "$OUT_DIR"

echo "==> Ad-hoc signing"
codesign --force --deep --sign - "$VST3_SRC"
codesign --force --deep --sign - "$AU_SRC"
codesign --verify --verbose=2 "$VST3_SRC"
codesign --verify --verbose=2 "$AU_SRC"

echo "==> Staging component pkg roots"
VST3_ROOT="$STAGE/vst3-root/Library/Audio/Plug-Ins/VST3"
AU_ROOT="$STAGE/au-root/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3_ROOT" "$AU_ROOT"
ditto "$VST3_SRC" "$VST3_ROOT/Premonition.vst3"
ditto "$AU_SRC"   "$AU_ROOT/Premonition.component"

echo "==> Building component pkgs"
pkgbuild --root "$STAGE/vst3-root" \
         --identifier "com.lumorait.premonition.vst3" \
         --version "$VERSION" \
         --install-location "/" \
         "$STAGE/Premonition-vst3.pkg"

pkgbuild --root "$STAGE/au-root" \
         --identifier "com.lumorait.premonition.au" \
         --version "$VERSION" \
         --install-location "/" \
         "$STAGE/Premonition-au.pkg"

echo "==> Building distribution pkg"
cat > "$STAGE/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>Premonition $VERSION</title>
    <organization>com.lumorait</organization>
    <domains enable_localSystem="true"/>
    <options customize="allow" require-scripts="false" rootVolumeOnly="true"/>
    <choices-outline>
        <line choice="vst3"/>
        <line choice="au"/>
    </choices-outline>
    <choice id="vst3" visible="true" title="Premonition VST3" start_selected="true">
        <pkg-ref id="com.lumorait.premonition.vst3"/>
    </choice>
    <choice id="au" visible="true" title="Premonition AU" start_selected="true">
        <pkg-ref id="com.lumorait.premonition.au"/>
    </choice>
    <pkg-ref id="com.lumorait.premonition.vst3" version="$VERSION" onConclusion="none">Premonition-vst3.pkg</pkg-ref>
    <pkg-ref id="com.lumorait.premonition.au"   version="$VERSION" onConclusion="none">Premonition-au.pkg</pkg-ref>
</installer-gui-script>
EOF

productbuild --distribution "$STAGE/distribution.xml" \
             --package-path "$STAGE" \
             "$STAGE/Premonition-Installer.pkg"

echo "==> Staging DMG contents"
DMG_STAGE="$STAGE/dmg"
mkdir -p "$DMG_STAGE"
cp "$STAGE/Premonition-Installer.pkg" "$DMG_STAGE/Premonition-Installer.pkg"
cp "$REPO_ROOT/scripts/INSTALL.txt"   "$DMG_STAGE/INSTALL.txt"

echo "==> Building DMG"
DMG_OUT="$OUT_DIR/Premonition-$VERSION.dmg"
rm -f "$DMG_OUT"
hdiutil create \
  -volname "Premonition $VERSION" \
  -srcfolder "$DMG_STAGE" \
  -ov -format UDZO \
  "$DMG_OUT"

echo ""
echo "Done: $DMG_OUT"

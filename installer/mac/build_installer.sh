#!/bin/bash
# Builds "Chupa Loops Installer.pkg" and "ChupaLoops-macOS.dmg".
# usage: build_installer.sh <artefacts dir> <version> <output dir> [installer signing identity]
set -euo pipefail

ART="$1"; VERSION="$2"; OUT="$3"; INSTALLER_ID="${4:-}"
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="$(mktemp -d)"
NAME="Chupa Loops"
mkdir -p "$OUT" "$WORK/pkgs" "$WORK/resources"

# one component package per format, installed for all users
make_component () {   # <id> <bundle path> <install dir> [scripts dir]
    local id="$1" bundle="$2" dest="$3" scripts="${4:-}"
    local root="$WORK/root_$id"   # contains only the bundle: system folders are never part of the payload
    mkdir -p "$root"
    cp -R "$bundle" "$root/"
    # never let Installer "relocate" the bundle to an old copy somewhere else on the disk
    pkgbuild --analyze --root "$root" "$WORK/$id.plist"
    local n; n=$(/usr/libexec/PlistBuddy -c "Print" "$WORK/$id.plist" | grep -c "RootRelativeBundlePath" || true)
    for ((i = 0; i < n; i++)); do
        # the key is not always present in the generated plist: set it, or add it when it isn't there
        /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" "$WORK/$id.plist" 2>/dev/null \
            || /usr/libexec/PlistBuddy -c "Add :$i:BundleIsRelocatable bool false" "$WORK/$id.plist"
    done
    local args=(--root "$root" --component-plist "$WORK/$id.plist" --identifier "com.perception.chupaloops.$id"
                --version "$VERSION" --install-location "$dest")
    if [ -n "$scripts" ]; then args+=(--scripts "$scripts"); fi
    pkgbuild "${args[@]}" "$WORK/pkgs/ChupaLoops-$id.pkg"
}

make_component vst3 "$ART/VST3/$NAME.vst3"       "/Library/Audio/Plug-Ins/VST3"
make_component au   "$ART/AU/$NAME.component"    "/Library/Audio/Plug-Ins/Components" "$HERE/scripts"
make_component app  "$ART/Standalone/$NAME.app"  "/Applications"

sed "s/@VERSION@/$VERSION/g" "$HERE/distribution.xml" > "$WORK/distribution.xml"
cp "$HERE/welcome.html" "$HERE/conclusion.html" "$WORK/resources/"

PKG="$OUT/$NAME Installer.pkg"
SIGN=()
if [ -n "$INSTALLER_ID" ]; then SIGN=(--sign "$INSTALLER_ID" --timestamp); fi
productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK/pkgs" --resources "$WORK/resources" \
             ${SIGN[@]+"${SIGN[@]}"} "$PKG"
echo "Built $PKG"

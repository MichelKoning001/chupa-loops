#!/bin/bash
# Installs Chupa Loops (VST3 + AU + standalone app) for the current user.
cd "$(dirname "$0")" || exit 1

echo ""
echo "  Installing CHUPA LOOPS..."
echo ""

if [ -e "/Library/Audio/Plug-Ins/VST3/Chupa Loops.vst3" ] || [ -e "/Library/Audio/Plug-Ins/Components/Chupa Loops.component" ]; then
    echo "  Note: Chupa Loops is already installed for all users (with the .pkg installer)."
    echo "  Installing it here too can make your DAW list it twice. Use the .pkg to update instead."
    echo ""
    read -n 1 -s -r -p "  Press any key to continue anyway, or close this window to stop..."
    echo ""
fi

VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3_DIR" "$AU_DIR"

rm -rf "$VST3_DIR/Chupa Loops.vst3" "$AU_DIR/Chupa Loops.component"
cp -R "Chupa Loops.vst3" "$VST3_DIR/" && echo "  - VST3 installed"
cp -R "Chupa Loops.component" "$AU_DIR/" && echo "  - Audio Unit installed"

if [ -d "Chupa Loops.app" ]; then
    rm -rf "/Applications/Chupa Loops.app" 2>/dev/null
    if cp -R "Chupa Loops.app" "/Applications/" 2>/dev/null; then
        echo "  - App installed in Applications"
    else
        mkdir -p "$HOME/Applications"
        cp -R "Chupa Loops.app" "$HOME/Applications/" && echo "  - App installed in ~/Applications"
    fi
fi

# remove the macOS download quarantine
xattr -dr com.apple.quarantine "$VST3_DIR/Chupa Loops.vst3" 2>/dev/null
xattr -dr com.apple.quarantine "$AU_DIR/Chupa Loops.component" 2>/dev/null
xattr -dr com.apple.quarantine "/Applications/Chupa Loops.app" 2>/dev/null
xattr -dr com.apple.quarantine "$HOME/Applications/Chupa Loops.app" 2>/dev/null

# refresh the Audio Unit list
killall -9 AudioComponentRegistrar 2>/dev/null

echo ""
echo "  Done! Restart your DAW."
echo "  Chupa Loops is listed under instruments (Percep-tion > Chupa Loops)."
echo ""
read -n 1 -s -r -p "  Press any key to close this window..."
echo ""

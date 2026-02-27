#!/bin/bash
set -e

PROJ="$( cd "$( dirname "$0" )" && pwd )"
BUILD="$PROJ/build"
APP="$PROJ/SF2.app"

echo "Building..."
cd "$BUILD"
make -j$(sysctl -n hw.ncpu) glutBasics

echo "Packaging SF2.app..."
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS"
mkdir -p "$APP/Contents/Resources/assets/music"

cp "$PROJ/Info.plist"          "$APP/Contents/"
cp "$PROJ/AppIcon.icns"        "$APP/Contents/Resources/" 2>/dev/null || true
cp "$BUILD/glutBasics"         "$APP/Contents/MacOS/"
cp "$BUILD/allroms.bin"        "$APP/Contents/Resources/"
cp "$BUILD/sf2gfx.bin"         "$APP/Contents/Resources/"
cp -R "$BUILD/assets/backgrounds" "$APP/Contents/Resources/assets/"
cp -R "$BUILD/assets/heads"       "$APP/Contents/Resources/assets/"
cp -R "$BUILD/assets/arms"        "$APP/Contents/Resources/assets/"
cp -R "$BUILD/assets/legs"        "$APP/Contents/Resources/assets/"
cp "$HOME/Desktop/Gimme_The_Loot_Loot_Mix.mp3" "$APP/Contents/Resources/assets/music/"

echo "Done: $APP"

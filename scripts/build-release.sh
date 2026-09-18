#!/bin/sh
# Build an unsigned Oni-Extended.app and an unsigned Oni-Extended.dmg.
#
#   scripts/build-release.sh                package into ./release
#   scripts/build-release.sh <BUILD_DIR>    use a different cmake binary dir
#
# Produces:
#   release/Oni-Extended.app    drag-installable bundle
#   release/Oni-Extended.dmg    compressed image, Applications symlink beside it
#
# "Unsigned" here means ad-hoc signed: no Developer ID, no hardened runtime,
# no notarization, no staple. Apple Silicon will not execute an arm64 binary
# that carries no signature at all, so ad-hoc is the weakest thing that runs.
# The consequence is Gatekeeper: a downloaded copy is quarantined and refused
# with "the developer cannot be verified". Clear it with
#
#   xattr -dr com.apple.quarantine /Applications/Oni-Extended.app
#
# or right-click the app and choose Open.

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SOURCE_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
BINARY_DIR="${1:-$SOURCE_DIR/build}"
RELEASE_DIR="$SOURCE_DIR/release"
APP_NAME="Oni-Extended"
BUILT_APP="$BINARY_DIR/bin/OniARM64.app"

echo "build-release.sh: configuring $BINARY_DIR"
cmake -S "$SOURCE_DIR" -B "$BINARY_DIR" -DPlatform_SDL=ON

echo "build-release.sh: building the Oni binary"
cmake --build "$BINARY_DIR" --target Oni --parallel

echo "build-release.sh: assembling $BUILT_APP (ad-hoc)"
"$SOURCE_DIR/macos/build-bundle.sh" "$SOURCE_DIR" "$BINARY_DIR" -

mkdir -p "$RELEASE_DIR"
rm -rf "$RELEASE_DIR/$APP_NAME.app"

echo "build-release.sh: staging $RELEASE_DIR/$APP_NAME.app"
ditto "$BUILT_APP" "$RELEASE_DIR/$APP_NAME.app"

INFO_PLIST="$RELEASE_DIR/$APP_NAME.app/Contents/Info.plist"
/usr/libexec/PlistBuddy -c "Set :CFBundleName $APP_NAME" "$INFO_PLIST"
/usr/libexec/PlistBuddy -c "Set :CFBundleDisplayName $APP_NAME" "$INFO_PLIST"

codesign --force --sign - "$RELEASE_DIR/$APP_NAME.app"
codesign --verify --strict --verbose=2 "$RELEASE_DIR/$APP_NAME.app"

STAGE_DIR=$(mktemp -d)
trap 'rm -rf "$STAGE_DIR"' EXIT INT TERM

ditto "$RELEASE_DIR/$APP_NAME.app" "$STAGE_DIR/$APP_NAME.app"
ln -s /Applications "$STAGE_DIR/Applications"

rm -f "$RELEASE_DIR/$APP_NAME.dmg"

echo "build-release.sh: writing $RELEASE_DIR/$APP_NAME.dmg"
hdiutil create -quiet -volname "$APP_NAME" -srcfolder "$STAGE_DIR" -ov -format UDZO "$RELEASE_DIR/$APP_NAME.dmg"

hdiutil verify -quiet "$RELEASE_DIR/$APP_NAME.dmg"

shasum -a 256 "$RELEASE_DIR/$APP_NAME.dmg"

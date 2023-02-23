#!/bin/sh -e
set -x

if [[ -n "$APPLICATION" ]]; then
  echo "Create a keychain"
  security create-keychain -p nr4aGPyz Keys.keychain

  echo $APPLICATION | base64 -D -o /tmp/Application.p12
  echo $INSTALLER | base64 -D -o /tmp/Installer.p12

  security import /tmp/Application.p12 -t agg -k Keys.keychain -P aym9PKWB -A -T /usr/bin/codesign
  security import /tmp/Installer.p12 -t agg -k Keys.keychain -P aym9PKWB -A -T /usr/bin/codesign

  security list-keychains -s Keys.keychain
  security default-keychain -s Keys.keychain
  security unlock-keychain -p nr4aGPyz Keys.keychain
  security set-keychain-settings -l -u -t 13600 Keys.keychain
  security set-key-partition-list -S apple-tool:,apple: -s -k nr4aGPyz Keys.keychain
fi

DEV_APP_ID="Developer ID Application: Roland Rabien (3FS7DJDG38)"
DEV_INST_ID="Developer ID Installer: Roland Rabien (3FS7DJDG38)"

ROOT=$(cd "$(dirname "$0")/../.."; pwd)
cd "$ROOT"

cd "$ROOT"
xcodebuild -configuration Release || exit 1

rm -Rf "$ROOT/ci/mac/bin"
mkdir -p "$ROOT/ci/mac/bin"
cp -R "$ROOT/build/Release/imguru" "$ROOT/ci/mac/bin"

cd "$ROOT/ci/mac/bin"
codesign --force -s "$DEV_APP_ID" -v "imguru" --deep --strict --options=runtime

zip -r imguru.zip imguru
rm imguru

xcrun notarytool submit --verbose --apple-id "$APPLE_USER" --password "$APPLE_PASS" --team-id 3FS7DJDG38 --wait --timeout 30m "./imguru.zip"

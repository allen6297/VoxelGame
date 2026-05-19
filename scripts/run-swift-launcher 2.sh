#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT_PATH="$ROOT_DIR/platform/apple/TerralieLauncherApp/TerralieLauncherApp.xcodeproj"
DERIVED_DATA_DIR="${DERIVED_DATA_DIR:-$ROOT_DIR/cmake-build-debug/xcode-derived-data}"
APP_PATH="$DERIVED_DATA_DIR/Build/Products/Debug/TerralieLauncherApp.app"

rm -rf "$APP_PATH"
if [[ -d "$DERIVED_DATA_DIR" ]]; then
  xattr -rc "$DERIVED_DATA_DIR" 2>/dev/null || true
fi

xcodebuild \
  -project "$PROJECT_PATH" \
  -scheme TerralieLauncherApp \
  -configuration Debug \
  -destination 'platform=macOS' \
  -derivedDataPath "$DERIVED_DATA_DIR" \
  build

TERRALITE_SOURCE_DIR="$ROOT_DIR" open "$APP_PATH"

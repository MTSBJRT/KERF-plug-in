#!/bin/bash
# Builds ChopForge on macOS (Apple Silicon or Intel), runs the tests and installs
# the VST3 and AU into the user plug-in folders.
#
#   ./scripts/build-macos.sh            # release build + install
#   ./scripts/build-macos.sh --no-install
#   ./scripts/build-macos.sh --universal  # arm64 + x86_64 binary
#
# Requirements: Xcode (or the Command Line Tools), CMake >= 3.22, Ninja.
#   xcode-select --install
#   brew install cmake ninja
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build-macos"
INSTALL=1
ARCHS="$(uname -m)"
for arg in "$@"; do
    case "$arg" in
        --no-install) INSTALL=0 ;;
        --universal)  ARCHS="arm64;x86_64" ;;
        *) echo "unknown option: $arg"; exit 1 ;;
    esac
done

for tool in cmake ninja clang++; do
    if ! command -v "$tool" >/dev/null; then
        echo "Missing '$tool'. Install Xcode / Command Line Tools and run: brew install cmake ninja"
        exit 1
    fi
done

if [ ! -f "$ROOT/third_party/JUCE/CMakeLists.txt" ]; then
    echo "==> Fetching JUCE"
    git clone --depth 1 --branch "$(cat "$ROOT/scripts/juce-version.txt")" https://github.com/juce-framework/JUCE.git "$ROOT/third_party/JUCE"
fi

echo "==> Configuring ($ARCHS)"
cmake -S "$ROOT" -B "$BUILD" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="$ARCHS" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
    -DCHOPFORGE_COPY_PLUGIN=$([ $INSTALL = 1 ] && echo ON || echo OFF)

echo "==> Building"
cmake --build "$BUILD" --target ChopForge_VST3 ChopForge_AU ChopForge_Standalone ChopForgeTests ChopForgeHostTests

echo "==> Running tests"
"$BUILD/ChopForgeTests_artefacts/Release/ChopForgeTests"
"$BUILD/ChopForgeHostTests"

ART="$BUILD/ChopForge_artefacts/Release"
echo
echo "Built:"
echo "  $ART/VST3/ChopForge.vst3"
echo "  $ART/AU/ChopForge.component"
echo "  $ART/Standalone/ChopForge.app"

if [ $INSTALL = 1 ]; then
    echo
    echo "Installed into ~/Library/Audio/Plug-Ins/VST3 and ~/Library/Audio/Plug-Ins/Components."
    echo "Refreshing the AudioUnit cache..."
    killall -9 AudioComponentRegistrar 2>/dev/null || true
    if command -v auval >/dev/null; then
        echo "==> auval (AudioUnit validation)"
        auval -v aumf Chpf Bjrt | tail -n 5 || echo "auval reported a problem; the VST3 is unaffected."
    fi
    echo
    echo "Now open Maschine 3 > Preferences > Plug-ins > Locations > Rescan."
fi

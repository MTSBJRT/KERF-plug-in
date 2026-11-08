#!/bin/bash
# One-shot installer for Kerf on macOS. No Homebrew needed.
#   - checks the Xcode Command Line Tools (asks macOS to install them if missing)
#   - downloads a self-contained CMake into the project folder if none is available
#   - fetches JUCE, builds VST3 + AU + tests natively for this Mac
#   - installs into the user AND system plug-in folders (system copy asks for your password)
#   - validates the AU with auval and lists what was installed
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build-macos"
TOOLS="$ROOT/.tools"
CMAKE_VERSION="3.31.6"
step() { printf '\n\033[1;36m==> %s\033[0m\n' "$*"; }
fail() { printf '\n\033[1;31mERREUR: %s\033[0m\n' "$*"; exit 1; }

# ------------------------------------------------------------------ 1. compiler
step "Vérification des outils Xcode"
if ! xcode-select -p >/dev/null 2>&1 || ! command -v clang++ >/dev/null 2>&1; then
    echo "Les Command Line Tools ne sont pas installés. macOS va afficher une fenêtre : cliquez Installer,"
    echo "attendez la fin, puis relancez exactement la même commande."
    xcode-select --install 2>/dev/null || true
    exit 2
fi
if ! clang++ --version >/dev/null 2>&1; then
    echo "La licence Xcode doit être acceptée. Entrez votre mot de passe si demandé."
    sudo xcodebuild -license accept 2>/dev/null || true
fi
command -v git >/dev/null || fail "git introuvable (il vient avec les Command Line Tools)"

# ------------------------------------------------------------------ 2. cmake
step "CMake"
CMAKE=""
if command -v cmake >/dev/null 2>&1 && cmake --version | head -1 | grep -qE 'version (3\.(2[2-9]|[3-9][0-9])|[4-9]\.)'; then
    CMAKE="$(command -v cmake)"
elif [ -x "$TOOLS/CMake.app/Contents/bin/cmake" ]; then
    CMAKE="$TOOLS/CMake.app/Contents/bin/cmake"
else
    echo "Téléchargement de CMake $CMAKE_VERSION (binaire officiel, universel)..."
    mkdir -p "$TOOLS"
    curl -fL --progress-bar -o "$TOOLS/cmake.tar.gz" \
        "https://github.com/Kitware/CMake/releases/download/v$CMAKE_VERSION/cmake-$CMAKE_VERSION-macos-universal.tar.gz" \
        || fail "téléchargement de CMake impossible (connexion internet ?)"
    tar -xzf "$TOOLS/cmake.tar.gz" -C "$TOOLS"
    mv "$TOOLS/cmake-$CMAKE_VERSION-macos-universal/CMake.app" "$TOOLS/CMake.app"
    rm -rf "$TOOLS/cmake.tar.gz" "$TOOLS/cmake-$CMAKE_VERSION-macos-universal"
    xattr -dr com.apple.quarantine "$TOOLS/CMake.app" 2>/dev/null || true
    CMAKE="$TOOLS/CMake.app/Contents/bin/cmake"
fi
echo "CMake : $CMAKE ($("$CMAKE" --version | head -1))"

# ------------------------------------------------------------------ 3. JUCE
if [ ! -f "$ROOT/third_party/JUCE/CMakeLists.txt" ]; then
    step "Téléchargement de JUCE $(cat "$ROOT/scripts/juce-version.txt")"
    git clone --depth 1 --branch "$(cat "$ROOT/scripts/juce-version.txt")" \
        https://github.com/juce-framework/JUCE.git "$ROOT/third_party/JUCE" || fail "clone de JUCE impossible"
fi

# ------------------------------------------------------------------ 4. build
ARCH="$(uname -m)"
step "Configuration ($ARCH, macOS 12+)"
"$CMAKE" -S "$ROOT" -B "$BUILD" -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="$ARCH" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
    -DKERF_COPY_PLUGIN=OFF || fail "la configuration CMake a échoué"

step "Compilation (quelques minutes la première fois)"
"$CMAKE" --build "$BUILD" --parallel "$(sysctl -n hw.ncpu)" \
    --target Kerf_VST3 Kerf_AU Kerf_Standalone KerfTests KerfHostTests || fail "la compilation a échoué"

step "Tests"
"$BUILD/KerfTests_artefacts/Release/KerfTests" || fail "tests moteur en échec"
"$BUILD/KerfHostTests" || fail "tests hôte en échec"

# ------------------------------------------------------------------ 5. install
ART="$BUILD/Kerf_artefacts/Release"
step "Installation dans les dossiers de plug-ins utilisateur"
mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components
rm -rf ~/Library/Audio/Plug-Ins/VST3/Kerf.vst3 ~/Library/Audio/Plug-Ins/Components/Kerf.component
cp -R "$ART/VST3/Kerf.vst3" ~/Library/Audio/Plug-Ins/VST3/
cp -R "$ART/AU/Kerf.component" ~/Library/Audio/Plug-Ins/Components/

step "Installation dans les dossiers système (Maschine, Ableton, Logic les scannent toujours)"
echo "Votre mot de passe de session Mac peut être demandé :"
if sudo -v; then
    sudo mkdir -p /Library/Audio/Plug-Ins/VST3 /Library/Audio/Plug-Ins/Components
    sudo rm -rf /Library/Audio/Plug-Ins/VST3/Kerf.vst3 /Library/Audio/Plug-Ins/Components/Kerf.component
    sudo cp -R "$ART/VST3/Kerf.vst3" /Library/Audio/Plug-Ins/VST3/
    sudo cp -R "$ART/AU/Kerf.component" /Library/Audio/Plug-Ins/Components/
else
    echo "(copie système ignorée, les copies utilisateur suffisent pour Ableton et Logic)"
fi

# ------------------------------------------------------------------ 6. verify
step "Vérification"
killall -9 AudioComponentRegistrar 2>/dev/null || true
ls -d ~/Library/Audio/Plug-Ins/VST3/Kerf.vst3 /Library/Audio/Plug-Ins/VST3/Kerf.vst3 2>/dev/null
file "$ART/VST3/Kerf.vst3/Contents/MacOS/Kerf" | sed 's/^/binaire : /'
if command -v auval >/dev/null; then
    if auval -v aumf Kerf Bjrt 2>&1 | tail -n 3 | grep -q "SUCCEEDED"; then echo "auval : AU VALIDATION SUCCEEDED"; else echo "auval : échec (le VST3 reste utilisable)"; fi
fi

printf '\n\033[1;32mKerf est installé.\033[0m\n'
cat <<'TXT'

Maschine 3 : Préférences > Plug-ins > Locations > Rescan, puis navigateur > Plug-ins > Effects > Bajart Audio > Kerf.
Ableton Live : Préférences > Plug-Ins > activer "Use VST3 Plug-In System Folders" (et "Use Audio Units"), puis Rescan avec Alt.
Logic Pro : Kerf apparaît dans Audio Units > Bajart Audio après redémarrage de Logic.
TXT

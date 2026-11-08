Automatic build of Kerf from the latest commit on `main`.

**macOS** (Apple Silicon and Intel, macOS 11+)
- Easiest: download **Kerf-macOS-Installer.pkg** and open it. The first time, macOS blocks it because the build is not notarised: go to *System Settings > Privacy & Security*, scroll down and click **Open Anyway**, then open the .pkg again. It installs the VST3 and the AU into the system plug-in folders.
- Or drag and drop: unzip **Kerf-VST3-macOS.zip** and move `Kerf.vst3` into `~/Library/Audio/Plug-Ins/VST3` (same for `Kerf.component` into `~/Library/Audio/Plug-Ins/Components`). If your DAW refuses to load it, run once in Terminal: `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Kerf.vst3`
- Then rescan plug-ins in your DAW (Ableton Live: Preferences > Plug-Ins > Rescan with Alt held; Maschine 3: Preferences > Plug-ins > Locations > Rescan).

**Windows** (64-bit)
- Unzip **Kerf-VST3-Windows.zip** and copy the `Kerf.vst3` folder into `C:\Program Files\Common Files\VST3`, then rescan plug-ins in your DAW.

**Standalone** apps are included for trying Kerf without a DAW.

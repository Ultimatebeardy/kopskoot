# Building KopSkoot

The build is the official **obs-plugintemplate** build (CMake presets + `buildspec.json`). On Windows the first configure downloads, verifies (SHA-256) and builds the exact OBS SDK pinned in `buildspec.json` (OBS 31.1.1 sources, obs-deps and Qt 6 from `obsproject/obs-deps`). Nothing depends on where OBS is installed on your PC.

## Windows 11 x64 (primary target)

### Prerequisites
- **Visual Studio 2022** (17.10+) with *Desktop development with C++*.
- **Windows 11 SDK 10.0.22621** (the preset asks for it; the template requires ≥ 10.0.20348). It contains the C++/WinRT headers used for OCR.
- **CMake 3.28–3.30** (the one bundled with VS 2022 17.10+ works; `cmake --version`).
- **Git** and an internet connection for the first configure.
- OBS Studio **31.x or newer** to run the plugin.

### Release build (recommended; RelWithDebInfo = optimized + PDB)
Open *x64 Native Tools Command Prompt for VS 2022* (or PowerShell) in the repository root:

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64
```

Outputs:
- `build_x64\RelWithDebInfo\realm-killsense.dll` (+ `.pdb`)
- A ready-to-copy layout in `build_x64\rundir\RelWithDebInfo\` (`realm-killsense.dll` and the `realm-killsense\` data folder).

Pure Release (no PDB):
```powershell
cmake --build build_x64 --config Release
```

### Debug build
```powershell
cmake --build build_x64 --config Debug
```
Note: a Debug plugin links the debug C runtime and debug Qt libraries, which a normal (release) OBS does not ship, so it will generally **not load** in a regular OBS install. For debugging inside OBS use **RelWithDebInfo** and attach Visual Studio to `obs64.exe` (symbols come from the PDB). Use Debug builds for the unit tests (`core/`), which do not need OBS.

### Install into OBS
The default install prefix is `%ALLUSERSPROFILE%\obs-studio\plugins` — the official per-machine plugin folder of OBS 28+ (typically `C:\ProgramData\obs-studio\plugins`). From an **elevated** prompt:

```powershell
cmake --install build_x64 --config RelWithDebInfo
```

Resulting layout:
```
C:\ProgramData\obs-studio\plugins\realm-killsense\
├── bin\64bit\realm-killsense.dll   (+ realm-killsense.pdb)
└── data\
    ├── locale\en-US.ini
    ├── profiles\generic.json, sample-killfeed.json
    ├── templates\sample-kill-icon.png
    └── test-assets\*.png
```
Without admin rights, install to a staging folder and copy it yourself:
```powershell
cmake --install build_x64 --config RelWithDebInfo --prefix "$PWD\dist"
# then copy dist\realm-killsense to C:\ProgramData\obs-studio\plugins\
```
Restart OBS. **Docks → KopSkoot** shows the dock.

User files (created at runtime):
```
%APPDATA%\obs-studio\plugin_config\realm-killsense\config.json
%APPDATA%\obs-studio\plugin_config\realm-killsense\profiles\        (your profiles)
```
Logs: *Help → Log Files* (lines start with `[realm-killsense]`).

### Build options
| CMake option | Default | Effect |
|---|---|---|
| `KILLSENSE_ENABLE_WINDOWS_OCR` | ON | Windows.Media.Ocr backend. If the C++/WinRT headers are missing, configure prints a warning and OCR detectors report *unavailable* (template/colour/change still work). |
| `KILLSENSE_BUILD_TESTS` | OFF (plugin build) / ON (`core` standalone) | GoogleTest unit tests + `killsense-replay`. |
| `KILLSENSE_BUILD_OBS_HARNESS` | OFF | Headless libobs integration test (developer tool). |

Example: `cmake --preset windows-x64 -DKILLSENSE_BUILD_TESTS=ON`.

### Unit tests and replay tool (no OBS needed)
```powershell
cmake -S core -B build_core -G "Visual Studio 17 2022" -A x64
cmake --build build_core --config Debug
ctest --test-dir build_core -C Debug --output-on-failure

build_core\tools\replay\Debug\killsense-replay.exe --profile data\profiles\sample-killfeed.json --frames tests\data\sequences\sample-killfeed --expect 4
```

## Linux (secondary, used for CI-style verification)
With OBS development packages installed (e.g. Ubuntu `libobs-dev` + `qt6-base-dev`):
```bash
cmake -S . -B build_linux -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build_linux
mkdir -p ~/.config/obs-studio/plugins/realm-killsense/bin/64bit
cp build_linux/realm-killsense.so ~/.config/obs-studio/plugins/realm-killsense/bin/64bit/
cp -r data ~/.config/obs-studio/plugins/realm-killsense/
```
(There is no OCR backend on Linux yet; OCR detectors report unavailable.)

## macOS
The template's macOS presets are kept, but macOS is untested and has no OCR backend yet (Apple Vision is a candidate). Planned.

## Troubleshooting
- *"OBS requires Windows 10 SDK version 10.0.20348.0 or more recent"* — install the Windows 11 SDK from the VS Installer.
- *Plugin not listed in Docks* — check the log for `[realm-killsense]`; confirm the folder layout above and that OBS is 31.x+ (a plugin built against a newer OBS major version is refused by older OBS).
- *OCR engine: unavailable* — install an OCR-capable language (Settings → Time & language → Language & region → add language → include "Optical character recognition"), or set the OCR language in KillSense Settings → Advanced.

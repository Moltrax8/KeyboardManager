# KeyboardManager
A lightweight C++ Windows keyboard manager utilizing Dear ImGui, miniaudio, and the Raw Input API for anti-cheat-safe custom binds, soundboarding, and background media control.

## Features

- Passive global input through `RegisterRawInputDevices`; input is never blocked or rewritten.
- Full-size, TKL, and 60% physical layouts with labels resolved from the active Windows keyboard language.
- Physical scan-code binds, including distinct E0/E1 extended keys and exact Ctrl/Shift/Alt/Win combinations.
- Hover summaries for assigned keys, grouped by modifier chord and action chain.
- Chained sound, file-launch, and media-key actions with initial-KeyDown debouncing.
- Profile persistence, output-device routing, master volume, tray operation, and a global `Ctrl+Alt+P` kill switch.
- Silent recovery from missing action targets. Diagnostics are written to `%LOCALAPPDATA%\KeyboardManager\KeyboardManager.log`.

Configuration is stored atomically at `%LOCALAPPDATA%\KeyboardManager\config.json`. Audio files placed in the `Sounds` directory can be assigned from the bind editor. Rescanning sounds also reloads decoded audio, so replacing a file with a new version under the same name takes effect immediately.

## Build

Requirements: Windows 10 or newer, Visual Studio 2022 with the C++ desktop workload, CMake 3.24+, Git, and an x64 generator.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

CMake fetches pinned Dear ImGui, miniaudio, and nlohmann/json revisions. The release executable is `build\Release\KeyboardManager.exe` and uses the static MSVC runtime.

## Tests

Configuration persistence, validation, and recovery are covered by CTest. The GitHub Actions workflow builds the complete application and runs the tests with Visual Studio 2022 on Windows.

```powershell
cmake -S . -B build -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## Installer

After a Release build, compile `installer\KeyboardManager.iss` with Inno Setup 6. The installer is emitted under `dist`, creates optional desktop and Start Menu shortcuts, and asks a running tray instance to shut down before an install or update replaces files.

To package a Release artifact from a different build directory, override `BuildOutputDir`:

```powershell
ISCC.exe /DBuildOutputDir=..\build-mingw installer\KeyboardManager.iss
```

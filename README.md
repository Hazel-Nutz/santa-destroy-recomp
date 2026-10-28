<p align="center"><img src="logo.png" width="256" alt="Santa Destroy Recomp"></p>

# Santa Destroy Recomp

A build kit that turns your own Xbox 360 disc into a native Windows program using **static recompilation**
with the [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk). The game's PowerPC code is recompiled into
C++ on your PC during the build.

This repository contains no game code, assets or executables. You must own the game and dump your own disc.

**Supported disc:** Xbox 360, Japan release, Title ID `4D5607D2`, Media ID `39F3F30C`.

## How to build

### 1. Install the tools
- **Visual Studio 2022** (Community is free). In the installer, tick:
  - Desktop development with C++
  - C++ Clang Compiler for Windows
  - C++ CMake tools for Windows
- **Git**: https://git-scm.com
- **extract-xiso**: download `extract-xiso.exe` from https://github.com/XboxDev/extract-xiso/releases
  and place it in this kit's `tools\` folder.

### 2. Dump your game
Make an ISO of your own disc (Title ID `4D5607D2`).

### 3. Set up the folders
Create a game folder and put the ISO and this kit inside it:

    My Game Folder\
      santa-destroy-recomp\    (this repository)
      YourDisc.iso

### 4. Build
Double-click `BUILD.bat` and leave the PC on. The first build:
1. Downloads and builds the ReXGlue SDK (30 to 90 minutes)
2. Extracts the ISO
3. Recompiles the game code and compiles the PC executable (20 minutes to a few hours)
4. Creates a `Santa Destroy Recomp` folder next to the kit

### 5. Play
Open `Santa Destroy Recomp\santadestroy_launcher.exe`, set your controls and display options, then press **PLAY**.

### Build options
Pass these to `BUILD.bat` from a command prompt: `-GameFolder <path>`, `-OutDir <path>`, `-SkipData`,
`-RebuildSdk`, `-SdkDir <prebuilt rexglue-sdk>`.
- `REBUILD-CPU-OPTIMISED.bat` builds for your exact CPU. Do not share builds made with it.
- `UPDATE-LAUNCHER.bat` rebuilds only the launcher.

### Troubleshooting
- **Build failed:** open `build.log` in the kit folder and check the last lines.
- **Characters stretch or explode:** set `readback_memexport = true` in `santadestroy.toml`.
- **Low frame rate with the GPU at 100%:** run `GPU mode - ROV.bat` in the game folder.
  Run `GPU mode - undo ROV.bat` to switch back.
- **Game crashes on another PC:** rebuild with `BUILD.bat`. Builds from `REBUILD-CPU-OPTIMISED.bat` only
  run on the CPU they were built on.

## Repository layout
| Path | Contents |
|---|---|
| `project\` | ReXGlue project: manifest, codegen overrides (`config\`), game fixes (`src\`), launcher |
| `patches\` | Changes to the ReXGlue SDK (input, audio/XMA, timers, frame limiter, GPU memexport) |
| `tools\build.ps1` | The build script |
| `game-extras\` | Copied into the game folder: GPU mode (ROV) and diagnostics switches |
| `santadestroy.default.toml` | Default settings written to `santadestroy.toml` on first build |

Game fixes in `project\src\santadestroy_fixes.cpp`: Xbox LIVE sign-in prompt, mouse capture at start.
`fast_fma.cpp` replaces the slow software FMA on CPUs without FMA3. `crash_log.cpp` writes crash reports to `logs\`.

## Status
Work in progress. Tested on an Intel i5-3470 with an RTX 3050. Japanese text only.

## Legal
This project is not affiliated with or endorsed by the game's publisher or developer. No copyrighted game
data is distributed. `project\launcher\imgui\` is Dear ImGui (MIT, see its LICENSE.txt). The `patches\`
modify the ReXGlue SDK (BSD 3-Clause, derived from Xenia) and keep its license.

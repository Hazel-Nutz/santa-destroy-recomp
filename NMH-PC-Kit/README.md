# No More Heroes: Heroes' Paradise (Xbox 360) - PC build kit

A build kit that turns **your own copy** of *No More Heroes: Heroes' Paradise* (Xbox 360, Japan release,
"Eiyuutachi no Rakuen") into a native Windows program, using **static recompilation** with the
[ReXGlue SDK](https://github.com/rexglue/rexglue-sdk).

This is a **recompilation, not a decompilation**: the game's PowerPC code is translated to C++ on your PC
during the build. This repository contains **no game code, assets or executables**. You must own the game
and dump your own disc.

## Requirements
- Windows 10/11 x64, CPU with AVX (Intel Ivy Bridge / AMD Bulldozer or newer), DirectX 12 GPU
- Visual Studio 2022 with "Desktop development with C++", "C++ Clang Compiler for Windows" and
  "C++ CMake tools for Windows"
- [Git](https://git-scm.com)
- [extract-xiso](https://github.com/XboxDev/extract-xiso/releases) (`extract-xiso.exe` in `tools\`),
  unless you provide an already extracted disc
- Your own disc image: *No More Heroes - Eiyuutachi no Rakuen (Japan)* (default.xex, TitleID 4D5607D2)

## Build
1. Make a game folder and put this kit inside it, next to your ISO:
   ```
   No More Heroes Heroes' Paradise\
     NMH-PC-Kit\                      <- this repository
     <your disc>.iso                   (or an extracted disc\ folder with default.xex)
   ```
2. Run `BUILD.bat`. The first build:
   1. downloads ReXGlue at the pinned commit (`patches\rexglue-base-commit.txt`), applies the kit's SDK
      patches and builds the SDK (30-90 min),
   2. extracts the ISO into `disc\`,
   3. recompiles `default.xex` to C++ and compiles `nmh.exe` + `nmh_launcher.exe`,
   4. creates `No More Heroes PC\` (assets are hard links to `disc\`, so no extra disk space).
3. Run `No More Heroes PC\nmh_launcher.exe`, set controls/display, press PLAY.

Options (pass to BUILD.bat): `-GameFolder <path>`, `-OutDir <path>`, `-SkipData`, `-RebuildSdk`,
`-SdkDir <prebuilt rexglue-sdk>`. `REBUILD-CPU-OPTIMISED.bat` builds for your exact CPU (do not share
those builds). `UPDATE-LAUNCHER.bat` rebuilds only the launcher. If a build fails, check `build.log`.

## What's in here
| Path | Contents |
|---|---|
| `project\` | ReXGlue project: manifest, codegen overrides (`config\`), game fixes (`src\`), launcher |
| `patches\` | Changes to the ReXGlue SDK (input, audio/XMA, timers, frame limiter, GPU memexport, diagnostics) |
| `tools\build.ps1` | The build script |
| `game-extras\` | Copied into the game folder: GPU mode (ROV) and diagnostics on/off switches |
| `nmh.default.toml` | Default settings written to `nmh.toml` on first build |

Game-specific fixes: Xbox LIVE sign-in prompt (`project\src\nmh_fixes.cpp`), software FMA replacement for
CPUs without FMA3 (`fast_fma.cpp`), crash logging, setjmp/longjmp and missing-function codegen entries.

## Status
Work in progress. Tested on an i5-3470 + RTX 3050. Japanese text only (no English patch). Cutscenes use the XMV/WMV path. Known options:
`readback_memexport = true` in `nmh.toml` if skinned characters stretch; `GPU mode - ROV.bat` if GPU-bound.

## Legal
No More Heroes is a trademark of its respective owners. This project is not affiliated with or endorsed by
them. No copyrighted game data is distributed. `project\launcher\imgui\` is Dear ImGui (MIT, see its
LICENSE.txt). The `patches\` modify the ReXGlue SDK (BSD 3-Clause, derived from Xenia) and keep its license.

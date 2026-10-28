<#
  No More Heroes (Xbox 360) - PC build script (ReXGlue static recompilation)
  1. fetches the ReXGlue SDK at the pinned commit (patches\rexglue-base-commit.txt), applies the
     kit's SDK patches (patches\) and builds it once in work\rexglue-sdk,
  2. extracts your disc ISO, 3. recompiles default.xex into C++, 4. compiles nmh.exe + nmh_launcher.exe
  and lays out a ready-to-play folder. No game files are included; you must supply your own disc.

  Usage: BUILD.bat [-GameFolder <path>] [-OutDir <path>] [-CpuArch native|ivybridge] [-SkipData]
                   [-SdkDir <prebuilt rexglue-sdk>] [-RebuildSdk]
#>
param(
  [string]$SdkDir = "",
  [string]$GameFolder = "",
  [string]$OutDir = "",
  [string]$CpuArch = "",
  [switch]$LauncherOnly,  # rebuild only nmh_launcher.exe and copy it into the game folder
  [switch]$SkipData,
  [switch]$RebuildSdk
)
$ErrorActionPreference = "Stop"
$Kit = Split-Path -Parent $PSScriptRoot
if (-not $GameFolder) { $GameFolder = Split-Path -Parent $Kit }
if (-not $OutDir) { $OutDir = Join-Path $GameFolder "No More Heroes PC" }
$Work = Join-Path $Kit "work"
$Proj = Join-Path $Work "nmh"
$Log = Join-Path $Kit "build.log"
New-Item -ItemType Directory -Force -Path $Work | Out-Null

function Log($m) { [IO.File]::AppendAllText($Log, "$m`r`n") }
function Step($m) { Write-Host ""; Write-Host "==> $m" -ForegroundColor Cyan; Log "==> $m" }
function Fail($m) { Write-Host ""; Write-Host "ERROR: $m" -ForegroundColor Red; Log "ERROR: $m"; exit 1 }
function Run($exe, [string[]]$a) {
  Log "> $exe $($a -join ' ')"
  $prev = $ErrorActionPreference; $ErrorActionPreference = "Continue"
  & $exe @a 2>&1 | ForEach-Object { $t = "$_"; Log $t; $t } | Out-Host
  $ErrorActionPreference = $prev
  if ($LASTEXITCODE -ne 0) { Fail "$exe failed (exit $LASTEXITCODE). See build.log" }
}
[IO.File]::WriteAllText($Log, "No More Heroes PC build $(Get-Date)`r`n")
try {
  Add-Type -Namespace Oc -Name Power -MemberDefinition '[DllImport("kernel32.dll")] public static extern uint SetThreadExecutionState(uint esFlags);'
  [void][Oc.Power]::SetThreadExecutionState([uint32]"0x80000001")  # keep the PC awake while building
} catch { Write-Warning "Could not disable sleep: $_" }

# ---------------------------------------------------------------- 1. toolchain
Step "Checking tools (Visual Studio 2022 + Clang, CMake, Ninja, Git)"
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { Fail "Visual Studio 2022 not found." }
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { Fail "Visual Studio C++ tools missing (workload 'Desktop development with C++')." }
$vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"
cmd /c "`"$vcvars`" >nul && set" | ForEach-Object { if ($_ -match "^([^=]+)=(.*)$") { Set-Item -Path "env:$($matches[1])" -Value $matches[2] } }
$llvm = Join-Path $vs "VC\Tools\Llvm\x64\bin"
if (Test-Path (Join-Path $llvm "clang++.exe")) { $env:PATH = "$llvm;$env:PATH" }
foreach ($t in "clang++", "cmake", "ninja", "git") {
  if (-not (Get-Command $t -ErrorAction SilentlyContinue)) { Fail "$t not found (Visual Studio: C++ Clang Compiler + C++ CMake tools; Git from https://git-scm.com)." }
}
Run "clang++" @("--version")

# ---------------------------------------------------------------- launcher only
# Configures a fresh standalone launcher build per PC (work\launcher-<PC name>), so CMake caches made on
# another PC (different Visual Studio path) are never reused. Builds in a few minutes.
if ($LauncherOnly) {
  Step "Rebuilding nmh_launcher.exe only"
  if (-not (Test-Path -LiteralPath $OutDir)) { Fail "Game folder not found ($OutDir) - pass -OutDir <game folder>." }
  $sdlCands = @((Join-Path $Kit "work\rexglue-sdk\thirdparty\sdl3"))
  if ($SdkDir) { $sdlCands = @((Join-Path $SdkDir "thirdparty\sdl3")) + $sdlCands }
  $sdl = $sdlCands | Where-Object { Test-Path -LiteralPath (Join-Path $_ "CMakeLists.txt") } | Select-Object -First 1
  if (-not $sdl) { Fail "SDL3 source not found - run BUILD.bat once first (it fetches the SDK), or pass -SdkDir." }
  $src = Join-Path $Kit "project\launcher"
  $lb = Join-Path $Kit "work\launcher-$($env:COMPUTERNAME)"
  $binOut = Join-Path $lb "bin"
  Run "cmake" @("--fresh", "-S", $src, "-B", $lb, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_C_COMPILER=clang",
    "-DCMAKE_CXX_COMPILER=clang++", "-DSDL3_SOURCE_DIR=$($sdl -replace '\\','/')",
    "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=$($binOut -replace '\\','/')")
  Run "cmake" @("--build", $lb, "--config", "Release", "--target", "nmh_launcher")
  $l = Join-Path $binOut "nmh_launcher.exe"
  if (-not (Test-Path -LiteralPath $l)) { Fail "nmh_launcher.exe was not produced." }
  Copy-Item -Force $l $OutDir
  Step "Done"
  Write-Host "Updated $OutDir\nmh_launcher.exe"
  exit 0
}

# ---------------------------------------------------------------- 2. ReXGlue SDK
if ($SdkDir) {
  Step "Using the prebuilt ReXGlue SDK in $SdkDir"
  if (-not (Test-Path (Join-Path $SdkDir "out\install\win-amd64"))) { Fail "No out\install\win-amd64 in $SdkDir." }
} else {
  $SdkDir = Join-Path $Work "rexglue-sdk"
  $stamp = Join-Path $SdkDir ".kit-sdk-built"
  if ($RebuildSdk -or -not (Test-Path $stamp) -or -not (Test-Path (Join-Path $SdkDir "out\install\win-amd64"))) {
    $base = (Get-Content (Join-Path $Kit "patches\rexglue-base-commit.txt")).Trim()
    Step "Fetching ReXGlue SDK $base"
    if (-not (Test-Path (Join-Path $SdkDir ".git"))) { Run "git" @("clone", "https://github.com/rexglue/rexglue-sdk.git", $SdkDir) }
    Push-Location $SdkDir
    Run "git" @("fetch", "origin")
    Run "git" @("reset", "--hard", $base)
    Run "git" @("clean", "-fdq", "-e", "out")
    Run "git" @("submodule", "update", "--init", "--recursive")
    # Git for Windows checks symlinks out as small text files holding the target path (libmspack);
    # replace each with its target file.
    Step "Resolving Git symlinks"
    $repos = @($SdkDir)
    $subs = & git -C $SdkDir submodule foreach --recursive --quiet 'echo $displaypath'
    foreach ($r in $subs) { if ($r) { $repos += (Join-Path $SdkDir $r.Trim()) } }
    $fixed = 0
    foreach ($repo in $repos) {
      foreach ($l in (& git -C $repo ls-files -s)) {
        if ($l -notmatch '^120000 \S+ \d+\t(.+)$') { continue }
        $f = Join-Path $repo $matches[1]
        if (-not (Test-Path -LiteralPath $f -PathType Leaf)) { continue }
        $item = Get-Item -LiteralPath $f
        if ($item.LinkType -or $item.Length -gt 1024) { continue }
        $target = (Get-Content -LiteralPath $f -Raw).Trim()
        if (-not $target -or $target.IndexOfAny([char[]]([IO.Path]::GetInvalidPathChars() + [char[]]"`r`n*?")) -ge 0) { continue }
        try {
          $src = Join-Path (Split-Path -Parent $f) $target
          if (Test-Path -LiteralPath $src -PathType Leaf) { Copy-Item -Force -LiteralPath $src $f; $fixed++ }
        } catch { continue }
      }
    }
    Write-Host "Replaced $fixed symlinked files"; Log "Replaced $fixed symlinked files"
    Step "Applying SDK patches"
    Run "git" @("apply", "--whitespace=nowarn", (Join-Path $Kit "patches\rexglue-bo3.patch"))
    foreach ($p in (Get-Content (Join-Path $Kit "patches\hotfix-order.txt"))) {
      $p = $p.Trim(); if (-not $p) { continue }
      $prev = $ErrorActionPreference; $ErrorActionPreference = "Continue"
      & git apply --check --reverse --whitespace=nowarn (Join-Path $Kit "patches\$p") 2>&1 | Out-Null
      $already = ($LASTEXITCODE -eq 0); $ErrorActionPreference = $prev
      if ($already) { Write-Host "Already included: $p"; Log "already included $p"; continue }
      Write-Host "Applying $p"
      Run "git" @("apply", "--whitespace=nowarn", (Join-Path $Kit "patches\$p"))
    }
    Step "Building the SDK (first time 30-90 min; uses all CPU cores)"
    $cfg = @("--fresh", "--preset", "win-amd64")
    if ($CpuArch) { $cfg += @("-DCMAKE_C_FLAGS=-march=$CpuArch", "-DCMAKE_CXX_FLAGS=-march=$CpuArch") }
    Run "cmake" $cfg
    Run "cmake" @("--build", "out/build/win-amd64", "--config", "Release", "--target", "install")
    Pop-Location
    [IO.File]::WriteAllText($stamp, "$base $CpuArch $(Get-Date)")
  } else {
    Step "ReXGlue SDK already built ($SdkDir) - use -RebuildSdk to rebuild"
  }
}
$Install = Join-Path $SdkDir "out\install\win-amd64"
$Cli = Join-Path $Install "bin\rexglue.exe"
if (-not (Test-Path $Cli)) {
  $c = Get-ChildItem -Path (Join-Path $SdkDir "out") -Filter "rexglue.exe" -Recurse | Select-Object -First 1
  if ($c) { $Cli = $c.FullName } else { Fail "rexglue.exe not found under $SdkDir\out." }
}
Write-Host "SDK     : $SdkDir"; Write-Host "rexglue : $Cli"; Log "SDK $SdkDir / $Cli"

# ---------------------------------------------------------------- 3. disc files
Step "Locating disc files"
# Uses game\ or disc\ if already extracted; otherwise extracts the ISO next to the kit into disc\.
$xex = $null
foreach ($d in @((Join-Path $GameFolder "game"), (Join-Path $GameFolder "disc"))) {
  if (Test-Path (Join-Path $d "default.xex")) { $xex = Get-Item (Join-Path $d "default.xex"); break }
}
if (-not $xex) {
  $Disc = Join-Path $GameFolder "disc"
  $iso = Get-ChildItem -Path $GameFolder -Filter "*.iso" -Recurse -Depth 2 -File | Select-Object -First 1
  if (-not $iso) { Fail "No extracted game folder (game\default.xex) and no disc ISO found under $GameFolder." }
  Step "Extracting $($iso.Name) to $Disc (4.5 GB, several minutes)"
  New-Item -ItemType Directory -Force -Path $Disc | Out-Null
  Push-Location $Disc
  $xiso = Join-Path $PSScriptRoot "extract-xiso.exe"
  if (-not (Test-Path $xiso)) { $c = Get-Command extract-xiso -ErrorAction SilentlyContinue; if ($c) { $xiso = $c.Source } }
  if (-not (Test-Path $xiso)) { Fail "extract-xiso.exe not found. Download it from https://github.com/XboxDev/extract-xiso/releases and put it in tools\." }
  Run $xiso @("-d", $Disc, "-x", $iso.FullName)
  Pop-Location
  $xex = Get-ChildItem -Path $Disc -Filter "default.xex" -Recurse -Depth 2 | Select-Object -First 1
  if (-not $xex) { Fail "default.xex not found after extracting the ISO." }
}
$DiscRoot = $xex.Directory.FullName
Write-Host "Disc files: $DiscRoot"

# ---------------------------------------------------------------- 4. recompile + compile
Step "Preparing project"
New-Item -ItemType Directory -Force -Path $Proj | Out-Null
Copy-Item -Recurse -Force (Join-Path $Kit "project\*") $Proj
New-Item -ItemType Directory -Force -Path (Join-Path $Proj "assets") | Out-Null
Copy-Item -Force $xex.FullName (Join-Path $Proj "assets\default.xex")
$ico = Join-Path $GameFolder "cover-icon.ico"
if (Test-Path $ico) { Copy-Item -Force $ico (Join-Path $Proj "src\nmh.ico"); Copy-Item -Force $ico (Join-Path $Proj "launcher\nmh.ico") }
Push-Location $Proj
Step "Recompiling PowerPC -> C++ (codegen, ~2 min; ~21,600 functions)"
Run $Cli @("codegen", "nmh_manifest.toml")
Step "Compiling nmh.exe + nmh_launcher.exe (~20 min to several hours on 4 cores; uses all CPU cores. Keep the PC on.)"
Run "cmake" @("--preset", "win-amd64-release", "-DCMAKE_PREFIX_PATH=$Install", "-DNMH_CPU_ARCH=$CpuArch", "-DNMH_SDL3_SOURCE_DIR=$($SdkDir -replace '\\','/')/thirdparty/sdl3")
Run "cmake" @("--build", "--preset", "win-amd64-release")
Pop-Location

# ---------------------------------------------------------------- 5. game folder
Step "Creating $OutDir"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$bin = Join-Path $Proj "out\build\win-amd64-release"
$exe = Get-ChildItem -Path $bin -Filter "nmh.exe" -Recurse | Select-Object -First 1
if (-not $exe) { Fail "nmh.exe was not produced." }
Copy-Item -Force $exe.FullName $OutDir
$launcher = Get-ChildItem -Path $bin -Filter "nmh_launcher.exe" -Recurse | Select-Object -First 1
if ($launcher) { Copy-Item -Force $launcher.FullName $OutDir } else { Write-Warning "nmh_launcher.exe was not produced (the game still works; edit nmh.toml)." }
# Project build dir first, SDK install last: the install tree holds the hotfixed DLLs.
foreach ($d in @($bin, (Join-Path $Install "lib"), (Join-Path $Install "bin"))) {
  Get-ChildItem -Path $d -Filter *.dll -Recurse -ErrorAction SilentlyContinue | ForEach-Object { Copy-Item -Force $_.FullName $OutDir }
}
function Link($src, $dst) {
  if (Test-Path -LiteralPath $dst) { return }
  New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dst) | Out-Null
  try { New-Item -ItemType HardLink -Path $dst -Target $src -ErrorAction Stop | Out-Null }
  catch { Copy-Item -LiteralPath $src $dst }
}
if (-not $SkipData) {
  Step "Linking game files into assets\ (hard links: no extra disk space)"
  $assets = Join-Path $OutDir "assets"
  Get-ChildItem -LiteralPath $DiscRoot -Recurse -File | ForEach-Object {
    $rel = $_.FullName.Substring($DiscRoot.Length).TrimStart('\')
    Link $_.FullName (Join-Path $assets $rel)
  }
  New-Item -ItemType Directory -Force -Path (Join-Path $OutDir "user") | Out-Null
}
$toml = Join-Path $OutDir "nmh.toml"
if (-not (Test-Path $toml)) {
  # TOML basic string (escaped): single-quoted literals break on the apostrophe in "Heroes' Paradise".
  $q = { param($p) '"' + ($p -replace '\\', '\\' -replace '"', '\"') + '"' }
  $cfg = (Get-Content -Raw (Join-Path $Kit "nmh.default.toml"))
  $cfg = $cfg.Replace("@GAME@", (& $q (Join-Path $OutDir 'assets'))).Replace("@USER@", (& $q (Join-Path $OutDir 'user'))).Replace("@UPDATE@", (& $q (Join-Path $OutDir 'update')))
  [IO.File]::WriteAllText($toml, $cfg)  # UTF-8 without BOM (TOML rejects a BOM)
}

# Extra files for the game folder (GPU mode switch .bat files etc.)
$extras = Join-Path $Kit "game-extras"
if (Test-Path -LiteralPath $extras) { Get-ChildItem -LiteralPath $extras -File | ForEach-Object { Copy-Item -Force $_.FullName $OutDir } }
Step "Done"
Write-Host "Game folder: $OutDir"
Write-Host "Run nmh_launcher.exe (controls, mouse, display) and press PLAY, or run nmh.exe directly."
Write-Host "Settings are stored in nmh.toml; logs in logs\."

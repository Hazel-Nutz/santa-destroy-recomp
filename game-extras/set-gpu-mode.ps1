# Sets the GPU render target mode in this game's settings (.toml). Used by the "GPU mode" .bat files.
#   rov  = pixel-shader interlock (can cut GPU load a lot; first runs stutter while shaders build;
#          some games show glitches or run slower - test each game)
#   auto = SDK default (RTV / host render targets on NVIDIA and AMD)
param([ValidateSet("rov", "auto")][string]$Mode = "rov")
$value = if ($Mode -eq "rov") { '"rov"' } else { '""' }
$line = "render_target_path_d3d12 = $value"
$rx = '(?m)^render_target_path_d3d12[ \t]*=[^\r\n]*'
# Game settings files sit next to this script and contain game_data_root (skips launcher/other files).
$files = Get-ChildItem -LiteralPath $PSScriptRoot -Filter *.toml -File |
  Where-Object { [IO.File]::ReadAllText($_.FullName) -match '(?m)^game_data_root' }
if (-not $files) { Write-Host "No game settings .toml found next to this script." -ForegroundColor Red; exit 1 }
foreach ($f in $files) {
  $t = [IO.File]::ReadAllText($f.FullName)
  if ([regex]::IsMatch($t, $rx)) { $t = [regex]::Replace($t, $rx, $line) }
  else { if (-not $t.EndsWith("`n")) { $t += "`r`n" }; $t += "$line`r`n" }
  [IO.File]::WriteAllText($f.FullName, $t)   # UTF-8 without BOM (TOML rejects a BOM)
  Write-Host "Updated $($f.Name)"
}
if ($Mode -eq "rov") { Write-Host "GPU mode set to ROV. Start the game as usual." -ForegroundColor Green }
else { Write-Host "GPU mode set back to default (RTV). Start the game as usual." -ForegroundColor Green }

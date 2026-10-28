# Turns the port's diagnostics off, or restores them. Used by the "Diagnostics" .bat files.
#   off     = save the current diagnostic values to diagnostics-backup.json, then switch them off
#   restore = put the saved values back (undo)
# Only keys already present in the game's .toml are changed (absent keys are off by default).
param([ValidateSet("off", "restore")][string]$Mode = "off")
$offValues = [ordered]@{
  perf_report_seconds = '0'      # per-thread CPU usage log
  perf_sample_thread  = '""'     # call-stack sampler (causes hitches)
  wait_profile        = 'false'  # per-thread wait/read log
  xma_diag            = 'false'  # XMA audio decoder log
  mnk_trace           = 'false'  # controller state log
}
$backup = Join-Path $PSScriptRoot "diagnostics-backup.json"
$files = Get-ChildItem -LiteralPath $PSScriptRoot -Filter *.toml -File |
  Where-Object { [IO.File]::ReadAllText($_.FullName) -match '(?m)^game_data_root' }
if (-not $files) { Write-Host "No game settings .toml found next to this script." -ForegroundColor Red; exit 1 }
function KeyRx($k) { '(?m)^' + [regex]::Escape($k) + '[ \t]*=[ \t]*([^\r\n]*?)[ \t]*(?=\r?$)' }  # CRLF-safe

if ($Mode -eq "off") {
  $saved = [ordered]@{}; $changed = $false
  foreach ($f in $files) {
    $t = [IO.File]::ReadAllText($f.FullName); $vals = [ordered]@{}
    foreach ($k in $offValues.Keys) {
      $m = [regex]::Match($t, (KeyRx $k))
      if (-not $m.Success) { continue }
      $vals[$k] = $m.Groups[1].Value
      if ($m.Groups[1].Value -ne $offValues[$k]) { $changed = $true }
      $t = [regex]::Replace($t, (KeyRx $k), "$k = $($offValues[$k])")
    }
    $saved[$f.Name] = $vals
    [IO.File]::WriteAllText($f.FullName, $t)   # UTF-8 without BOM (TOML rejects a BOM)
  }
  # Keep the old backup when everything was already off (so "redo" never overwrites the undo point).
  if ($changed -or -not (Test-Path -LiteralPath $backup)) {
    [IO.File]::WriteAllText($backup, ($saved | ConvertTo-Json -Depth 4))
    Write-Host "Diagnostics turned off. Previous values saved to diagnostics-backup.json." -ForegroundColor Green
  } else {
    Write-Host "Diagnostics were already off (backup kept for undo)." -ForegroundColor Green
  }
} else {
  if (-not (Test-Path -LiteralPath $backup)) { Write-Host "No diagnostics-backup.json - nothing to undo." -ForegroundColor Red; exit 1 }
  $saved = Get-Content -Raw -LiteralPath $backup | ConvertFrom-Json
  foreach ($f in $files) {
    $vals = $saved.($f.Name); if (-not $vals) { continue }
    $t = [IO.File]::ReadAllText($f.FullName)
    foreach ($p in $vals.PSObject.Properties) {
      $line = "$($p.Name) = $($p.Value)"
      if ([regex]::IsMatch($t, (KeyRx $p.Name))) { $t = [regex]::Replace($t, (KeyRx $p.Name), $line.Replace('$', '$$')) }
      else { if (-not $t.EndsWith("`n")) { $t += "`r`n" }; $t += "$line`r`n" }
    }
    [IO.File]::WriteAllText($f.FullName, $t)
    Write-Host "Restored $($f.Name)"
  }
  Write-Host "Diagnostics restored to the saved values." -ForegroundColor Green
}

<#
.SYNOPSIS
Records Monster Hunter: World frame timing with the Intel PresentMon console
application, for the performance check.

.DESCRIPTION
Start it before or after the game. In game, each press of the hotkey records
-Seconds seconds into a new CSV under local\perf\<session>\ (PresentMon adds
the recording's index to the file name). Close the window when done, then run
tools\perf\presentmon_summary.py on the folder.

PresentMon reads Windows ETW events and needs administrator rights, so the
script asks once (UAC) and continues in an elevated window.

The console application is taken from local\tools\presentmon-*\ (a copy of
the GitHub release) or from the Intel PresentMon install, newest version
first. Pass -PresentMon to choose one.

.EXAMPLE
pwsh -File tools\perf\presentmon_capture.ps1
pwsh -File tools\perf\presentmon_capture.ps1 -Session baseline -Seconds 60
#>
param(
    [string]$Session = (Get-Date -Format 'yyyyMMdd-HHmmss'),
    [ValidateRange(5, 600)][int]$Seconds = 30,
    [string]$Hotkey = 'ALT+SHIFT+F11',
    [string]$PresentMon = ''
)
$ErrorActionPreference = 'Stop'
if ($Session -notmatch '^[A-Za-z0-9._-]+$') { throw "Session names use letters, digits, '.', '_' and '-' only." }
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path

if (-not $PresentMon) {
    $found = @()
    $found += @(Get-ChildItem (Join-Path $repo 'local\tools') -Recurse -Filter 'PresentMon-*-x64.exe' -ErrorAction SilentlyContinue)
    $found += @(Get-ChildItem (Join-Path $env:ProgramFiles 'Intel\PresentMon\PresentMonConsoleApplication') -Filter 'PresentMon-*-x64.exe' -ErrorAction SilentlyContinue)
    $newest = $found | Where-Object { $_.Name -match '^PresentMon-(\d+(\.\d+)+)-x64\.exe$' } |
        Sort-Object { [version]($_.Name -replace '^PresentMon-([\d.]+)-x64\.exe$', '$1') } -Descending | Select-Object -First 1
    if ($newest) { $PresentMon = $newest.FullName }
}
if (-not $PresentMon -or -not (Test-Path -LiteralPath $PresentMon)) {
    throw 'The PresentMon console application was not found; pass -PresentMon <path to PresentMon-x.y.z-x64.exe>.'
}

$identity = [Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
if (-not $identity.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    $shell = (Get-Process -Id $PID).Path
    $relaunch = @('-NoProfile', '-NoExit', '-File', "`"$PSCommandPath`"", '-Session', $Session,
        '-Seconds', $Seconds, '-Hotkey', $Hotkey, '-PresentMon', "`"$PresentMon`"")
    Start-Process -FilePath $shell -Verb RunAs -ArgumentList $relaunch
    Write-Host "Continuing in an elevated window (session $Session)."
    return
}

$out = Join-Path $repo "local\perf\$Session"
New-Item -ItemType Directory -Force -Path $out | Out-Null
[ordered]@{
    session    = $Session
    presentmon = (Split-Path $PresentMon -Leaf)
    seconds    = $Seconds
    hotkey     = $Hotkey
    started    = (Get-Date).ToString('o')
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $out 'session.json') -Encoding utf8

Write-Host "PresentMon: $PresentMon"
Write-Host "Output:     $out"
Write-Host "In game, press $Hotkey to record $Seconds s; every press makes a new CSV."
Write-Host 'Close this window when you are done.'
# --date_time: wall-clock start times, to line captures up with the mod's
# menu log (mhw_dlss.log). --track_pc_latency: Reflex's PC Latency events.
& $PresentMon --process_name MonsterHunterWorld.exe --output_file (Join-Path $out 'capture.csv') `
    --hotkey $Hotkey --timed $Seconds --date_time --v2_metrics --track_pc_latency `
    --session_name MHWPerf --stop_existing_session

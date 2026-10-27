<#
.SYNOPSIS
Records Monster Hunter: World frame timing with the Intel PresentMon console
application, for the performance check.

.DESCRIPTION
Start it before or after the game. It records every frame of every
MonsterHunterWorld.exe it sees, continuously, into one CSV per game launch
under local\perf\<session>\ (the file names end in the process ID). Press
Ctrl+C in its window when done, then run tools\perf\presentmon_summary.py on
the folder: it cuts the captures into time slices, or into the settings the
mod's menu log records.

There is no hotkey: MHW keeps other programs' hotkeys from arriving while it
has focus (2026-10-13: PresentMon saw the game, Alt+Shift+F11 did nothing).

PresentMon reads Windows ETW events and needs administrator rights, so the
script asks once (UAC) and continues in an elevated window.

The console application is taken from local\tools\presentmon-*\ (a copy of
the GitHub release) or from the Intel PresentMon install, newest version
first. Pass -PresentMon to choose one.

.EXAMPLE
pwsh -File tools\perf\presentmon_capture.ps1
pwsh -File tools\perf\presentmon_capture.ps1 -Session baseline
#>
param(
    [string]$Session = (Get-Date -Format 'yyyyMMdd-HHmmss'),
    [string]$PresentMon = '',
    # QPC start times instead of wall-clock ones, to line frames up with the
    # mod's log (its frame heartbeats carry a QPC/tick clock pair; 0.32.0).
    [switch]$QpcTime
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
        '-PresentMon', "`"$PresentMon`"")
    if ($QpcTime) { $relaunch += '-QpcTime' }
    Start-Process -FilePath $shell -Verb RunAs -ArgumentList $relaunch
    Write-Host "Continuing in an elevated window (session $Session)."
    return
}

$out = Join-Path $repo "local\perf\$Session"
New-Item -ItemType Directory -Force -Path $out | Out-Null
[ordered]@{
    session    = $Session
    presentmon = (Split-Path $PresentMon -Leaf)
    recording  = 'continuous, one CSV per process'
    time       = if ($QpcTime) { 'qpc' } else { 'date_time' }
    started    = (Get-Date).ToString('o')
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $out 'session.json') -Encoding utf8

Write-Host "PresentMon: $PresentMon"
Write-Host "Output:     $out"
Write-Host 'Recording now, continuously: one CSV per game launch.'
Write-Host 'Press Ctrl+C in this window when you are done.'
# No --hotkey: recording starts at once and runs until Ctrl+C. --multi_csv:
# one file per process, so each launch has its own. --date_time: local
# wall-clock start times, to line captures up with the mod's menu log
# (mhw_dlss.log); -QpcTime: --qpc_time instead. --track_pc_latency: Reflex's
# PC Latency events.
$time = if ($QpcTime) { '--qpc_time' } else { '--date_time' }
& $PresentMon --process_name MonsterHunterWorld.exe --output_file (Join-Path $out 'capture.csv') --multi_csv `
    $time --v2_metrics --track_pc_latency --session_name MHWPerf --stop_existing_session

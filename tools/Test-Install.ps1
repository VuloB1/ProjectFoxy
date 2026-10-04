<#
.SYNOPSIS
    Installs Project Foxy into a scratch folder and proves that folder is self-contained.

.DESCRIPTION
    `cmake --install` is what produces the folder that gets shipped. This script checks it:

      1. installs the already-built project into -Prefix (wiped first, so it must be a scratch path);
      2. adds Qt's "offscreen" platform plugin to the copy (only so it can run without a window);
      3. starts the INSTALLED ProjectFoxy.exe with a PATH reduced to the Windows folders, so a
         DLL that was forgotten in the install (and merely found on this machine's PATH, in the
         vcpkg tree or in the Qt SDK) makes the program fail to start;
      4. fails if it dies or writes anything to its error output (same rule as the smoke_startup
         test, see tests/smoke_startup.cmake).

    It also reports any DLL in the installed folder that the build tree does not have, and any
    GPL component that must never be shipped (libx265).

.PARAMETER Build
    Build directory (default: C:\build\imageviewer\release).

.PARAMETER Prefix
    Scratch install folder (default: C:\build\imageviewer\dist_test). Deleted and recreated.
#>
param(
    [string]$Build  = 'C:\build\imageviewer\release',
    [string]$Prefix = 'C:\build\imageviewer\dist_test'
)
$ErrorActionPreference = 'Stop'

$root  = Split-Path -Parent $PSScriptRoot
$cmake = (Get-Command cmake).Source

# The prefix is deleted: refuse anything that is not clearly a scratch folder.
if ($Prefix -notmatch 'dist|install|scratch|tmp|temp') { throw "-Prefix '$Prefix' does not look like a scratch folder." }
if (Test-Path -LiteralPath $Prefix) { Remove-Item -LiteralPath $Prefix -Recurse -Force }

Write-Host "== cmake --install -> $Prefix"
& $cmake --install $Build --prefix $Prefix | Out-Null
if ($LASTEXITCODE -ne 0) { throw "cmake --install failed ($LASTEXITCODE)" }

$exe = Join-Path $Prefix 'ProjectFoxy.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw "ProjectFoxy.exe is not in $Prefix" }

Write-Host '== content checks'
if (Test-Path -LiteralPath (Join-Path $Prefix 'libx265.dll')) { throw 'libx265.dll (GPL) is in the installed folder' }
foreach ($need in 'qml\QtQuick\Effects\effectsplugin.dll', 'Qt6QuickEffects.dll', 'plugins\platforms\qwindows.dll',
                  'licenses\THIRD_PARTY_NOTICES.txt', 'vips-42.dll', 'exiv2.dll', 'heif.dll', 'libde265.dll', 'raw_r.dll') {
    if (-not (Test-Path -LiteralPath (Join-Path $Prefix $need))) { throw "missing in the installed folder: $need" }
}
$dllsHere  = Get-ChildItem -LiteralPath $Prefix -Filter *.dll -File | ForEach-Object Name
$dllsBuild = Get-ChildItem -LiteralPath $Build  -Filter *.dll -File | ForEach-Object Name
$extra = $dllsHere | Where-Object { $dllsBuild -notcontains $_ }
$gone  = $dllsBuild | Where-Object { $dllsHere -notcontains $_ }
if ($extra) { Write-Host ('   only in the install (expected: the VC runtime): ' + ($extra -join ', ')) }
if ($gone)  { Write-Host ('   only in the build tree (check them!): ' + ($gone -join ', ')) }

# Qt's offscreen platform plugin is not deployed with the program; borrow it for the test only.
$qtDir = (Select-String -Path (Join-Path $Build 'CMakeCache.txt') -Pattern '^Qt6_DIR:PATH=(.+)$').Matches[0].Groups[1].Value
$qtPlugins = [System.IO.Path]::GetFullPath((Join-Path $qtDir '..\..\..\plugins'))
Copy-Item -LiteralPath (Join-Path $qtPlugins 'platforms\qoffscreen.dll') -Destination (Join-Path $Prefix 'plugins\platforms') -Force

Write-Host '== starting the installed program with a minimal PATH'
$cleanPath = "$env:SystemRoot\System32;$env:SystemRoot"
$env:PATH = $cleanPath
& $cmake "-DEXE=$exe" "-DIMAGE=$root\tests\fixtures\exif_orient_6.jpg" "-DQT_PLUGINS=$Prefix\plugins" `
    -P (Join-Path $root 'tests\smoke_startup.cmake')
if ($LASTEXITCODE -ne 0) { throw 'the installed program did not start cleanly' }
Write-Host 'OK: the installed folder is self-contained.'

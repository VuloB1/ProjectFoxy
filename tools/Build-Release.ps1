<#
.SYNOPSIS
    Builds Project Foxy and produces what gets published: the installer and the portable .zip.

.DESCRIPTION
    1. configures and builds the preset (and runs the tests, unless -SkipTests);
    2. `cmake --install` into dist\ProjectFoxy (the complete folder: the program, Qt, the libraries, the licences);
    3. dist\ProjectFoxy-portable-<version>.zip: that folder plus portable.txt (so the program keeps its settings
       beside itself) and a LEEME.txt;
    4. dist\installer\ProjectFoxy-Setup-<version>.exe with Inno Setup (installer\ProjectFoxy.iss), when ISCC.exe is
       installed (https://jrsoftware.org/isinfo.php); without it the step is skipped and says so;
    5. dist\SHA256SUMS.txt with the checksum of each file, to paste in the release notes.

    Run it from a "x64 Native Tools Command Prompt for VS 2022" (or any shell where cl.exe and cmake are on the PATH);
    if cl.exe is not found it tries to load Visual Studio's environment itself.

.PARAMETER Version
    The version number written in the file names and in the installer (default: the one in CMakeLists.txt).

.PARAMETER Preset
    CMake preset to build: `windows-release` (needs VCPKG_ROOT and Qt in CMAKE_PREFIX_PATH) or a `local-release` of
    your own CMakeUserPresets.json.

.EXAMPLE
    .\tools\Build-Release.ps1 -Preset local-release
#>
param(
    [string]$Version = "",
    [string]$Preset = "windows-release",
    [switch]$SkipTests,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

# ---- the version
$cmakeVersion = ([regex]::Match((Get-Content "$root\CMakeLists.txt" -Raw), 'project\(\w+\s+VERSION\s+([\d.]+)')).Groups[1].Value
if (-not $Version) { $Version = $cmakeVersion }
if ($Version -ne $cmakeVersion) {
    Write-Warning "The version asked for ($Version) is not the one in CMakeLists.txt ($cmakeVersion): the program itself still says $cmakeVersion."
}
Write-Host "Project Foxy $Version" -ForegroundColor Cyan

# ---- the compiler's environment
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { throw "cl.exe is not on the PATH and Visual Studio was not found. Use an x64 Native Tools prompt." }
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    $bat = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"
    Write-Host "Loading $bat"
    cmd /c "`"$bat`" >nul 2>nul && set" | ForEach-Object {
        if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Path "env:$($Matches[1])" -Value $Matches[2] }
    }
}

# ---- where the preset builds
function Get-BinaryDir([string]$name) {
    foreach ($file in "CMakeUserPresets.json", "CMakePresets.json") {
        $path = Join-Path $root $file
        if (-not (Test-Path $path)) { continue }
        $json = Get-Content $path -Raw | ConvertFrom-Json
        $p = $json.configurePresets | Where-Object { $_.name -eq $name } | Select-Object -First 1
        if (-not $p) { continue }
        if ($p.binaryDir) { return ($p.binaryDir -replace '\$\{sourceDir\}', $root.Replace('\', '/')) }
        if ($p.inherits) { return Get-BinaryDir ($p.inherits | Select-Object -First 1) }
    }
    throw "Preset '$name' not found (or it has no binaryDir)."
}
$build = Get-BinaryDir $Preset

if (-not $SkipBuild) {
    cmake --preset $Preset; if ($LASTEXITCODE) { throw "configure failed" }
    cmake --build --preset $Preset; if ($LASTEXITCODE) { throw "build failed" }
}
if (-not $SkipTests) {
    ctest --preset $Preset; if ($LASTEXITCODE) { throw "tests failed" }
}

# ---- the folder that is shipped
$dist = Join-Path $root "dist"
$app = Join-Path $dist "ProjectFoxy"
if (Test-Path $app) { Remove-Item $app -Recurse -Force }
New-Item -ItemType Directory -Force $dist | Out-Null
cmake --install $build --prefix $app; if ($LASTEXITCODE) { throw "install failed" }
if (-not (Test-Path "$app\ProjectFoxy.exe")) { throw "dist\ProjectFoxy\ProjectFoxy.exe is not there" }

# ---- the portable zip
$stage = Join-Path $dist "portable-stage"
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
Copy-Item $app "$stage\ProjectFoxy" -Recurse
Copy-Item "$root\installer\portable.txt" "$stage\ProjectFoxy\portable.txt"
Copy-Item "$root\installer\LEEME-portable.txt" "$stage\ProjectFoxy\LEEME.txt"
$zip = Join-Path $dist "ProjectFoxy-portable-$Version.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path "$stage\ProjectFoxy" -DestinationPath $zip -CompressionLevel Optimal
Remove-Item $stage -Recurse -Force
Write-Host "Portable: $zip" -ForegroundColor Green

# ---- the installer
$iscc = (Get-Command ISCC.exe -ErrorAction SilentlyContinue).Source
if (-not $iscc) {
    foreach ($c in "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe", "$env:ProgramFiles\Inno Setup 6\ISCC.exe", "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe") {
        if (Test-Path $c) { $iscc = $c; break }
    }
}
$setup = Join-Path $dist "installer\ProjectFoxy-Setup-$Version.exe"
if ($iscc) {
    & $iscc "/DAppVersion=$Version" "$root\installer\ProjectFoxy.iss"; if ($LASTEXITCODE) { throw "Inno Setup failed" }
    Write-Host "Installer: $setup" -ForegroundColor Green
} else {
    Write-Warning "Inno Setup (ISCC.exe) is not installed: the installer was NOT built. Install it from https://jrsoftware.org/isdl.php (or: winget install JRSoftware.InnoSetup)."
}

# ---- checksums
$sums = @()
foreach ($f in @($zip, $setup)) {
    if (Test-Path $f) { $sums += "{0}  {1}" -f (Get-FileHash $f -Algorithm SHA256).Hash.ToLower(), (Split-Path $f -Leaf) }
}
$sums | Set-Content (Join-Path $dist "SHA256SUMS.txt") -Encoding ascii
$sums | ForEach-Object { Write-Host $_ }

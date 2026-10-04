# Builds docs\THIRD_PARTY_NOTICES.txt from the licence files vcpkg keeps for every installed port,
# for the libraries whose DLLs ship next to ProjectFoxy.exe. Run it after a build:
#   tools\make_third_party_notices.ps1 -Installed C:\build\imageviewer\release\vcpkg_installed\x64-windows
param(
    [string]$Installed = 'C:\build\imageviewer\release\vcpkg_installed\x64-windows',
    [string]$Out = (Join-Path $PSScriptRoot '..\docs\THIRD_PARTY_NOTICES.txt')
)
$inst = $Installed
$status = Get-Content "$inst\..\vcpkg\status" -Raw -Encoding UTF8
$versions = @{}
foreach ($blk in ($status -split "`r?`n`r?`n")) {
    if ($blk -match '(?m)^Package: (\S+)' -and $blk -notmatch '(?m)^Feature: ') {
        $n = $Matches[1]; $v = ''
        if ($blk -match '(?m)^Version: (\S+)') { $v = $Matches[1] }
        $versions[$n] = $v
    }
}
# port -> (what it is, SPDX licence declared by the vcpkg port)
$ports = [ordered]@{
    'libvips'       = 'libvips - image decoding/encoding (JPEG, PNG, WebP, TIFF)'
    'libraw'        = 'LibRaw - camera RAW decoding'
    'libheif'       = 'libheif - HEIC/HEIF/AVIF container'
    'libde265'      = 'libde265 - HEVC (H.265) decoder used by libheif'
    'aom'           = 'libaom - AV1 codec (AVIF)'
    'exiv2'         = 'Exiv2 - EXIF/IPTC/XMP metadata'
    'glib'          = 'GLib/GObject/GIO - base library of libvips'
    'gettext-libintl' = 'GNU libintl - GLib dependency'
    'libiconv'      = 'GNU libiconv - GLib dependency'
    'libffi'        = 'libffi - GLib dependency'
    'pcre2'         = 'PCRE2 - GLib dependency'
    'expat'         = 'Expat - XML parsing (Exiv2, libvips)'
    'lcms'          = 'Little CMS - colour management (LibRaw, libvips)'
    'libjpeg-turbo' = 'libjpeg-turbo - JPEG'
    'libpng'        = 'libpng - PNG'
    'libwebp'       = 'libwebp - WebP'
    'tiff'          = 'libtiff - TIFF'
    'liblzma'       = 'liblzma (XZ Utils) - compression'
    'zlib'          = 'zlib - compression'
}
$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine('Project Foxy - third-party software notices')
[void]$sb.AppendLine('==========================================')
[void]$sb.AppendLine('')
[void]$sb.AppendLine('Project Foxy uses the open-source components listed below. Each keeps its own licence;')
[void]$sb.AppendLine('the licence/copyright text of every one follows, exactly as vcpkg installed it.')
[void]$sb.AppendLine('The Qt framework (LGPLv3) is used as dynamic libraries; its licence texts and source')
[void]$sb.AppendLine('offer are available from https://www.qt.io/licensing/ and must accompany any distribution.')
[void]$sb.AppendLine('')
foreach ($p in $ports.Keys) {
    $cf = "$inst\share\$p\copyright"
    $ver = $versions[$p]
    [void]$sb.AppendLine(('=' * 78))
    [void]$sb.AppendLine("$($ports[$p])")
    [void]$sb.AppendLine("vcpkg port: $p $ver")
    [void]$sb.AppendLine(('=' * 78))
    if (Test-Path -LiteralPath $cf) {
        [void]$sb.AppendLine(([IO.File]::ReadAllText($cf)).Replace("`r`n", "`n").TrimEnd())
    } else {
        [void]$sb.AppendLine('(no licence file found in the vcpkg install tree)')
    }
    [void]$sb.AppendLine('')
}
$out = [IO.Path]::GetFullPath($Out)
[IO.File]::WriteAllText($out, $sb.ToString(), (New-Object System.Text.UTF8Encoding($false)))
"{0}: {1:N0} KB, {2} components" -f $out, ((Get-Item $out).Length / 1KB), $ports.Count

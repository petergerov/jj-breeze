<#
.SYNOPSIS
    Find the AAX SDK for this checkout, unpacking it on first use, and write
    its path to the pipeline.

.DESCRIPTION
    The Windows counterpart to scripts/aax-sdk.sh.

    AAX is Avid's Pro Tools plugin format. Its SDK may not be redistributed,
    so nothing of it is vendored here: you download aax-sdk-<version>.zip from
    your Avid developer account and drop it into AAX\. Everything AAX in this
    repo is conditional on that — a checkout without the SDK builds VST3 and
    Standalone exactly as it did before.

    Writes the path to the pipeline and exits 0 when an SDK is available;
    writes nothing and exits 1 when there is none. Progress goes to the
    information stream, so `$sdk = & scripts\aax-sdk.ps1` gets just the path.

.PARAMETER SdkPath
    Use an SDK sitting somewhere else entirely and skip the search. Falls back
    to AAX_SDK_PATH from the environment. Being set but wrong is an error, not
    a silent skip: it was asked for explicitly.

.PARAMETER Quiet
    Suppress the "unpacking..." progress note.
#>
[CmdletBinding()]
param(
    [string]$SdkPath = $env:AAX_SDK_PATH,
    [switch]$Quiet
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Write-Note {
    param([string]$Message)
    if (-not $Quiet) { Write-Host $Message }
}

# What JUCE's juce_set_aax_sdk_path() itself checks for. Testing the same
# thing here means a half-unpacked or wrong-layout directory is rejected here,
# with a sentence about it, rather than by CMake with a FATAL_ERROR in the
# middle of a release build.
function Test-AaxSdk {
    param([string]$Path)
    return (-not [string]::IsNullOrEmpty($Path)) -and (Test-Path (Join-Path $Path 'Interfaces\ACF'))
}

# Sorts aax-sdk-2-10-0 after aax-sdk-2-9-0, which a plain name sort would not:
# the version is dash-separated, so compare it component by component.
function Get-Newest {
    param([System.IO.FileSystemInfo[]]$Items)
    return $Items | Sort-Object -Property @{ Expression = {
        $digits = [regex]::Matches($_.Name, '\d+') | ForEach-Object { [int]$_.Value }
        # Pad so the comparison is numeric per component, not lexicographic.
        ($digits | ForEach-Object { $_.ToString('D10') }) -join '.'
    } } | Select-Object -Last 1
}

$RootDir = Split-Path -Parent $PSScriptRoot
$AaxDir = Join-Path $RootDir 'AAX'

if (-not [string]::IsNullOrEmpty($SdkPath)) {
    if (-not (Test-AaxSdk $SdkPath)) {
        Write-Error "AAX_SDK_PATH is set to '$SdkPath', which has no Interfaces\ACF — that is not an unpacked AAX SDK"
        exit 1
    }
    (Resolve-Path $SdkPath).Path
    exit 0
}

if (-not (Test-Path $AaxDir)) {
    Write-Note "note: no AAX SDK found (no $AaxDir directory)"
    exit 1
}

# Already unpacked? Newest version wins, so dropping a 2.10 zip beside a 2.9
# one and re-running picks up the new SDK instead of the old directory.
$unpacked = @(Get-ChildItem -Path $AaxDir -Directory -Filter 'aax-sdk-*' -ErrorAction SilentlyContinue |
    Where-Object { Test-AaxSdk $_.FullName })
if ($unpacked.Count -gt 0) {
    (Get-Newest $unpacked).FullName
    exit 0
}

$zips = @(Get-ChildItem -Path $AaxDir -File -Filter 'aax-sdk-*.zip' -ErrorAction SilentlyContinue)
if ($zips.Count -eq 0) {
    Write-Note 'note: no AAX SDK found (looked for AAX\aax-sdk-*.zip and AAX\aax-sdk-*\)'
    exit 1
}

$zip = Get-Newest $zips
$dest = Join-Path $AaxDir ([System.IO.Path]::GetFileNameWithoutExtension($zip.Name))

Write-Note "==> Unpacking $($zip.Name) (first use; ~100 MB)"

# Unpack beside the destination and move into place only once it is complete,
# so an interrupted run can't leave a half-extracted directory that the check
# above would then happily accept on the next run.
# Kept short on purpose: the SDK's Doxygen output nests deeply, and every
# character here is one closer to the 260-character path limit that
# Expand-Archive still trips over on a machine without long paths enabled.
$tmpDir = Join-Path $AaxDir '.unpack'
try {
    if (Test-Path $tmpDir) { Remove-Item -Recurse -Force $tmpDir }
    New-Item -ItemType Directory -Path $tmpDir -Force | Out-Null
    Expand-Archive -LiteralPath $zip.FullName -DestinationPath $tmpDir -Force

    # Avid's zip wraps everything in a single aax-sdk-<version>\ directory,
    # but don't bank on it: accept a zip that holds the SDK at its root too.
    $src = $null
    if (Test-AaxSdk $tmpDir) {
        $src = $tmpDir
    } else {
        $src = Get-ChildItem -Path $tmpDir -Directory |
            Where-Object { Test-AaxSdk $_.FullName } |
            Select-Object -First 1 -ExpandProperty FullName
    }

    if (-not $src) {
        Write-Error "$($zip.FullName) does not look like an AAX SDK (no Interfaces\ACF inside)"
        exit 1
    }

    if (Test-Path $dest) { Remove-Item -Recurse -Force $dest }
    Move-Item -LiteralPath $src -Destination $dest
}
finally {
    if (Test-Path $tmpDir) { Remove-Item -Recurse -Force $tmpDir }
}

Write-Note "==> AAX SDK ready: $dest"
$dest

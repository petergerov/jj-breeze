<#
.SYNOPSIS
    Build jj-breeze (VST3 + Standalone) in Release and assemble a signed
    Windows .exe installer under dist\.

.DESCRIPTION
    The Windows counterpart to scripts/dist-macos-pkg.sh. Builds with CMake,
    optionally code-signs the binaries, then compiles
    installer/windows/jj-breeze.iss with Inno Setup into:

        VST3            -> C:\Program Files\Common Files\VST3
        Standalone app  -> C:\Program Files\Gerov\<product>  (+ Start menu entry)

    Inno Setup 6.3+ must be installed (it is preinstalled on GitHub's
    windows-2022 runners). Download: https://jrsoftware.org/isdl.php

.PARAMETER Clean
    Remove build\ first, forcing a full rebuild.

.PARAMETER CertPath
    Path to a .pfx code-signing certificate. When omitted (the default), the
    binaries and the installer are left unsigned — SmartScreen will warn on
    other machines, but the installer works. Defaults to $env:WINDOWS_CERT_PFX.

.PARAMETER CertPassword
    Password for the .pfx. Defaults to $env:WINDOWS_CERT_PASSWORD.

.PARAMETER TimestampUrl
    RFC 3161 timestamp server. Timestamping is what keeps a signature valid
    after the certificate itself expires, so it is on by default.

.EXAMPLE
    scripts\dist-windows.ps1

.EXAMPLE
    scripts\dist-windows.ps1 -Clean -CertPath C:\certs\gerov.pfx -CertPassword hunter2
#>
[CmdletBinding()]
param(
    [switch]$Clean,
    [string]$CertPath = $env:WINDOWS_CERT_PFX,
    [string]$CertPassword = $env:WINDOWS_CERT_PASSWORD,
    [string]$TimestampUrl = 'http://timestamp.digicert.com'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Native tools signal failure through $LASTEXITCODE rather than by throwing, so
# every one of them below is followed by this.
function Assert-ExitCode {
    param([string]$What)
    if ($LASTEXITCODE -ne 0) {
        throw "$What failed with exit code $LASTEXITCODE"
    }
}

$RootDir = Split-Path -Parent $PSScriptRoot
Push-Location $RootDir
try {
    $BuildDir = 'build'
    $DistDir = 'dist'

    if ($Clean -and (Test-Path $BuildDir)) {
        Write-Host "==> Removing existing $BuildDir"
        Remove-Item -Recurse -Force $BuildDir
    }

    # Version and product name are read out of CMakeLists.txt rather than
    # duplicated here, for the same reason the macOS scripts do it: a rename or
    # version bump there must not leave this script packaging a stale path.
    $cmakeLists = Get-Content -Raw 'CMakeLists.txt'

    $versionMatch = [regex]::Match($cmakeLists, 'project\(jj-breeze VERSION (\d+\.\d+\.\d+)')
    if (-not $versionMatch.Success) {
        throw 'could not determine version from CMakeLists.txt'
    }
    $Version = $versionMatch.Groups[1].Value

    $nameMatch = [regex]::Match($cmakeLists, 'PRODUCT_NAME "([^"]*)"')
    if (-not $nameMatch.Success) {
        throw 'could not determine PRODUCT_NAME from CMakeLists.txt'
    }
    $ProductName = $nameMatch.Groups[1].Value

    Write-Host "==> Building $ProductName $Version (Release)"
    # COPY_PLUGIN_AFTER_BUILD off: that post-build step writes into
    # C:\Program Files\Common Files\VST3, which needs elevation. The installer
    # this script produces is what puts the plugin there instead.
    cmake -B $BuildDir -G 'Visual Studio 17 2022' -A x64 -DJJ_BREEZE_COPY_PLUGIN_AFTER_BUILD=OFF
    Assert-ExitCode 'cmake configure'
    cmake --build $BuildDir --config Release --parallel
    Assert-ExitCode 'cmake build'

    $ArtefactsDir = Join-Path $RootDir "$BuildDir\jj_breeze_artefacts\Release"
    $StandalonePath = Join-Path $ArtefactsDir "Standalone\$ProductName.exe"
    # The VST3 "file" is a bundle directory on Windows; the actual DLL that
    # needs signing lives at Contents\x86_64-win\ inside it.
    $Vst3BundlePath = Join-Path $ArtefactsDir "VST3\$ProductName.vst3"
    $Vst3BinaryPath = Join-Path $Vst3BundlePath "Contents\x86_64-win\$ProductName.vst3"

    foreach ($path in @($StandalonePath, $Vst3BundlePath, $Vst3BinaryPath)) {
        if (-not (Test-Path $path)) {
            throw "expected build artefact not found: $path"
        }
    }

    if ([string]::IsNullOrEmpty($CertPath)) {
        Write-Host '==> No signing certificate given, leaving binaries unsigned (SmartScreen will warn on other machines)'
        $SignTool = $null
    } else {
        if (-not (Test-Path $CertPath)) {
            throw "signing certificate not found: $CertPath"
        }

        # signtool ships with the Windows SDK and is not on PATH by default;
        # several SDK versions are usually installed side by side, so take the
        # newest x64 one.
        $SignTool = Get-ChildItem -Path "${env:ProgramFiles(x86)}\Windows Kits\10\bin" `
            -Filter 'signtool.exe' -Recurse -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match '\\x64\\' } |
            Sort-Object -Property FullName -Descending |
            Select-Object -First 1 -ExpandProperty FullName

        if (-not $SignTool) {
            throw 'signing was requested but signtool.exe could not be found (install the Windows SDK)'
        }

        Write-Host "==> Signing binaries with $CertPath"
        & $SignTool sign /f $CertPath /p $CertPassword /fd SHA256 `
            /tr $TimestampUrl /td SHA256 $StandalonePath $Vst3BinaryPath
        Assert-ExitCode 'signtool sign (binaries)'
    }

    $iscc = (Get-Command 'ISCC.exe' -ErrorAction SilentlyContinue).Source
    if (-not $iscc) {
        $iscc = @(
            "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
            "$env:ProgramFiles\Inno Setup 6\ISCC.exe"
        ) | Where-Object { Test-Path $_ } | Select-Object -First 1
    }
    if (-not $iscc) {
        throw 'ISCC.exe (Inno Setup 6) not found — install it from https://jrsoftware.org/isdl.php'
    }

    New-Item -ItemType Directory -Force -Path $DistDir | Out-Null
    $OutputDir = Join-Path $RootDir $DistDir

    Write-Host '==> Building installer with Inno Setup'
    & $iscc `
        "/DProductName=$ProductName" `
        "/DAppVersion=$Version" `
        "/DArtefactsDir=$ArtefactsDir" `
        "/DOutputDir=$OutputDir" `
        'installer\windows\jj-breeze.iss'
    Assert-ExitCode 'ISCC'

    $InstallerPath = Join-Path $OutputDir "jj-breeze-$Version-windows.exe"
    if (-not (Test-Path $InstallerPath)) {
        throw "Inno Setup reported success but $InstallerPath is missing"
    }

    # The installer is a separate executable from the ones signed above, so it
    # needs its own signature — an unsigned installer wrapping signed payloads
    # still trips SmartScreen at download time.
    if ($SignTool) {
        Write-Host '==> Signing installer'
        & $SignTool sign /f $CertPath /p $CertPassword /fd SHA256 `
            /tr $TimestampUrl /td SHA256 $InstallerPath
        Assert-ExitCode 'signtool sign (installer)'
    }

    Write-Host "==> Done: $InstallerPath"
}
finally {
    Pop-Location
}

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
        AAX             -> C:\Program Files\Common Files\Avid\Audio\Plug-Ins

    The AAX component is built and packaged only when the AAX SDK is available
    (see scripts\aax-sdk.ps1); without it this produces the same VST3 +
    Standalone installer it always did. Note that Pro Tools loads an AAX
    plugin only once it has been PACE-signed with Avid's wraptool, which this
    script does not do - see the AAX section of RELEASE.md.

    Inno Setup 6.3+ must be installed (it is preinstalled on GitHub's
    windows-2022 runners). Download: https://jrsoftware.org/isdl.php

.PARAMETER Clean
    Remove build\ first, forcing a full rebuild.

.PARAMETER NoAax
    Skip the AAX build even if the SDK is there.

.PARAMETER CertPath
    Path to a .pfx code-signing certificate. When omitted (the default), the
    binaries and the installer are left unsigned — SmartScreen will warn on
    other machines, but the installer works. Falls back to WINDOWS_CERT_PFX
    from the environment or from scripts\.env (gitignored; see RELEASE.md).

.PARAMETER CertPassword
    Password for the .pfx. Falls back to WINDOWS_CERT_PASSWORD the same way.

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
    [switch]$NoAax,
    [string]$CertPath = $env:WINDOWS_CERT_PFX,
    [string]$CertPassword = $env:WINDOWS_CERT_PASSWORD,
    [string]$TimestampUrl = 'http://timestamp.digicert.com'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Read scripts\.env (gitignored; see RELEASE.md) so credentials live in one
# place for both platforms. A variable already present in the environment is
# left alone, so CI — which sets real environment variables and ships no .env
# — always wins. Parsing is deliberately forgiving: a malformed line is
# skipped rather than being allowed to fail the build.
function Import-DotEnv {
    param([string]$Path)
    if (-not (Test-Path $Path)) { return }
    foreach ($line in Get-Content -LiteralPath $Path) {
        $trimmed = $line.Trim()
        if ($trimmed -eq '' -or $trimmed.StartsWith('#')) { continue }
        $split = $trimmed.IndexOf('=')
        if ($split -lt 1) { continue }
        $key = $trimmed.Substring(0, $split).Trim() -replace '^export\s+', ''
        if ($key -notmatch '^[A-Za-z_][A-Za-z0-9_]*$') { continue }
        # Already set in the environment: leave it.
        if (Test-Path "env:$key") { continue }
        $value = $trimmed.Substring($split + 1).Trim()
        if ($value.Length -ge 2 -and
            (($value.StartsWith('"') -and $value.EndsWith('"')) -or
             ($value.StartsWith("'") -and $value.EndsWith("'")))) {
            $value = $value.Substring(1, $value.Length - 2)
        }
        Set-Item -Path "env:$key" -Value $value
    }
}
Import-DotEnv (Join-Path $PSScriptRoot '.env')

# Re-read the parameter defaults now that .env has been applied: param() is
# evaluated before any of the body runs, so an unbound -CertPath still held
# whatever $env: looked like a moment ago (usually nothing).
if (-not $PSBoundParameters.ContainsKey('CertPath')) { $CertPath = $env:WINDOWS_CERT_PFX }
if (-not $PSBoundParameters.ContainsKey('CertPassword')) { $CertPassword = $env:WINDOWS_CERT_PASSWORD }

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

    # An absent SDK is the normal case, not a failure: aax-sdk.ps1 exits 1 and
    # the installer is built without the AAX component, exactly as before. Only
    # an SDK that is present but broken stops us, with an explicit error from
    # that script rather than a FATAL_ERROR out of CMake.
    $AaxSdk = ''
    if (-not $NoAax) {
        # Seeded because Set-StrictMode makes reading an as-yet-unset
        # $LASTEXITCODE an error, and no native command has run at this point.
        $global:LASTEXITCODE = 0
        $sdkOutput = & (Join-Path $PSScriptRoot 'aax-sdk.ps1')
        if ($LASTEXITCODE -eq 0 -and $sdkOutput) { $AaxSdk = "$sdkOutput".Trim() }
    }

    if ($AaxSdk) {
        Write-Host "==> Building $ProductName $Version (Release, with AAX)"
    } else {
        Write-Host "==> Building $ProductName $Version (Release)"
    }
    # COPY_PLUGIN_AFTER_BUILD off: that post-build step writes into
    # C:\Program Files\Common Files\VST3, which needs elevation. The installer
    # this script produces is what puts the plugin there instead.
    cmake -B $BuildDir -G 'Visual Studio 17 2022' -A x64 -DJJ_BREEZE_COPY_PLUGIN_AFTER_BUILD=OFF `
        "-DJJ_BREEZE_AAX_SDK_PATH=$AaxSdk"
    Assert-ExitCode 'cmake configure'
    cmake --build $BuildDir --config Release --parallel
    Assert-ExitCode 'cmake build'

    $ArtefactsDir = Join-Path $RootDir "$BuildDir\jj_breeze_artefacts\Release"
    $StandalonePath = Join-Path $ArtefactsDir "Standalone\$ProductName.exe"
    # The VST3 "file" is a bundle directory on Windows; the actual DLL that
    # needs signing lives at Contents\x86_64-win\ inside it.
    $Vst3BundlePath = Join-Path $ArtefactsDir "VST3\$ProductName.vst3"
    $Vst3BinaryPath = Join-Path $Vst3BundlePath "Contents\x86_64-win\$ProductName.vst3"

    # Like the VST3, the AAX "file" is a bundle directory; its binary sits at
    # Contents\x64\ inside it and is what signtool has to be pointed at.
    $AaxBundlePath = Join-Path $ArtefactsDir "AAX\$ProductName.aaxplugin"
    $AaxBinaryPath = Join-Path $AaxBundlePath "Contents\x64\$ProductName.aaxplugin"

    $expected = @($StandalonePath, $Vst3BundlePath, $Vst3BinaryPath)
    if ($AaxSdk) { $expected += @($AaxBundlePath, $AaxBinaryPath) }

    foreach ($path in $expected) {
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

        $ToSign = @($StandalonePath, $Vst3BinaryPath)
        if ($AaxSdk) { $ToSign += $AaxBinaryPath }

        Write-Host "==> Signing binaries with $CertPath"
        & $SignTool sign /f $CertPath /p $CertPassword /fd SHA256 `
            /tr $TimestampUrl /td SHA256 @ToSign
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

    # /DWithAax is what switches the AAX component on in the .iss; leaving it
    # undefined (the no-SDK case) is what keeps the installer unchanged.
    $isccArgs = @(
        "/DProductName=$ProductName",
        "/DAppVersion=$Version",
        "/DArtefactsDir=$ArtefactsDir",
        "/DOutputDir=$OutputDir"
    )
    if ($AaxSdk) { $isccArgs += '/DWithAax' }
    $isccArgs += 'installer\windows\jj-breeze.iss'

    Write-Host '==> Building installer with Inno Setup'
    & $iscc @isccArgs
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
    if ($AaxSdk) {
        Write-Host '    note: the AAX component is not PACE-signed, so Pro Tools will'
        Write-Host '          refuse to load it - see the AAX section of RELEASE.md'
    }
}
finally {
    Pop-Location
}

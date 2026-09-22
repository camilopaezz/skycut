#Requires -RunAsAdministrator
# Install the asInvoker CameraCut bridge over vendor CameraCut.exe.
# Run once elevated. Daily use: CameraCut.exe and Corel as a normal user.

[CmdletBinding()]
param(
    [string]$InstallDir = "C:\Program Files\CameraCut",
    [Parameter(Mandatory = $true)]
    [string]$BridgeExe,
    [string]$RepoMch
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Test-IsAdmin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    $p = New-Object Security.Principal.WindowsPrincipal($id)
    return $p.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Backup-VendorFile {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) {
        return
    }
    $bak = "$Path.vendor.bak"
    if (Test-Path -LiteralPath $bak) {
        Write-Host "Keeping existing backup $bak"
        return
    }
    Copy-Item -LiteralPath $Path -Destination $bak
    Write-Host "Backed up $Path -> $bak"
}

function Find-Python {
    foreach ($name in @("python", "python3")) {
        $cmd = Get-Command $name -ErrorAction SilentlyContinue
        if ($null -eq $cmd) { continue }
        & $cmd.Source --version 2>&1 | Out-Null
        if ($LASTEXITCODE -eq 0) {
            return @{ File = $cmd.Source; Prefix = @() }
        }
    }
    $py = Get-Command py -ErrorAction SilentlyContinue
    if ($null -ne $py) {
        & $py.Source -3 --version 2>&1 | Out-Null
        if ($LASTEXITCODE -eq 0) {
            return @{ File = $py.Source; Prefix = @("-3") }
        }
    }
    return $null
}

function Find-PatchScript {
    $candidates = @(
        (Join-Path $PSScriptRoot "..\tools\patch_mch_manifest.py"),
        (Join-Path $PSScriptRoot "patch_mch_manifest.py")
    )
    foreach ($c in $candidates) {
        $full = [System.IO.Path]::GetFullPath($c)
        if (Test-Path -LiteralPath $full) {
            return $full
        }
    }
    return $null
}

function Resolve-RepoMch {
    param([string]$Source)
    if ([string]::IsNullOrWhiteSpace($Source)) {
        return $null
    }
    $full = [System.IO.Path]::GetFullPath($Source)
    if (-not (Test-Path -LiteralPath $full)) {
        throw "RepoMch not found: $Source"
    }
    if (Test-Path -LiteralPath $full -PathType Container) {
        $exe = Join-Path $full "CameraCutMch.exe"
        if (-not (Test-Path -LiteralPath $exe)) {
            throw "RepoMch directory has no CameraCutMch.exe: $full"
        }
        return $exe
    }
    return $full
}

if (-not (Test-IsAdmin)) {
    Write-Error "Administrator privileges required. Aborting."
    exit 1
}

if (-not (Test-Path -LiteralPath $BridgeExe)) {
    throw "BridgeExe not found: $BridgeExe"
}

$InstallDir = [System.IO.Path]::GetFullPath($InstallDir)
$BridgeExe = [System.IO.Path]::GetFullPath($BridgeExe)
$destExe = Join-Path $InstallDir "CameraCut.exe"
$destMch = Join-Path $InstallDir "CameraCutMch.exe"
# Vendor basename MUST be CameraCut.exe or Setup [SETUP] values stay zero.
# Keep it under engine\ so it does not replace the bridge CameraCut.exe.
$engineDir = Join-Path $InstallDir "engine"
$destCore = Join-Path $engineDir "CameraCut.exe"
$destCoreLegacy = Join-Path $InstallDir "CameraCutCore.exe"
$repoMchPath = Resolve-RepoMch $RepoMch

Write-Host "InstallDir=$InstallDir"
Write-Host "BridgeExe=$BridgeExe"
Write-Host "EngineCore=$destCore"

New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
New-Item -ItemType Directory -Force -Path $engineDir | Out-Null

Backup-VendorFile -Path $destExe
Backup-VendorFile -Path $destMch

$patchPy = Find-PatchScript
$py = Find-Python

function Invoke-PePatch {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Dest,
        [switch]$AlsoRunas
    )
    if ($null -eq $patchPy) {
        throw "patch_mch_manifest.py not found (expected ..\tools\ next to install)."
    }
    if ($null -eq $py) {
        throw "python not found; needed to patch PE manifests."
    }
    $tmp = "$Dest.patchtmp"
    Copy-Item -LiteralPath $Source -Destination $tmp -Force
    $pyArgs = @()
    $pyArgs += $py.Prefix
    $pyArgs += @($patchPy, "--in-place", $tmp)
    if ($AlsoRunas) {
        $pyArgs += "--also-patch-runas"
    }
    Write-Host ("Patching PE: {0} {1}" -f $py.File, ($pyArgs -join " "))
    & $py.File @pyArgs
    if ($LASTEXITCODE -ne 0) {
        Remove-Item -LiteralPath $tmp -Force -ErrorAction SilentlyContinue
        throw "patch_mch_manifest.py failed with exit $LASTEXITCODE for $Dest"
    }
    Move-Item -LiteralPath $tmp -Destination $Dest -Force
    Write-Host "Patched $Dest"
}

if ($null -ne $repoMchPath) {
    Copy-Item -LiteralPath $repoMchPath -Destination $destMch -Force
    Write-Host "Copied vendor Mch $repoMchPath -> $destMch"
    Backup-VendorFile -Path $destMch
}

# Vendor CameraCut (real cut engine) -> engine\CameraCut.exe, asInvoker, no runas
# Basename must stay CameraCut.exe or vendor skips loading [SETUP] from cfg.
$vendorCam = Join-Path $InstallDir "CameraCut.exe.vendor.bak"
if (-not (Test-Path -LiteralPath $vendorCam)) {
    $repoCam = $null
    if ($null -ne $RepoMch) {
        $repoRoot = [System.IO.Path]::GetFullPath($RepoMch)
        if (Test-Path -LiteralPath $repoRoot -PathType Container) {
            $cand = Join-Path $repoRoot "CameraCut.exe"
            if (Test-Path -LiteralPath $cand) { $repoCam = $cand }
        }
    }
    if ($null -ne $repoCam) {
        Copy-Item -LiteralPath $repoCam -Destination $vendorCam -Force
        Write-Host "Seeded vendor bak from $repoCam"
    }
}
if (Test-Path -LiteralPath $vendorCam) {
    Invoke-PePatch -Source $vendorCam -Dest $destCore -AlsoRunas
} else {
    Write-Warning "No CameraCut.exe.vendor.bak - engine\CameraCut.exe not installed. Cutting will not work until core is present."
}

# Drop legacy flat CameraCutCore.exe so it cannot be started by accident
if (Test-Path -LiteralPath $destCoreLegacy) {
    Remove-Item -LiteralPath $destCoreLegacy -Force
    Write-Host "Removed legacy $destCoreLegacy"
}

Copy-Item -LiteralPath $BridgeExe -Destination $destExe -Force
Write-Host "Installed bridge $BridgeExe -> $destExe"

if (Test-Path -LiteralPath $destMch) {
    Invoke-PePatch -Source $destMch -Dest $destMch
}

# BRIDGE defaults: use vendor core for real cuts (idempotent)
$cfgPath = Join-Path $InstallDir "CameraCut.cfg"
if (-not (Test-Path -LiteralPath $cfgPath)) {
    Set-Content -LiteralPath $cfgPath -Value "" -Encoding ASCII
}
$cfgText = Get-Content -LiteralPath $cfgPath -Raw -ErrorAction SilentlyContinue
if ($null -eq $cfgText) { $cfgText = "" }
if ($cfgText -notmatch '(?m)^\[BRIDGE\]') {
    $bridgeBlock = @(
        ""
        "[BRIDGE]"
        "UseVendorCore=1"
        "MchAutoStart=0"
    ) -join "`r`n"
    Add-Content -LiteralPath $cfgPath -Value $bridgeBlock -Encoding ASCII
    Write-Host "Appended [BRIDGE] to $cfgPath"
} else {
    Write-Host "Keeping existing [BRIDGE] in $cfgPath"
}

# Vendor reads cfg / default.fil next to its own EXE (engine\). Hardlink so one file.
function Install-SharedFile {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [byte[]]$EmptyBytes
    )
    $root = Join-Path $InstallDir $Name
    $eng = Join-Path $engineDir $Name
    if (-not (Test-Path -LiteralPath $root)) {
        if ($null -ne $EmptyBytes) {
            [System.IO.File]::WriteAllBytes($root, $EmptyBytes)
        } else {
            Set-Content -LiteralPath $root -Value "" -Encoding ASCII
        }
        Write-Host "Created $root"
    }
    if (Test-Path -LiteralPath $eng) {
        Remove-Item -LiteralPath $eng -Force
    }
    try {
        cmd.exe /c "mklink /H `"$eng`" `"$root`"" | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "mklink exit $LASTEXITCODE" }
        Write-Host "Hardlinked $eng <-> $root"
    } catch {
        Copy-Item -LiteralPath $root -Destination $eng -Force
        Write-Host "Copied $root -> $eng (hardlink failed: $_)"
    }
}

# Prefer repo default.fil when seeding
$repoDefault = $null
if ($null -ne $repoMchPath) {
    $cand = Join-Path (Split-Path -Parent $repoMchPath) "default.fil"
    if (Test-Path -LiteralPath $cand) { $repoDefault = $cand }
}
$defaultRoot = Join-Path $InstallDir "default.fil"
if (-not (Test-Path -LiteralPath $defaultRoot)) {
    if ($null -ne $repoDefault) {
        Copy-Item -LiteralPath $repoDefault -Destination $defaultRoot -Force
    } else {
        [System.IO.File]::WriteAllBytes($defaultRoot, [byte[]]@())
    }
}
Install-SharedFile -Name "CameraCut.cfg"
Install-SharedFile -Name "default.fil" -EmptyBytes @()

# Builtin\Users SID - language-independent (Users / Usuarios / etc.)
$aclTargets = @(
    "*S-1-5-32-545",
    $env:USERNAME
)
foreach ($who in $aclTargets) {
    if ([string]::IsNullOrWhiteSpace($who)) { continue }
    $grant = "${who}:(OI)(CI)M"
    Write-Host "icacls `"$InstallDir`" /grant `"$grant`" /T"
    & icacls.exe $InstallDir /grant $grant /T
    if ($LASTEXITCODE -ne 0) {
        throw "icacls failed for '$who' with exit $LASTEXITCODE"
    }
}

$logPath = Join-Path $InstallDir "install.log"
$mchPresent = Test-Path -LiteralPath $destMch
$corePresent = Test-Path -LiteralPath $destCore
@(
    "CameraCut bridge install"
    "time=$(Get-Date -Format o)"
    "installDir=$InstallDir"
    "bridgeExe=$BridgeExe"
    "mchPresent=$mchPresent"
    "corePresent=$corePresent"
    "repoMch=$repoMchPath"
    "user=$env:USERNAME"
) | Set-Content -LiteralPath $logPath -Encoding UTF8
Write-Host "Wrote $logPath"

Write-Host ""
Write-Host "Next steps:"
Write-Host "  Run CameraCut.exe as a normal user (no elevation)."
Write-Host "  Do not run Corel as administrator."
Write-Host "  Bridge owns FindWindow; CameraCutCore.exe does the real cut."

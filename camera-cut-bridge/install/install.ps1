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
$destCore = Join-Path $InstallDir "CameraCutCore.exe"
$repoMchPath = Resolve-RepoMch $RepoMch

Write-Host "InstallDir=$InstallDir"
Write-Host "BridgeExe=$BridgeExe"

New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null

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

# Vendor CameraCut (real cut engine) -> CameraCutCore.exe, asInvoker, no runas
$vendorCam = Join-Path $InstallDir "CameraCut.exe.vendor.bak"
if (-not (Test-Path -LiteralPath $vendorCam)) {
    # First install may have just created bak from live dest, or repo CameraCut.exe
    $repoCam = $null
    if ($null -ne $RepoMch) {
        $repoRoot = [System.IO.Path]::GetFullPath($RepoMch)
        if (Test-Path -LiteralPath $repoRoot -PathType Container) {
            $cand = Join-Path $repoRoot "CameraCut.exe"
            if (Test-Path -LiteralPath $cand) { $repoCam = $cand }
        }
    }
    if ($null -eq $repoCam -and (Test-Path -LiteralPath $destExe)) {
        # Before overwriting with bridge, bak should exist; if not, user must supply bak
    }
    if ($null -ne $repoCam) {
        Copy-Item -LiteralPath $repoCam -Destination $vendorCam -Force
        Write-Host "Seeded vendor bak from $repoCam"
    }
}
if (Test-Path -LiteralPath $vendorCam) {
    Invoke-PePatch -Source $vendorCam -Dest $destCore -AlsoRunas
} else {
    Write-Warning "No CameraCut.exe.vendor.bak — CameraCutCore.exe not installed. Cutting will not work until core is present."
}

Copy-Item -LiteralPath $BridgeExe -Destination $destExe -Force
Write-Host "Installed bridge $BridgeExe -> $destExe"

if (Test-Path -LiteralPath $destMch) {
    Invoke-PePatch -Source $destMch -Dest $destMch
}

# BRIDGE defaults: use vendor core for real cuts
$cfgPath = Join-Path $InstallDir "CameraCut.cfg"
@"
[BRIDGE]
UseVendorCore=1
MchAutoStart=0
"@ | Add-Content -LiteralPath $cfgPath -Encoding ASCII
Write-Host "Appended [BRIDGE] to $cfgPath"

# Builtin\Users SID — language-independent (Users / Usuarios / etc.)
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

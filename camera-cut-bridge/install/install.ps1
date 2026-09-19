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
$repoMchPath = Resolve-RepoMch $RepoMch

Write-Host "InstallDir=$InstallDir"
Write-Host "BridgeExe=$BridgeExe"

New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null

Backup-VendorFile -Path $destExe
Backup-VendorFile -Path $destMch

if ($null -ne $repoMchPath) {
    Copy-Item -LiteralPath $repoMchPath -Destination $destMch -Force
    Write-Host "Copied vendor Mch $repoMchPath -> $destMch"
    Backup-VendorFile -Path $destMch
}

Copy-Item -LiteralPath $BridgeExe -Destination $destExe -Force
Write-Host "Installed bridge $BridgeExe -> $destExe"

if (Test-Path -LiteralPath $destMch) {
    $patchPy = Find-PatchScript
    if ($null -eq $patchPy) {
        throw "CameraCutMch.exe present but patch_mch_manifest.py not found (expected ..\tools\patch_mch_manifest.py next to install)."
    }
    $py = Find-Python
    if ($null -eq $py) {
        throw "python not found; needed to patch CameraCutMch.exe manifest."
    }
    $tmp = Join-Path $InstallDir "CameraCutMch.exe.patchtmp"
    Copy-Item -LiteralPath $destMch -Destination $tmp -Force
    $pyArgs = @()
    $pyArgs += $py.Prefix
    $pyArgs += @($patchPy, "--in-place", $tmp)
    Write-Host ("Patching Mch manifest: {0} {1}" -f $py.File, ($pyArgs -join " "))
    & $py.File @pyArgs
    if ($LASTEXITCODE -ne 0) {
        Remove-Item -LiteralPath $tmp -Force -ErrorAction SilentlyContinue
        throw "patch_mch_manifest.py failed with exit $LASTEXITCODE"
    }
    Move-Item -LiteralPath $tmp -Destination $destMch -Force
    Write-Host "Patched $destMch"
}

Write-Host "icacls $InstallDir /grant Users:(OI)(CI)M /T"
& icacls.exe $InstallDir /grant "Users:(OI)(CI)M" /T
if ($LASTEXITCODE -ne 0) {
    throw "icacls failed with exit $LASTEXITCODE"
}

$logPath = Join-Path $InstallDir "install.log"
$mchPresent = Test-Path -LiteralPath $destMch
@(
    "CameraCut bridge install"
    "time=$(Get-Date -Format o)"
    "installDir=$InstallDir"
    "bridgeExe=$BridgeExe"
    "mchPresent=$mchPresent"
    "repoMch=$repoMchPath"
    "user=$env:USERNAME"
) | Set-Content -LiteralPath $logPath -Encoding UTF8
Write-Host "Wrote $logPath"

Write-Host ""
Write-Host "Next steps:"
Write-Host "  Run CameraCut.exe as a normal user (no elevation)."
Write-Host "  Do not run Corel as administrator. UIPI will block GMS FindWindow/SendMessage."

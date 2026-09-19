#Requires -RunAsAdministrator
# Restore vendor CameraCut.exe / CameraCutMch.exe from *.vendor.bak.
# Does not undo the Users modify ACL unless -ResetAcl.

[CmdletBinding()]
param(
    [string]$InstallDir = "C:\Program Files\CameraCut",
    [switch]$ResetAcl
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Test-IsAdmin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    $p = New-Object Security.Principal.WindowsPrincipal($id)
    return $p.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Restore-VendorFile {
    param([Parameter(Mandatory = $true)][string]$Path)
    $bak = "$Path.vendor.bak"
    if (-not (Test-Path -LiteralPath $bak)) {
        Write-Host "No backup $bak, skip"
        return
    }
    Copy-Item -LiteralPath $bak -Destination $Path -Force
    Write-Host "Restored $Path from $bak"
}

if (-not (Test-IsAdmin)) {
    Write-Error "Administrator privileges required. Aborting."
    exit 1
}

if (-not (Test-Path -LiteralPath $InstallDir)) {
    throw "InstallDir not found: $InstallDir"
}

$InstallDir = [System.IO.Path]::GetFullPath($InstallDir)
$destExe = Join-Path $InstallDir "CameraCut.exe"
$destMch = Join-Path $InstallDir "CameraCutMch.exe"

Write-Host "InstallDir=$InstallDir"

Restore-VendorFile -Path $destExe
Restore-VendorFile -Path $destMch

if ($ResetAcl) {
    Write-Host "icacls $InstallDir /reset /T"
    & icacls.exe $InstallDir /reset /T
    if ($LASTEXITCODE -ne 0) {
        throw "icacls /reset failed with exit $LASTEXITCODE"
    }
} else {
    Write-Host "Left ACL unchanged (pass -ResetAcl to drop Users modify)."
}

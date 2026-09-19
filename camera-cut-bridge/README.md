# CameraCut bridge

Drop-in `asInvoker` `CameraCut.exe` so Corel GMS can `FindWindowA("#32770","CameraCut")` / `SendMessage` without UIPI.

Dialog caption is exactly `CameraCut`. Win32, UNICODE, no MFC/Qt. No elevation; never `ShellExecute` `runas`.

Install once as administrator. Daily: CameraCut and Corel as a normal user. Do not run Corel as admin.

## Build

MSVC on Windows. Prefer **x64** for CorelDRAW 26 / Windows 11 (`FindWindow`/`SendMessage` work across bitness). Use `-A Win32` only for old 32-bit Corel X4 installs.

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Targets: `CameraCut`, `sendmsg`, `fake_mch`.

## Install

Elevated, once:

```
powershell -ExecutionPolicy Bypass -File install\install.ps1 -BridgeExe path\to\CameraCut.exe
```

`-InstallDir` defaults to `C:\Program Files\CameraCut`. `-RepoMch` is an optional vendor `CameraCutMch.exe` (file or directory).

The script backs up existing `CameraCut.exe` / `CameraCutMch.exe` to `*.vendor.bak` (does not overwrite an existing bak), copies the bridge over `CameraCut.exe`, patches the Mch manifest if present, and grants `Users` modify on the install dir (`icacls … /grant Users:(OI)(CI)M /T`) so a normal user can write `CameraCut.cfg`.

Uninstall:

```
powershell -ExecutionPolicy Bypass -File install\uninstall.ps1
```

Restores `*.vendor.bak` if present. Does not undo ACL changes unless `-ResetAcl`.

## Messages

| msg | hex | meaning |
| --- | --- | --- |
| `WM_USER+100` | `0x464` | ping |
| `WM_USER+103` | `0x467` | cfg |
| `WM_USER+104` | `0x468` | go |
| `WM_USER+10` | `0x40A` | path |

## Log

`%LOCALAPPDATA%\CameraCut\bridge.log`

## Test

Build `sendmsg` and `fake_mch`. Run the bridge unelevated.

- `sendmsg` — `SendMessage` to `FindWindowA("#32770","CameraCut")` for ping/cfg/go/path. Check `bridge.log`.
- `fake_mch` — dialog titled `Camera Cutter`. In a scratch directory, use it as `CameraCutMch.exe` so `MchEnsureRunning` succeeds without vendor USB code.

## Risks

- The actual cut path may live in vendor `CameraCut`, not `CameraCutMch`.
- USB cutter access may still require the user to have device permission; this bridge does not grant it.

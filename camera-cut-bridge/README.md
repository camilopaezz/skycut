# CameraCut bridge

Drop-in `asInvoker` `CameraCut.exe` so Corel GMS can `FindWindowA("#32770","CameraCut")` / `SendMessage` without UIPI.

Dialog caption is exactly `CameraCut`. Win32, UNICODE, no MFC/Qt. No elevation; never `ShellExecute` `runas`.

Install once as administrator. Daily: CameraCut and Corel as a normal user. Do not run Corel as admin.

## Build

MSVC on Windows (any recent Visual Studio that ships a C++ toolset). Prefer **x64** for CorelDRAW 26 / Windows 11 (`FindWindow`/`SendMessage` work across bitness). Use `-A Win32` only for old 32-bit Corel X4 installs.

CMake only cares about the generator name matching *your* VS install:

| Visual Studio | CMake `-G` | Notes |
| --- | --- | --- |
| **2026** | `"Visual Studio 18 2026"` | Needs **CMake ≥ 4.2** ([generator docs](https://cmake.org/cmake/help/latest/generator/Visual%20Studio%2018%202026.html)); default toolset `v145` |
| 2022 | `"Visual Studio 17 2022"` | CMake 3.21+ |
| 2019 | `"Visual Studio 16 2019"` | |
| 2017 | `"Visual Studio 15 2017"` | |

**Visual Studio 2026** (recommended on your machine):

```
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
```

If `cmake -G` does not list `Visual Studio 18 2026`, your CMake is too old — use the CMake that ships with VS 2026 (Developer PowerShell: `cmake --version`), or install a newer CMake, or open the folder in VS (**File → Open → CMake…**) and build there.

VS 2022 fallback (if the 2026 generator is unavailable but the VS 2022 toolset is installed):

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Or open `CMakeLists.txt` in Visual Studio (**File → Open → CMake…**) / VS Code CMake Tools and build the `CameraCut` target — no generator string needed.

Ninja + Developer Command Prompt also works:

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Need: **Desktop development with C++** workload (MSVC, Windows SDK, CMake optional). No MFC required.

Targets: `CameraCut`, `sendmsg`, `fake_mch`.

## Install

Elevated, once:

```
powershell -ExecutionPolicy Bypass -File install\install.ps1 -BridgeExe path\to\CameraCut.exe
```

`-InstallDir` defaults to `C:\Program Files\CameraCut`. `-RepoMch` is an optional vendor `CameraCutMch.exe` (file or directory).

The script backs up existing `CameraCut.exe` / `CameraCutMch.exe` to `*.vendor.bak` (does not overwrite an existing bak), copies the bridge over `CameraCut.exe`, patches the Mch manifest if present, and grants modify on the install dir to Builtin Users **by SID** (`*S-1-5-32-545`) plus the current user so SKYMark can write `CameraCut.cfg` on non-English Windows (e.g. `Usuarios`).

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

## How cutting works

GMS talks only to the **bridge** (`CameraCut.exe`, title `CameraCut`). The bridge logs the job and, with `UseVendorCore=1` (default), starts **`CameraCutCore.exe`** (vendor CameraCut, `asInvoker`, `runas` patched to `open`) and forwards `WM_USER+104` / `+10`. Mch alone does not receive Corel jobs — that is why an empty “Camera Cutter” window is not enough.

## Risks

- Core forward depends on finding the vendor window (same title `CameraCut`, different process).
- USB cutter access may still require the user to have device permission; this bridge does not grant it.

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
| `WM_USER+100` | `0x464` | export complete, registration x/y |
| `WM_USER+103` | `0x467` | cfg |
| `WM_USER+104` | `0x468` | pre-export setup |
| `WM_USER+10` | `0x40A` | path |

## Log

`%LOCALAPPDATA%\CameraCut\bridge.log`

## Test

Build `sendmsg` and `fake_mch`. Run the bridge unelevated.

- `sendmsg` — `SendMessage` to `FindWindowA("#32770","CameraCut")` for ping/cfg/go/path. Check `bridge.log`.
- `fake_mch` — dialog titled `Camera Cutter`. In a scratch directory, use it as `CameraCutMch.exe` so `MchEnsureRunning` succeeds without vendor USB code.

## How cutting works

GMS talks only to the **bridge** (`CameraCut.exe`, title `CameraCut`). The bridge logs the job and, with `UseVendorCore=1` (default), starts **`engine\CameraCut.exe`** (vendor binary kept under that exact basename — otherwise Setup never loads `[SETUP]` from cfg). Manifest is `asInvoker`, `runas` patched to `open`. Forwards the pre-export `WM_USER+104` handshake, then prepares the exported job before forwarding `WM_USER+100` with its original registration parameters. Legacy `WM_USER+10` also prepares the job before forwarding. Mch alone does not receive Corel jobs — that is why an empty “Camera Cutter” window is not enough.

`CameraCut.cfg` and `default.fil` are hardlinked between the install root and `engine\` so the bridge and vendor share one settings file.

## Risks

- Core forward depends on finding the vendor window (same title `CameraCut`, different process).
- USB cutter access may still require the user to have device permission; this bridge does not grant it.

## Convert a PLT outer frame to guías

`tools/plt_guides.py` converts the enclosing rectangle in the supplied
`corteLa.plt` dialect to the corner marks used in `corteLaGuias.plt`. It preserves
all inner cut commands. The rectangle may have small corner compensation moves
and a side split into multiple segments.

Run with Python 3, using a new output filename:

```
python tools/plt_guides.py input.plt output-guias.plt
python -m unittest discover -s tests -p 'test_plt_guides.py'
```

The supported dialect uses space-delimited `IN TB26,width,height CT1` followed
by `U`/`D` moves. Detection requires one enclosing frame matching those bounds.
The converter uses the sample's guide dimensions in plotter units: 400-unit
corner arms and a short mark from y=640 to y=520. Corner compensation tolerance
is 40 units. These constants have only been checked against the supplied pair;
other guide dimensions or PLT dialects need their own samples.

Already guided, unsupported, and ambiguous files are copied unchanged. The CLI
reports whether it converted the file and refuses to overwrite either the input
or an existing output. It does not send anything to the cutter.

### Observed CutOutline behavior

The user confirmed that enabling CutOutline was the only change between these
exports. The original inputs and all other settings stayed the same.

| Output | CutOutline | Size | Observed commands |
| --- | --- | --- | --- |
| `corteLaTest.plt` | Disabled | 1,014 bytes | No `TB26`/`CT1` header; one trailing `@` |
| `corteLaTest2.plt` | Enabled | 1,036 bytes | Adds `TB26,4800,40800 CT1` after `IN`; two trailing `@` tokens |

All `U` and `D` motion commands are identical between the two files, including
the outer rectangle's cutting path. Enabling CutOutline did not convert the
rectangle to guías or remove that path from this output. `corteLaTest2.plt` is
also byte-for-byte identical to the original supplied `corteLa.plt`.

The earlier intermediate exports, `skycutexport.job` and `skycutexport_2.job`,
are byte-for-byte identical at 1,157 bytes, despite enabling CutOutline for the
second export. Their SHA-256 is
`99e3055a792f61af3a2bfffcd34614c6a6f4ad425d7847782dc8bb4aaba5204f`.
The bridge therefore reads the saved `CutOutlineEn` setting before each send;
the intermediate job does not distinguish the setting in this comparison.

These observations establish the differences in the supplied files. The
firmware meanings of `TB26`, `CT1`, and the extra terminator have not been
independently verified, and the comparison does not establish physical cutter
behavior.

### Automatic conversion on Send

The bridge includes a native C++ converter for both the final cutter dialect
and the semicolon-delimited Corel HPGL export. No Python installation
is required for automatic conversion. Rebuild and install the updated bridge
using the instructions above, then use the normal Corel Send button.

Automatic rectangle-to-guias conversion runs only when **CutOutline is enabled**
(`CutOutlineEn=1`). With CutOutline disabled, the original job is forwarded
unchanged, without creating a conversion backup. The bridge reloads the saved
configuration before each send, so toggling this setting applies to the next job.
Other cutting settings remain the vendor engine's responsibility.

The VBA source embedded in `SKYCut.gms` shows that Send first issues
`WM_USER+104`, exports `%TEMP%\skycutexport.job` with the `cdrPLT` filter, closes
the file, and finally issues `WM_USER+100` with registration x/y parameters.
The bridge now follows that sequence instead of reading a previous job during
the initial handshake. On the final notification, it converts the job before
notifying the vendor. Duplicate final notifications without a new handshake are
ignored. There is no one-second debounce that could discard a new export.

When conversion succeeds, the original is saved as
`%TEMP%\skycutexport.job.before-guides`, replacing the previous conversion's
backup. The converted file replaces the job atomically. Read, write, backup, or
replacement failures stop this send and are logged. Unsupported or ambiguous
jobs are forwarded unchanged and logged as unchanged; this does **not** mean
their enclosing rectangle has been removed.

The supplied `skycutexport.job` uses absolute HPGL coordinates with `PU`/`PD`
commands and pen selection. The native converter recognizes a closed four-sided,
axis-aligned pen-1 rectangle that strictly encloses the remaining artwork with
at least 40 plotter units of clearance. It replaces only that rectangle's motion
commands with the guide geometry, retaining the original coordinates, pen,
header, and all other commands. Relative moves, scaling, unsupported commands,
missing artwork, and ambiguous enclosures are left unchanged. This HPGL support
is in the bridge; the standalone Python utility still handles only the final
cutter dialect described above.

Tests verify the actual Corel job through the send handler, preserve all pen-5
artwork byte for byte, and map the replacement coordinates to the same guide
geometry as the user's manual `corteLaGuias.plt`. Existing final cutter fixtures
also pass byte-for-byte comparison to the manual result. The generated HPGL
retains pen 1 rather than guessing another pen's meaning. A Corel `.job` export
of the manual guias version is still needed to compare guide pen settings;
vendor registration recognition and operation on the real Windows cutter have
not been validated.

Native tests can also run on Linux without the Windows SDK:

```
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

IPC tests use the real dispatcher with a simulated message queue. Send-pipeline
tests use the real job handler with in-memory Win32 file operations and a fake
vendor receiver. They verify conversion before forwarding, backups, registration
parameters, repeated sends, and failure handling. These are not hardware tests.

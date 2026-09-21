#include "CameraCutBridge.h"

#include <stdio.h>
#include <string.h>
#include <tlhelp32.h>

typedef struct CoreEnumCtx {
    DWORD pid;
    HWND hwnd;
    HWND skip;
} CoreEnumCtx;

static BOOL CALLBACK CoreEnumProc(HWND hwnd, LPARAM lp)
{
    CoreEnumCtx *ctx = (CoreEnumCtx *)lp;
    DWORD pid = 0;
    wchar_t title[64];
    wchar_t cls[64];

    if (ctx->skip && hwnd == ctx->skip)
        return TRUE;

    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != ctx->pid)
        return TRUE;

    title[0] = 0;
    cls[0] = 0;
    GetWindowTextW(hwnd, title, 64);
    GetClassNameW(hwnd, cls, 64);
    /* Vendor core keeps title "CameraCut", often class #32770. */
    if (_wcsicmp(title, CAMERACUT_TITLE) != 0)
        return TRUE;
    if (cls[0] && _wcsicmp(cls, L"#32770") != 0)
        return TRUE;

    ctx->hwnd = hwnd;
    return FALSE;
}

static DWORD CoreFindPid(void)
{
    HANDLE snap;
    PROCESSENTRY32W pe;
    DWORD pid = 0;

    snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return 0;

    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, L"CameraCutCore.exe") == 0) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

bool CoreIsRunning(void)
{
    return CoreFindPid() != 0;
}

HWND CoreFindWindow(void)
{
    CoreEnumCtx ctx;
    DWORD pid = CoreFindPid();
    int i;

    if (!pid)
        return NULL;

    ZeroMemory(&ctx, sizeof(ctx));
    ctx.pid = pid;
    ctx.skip = GetMainDlg();

    for (i = 0; i < 50; i++) {
        ctx.hwnd = NULL;
        EnumWindows(CoreEnumProc, (LPARAM)&ctx);
        if (ctx.hwnd)
            return ctx.hwnd;
        Sleep(50);
    }
    return NULL;
}

bool CoreEnsureRunning(void)
{
    const Paths *paths;
    wchar_t cmd[MAX_PATH + 4];
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    DWORD attr;
    HWND hwnd;
    HWND bridge;
    BOOL renamed = FALSE;

    hwnd = CoreFindWindow();
    if (hwnd)
        return true;

    if (CoreIsRunning()) {
        /* process up, window not yet */
        hwnd = CoreFindWindow();
        if (hwnd)
            return true;
        Logf(L"core process up but window missing; relaunching");
    }

    paths = GetPaths();
    if (!paths || !paths->corePath[0]) {
        Logf(L"CoreEnsureRunning: no core path");
        return false;
    }

    attr = GetFileAttributesW(paths->corePath);
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        Logf(L"CameraCutCore.exe missing: %s", paths->corePath);
        Logf(L"Install vendor CameraCut as CameraCutCore.exe (asInvoker) to enable cutting");
        return false;
    }

    /*
     * Vendor CameraCut single-instances on FindWindow("#32770","CameraCut").
     * Hide our bridge title while we spawn core so it does not see us and exit.
     */
    bridge = GetMainDlg();
    if (bridge) {
        SetWindowTextW(bridge, L"CameraCutBridge");
        renamed = TRUE;
        /* Let the title change settle before CreateProcess. */
        Sleep(50);
    }

    _snwprintf(cmd, MAX_PATH + 4, L"\"%s\"", paths->corePath);
    cmd[MAX_PATH + 3] = L'\0';

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWMINNOACTIVE;
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessW(paths->corePath, cmd, NULL, NULL, FALSE, 0, NULL,
                        paths->installDir[0] ? paths->installDir : NULL, &si, &pi)) {
        Logf(L"CreateProcessW Core failed gle=%lu", GetLastError());
        if (renamed && bridge)
            SetWindowTextW(bridge, CAMERACUT_TITLE);
        return false;
    }

    Logf(L"core started pid=%lu path=%s", pi.dwProcessId, paths->corePath);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    hwnd = CoreFindWindow();
    if (!hwnd)
        Logf(L"core window not found yet (will retry on forward)");
    else
        Logf(L"core window hwnd=%p", hwnd);

    if (renamed && bridge)
        SetWindowTextW(bridge, CAMERACUT_TITLE);

    return true;
}

LRESULT CoreForward(UINT msg, WPARAM wParam, LPARAM lParam)
{
    HWND hwnd;
    DWORD_PTR result = 0;

    if (!CoreEnsureRunning())
        return 0;

    hwnd = CoreFindWindow();
    if (!hwnd) {
        Logf(L"CoreForward: no hwnd msg=0x%04X", msg);
        return 0;
    }

    Logf(L"CoreForward hwnd=%p msg=0x%04X w=%llu l=%lld",
         hwnd, msg, (unsigned long long)wParam, (long long)lParam);

    if (!SendMessageTimeoutW(hwnd, msg, wParam, lParam, SMTO_ABORTIFHUNG, 8000, &result)) {
        Logf(L"CoreForward SendMessageTimeout gle=%lu", GetLastError());
        return 0;
    }
    Logf(L"CoreForward LRESULT=%lld", (long long)(LONG_PTR)result);
    return (LRESULT)result;
}

#include "CameraCutBridge.h"

#include <stdio.h>

bool MchIsRunning(void)
{
    if (FindWindowW(NULL, MCH_TITLE))
        return true;
    if (FindWindowA("#32770", MCH_TITLE_A))
        return true;
    return false;
}

bool MchEnsureRunning(void)
{
    const Paths *paths;
    const Cfg *cfg;
    const wchar_t *exe;
    wchar_t cmd[MAX_PATH + 4];
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    DWORD attr;

    if (MchIsRunning())
        return true;

    paths = GetPaths();
    cfg = GetCfg();
    if (!paths) {
        Logf(L"MchEnsureRunning: no paths");
        return false;
    }

    exe = (cfg && cfg->useVendorCore) ? paths->corePath : paths->mchPath;
    if (!exe || !exe[0]) {
        Logf(L"MchEnsureRunning: empty exe path");
        return false;
    }

    attr = GetFileAttributesW(exe);
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        Logf(L"Mch exe missing: %s", exe);
        return false;
    }

    _snwprintf(cmd, MAX_PATH + 4, L"\"%s\"", exe);
    cmd[MAX_PATH + 3] = L'\0';

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessW(exe, cmd, NULL, NULL, FALSE, 0, NULL,
                        paths->installDir[0] ? paths->installDir : NULL, &si, &pi)) {
        Logf(L"CreateProcessW failed gle=%lu exe=%s", GetLastError(), exe);
        return false;
    }

    Logf(L"mch started pid=%lu exe=%s", pi.dwProcessId, exe);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    /* Do not wait here: GMS SendMessage and WinMain run on the UI thread. */
    return true;
}

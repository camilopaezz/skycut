#include "CameraCutBridge.h"

#include <stdio.h>

static int g_jobBusy;
static DWORD g_jobLastTick;

static int JobBytesPrintable(const unsigned char *p, DWORD n)
{
    DWORD i;

    for (i = 0; i < n; i++) {
        unsigned char c = p[i];
        if (c == '\t' || c == '\r' || c == '\n')
            continue;
        if (c < 32 || c > 126)
            return 0;
    }
    return 1;
}

static void JobLogPreview(const unsigned char *p, DWORD n)
{
    wchar_t line[1024];
    DWORD i, used;

    if (JobBytesPrintable(p, n)) {
        used = n;
        if (used > 500)
            used = 500;
        for (i = 0; i < used; i++)
            line[i] = (wchar_t)p[i];
        line[used] = L'\0';
        Logf(L"job text: %s", line);
        return;
    }

    used = 0;
    for (i = 0; i < n && used + 3 < 1000; i++) {
        _snwprintf(line + used, 1024 - used, L"%02X ", p[i]);
        used = (DWORD)wcslen(line);
    }
    line[used] = L'\0';
    Logf(L"job hex: %s", line);
}

static void JobLogFile(const wchar_t *path)
{
    HANDLE h;
    DWORD sizeHi = 0, sizeLo, got = 0;
    unsigned char buf[512];

    h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        Logf(L"job open failed gle=%lu %s", GetLastError(), path);
        return;
    }
    sizeLo = GetFileSize(h, &sizeHi);
    if (sizeLo == INVALID_FILE_SIZE && GetLastError() != NO_ERROR) {
        Logf(L"job size failed gle=%lu %s", GetLastError(), path);
        CloseHandle(h);
        return;
    }
    Logf(L"job size=%lu %s", sizeLo, path);
    if (ReadFile(h, buf, sizeof(buf), &got, NULL) && got > 0)
        JobLogPreview(buf, got);
    CloseHandle(h);
}

static void JobFindGlob(const wchar_t *dir, const wchar_t *glob)
{
    wchar_t pattern[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE h;
    size_t n;
    int slash;

    if (!dir || !dir[0])
        return;
    n = wcslen(dir);
    slash = (n > 0 && dir[n - 1] != L'\\' && dir[n - 1] != L'/') ? 1 : 0;
    _snwprintf(pattern, MAX_PATH, L"%s%s%s", dir, slash ? L"\\" : L"", glob);
    pattern[MAX_PATH - 1] = L'\0';

    h = FindFirstFileW(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;
        Logf(L"temp match %s: %s", glob, fd.cFileName);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static void JobLogIfExistsA(const char *pathA)
{
    wchar_t w[64];
    DWORD attr = GetFileAttributesA(pathA);
    if (attr == INVALID_FILE_ATTRIBUTES)
        return;
    MultiByteToWideChar(CP_ACP, 0, pathA, -1, w, 64);
    w[63] = L'\0';
    Logf(L"found %s", w);
}

void JobOnGo(HWND hDlg)
{
    const Paths *paths;
    DWORD now = GetTickCount();
    DWORD attr;
    int haveJob;

    if (g_jobBusy) {
        Logf(L"JobOnGo skipped (busy)");
        return;
    }
    if (g_jobLastTick != 0 && (now - g_jobLastTick) < 1000) {
        Logf(L"JobOnGo skipped (debounce)");
        return;
    }

    g_jobBusy = 1;

    Logf(L"go");

    paths = GetPaths();
    haveJob = 0;
    if (paths && paths->jobPath[0]) {
        attr = GetFileAttributesW(paths->jobPath);
        if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
            haveJob = 1;
            JobLogFile(paths->jobPath);
        }
    }

    if (paths)
        JobFindGlob(paths->tempDir, L"*.plt");
    if (paths)
        JobFindGlob(paths->tempDir, L"*.prn");

    JobLogIfExistsA("c:\\myoutput.prn");
    JobLogIfExistsA("c:\\myoutput");

    {
        Cfg *cfg = GetCfg();
        if (cfg && cfg->useVendorCore) {
            /* Real PLT/cut lives in vendor CameraCut — forward GO to CameraCutCore.exe */
            if (!CoreEnsureRunning())
                SetStatus(hDlg, L"engine\\CameraCut.exe missing — reinstall");
            else {
                CoreForward(WM_CC_GO, 0, 0);
                CoreForward(WM_CC_PATH, 4, 0);
            }
        } else {
            MchEnsureRunning();
        }
    }

    if (haveJob)
        SetStatus(hDlg, paths->jobPath);
    else if (!GetCfg() || !GetCfg()->useVendorCore)
        SetStatus(hDlg, L"no job file");

    g_jobLastTick = GetTickCount();
    g_jobBusy = 0;
}

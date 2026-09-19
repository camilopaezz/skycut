#include "CameraCutBridge.h"

#include <stdarg.h>
#include <strsafe.h>

static HANDLE g_log = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION g_cs;
static BOOL g_csInit = FALSE;

void LogInit(const Paths *paths)
{
    if (!g_csInit) {
        InitializeCriticalSection(&g_cs);
        g_csInit = TRUE;
    }

    if (paths && paths->dataDir[0])
        CreateDirectoryW(paths->dataDir, NULL);

    if (g_log != INVALID_HANDLE_VALUE) {
        CloseHandle(g_log);
        g_log = INVALID_HANDLE_VALUE;
    }

    if (paths && paths->logPath[0]) {
        g_log = CreateFileW(
            paths->logPath,
            FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
        if (g_log == INVALID_HANDLE_VALUE)
            OutputDebugStringW(L"CameraCut: failed to open bridge.log\r\n");
    }
}

void Logf(const wchar_t *fmt, ...)
{
    wchar_t msg[1536];
    wchar_t line[1800];
    SYSTEMTIME st;
    va_list ap;

    if (!fmt)
        return;

    va_start(ap, fmt);
    StringCchVPrintfW(msg, ARRAYSIZE(msg), fmt, ap);
    va_end(ap);

    GetLocalTime(&st);
    StringCchPrintfW(
        line,
        ARRAYSIZE(line),
        L"%04u-%02u-%02u %02u:%02u:%02u %s\r\n",
        st.wYear,
        st.wMonth,
        st.wDay,
        st.wHour,
        st.wMinute,
        st.wSecond,
        msg);

    OutputDebugStringW(line);

    if (g_csInit)
        EnterCriticalSection(&g_cs);
    if (g_log != INVALID_HANDLE_VALUE) {
        char utf8[3600];
        int n = WideCharToMultiByte(CP_UTF8, 0, line, -1, utf8, sizeof(utf8), NULL, NULL);
        if (n > 1) {
            DWORD written = 0;
            WriteFile(g_log, utf8, (DWORD)(n - 1), &written, NULL);
        }
    }
    if (g_csInit)
        LeaveCriticalSection(&g_cs);
}

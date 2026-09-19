#include "CameraCutBridge.h"

#include <string.h>

#pragma comment(lib, "user32.lib")

static const wchar_t kClass[] = L"FakeMchClass";

static void LogCmdLine(void)
{
    wchar_t path[MAX_PATH];
    const wchar_t *cmd;
    DWORD n, wrote;
    HANDLE h;
    char utf8[4096];
    int bytes;

    n = GetTempPathW(MAX_PATH, path);
    if (n == 0 || n >= MAX_PATH)
        return;
    if (path[n - 1] != L'\\' && path[n - 1] != L'/') {
        if (n + 1 >= MAX_PATH)
            return;
        path[n++] = L'\\';
        path[n] = 0;
    }
    if (n + 12 >= MAX_PATH)
        return;
    memcpy(path + n, L"fake_mch.log", 13 * sizeof(wchar_t));

    cmd = GetCommandLineW();
    if (!cmd)
        cmd = L"";
    bytes = WideCharToMultiByte(CP_UTF8, 0, cmd, -1, utf8, (int)sizeof(utf8) - 2,
                                NULL, NULL);
    if (bytes <= 1)
        return;
    bytes -= 1;
    utf8[bytes++] = '\n';

    h = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                    NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return;
    WriteFile(h, utf8, (DWORD)bytes, &wrote, NULL);
    CloseHandle(h);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSW wc;
    HWND hwnd;
    MSG msg;

    (void)hPrev;
    (void)lpCmdLine;

    LogCmdLine();

    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = kClass;
    if (!RegisterClassW(&wc))
        return 1;

    hwnd = CreateWindowW(kClass, MCH_TITLE,
                         WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                         CW_USEDEFAULT, CW_USEDEFAULT, 280, 120,
                         NULL, NULL, hInst, NULL);
    if (!hwnd)
        return 1;

    ShowWindow(hwnd, nCmdShow > 0 ? nCmdShow : SW_SHOW);
    UpdateWindow(hwnd);

    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

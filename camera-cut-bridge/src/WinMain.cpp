#include "CameraCutBridge.h"

#include <commctrl.h>
#include <stdio.h>

/* True if hwnd belongs to another CameraCut.exe (bridge), not Core/Corel. */
static int IsOtherBridgeInstance(HWND hwnd)
{
    DWORD pid = 0;
    HANDLE h = NULL;
    wchar_t path[MAX_PATH];
    DWORD n;
    const wchar_t *base;

    if (!hwnd)
        return 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (!pid || pid == GetCurrentProcessId())
        return 0;

    h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h)
        return 0;
    n = MAX_PATH;
    path[0] = L'\0';
    if (!QueryFullProcessImageNameW(h, 0, path, &n)) {
        CloseHandle(h);
        return 0;
    }
    CloseHandle(h);

    base = wcsrchr(path, L'\\');
    base = base ? base + 1 : path;
    /* Only treat another bridge as the single-instance owner. */
    return _wcsicmp(base, L"CameraCut.exe") == 0;
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR lpCmdLine, int nCmdShow)
{
    INITCOMMONCONTROLSEX icc;
    Paths paths;
    HWND existing;
    HWND hDlg;
    MSG msg;
    Cfg *cfg;

    (void)hPrev;
    (void)lpCmdLine;
    (void)nCmdShow;

    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);

    if (!PathsInit(&paths))
        return 1;
    LogInit(GetPaths());
    CfgLoad(GetCfg());

    Logf(L"startup installDir=%s dataDir=%s", paths.installDir, paths.dataDir);

    existing = FindWindowA(CAMERACUT_CLASS_A, CAMERACUT_TITLE_A);
    if (existing && IsOtherBridgeInstance(existing)) {
        SetForegroundWindow(existing);
        return 0;
    }

    hDlg = CreateDialogParamW(hInst, MAKEINTRESOURCE(IDD_MAIN), NULL, CameraCutDlgProc, 0);
    if (!hDlg) {
        Logf(L"CreateDialogParam failed err=%lu", GetLastError());
        return 1;
    }

    cfg = GetCfg();
    if (cfg && cfg->useVendorCore)
        CoreEnsureRunning();
    else if (cfg && cfg->mchAutoStart)
        MchEnsureRunning();

    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        if (!IsDialogMessage(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return (int)msg.wParam;
}

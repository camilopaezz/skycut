#include "CameraCutBridge.h"

#include <commctrl.h>

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
    if (existing) {
        SetForegroundWindow(existing);
        return 0;
    }

    hDlg = CreateDialogParamW(hInst, MAKEINTRESOURCE(IDD_MAIN), NULL, CameraCutDlgProc, 0);
    if (!hDlg) {
        Logf(L"CreateDialogParam failed err=%lu", GetLastError());
        return 1;
    }
    ShowWindow(hDlg, SW_SHOW);

    cfg = GetCfg();
    if (cfg && cfg->mchAutoStart)
        MchEnsureRunning();

    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        if (!IsDialogMessage(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return (int)msg.wParam;
}

#include "CameraCutBridge.h"

#include <stdio.h>

static void SetLastMsg(HWND hDlg, const wchar_t *text)
{
    if (hDlg && text)
        SetDlgItemTextW(hDlg, IDC_LASTMSG, text);
}

INT_PTR HandleUserMessage(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    wchar_t last[160];
    const Paths *paths;
    DWORD attr;

    Logf(L"IPC msg=0x%04X w=%llu l=%lld",
         msg, (unsigned long long)wParam, (long long)lParam);

    _snwprintf(last, 160, L"msg=0x%04X w=%llu l=%lld",
               msg, (unsigned long long)wParam, (long long)lParam);
    last[159] = L'\0';
    SetLastMsg(hDlg, last);

    if (msg < WM_USER)
        return 0;

    switch (msg) {
    case WM_CC_PING:
        SetLastMsg(hDlg, L"PING");
        return 1;

    case WM_CC_CFG:
        CfgLoad(GetCfg());
        Logf(L"WM_CC_CFG lParam=%lld", (long long)lParam);
        SetStatus(hDlg, L"Setcfg");
        SetLastMsg(hDlg, L"CFG");
        if (GetCfg() && GetCfg()->useVendorCore)
            CoreForward(WM_CC_CFG, wParam, lParam);
        return 1;

    case WM_CC_GO:
        SetLastMsg(hDlg, L"GO");
        /* Defer to WM_COMMAND so SendMessage returns 1 without nesting JobOnGo
         * (button click and GO share one path; g_jobBusy/debounce still apply). */
        PostMessage(hDlg, WM_COMMAND, MAKEWPARAM(IDC_ACTION, BN_CLICKED), 0);
        return 1;

    case WM_CC_PATH:
        Logf(L"WM_CC_PATH wParam=%llu", (unsigned long long)wParam);
        SetLastMsg(hDlg, L"PATH");
        paths = GetPaths();
        attr = (paths && paths->jobPath[0])
                   ? GetFileAttributesW(paths->jobPath)
                   : INVALID_FILE_ATTRIBUTES;
        if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY))
            PostMessage(hDlg, WM_COMMAND, MAKEWPARAM(IDC_ACTION, BN_CLICKED), 0);
        else
            Logf(L"WM_CC_PATH job file missing");
        return 1;

    default:
        Logf(L"IPC unknown WM_USER msg=0x%04X", msg);
        SetLastMsg(hDlg, L"unknown");
        return 1;
    }
}

#include "CameraCutBridge.h"

static HWND g_mainDlg;

HWND GetMainDlg(void)
{
    return g_mainDlg;
}

void SetMainDlg(HWND hDlg)
{
    g_mainDlg = hDlg;
}

void SetStatus(HWND hDlg, const wchar_t *text)
{
    if (hDlg && text)
        SetDlgItemTextW(hDlg, IDC_STATUS, text);
    if (text)
        Logf(L"%s", text);
}

INT_PTR CALLBACK CameraCutDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_INITDIALOG:
        SetMainDlg(hDlg);
        SetWindowTextW(hDlg, CAMERACUT_TITLE);
        SetStatus(hDlg, L"Ready");
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_ACTION)
            JobOnGo(hDlg);
        return TRUE;
    case WM_CLOSE:
        PostQuitMessage(0);
        DestroyWindow(hDlg);
        return TRUE;
    case WM_DESTROY:
        SetMainDlg(NULL);
        PostQuitMessage(0);
        return TRUE;
    default:
        break;
    }

    if (msg >= WM_USER) {
        INT_PTR result = HandleUserMessage(hDlg, msg, wParam, lParam);
        SetWindowLongPtr(hDlg, DWLP_MSGRESULT, result);
        return TRUE;
    }
    return FALSE;
}

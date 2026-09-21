#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CAMERACUT_TITLE L"CameraCut"
#define CAMERACUT_TITLE_A "CameraCut"
#define CAMERACUT_CLASS_A "#32770"
#define MCH_TITLE L"Camera Cutter"
#define MCH_TITLE_A "Camera Cutter"

#define WM_CC_PING (WM_USER + 100) /* 0x464 */
#define WM_CC_CFG (WM_USER + 103)  /* 0x467 */
#define WM_CC_GO (WM_USER + 104)   /* 0x468 */
#define WM_CC_PATH (WM_USER + 10)  /* 0x40A */

#define IDD_MAIN 102
#define IDC_ACTION 1001
#define IDC_STATUS 1002
#define IDC_LASTMSG 1003

typedef struct Paths {
    wchar_t installDir[MAX_PATH];
    wchar_t dataDir[MAX_PATH];
    wchar_t tempDir[MAX_PATH];
    wchar_t cfgPath[MAX_PATH];
    wchar_t logPath[MAX_PATH];
    wchar_t jobPath[MAX_PATH];
    wchar_t mchPath[MAX_PATH];
    wchar_t corePath[MAX_PATH];
} Paths;

typedef struct Cfg {
    double closeMm;
    double offsetMm;
    int cutOutlineEn;
    int closeEn;
    int offsetEn;
    int dashedEn;
    double lineLen;
    double lineSpace;
    double knifeOffset;
    int mchAutoStart;
    int useVendorCore;
} Cfg;

void LogInit(const Paths *paths);
void Logf(const wchar_t *fmt, ...);

bool PathsInit(Paths *p);
const Paths *GetPaths(void);

bool CfgLoad(Cfg *c);
bool CfgSave(const Cfg *c);
Cfg *GetCfg(void);

void JobOnGo(HWND hDlg);

bool MchIsRunning(void);
bool MchEnsureRunning(void);

/* Vendor engine\CameraCut.exe (basename CameraCut.exe) — owns real PLT/cut logic. */
bool CoreIsRunning(void);
bool CoreEnsureRunning(void);
HWND CoreFindWindow(void);
LRESULT CoreForward(UINT msg, WPARAM wParam, LPARAM lParam);

HWND GetMainDlg(void);
void SetMainDlg(HWND hDlg);
void SetStatus(HWND hDlg, const wchar_t *text);

INT_PTR CALLBACK CameraCutDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR HandleUserMessage(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);

#ifdef __cplusplus
}
#endif

#pragma once
#include <cstdint>
#include <cwchar>
using HWND = void *;
using UINT = unsigned;
using DWORD = unsigned long;
using WPARAM = uintptr_t;
using LPARAM = intptr_t;
using INT_PTR = intptr_t;
#define WM_USER 0x400
#define WM_APP 0x8000
#define WM_CC_PING (WM_USER+100)
#define WM_CC_CFG (WM_USER+103)
#define WM_CC_GO (WM_USER+104)
#define WM_CC_PATH (WM_USER+10)
#define WM_CC_JOB_READY (WM_APP+1)
#define IDC_LASTMSG 1003
#define INVALID_FILE_ATTRIBUTES 0xffffffff
#define FILE_ATTRIBUTE_DIRECTORY 16
#define _snwprintf swprintf
struct Paths { wchar_t jobPath[64]; wchar_t tempDir[64]; };
struct Cfg { int useVendorCore; int cutOutlineEn; };
void SetDlgItemTextW(HWND,int,const wchar_t*);
void Logf(const wchar_t*,...);
void SetStatus(HWND,const wchar_t*);
Cfg *GetCfg();
bool CfgLoad(Cfg*);
INT_PTR CoreForward(UINT,WPARAM,LPARAM);
bool PostMessage(HWND,UINT,WPARAM,LPARAM);
const Paths *GetPaths();
DWORD GetFileAttributesW(const wchar_t*);
void JobOnGo(HWND,UINT,WPARAM,LPARAM);
INT_PTR HandleUserMessage(HWND,UINT,WPARAM,LPARAM);

#include <string>
using HANDLE = void *;
using BOOL = int;
struct LARGE_INTEGER { long long QuadPart; };
struct WIN32_FIND_DATAW { DWORD dwFileAttributes; wchar_t cFileName[260]; };
#define MAX_PATH 260
#define TRUE 1
#define FALSE 0
#define GENERIC_READ 1
#define GENERIC_WRITE 2
#define FILE_SHARE_READ 1
#define FILE_SHARE_WRITE 2
#define FILE_SHARE_DELETE 4
#define OPEN_EXISTING 3
#define TRUNCATE_EXISTING 5
#define FILE_ATTRIBUTE_NORMAL 128
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define INVALID_FILE_SIZE 0xffffffff
#define NO_ERROR 0
#define CP_ACP 0
HANDLE CreateFileW(const wchar_t*,DWORD,DWORD,void*,DWORD,DWORD,HANDLE);
BOOL CloseHandle(HANDLE);
DWORD GetLastError();
DWORD GetFileSize(HANDLE,DWORD*);
BOOL GetFileSizeEx(HANDLE,LARGE_INTEGER*);
BOOL ReadFile(HANDLE,void*,DWORD,DWORD*,void*);
BOOL WriteFile(HANDLE,const void*,DWORD,DWORD*,void*);
BOOL FlushFileBuffers(HANDLE);
BOOL CopyFileW(const wchar_t*,const wchar_t*,BOOL);
BOOL ReplaceFileW(const wchar_t*,const wchar_t*,const wchar_t*,DWORD,void*,void*);
BOOL DeleteFileW(const wchar_t*);
UINT GetTempFileNameW(const wchar_t*,const wchar_t*,UINT,wchar_t*);
HANDLE FindFirstFileW(const wchar_t*,WIN32_FIND_DATAW*);
BOOL FindNextFileW(HANDLE,WIN32_FIND_DATAW*);
BOOL FindClose(HANDLE);
DWORD GetFileAttributesA(const char*);
int MultiByteToWideChar(UINT,DWORD,const char*,int,wchar_t*,int);
bool CoreEnsureRunning();
bool MchEnsureRunning();

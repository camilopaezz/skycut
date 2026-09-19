#include "CameraCutBridge.h"

#include <shlobj.h>
#include <string.h>
#include <wchar.h>

static Paths g_paths;

static bool PathJoin(wchar_t *dst, size_t cch, const wchar_t *dir, const wchar_t *name)
{
    size_t ld;
    size_t ln;
    int slash = 0;

    if (!dst || cch == 0)
        return false;

    dst[0] = 0;
    if (!dir)
        dir = L"";
    if (!name)
        name = L"";

    ld = wcslen(dir);
    ln = wcslen(name);
    while (ln > 0 && (name[0] == L'\\' || name[0] == L'/')) {
        name++;
        ln--;
    }
    if (ld > 0 && dir[ld - 1] != L'\\' && dir[ld - 1] != L'/')
        slash = 1;
    if (ld + (size_t)slash + ln + 1 > cch)
        return false;

    memcpy(dst, dir, ld * sizeof(wchar_t));
    if (slash)
        dst[ld++] = L'\\';
    memcpy(dst + ld, name, (ln + 1) * sizeof(wchar_t));
    return true;
}

bool PathsInit(Paths *p)
{
    wchar_t module[MAX_PATH];
    wchar_t localApp[MAX_PATH];
    DWORD n;
    wchar_t *cut;

    memset(&g_paths, 0, sizeof(g_paths));

    n = GetModuleFileNameW(NULL, module, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
        return false;
    cut = wcsrchr(module, L'\\');
    if (!cut)
        cut = wcsrchr(module, L'/');
    if (!cut)
        return false;
    *cut = 0;
    if (wcslen(module) >= MAX_PATH)
        return false;
    memcpy(g_paths.installDir, module, (wcslen(module) + 1) * sizeof(wchar_t));

    if (FAILED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, localApp)))
        return false;
    if (!PathJoin(g_paths.dataDir, MAX_PATH, localApp, L"CameraCut"))
        return false;
    CreateDirectoryW(g_paths.dataDir, NULL);

    n = GetTempPathW(MAX_PATH, g_paths.tempDir);
    if (n == 0 || n >= MAX_PATH)
        return false;

    if (!PathJoin(g_paths.cfgPath, MAX_PATH, g_paths.installDir, L"CameraCut.cfg"))
        return false;
    if (!PathJoin(g_paths.logPath, MAX_PATH, g_paths.dataDir, L"bridge.log"))
        return false;
    if (!PathJoin(g_paths.jobPath, MAX_PATH, g_paths.tempDir, L"skycutexport.job"))
        return false;
    if (!PathJoin(g_paths.mchPath, MAX_PATH, g_paths.installDir, L"CameraCutMch.exe"))
        return false;
    if (!PathJoin(g_paths.corePath, MAX_PATH, g_paths.installDir, L"CameraCutCore.exe"))
        return false;

    if (p)
        *p = g_paths;
    return true;
}

const Paths *GetPaths(void)
{
    return &g_paths;
}

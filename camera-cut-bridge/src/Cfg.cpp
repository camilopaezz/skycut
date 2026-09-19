#include "CameraCutBridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Cfg g_cfg;
static int g_cfgReady;

static void CfgDefaults(Cfg *c)
{
    c->closeMm = 0.40;
    c->offsetMm = 0.35;
    c->cutOutlineEn = 0;
    c->closeEn = 1;
    c->offsetEn = 1;
    c->dashedEn = 0;
    c->lineLen = 0.40;
    c->lineSpace = 0.40;
    c->knifeOffset = 0;
    c->mchAutoStart = 1;
    c->useVendorCore = 0;
}

static int WideToAnsiPath(const wchar_t *wide, char *ansi, int cap)
{
    int n;

    if (!wide || !ansi || cap <= 0)
        return 0;
    n = WideCharToMultiByte(CP_ACP, 0, wide, -1, ansi, cap, NULL, NULL);
    return n > 0;
}

static double IniGetDoubleA(const char *section, const char *key, const char *def,
                            const char *ini)
{
    char buf[64];

    GetPrivateProfileStringA(section, key, def, buf, (DWORD)sizeof(buf), ini);
    return atof(buf);
}

static void IniWriteDoubleA(const char *section, const char *key, double v, const char *ini)
{
    char buf[64];

    _snprintf(buf, sizeof(buf), "%.2f", v);
    buf[sizeof(buf) - 1] = '\0';
    WritePrivateProfileStringA(section, key, buf, ini);
}

static void IniWriteIntA(const char *section, const char *key, int v, const char *ini)
{
    char buf[32];

    _snprintf(buf, sizeof(buf), "%d", v);
    buf[sizeof(buf) - 1] = '\0';
    WritePrivateProfileStringA(section, key, buf, ini);
}

Cfg *GetCfg(void)
{
    if (!g_cfgReady) {
        CfgDefaults(&g_cfg);
        g_cfgReady = 1;
    }
    return &g_cfg;
}

bool CfgLoad(Cfg *c)
{
    const Paths *paths;
    char iniA[MAX_PATH];
    DWORD attr;

    if (!c)
        c = GetCfg();
    CfgDefaults(c);

    paths = GetPaths();
    if (!paths || paths->cfgPath[0] == L'\0') {
        Logf(L"cfg path unavailable, using defaults");
        g_cfg = *c;
        g_cfgReady = 1;
        return false;
    }

    attr = GetFileAttributesW(paths->cfgPath);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        Logf(L"cfg missing, using defaults: %s", paths->cfgPath);
        g_cfg = *c;
        g_cfgReady = 1;
        return false;
    }

    if (!WideToAnsiPath(paths->cfgPath, iniA, (int)sizeof(iniA))) {
        Logf(L"cfg path convert failed, using defaults");
        g_cfg = *c;
        g_cfgReady = 1;
        return false;
    }

    c->closeMm = IniGetDoubleA("SETUP", "Close", "0.40", iniA);
    c->offsetMm = IniGetDoubleA("SETUP", "Offset", "0.35", iniA);
    c->cutOutlineEn = (int)GetPrivateProfileIntA("SETUP", "CutOutlineEn", 0, iniA);
    c->closeEn = (int)GetPrivateProfileIntA("SETUP", "CloseEn", 1, iniA);
    c->offsetEn = (int)GetPrivateProfileIntA("SETUP", "OffsetEn", 1, iniA);
    c->dashedEn = (int)GetPrivateProfileIntA("SETUP", "DashedEn", 0, iniA);
    c->lineLen = IniGetDoubleA("SETUP", "LineLen", "0.40", iniA);
    c->lineSpace = IniGetDoubleA("SETUP", "LineSpace", "0.40", iniA);
    c->knifeOffset = IniGetDoubleA("SETUP", "KnifeOffset", "0", iniA);
    c->mchAutoStart = (int)GetPrivateProfileIntA("BRIDGE", "MchAutoStart", 1, iniA);
    c->useVendorCore = (int)GetPrivateProfileIntA("BRIDGE", "UseVendorCore", 0, iniA);

    g_cfg = *c;
    g_cfgReady = 1;
    Logf(L"cfg loaded %s close=%.2f offset=%.2f mchAutoStart=%d useVendorCore=%d",
         paths->cfgPath, c->closeMm, c->offsetMm, c->mchAutoStart, c->useVendorCore);
    return true;
}

bool CfgSave(const Cfg *c)
{
    const Paths *paths;
    char iniA[MAX_PATH];

    if (!c)
        c = GetCfg();

    paths = GetPaths();
    if (!paths || paths->cfgPath[0] == L'\0') {
        Logf(L"CfgSave: no cfg path");
        return false;
    }
    if (!WideToAnsiPath(paths->cfgPath, iniA, (int)sizeof(iniA))) {
        Logf(L"CfgSave: path convert failed");
        return false;
    }

    IniWriteDoubleA("SETUP", "Close", c->closeMm, iniA);
    IniWriteDoubleA("SETUP", "Offset", c->offsetMm, iniA);
    IniWriteIntA("SETUP", "CutOutlineEn", c->cutOutlineEn, iniA);
    IniWriteIntA("SETUP", "CloseEn", c->closeEn, iniA);
    IniWriteIntA("SETUP", "OffsetEn", c->offsetEn, iniA);
    IniWriteIntA("SETUP", "DashedEn", c->dashedEn, iniA);
    IniWriteDoubleA("SETUP", "LineLen", c->lineLen, iniA);
    IniWriteDoubleA("SETUP", "LineSpace", c->lineSpace, iniA);
    IniWriteDoubleA("SETUP", "KnifeOffset", c->knifeOffset, iniA);
    IniWriteIntA("BRIDGE", "MchAutoStart", c->mchAutoStart, iniA);
    IniWriteIntA("BRIDGE", "UseVendorCore", c->useVendorCore, iniA);

    Logf(L"cfg saved %s", paths->cfgPath);
    return true;
}

#include "CameraCutBridge.h"
#include "PltGuides.h"
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <vector>
struct File {std::wstring path;};
struct Event {UINT msg;WPARAM w;LPARAM l;};
static std::map<std::wstring,std::string> files;
static std::vector<Event> sent;
static bool failWrite=false, failReplace=false;
static Cfg cfg={1,1};
static int savedCutOutline=1;
static Paths paths={L"job",L"temp"};
static std::string expected;
static void Check(bool b) {if(!b) std::abort();}
void Logf(const wchar_t*,...) {}
void SetStatus(HWND,const wchar_t*) {}
Cfg *GetCfg() {return &cfg;}
bool CfgLoad(Cfg *c) {c->cutOutlineEn=savedCutOutline;return true;}
const Paths *GetPaths() {return &paths;}
HANDLE CreateFileW(const wchar_t*p,DWORD,DWORD,void*,DWORD mode,DWORD,HANDLE) {
    if(!files.count(p)) return INVALID_HANDLE_VALUE;
    if(mode==TRUNCATE_EXISTING) files[p].clear();
    return new File{p};
}
BOOL CloseHandle(HANDLE h) {delete static_cast<File*>(h);return TRUE;}
DWORD GetLastError() {return 5;}
DWORD GetFileSize(HANDLE h,DWORD*) {return files[static_cast<File*>(h)->path].size();}
BOOL GetFileSizeEx(HANDLE h,LARGE_INTEGER*s) {s->QuadPart=GetFileSize(h,nullptr);return TRUE;}
BOOL ReadFile(HANDLE h,void*buf,DWORD n,DWORD*got,void*) {
    auto &s=files[static_cast<File*>(h)->path];
    *got=std::min(n,(DWORD)s.size());
    if(*got) std::memcpy(buf,s.data(),*got);
    return TRUE;
}
BOOL WriteFile(HANDLE h,const void*buf,DWORD n,DWORD*got,void*) {
    if(failWrite) return FALSE;
    files[static_cast<File*>(h)->path]=std::string(static_cast<const char*>(buf),n);
    *got=n;return TRUE;
}
BOOL FlushFileBuffers(HANDLE) {return TRUE;}
BOOL CopyFileW(const wchar_t*a,const wchar_t*b,BOOL) {files[b]=files[a];return TRUE;}
BOOL ReplaceFileW(const wchar_t*a,const wchar_t*b,const wchar_t*,DWORD,void*,void*) {
    if(failReplace) return FALSE;
    files[a]=files[b];files.erase(b);return TRUE;
}
BOOL DeleteFileW(const wchar_t*p) {files.erase(p);return TRUE;}
UINT GetTempFileNameW(const wchar_t*,const wchar_t*,UINT,wchar_t*out) {
    std::wcscpy(out,L"tempfile");files[out]="";return 1;
}
HANDLE FindFirstFileW(const wchar_t*,WIN32_FIND_DATAW*) {return INVALID_HANDLE_VALUE;}
BOOL FindNextFileW(HANDLE,WIN32_FIND_DATAW*) {return FALSE;}
BOOL FindClose(HANDLE) {return TRUE;}
DWORD GetFileAttributesW(const wchar_t*p) {return files.count(p)?0:INVALID_FILE_ATTRIBUTES;}
DWORD GetFileAttributesA(const char*) {return INVALID_FILE_ATTRIBUTES;}
int MultiByteToWideChar(UINT,DWORD,const char*,int,wchar_t*,int) {return 0;}
bool CoreEnsureRunning() {return true;}
bool MchEnsureRunning() {return true;}
INT_PTR CoreForward(UINT msg,WPARAM w,LPARAM l) {
    Check(files[L"job"]==expected); // Assert conversion completed before forwarding.
    sent.push_back({msg,w,l});return 1;
}
std::string Read(const std::string&p) {
    std::ifstream f(p.c_str(),std::ios::binary);
    if(!f) throw std::runtime_error("Fixture missing");
    return std::string(std::istreambuf_iterator<char>(f),{});
}
int main(int argc,char **argv) {
    if(argc!=2) return 2;
    std::string dir=argv[1], original=Read(dir+"/corteLa.plt");
    expected=Read(dir+"/corteLaGuias.plt");files[L"job"]=original;
    JobOnGo(nullptr,WM_CC_PING,12,34);
    Check(sent.size()==1 && sent[0].msg==WM_CC_PING && sent[0].w==12 && sent[0].l==34);
    Check(files[L"job.before-guides"]==original);
    // A new export immediately after the first is still processed.
    files[L"job"]=original;
    JobOnGo(nullptr,WM_CC_PATH,4,9);
    Check(sent.size()==2 && sent[1].msg==WM_CC_PATH && sent[1].l==9);
    JobOnGo(nullptr,WM_CC_PING,0,0); // Already guided, unchanged.
    Check(sent.size()==3);
    for(int mode=0;mode<2;++mode) {
        files[L"job"]=original;failWrite=mode==0;failReplace=mode==1;
        JobOnGo(nullptr,WM_CC_PING,0,0);
        Check(sent.size()==3 && files[L"job"]==original && !files.count(L"tempfile"));
    }
    failWrite=failReplace=false;
    files.erase(L"job");
    JobOnGo(nullptr,WM_CC_PING,0,0);
    Check(sent.size()==3);
    expected="IN;PU0,0;PD100,100;";files[L"job"]=expected;
    JobOnGo(nullptr,WM_CC_PING,0,0);
    Check(sent.size()==4);
    // Exercise the real client's intermediate Corel HPGL through the send hook.
    std::string corel=Read(dir+"/skycutexport.job");
    Check(PltConvertGuides(corel,expected));
    files[L"job"]=corel;
    JobOnGo(nullptr,WM_CC_PING,0,0);
    Check(sent.size()==5 && files[L"job.before-guides"]==corel);
    // Turning CutOutline off between sends must bypass conversion for both dialects.
    savedCutOutline=0;
    std::string backup=files[L"job.before-guides"];
    failWrite=failReplace=true;
    for(const std::string &input:std::vector<std::string>{original,corel}) {
        expected=input;files[L"job"]=input;
        JobOnGo(nullptr,WM_CC_PING,0,0);
        Check(files[L"job"]==input && files[L"job.before-guides"]==backup);
        Check(!files.count(L"tempfile"));
    }
    Check(sent.size()==7);
    // Re-enabling the saved setting must take effect on the very next send.
    savedCutOutline=1;failWrite=failReplace=false;
    Check(PltConvertGuides(corel,expected));files[L"job"]=corel;
    JobOnGo(nullptr,WM_CC_PING,0,0);
    Check(sent.size()==8);
    std::cout<<"Send pipeline: CutOutline off/on,  conversion before forward, backup, registration parameters, repeat sends, existing guides, failed write/replace, missing and unsupported jobs passed.\n";
}

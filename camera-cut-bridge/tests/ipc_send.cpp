#include "CameraCutBridge.h"
#include <cstdlib>
#include <iostream>
#include <vector>
struct Event { UINT msg; WPARAM w; LPARAM l; };
static std::vector<Event> forwarded, posted, jobs;
static Cfg cfg={1};
static Paths paths={L"skycutexport.job",L"temp"};
static bool fileExists=true;
void SetDlgItemTextW(HWND,int,const wchar_t*) {}
void Logf(const wchar_t*,...) {}
void SetStatus(HWND,const wchar_t*) {}
Cfg *GetCfg() {return &cfg;}
bool CfgLoad(Cfg*) {return true;}
INT_PTR CoreForward(UINT m,WPARAM w,LPARAM l) {forwarded.push_back({m,w,l}); return 1;}
bool PostMessage(HWND,UINT m,WPARAM w,LPARAM l) {posted.push_back({m,w,l}); return true;}
const Paths *GetPaths() {return &paths;}
DWORD GetFileAttributesW(const wchar_t*) {return fileExists?0:INVALID_FILE_ATTRIBUTES;}
void JobOnGo(HWND,UINT m,WPARAM w,LPARAM l) {jobs.push_back({m,w,l});}
static void Check(bool condition) {if (!condition) std::abort();}
int main() {
    // Standalone pings cannot send a stale export.
    Check(HandleUserMessage(nullptr,WM_CC_PING,0,0)==1);
    Check(posted.empty() && jobs.empty() && forwarded.empty());
    // GO happens before Corel has exported. It must not run JobOnGo.
    Check(HandleUserMessage(nullptr,WM_CC_GO,0,0)==1);
    Check(posted.empty() && jobs.empty());
    Check(forwarded.size()==1 && forwarded[0].msg==WM_CC_GO);
    // The final notification preserves registration coordinates.
    Check(HandleUserMessage(nullptr,WM_CC_PING,12,34)==1);
    Check(posted.size()==1 && posted[0].msg==WM_CC_JOB_READY);
    Check(posted[0].w==12 && posted[0].l==34);
    HandleUserMessage(nullptr,WM_CC_PING,12,34);
    Check(posted.size()==1); // Duplicate notification ignored.
    HandleUserMessage(nullptr,WM_CC_GO,0,0);
    HandleUserMessage(nullptr,WM_CC_PING,0,0);
    Check(posted.size()==2); // Next send is not debounced away.
    HandleUserMessage(nullptr,WM_CC_PATH,4,9);
    Check(jobs.size()==1 && jobs[0].msg==WM_CC_PATH && jobs[0].w==4 && jobs[0].l==9);
    fileExists=false;
    HandleUserMessage(nullptr,WM_CC_PATH,4,0);
    Check(jobs.size()==1);
    std::cout<<"IPC: pre-export handshake, ready notification, parameters, duplicates, next send, legacy path passed.\n";
}

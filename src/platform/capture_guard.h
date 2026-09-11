#pragma once
#include <windows.h>
#include <string>
namespace delight {
inline bool captureWindow(HWND window){
    if(!window)return false;
    wchar_t name[128]{};GetClassNameW(window,name,128);
    if(wcsstr(name,L"ScreenClipping")||wcsstr(name,L"Snipping"))return true;
    DWORD pid=0;GetWindowThreadProcessId(window,&pid);
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!process)return false;
    wchar_t path[1024]{};DWORD count=1024;bool capture=false;
    if(QueryFullProcessImageNameW(process,0,path,&count)){
        auto base=wcsrchr(path,L'\\');base=base?base+1:path;
        capture=_wcsicmp(base,L"SnippingTool.exe")==0||_wcsicmp(base,L"ScreenClippingHost.exe")==0||_wcsicmp(base,L"ScreenSketch.exe")==0;
    }
    CloseHandle(process);return capture;
}
}

#pragma once
#include <windows.h>
#include <dwmapi.h>
#include "model.h"
namespace delight::search {
inline std::vector<Result> windows(){
    std::vector<Result> results;
    EnumWindows([](HWND window,LPARAM context)->BOOL {
        auto& list=*reinterpret_cast<std::vector<Result>*>(context);if(list.size()>=256)return FALSE;
        auto style=GetWindowLongPtrW(window,GWL_EXSTYLE);DWORD pid=0;GetWindowThreadProcessId(window,&pid);
        if(!IsWindowVisible(window)||pid==GetCurrentProcessId()||(style&(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE)))return TRUE;
        DWORD cloaked=0;if(SUCCEEDED(DwmGetWindowAttribute(window,DWMWA_CLOAKED,&cloaked,sizeof cloaked))&&cloaked)return TRUE;
        wchar_t title[1025]{},cls[256]{};GetWindowTextW(window,title,1025);if(!title[0])return TRUE;GetClassNameW(window,cls,256);
        if(wcscmp(cls,L"Progman")==0||wcscmp(cls,L"WorkerW")==0)return TRUE;
        Result r;r.title=title;r.kind=Kind::Window;r.window=reinterpret_cast<std::uintptr_t>(window);r.process=pid;r.target=cls;r.id=L"window:"+std::to_wstring(r.window)+L":"+std::to_wstring(pid);r.detail=L"Open window";
        if(HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid)){wchar_t path[1024]{};DWORD length=1024;if(QueryFullProcessImageNameW(process,0,path,&length)){std::wstring name=path;auto slash=name.find_last_of(L"\\/");if(slash!=name.npos)name.erase(0,slash+1);r.detail=name+L" / Open window";if(fold(name)==L"code.exe")r.aliases=L"vscode visual studio code";}CloseHandle(process);}
        list.push_back(std::move(r));return TRUE;
    },reinterpret_cast<LPARAM>(&results));return results;
}
inline bool switchWindow(const Result& r){
    auto window=reinterpret_cast<HWND>(r.window);DWORD pid=0;GetWindowThreadProcessId(window,&pid);wchar_t title[1025]{},cls[256]{};GetWindowTextW(window,title,1025);GetClassNameW(window,cls,256);
    if(!IsWindow(window)||pid!=r.process||r.title!=title||r.target!=cls)return false;
    if(IsIconic(window))ShowWindowAsync(window,SW_RESTORE);
    if(SetForegroundWindow(window))return true;
    FLASHWINFO flash{sizeof flash,window,FLASHW_TRAY,2,0};FlashWindowEx(&flash);return false;
}
}

#pragma once
#include <windows.h>
#include <atomic>
#include <algorithm>
#include <mutex>
#include <string>
#include <thread>
#include <cwctype>
#include <utility>
#include <vector>
#include <tlhelp32.h>
#include "services/call_latch.h"
#include "services/microphone.h"

namespace delight {
struct BrowserCallSnapshot { bool incoming=false; std::wstring caller; std::wstring app; HWND window=nullptr; };

// Discord changes its desktop window title while an incoming call rings.
// Reading that title avoids Electron's slow and often unavailable UIA tree.
class BrowserCallMonitor {
    inline static std::atomic<BrowserCallMonitor*> activeMonitor=nullptr;
    HWND notify;
    MicrophoneMonitor* microphone;
    std::atomic_bool stopping=false, enabled=true;
    HANDLE wake=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    HWINEVENTHOOK nameHook=nullptr,showHook=nullptr;
    std::thread worker;
    std::mutex mutex;
    BrowserCallSnapshot snapshot;
    std::atomic<ULONGLONG> lastWake=0;

    static void CALLBACK windowChanged(HWINEVENTHOOK,DWORD,HWND,LONG object,LONG child,DWORD,DWORD) {
        if(object!=OBJID_WINDOW||child!=CHILDID_SELF)return;
        if(auto monitor=activeMonitor.load()){
            auto now=GetTickCount64(),previous=monitor->lastWake.load();
            if(now-previous>=100&&monitor->lastWake.compare_exchange_strong(previous,now))SetEvent(monitor->wake);
        }
    }

    static BrowserCallSnapshot incomingWindow(bool whatsAppCapture) {
        BrowserCallSnapshot found;
        struct Search {BrowserCallSnapshot* found;bool whatsAppCapture;std::vector<std::pair<DWORD,std::wstring>> apps;};
        Search search{&found,whatsAppCapture,{}};
        HANDLE processes=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
        if(processes==INVALID_HANDLE_VALUE)return found;
        PROCESSENTRY32W entry{sizeof(entry)};
        if(Process32FirstW(processes,&entry))do {
            if(_wcsicmp(entry.szExeFile,L"Discord.exe")==0)search.apps.emplace_back(entry.th32ProcessID,L"Discord");
            else if(_wcsicmp(entry.szExeFile,L"WhatsApp.exe")==0||_wcsicmp(entry.szExeFile,L"WhatsApp.Root.exe")==0)
                search.apps.emplace_back(entry.th32ProcessID,L"WhatsApp");
        }while(Process32NextW(processes,&entry));
        CloseHandle(processes);
        EnumWindows([](HWND window,LPARAM context)->BOOL {
            auto& search=*reinterpret_cast<Search*>(context);
            DWORD pid=0;GetWindowThreadProcessId(window,&pid);
            auto app=std::find_if(search.apps.begin(),search.apps.end(),[pid](const auto& candidate){return candidate.first==pid;});
            if(app==search.apps.end())return TRUE;
            wchar_t title[256]{};
            GetWindowTextW(window,title,256);
            std::wstring name(title);
            for(auto& c:name)c=std::towlower(c);
            if(name!=L"voice call"&&name!=L"incoming call - discord"&&
               !(name.find(L"incoming")!=std::wstring::npos&&name.find(L"call")!=std::wstring::npos))return TRUE;
            if(app->second==L"WhatsApp"&&!IsWindowVisible(window))return TRUE;
            bool incoming=app->second==L"Discord"?name==L"incoming call - discord":
                (!search.whatsAppCapture&&name==L"voice call")||(name.find(L"incoming")!=std::wstring::npos&&name.find(L"call")!=std::wstring::npos);
            if(incoming) {
                *search.found={true,L"",app->second,window};
                return FALSE;
            }
            return TRUE;
        },reinterpret_cast<LPARAM>(&search));
        return found;
    }
    void run() {
        CallLatch latch;
        while(!stopping) {
            auto mic=microphone?microphone->get():MicrophoneSnapshot{};
            BrowserCallSnapshot next=enabled?incomingWindow(mic.active&&mic.app==L"WhatsApp"):BrowserCallSnapshot{};
            auto now=GetTickCount64();
            bool changed=false;
            { std::scoped_lock lock(mutex);
              bool active=mic.active&&mic.app==(next.incoming?next.app:snapshot.app);
              bool shown=latch.update(next.incoming,active,next.incoming||IsWindow(snapshot.window),enabled,now);
              if(!shown)next={};
              else if(!next.incoming)next=snapshot;
              changed=next.incoming!=snapshot.incoming||next.window!=snapshot.window||next.app!=snapshot.app;
              snapshot=std::move(next);
            }
            if(changed)PostMessageW(notify,WM_APP+15,0,0);
            WaitForSingleObject(wake,750);
        }
    }
public:
    explicit BrowserCallMonitor(HWND window,MicrophoneMonitor* mic,bool on):notify(window),microphone(mic),enabled(on){
        activeMonitor=this;
        nameHook=SetWinEventHook(EVENT_OBJECT_NAMECHANGE,EVENT_OBJECT_NAMECHANGE,nullptr,windowChanged,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
        showHook=SetWinEventHook(EVENT_OBJECT_SHOW,EVENT_OBJECT_SHOW,nullptr,windowChanged,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
        worker=std::thread([this]{run();});
    }
    ~BrowserCallMonitor(){activeMonitor=nullptr;if(nameHook)UnhookWinEvent(nameHook);if(showHook)UnhookWinEvent(showHook);stopping=true;SetEvent(wake);worker.join();CloseHandle(wake);}
    void enable(bool on){enabled=on;SetEvent(wake);}
    BrowserCallSnapshot get(){std::scoped_lock lock(mutex);return snapshot;}
};
}

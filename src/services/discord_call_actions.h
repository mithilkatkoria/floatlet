#pragma once
#include <windows.h>
#include <UIAutomation.h>
#include <tlhelp32.h>
#include <algorithm>
#include <cwctype>
#include <string>
#include <vector>

namespace delight {
enum class DiscordCallActionStatus { Invoked, NoMainWindow, AutomationUnavailable, NoVisibleButton, InvokeFailed };

inline void recordDiscordCallAction(DiscordCallActionStatus status) {
    wchar_t folder[MAX_PATH]{};
    if(!GetTempPathW(MAX_PATH,folder))return;
    HANDLE file=CreateFileW((std::wstring(folder)+L"floatlet-call-action.log").c_str(),FILE_APPEND_DATA,FILE_SHARE_READ,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return;
    const char* result=status==DiscordCallActionStatus::Invoked?"invoked":status==DiscordCallActionStatus::NoMainWindow?"no-main-window":status==DiscordCallActionStatus::AutomationUnavailable?"automation-unavailable":status==DiscordCallActionStatus::NoVisibleButton?"no-visible-button":"invoke-failed";
    std::string line=std::to_string(GetTickCount64())+" "+result+"\r\n";
    DWORD written=0;WriteFile(file,line.data(),DWORD(line.size()),&written,nullptr);CloseHandle(file);
}

inline HWND discordMainWindow() {
    std::vector<DWORD> pids;
    HANDLE processes=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(processes==INVALID_HANDLE_VALUE)return nullptr;
    PROCESSENTRY32W entry{sizeof(entry)};
    if(Process32FirstW(processes,&entry))do {
        if(_wcsicmp(entry.szExeFile,L"Discord.exe")==0)pids.push_back(entry.th32ProcessID);
    }while(Process32NextW(processes,&entry));
    CloseHandle(processes);
    struct Candidate {const std::vector<DWORD>* pids;HWND window=nullptr;int score=0;long long area=0;} candidate{&pids};
    EnumWindows([](HWND window,LPARAM context)->BOOL {
        auto& best=*reinterpret_cast<Candidate*>(context);
        DWORD pid=0;GetWindowThreadProcessId(window,&pid);
        if(std::find(best.pids->begin(),best.pids->end(),pid)==best.pids->end())return TRUE;
        wchar_t title[256]{};GetWindowTextW(window,title,256);
        std::wstring name(title);for(auto& ch:name)ch=std::towlower(ch);
        if(name.empty()||name==L"incoming call - discord")return TRUE;
        int score=name.ends_with(L" - discord")?3:name.find(L"discord")!=std::wstring::npos?2:0;
        if(score==0)return TRUE;
        RECT bounds{};GetWindowRect(window,&bounds);
        long long area=std::max(0L,bounds.right-bounds.left)*std::max(0L,bounds.bottom-bounds.top);
        if(score>best.score||(score==best.score&&area>best.area)){
            best.window=window;best.score=score;best.area=area;
        }
        return TRUE;
    },reinterpret_cast<LPARAM>(&candidate));
    return candidate.window;
}

inline DiscordCallActionStatus invokeDiscordCallAction(bool answer) {
    HWND window=discordMainWindow();if(!window)return DiscordCallActionStatus::NoMainWindow;
    ShowWindowAsync(window,SW_RESTORE);
    SetForegroundWindow(window);
    Sleep(120);
    HRESULT initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    IUIAutomation* automation=nullptr;
    if(FAILED(CoCreateInstance(CLSID_CUIAutomation,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&automation)))||!automation){
        if(SUCCEEDED(initialized))CoUninitialize();return DiscordCallActionStatus::AutomationUnavailable;
    }
    IUIAutomation2* settings=nullptr;
    if(SUCCEEDED(automation->QueryInterface(IID_PPV_ARGS(&settings)))){
        settings->put_ConnectionTimeout(1200);settings->put_TransactionTimeout(1500);settings->Release();
    }
    VARIANT type{};type.vt=VT_I4;type.lVal=UIA_ButtonControlTypeId;
    IUIAutomationCondition* condition=nullptr;
    automation->CreatePropertyCondition(UIA_ControlTypePropertyId,type,&condition);
    bool invoked=false,visibleButton=false;
    const auto deadline=GetTickCount64()+2400;
    do {
        IUIAutomationElement* root=nullptr;
        HRESULT result=automation->ElementFromHandle(window,&root);
        if(SUCCEEDED(result)&&root&&condition){
            IUIAutomationElementArray* buttons=nullptr;
            result=root->FindAll(TreeScope_Descendants,condition,&buttons);
            if(SUCCEEDED(result)&&buttons){int count=0;buttons->get_Length(&count);
                for(int i=0;i<std::min(count,512)&&!invoked;++i){IUIAutomationElement* item=nullptr;
                    if(FAILED(buttons->GetElement(i,&item))||!item)continue;
                    BSTR raw=nullptr;
                    if(SUCCEEDED(item->get_CurrentName(&raw))&&raw){
                        std::wstring name(raw);SysFreeString(raw);
                        for(auto& ch:name)ch=std::towlower(ch);
                        bool match=answer?
                            (name.find(L"accept")!=std::wstring::npos||name.find(L"answer")!=std::wstring::npos||name.find(L"join call")!=std::wstring::npos):
                            (name.find(L"decline")!=std::wstring::npos||name.find(L"reject")!=std::wstring::npos);
                        if(match){BOOL enabled=FALSE,offscreen=TRUE;
                            item->get_CurrentIsEnabled(&enabled);item->get_CurrentIsOffscreen(&offscreen);
                            if(enabled&&!offscreen){visibleButton=true;IUnknown* pattern=nullptr;
                                if(SUCCEEDED(item->GetCurrentPattern(UIA_InvokePatternId,&pattern))&&pattern){
                                    IUIAutomationInvokePattern* invoke=nullptr;
                                    if(SUCCEEDED(pattern->QueryInterface(IID_PPV_ARGS(&invoke)))&&invoke){
                                        invoked=SUCCEEDED(invoke->Invoke());invoke->Release();
                                    }
                                    pattern->Release();
                                }
                            }
                        }
                    }
                    item->Release();
                }
                buttons->Release();
            }
            root->Release();
        }
        if(!invoked&&GetTickCount64()<deadline)Sleep(120);
    }while(!invoked&&GetTickCount64()<deadline&&IsWindow(window));
    if(condition)condition->Release();automation->Release();
    if(SUCCEEDED(initialized))CoUninitialize();
    return invoked?DiscordCallActionStatus::Invoked:visibleButton?DiscordCallActionStatus::InvokeFailed:DiscordCallActionStatus::NoVisibleButton;
}
}

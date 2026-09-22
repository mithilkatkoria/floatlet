#include "everything.h"
#include "everything_protocol.h"
#include <chrono>
#include <cstring>
#include <filesystem>
namespace delight::search {
namespace {
std::wstring expression(const std::wstring& query,bool approximate){auto parts=tokens(query);std::wstring re=L"^";if(approximate){if(parts.size()!=1||parts[0].size()<5)return {};auto& p=parts[0];re=L"\""+p.substr(0,3)+L"\" | \""+p.substr(p.size()-3)+L"\"";}else {re.clear();for(auto p:parts){p.erase(std::remove(p.begin(),p.end(),L'"'),p.end());re+=L"\""+p+L"\" ";}}return re;}
}
Everything::Installation Everything::installation(){
    Installation found;
    for(auto hive:{HKEY_CURRENT_USER,HKEY_LOCAL_MACHINE})for(auto view:{KEY_WOW64_64KEY,KEY_WOW64_32KEY}){
        HKEY key=nullptr;
        if(RegOpenKeyExW(hive,L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Everything",0,KEY_READ|view,&key)!=ERROR_SUCCESS)continue;
        auto value=[&](const wchar_t* name){wchar_t text[32768]{};DWORD bytes=sizeof text;return RegGetValueW(key,nullptr,name,RRF_RT_REG_SZ,nullptr,text,&bytes)==ERROR_SUCCESS?std::wstring(text):std::wstring();};
        auto location=value(L"InstallLocation"),name=fold(value(L"DisplayName"));RegCloseKey(key);
        if(!location.empty()){auto executable=std::filesystem::path(location)/L"Everything.exe";auto attrs=GetFileAttributesW(executable.c_str());if(attrs!=INVALID_FILE_ATTRIBUTES&&!(attrs&FILE_ATTRIBUTE_DIRECTORY)){bool lite=name.find(L"lite")!=name.npos;if(found.executable.empty()||(!lite&&found.lite))found={executable.wstring(),lite};}}
    }
    if(!found.executable.empty())return found;
    for(auto variable:{L"ProgramFiles",L"ProgramFiles(x86)",L"LOCALAPPDATA"}){wchar_t base[32768]{};auto count=GetEnvironmentVariableW(variable,base,32768);if(!count||count>=32768)continue;auto executable=std::filesystem::path(base)/L"Everything"/L"Everything.exe";auto attrs=GetFileAttributesW(executable.c_str());if(attrs!=INVALID_FILE_ATTRIBUTES&&!(attrs&FILE_ATTRIBUTE_DIRECTORY))return {executable.wstring(),false};}
    return found;
}
Everything::Everything(){WNDCLASSW c{};c.hInstance=GetModuleHandleW(nullptr);c.lpszClassName=L"Floatlet.Search.IPC";c.lpfnWndProc=proc;RegisterClassW(&c);window=CreateWindowExW(0,c.lpszClassName,L"",0,0,0,0,0,nullptr,nullptr,c.hInstance,this);if(window)ChangeWindowMessageFilterEx(window,WM_COPYDATA,MSGFLT_ALLOW,nullptr);}
Everything::~Everything(){if(window)DestroyWindow(window);}
LRESULT CALLBACK Everything::proc(HWND h,UINT m,WPARAM w,LPARAM l){auto p=reinterpret_cast<Everything*>(GetWindowLongPtrW(h,GWLP_USERDATA));if(m==WM_NCCREATE){p=static_cast<Everything*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(p));}if(m!=WM_COPYDATA||!p)return DefWindowProcW(h,m,w,l);
    auto packet=reinterpret_cast<COPYDATASTRUCT*>(l);++p->replyPackets;
    if(packet->dwData!=p->expected){++p->stalePackets;return FALSE;}
    auto decoded=decodeEverythingReply(packet->lpData,packet->cbData);
    if(!decoded){++p->invalidPackets;return FALSE;}
    p->received=std::move(*decoded);p->complete=true;return TRUE;
}
std::vector<Result> Everything::query(const std::wstring& text,bool path,unsigned generation,const std::atomic<unsigned>& latest,bool approximate,unsigned timeoutMs){
    received.clear();complete=false;replyPackets=stalePackets=invalidPackets=0;server=FindWindowW(L"EVERYTHING_TASKBAR_NOTIFICATION",nullptr);if(!server)server=FindWindowW(L"EVERYTHING_TASKBAR_NOTIFICATION_(1.5a)",nullptr);
    present=server!=nullptr;if(!server){ready=false;status=L"Everything is not running, uses a custom instance, or IPC is unavailable";return {};}
    auto search=expression(text,approximate);if(search.empty()||!window)return {};expected=++serial;std::vector<unsigned char> bytes(20+(search.size()+1)*2);DWORD fields[]={static_cast<DWORD>(reinterpret_cast<UINT_PTR>(window)),expected,DWORD(path?4:0),0,512};memcpy(bytes.data(),fields,20);memcpy(bytes.data()+20,search.c_str(),(search.size()+1)*2);
    COPYDATASTRUCT data{2,static_cast<DWORD>(bytes.size()),bytes.data()};DWORD_PTR accepted=0;
    SetLastError(ERROR_SUCCESS);auto sent=SendMessageTimeoutW(server,WM_COPYDATA,reinterpret_cast<WPARAM>(window),reinterpret_cast<LPARAM>(&data),SMTO_ABORTIFHUNG,500,&accepted);auto sendError=GetLastError();
    // A send timeout does not prove rejection: the server can still reply later.
    if((sent&&!accepted)||(!sent&&sendError!=ERROR_TIMEOUT&&sendError!=ERROR_SUCCESS)){ready=false;status=L"Everything IPC unavailable. Check that the full desktop version is running.";return {};}
    auto deadline=GetTickCount64()+std::clamp(timeoutMs,100u,15000u);while(!complete&&generation==latest.load()&&GetTickCount64()<deadline){MsgWaitForMultipleObjectsEx(0,nullptr,20,QS_ALLINPUT,MWMO_INPUTAVAILABLE);MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}}
    if(!complete){ready=false;status=invalidPackets?L"Everything returned an invalid response. Try again.":L"Everything is taking longer than expected. Try again.";return {};}ready=true;status=L"Everything connected";return std::move(received);
}
}

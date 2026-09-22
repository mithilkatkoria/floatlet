#include <windows.h>
#include <ole2.h>
#include <psapi.h>
#include <winrt/base.h>
#include "search/launcher.h"
#include "search/everything.h"
#include "storage/settings.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
using namespace delight;
using namespace delight::search;
namespace {
int checks=0;
void require(bool condition,const char* label){++checks;if(!condition)throw std::runtime_error(label);}
template<class F> bool pump(Launcher& launcher,F done,unsigned timeout=15000){auto end=GetTickCount64()+timeout;do{MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){if(!launcher.translate(msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}if(done())return true;MsgWaitForMultipleObjectsEx(0,nullptr,10,QS_ALLINPUT,MWMO_INPUTAVAILABLE);}while(GetTickCount64()<end);return false;}
int find(HWND list,const wchar_t* name){int count=int(SendMessageW(list,LB_GETCOUNT,0,0));for(int i=0;i<count;++i){auto n=SendMessageW(list,LB_GETTEXTLEN,i,0);if(n<0||n>32760)continue;std::wstring text(size_t(n)+1,L'\0');SendMessageW(list,LB_GETTEXT,i,reinterpret_cast<LPARAM>(text.data()));if(text.starts_with(name))return i;}return -1;}
}
int main(int argc,char** argv){bool full=argc==2&&std::string(argv[1])=="--full-providers";SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);if(FAILED(OleInitialize(nullptr)))return 1;auto root=std::filesystem::temp_directory_path()/(L"FloatletLauncherFixture"+std::to_wstring(GetCurrentProcessId()));int code=0;
    try{
        std::filesystem::create_directories(root/L"Certificates");auto original=root/L"Certificates"/L"Maths Certificate.txt";std::ofstream(original)<<"Floatlet disposable fixture";
        {Everything probe;std::atomic<unsigned> serial=1;bool indexed=false;for(int attempt=0;attempt<15&&!indexed;++attempt){auto rows=probe.query(root.filename().wstring(),true,1,serial);for(auto& r:rows)if(r.target==original.wstring())indexed=true;if(!indexed)Sleep(200);}if(!indexed)std::wcout<<L"Fixture prerequisite: present="<<probe.present<<L" ready="<<probe.ready<<L" status="<<probe.status<<std::endl;require(indexed,"Everything has not indexed the disposable fixture");}
        WNDCLASSW cls{};cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"Floatlet.Search.TestOwner";cls.lpfnWndProc=DefWindowProcW;RegisterClassW(&cls);
        HWND owner=CreateWindowExW(WS_EX_APPWINDOW,cls.lpszClassName,L"Floatlet Search test owner",WS_POPUP,0,0,1,1,nullptr,nullptr,cls.hInstance,nullptr);
        Settings settings;int added=0,executed=0;Result command;command.id=L"command:test";command.kind=Kind::Command;command.title=L"Fixture command";command.command=7;
        {
            Launcher launcher(owner,{command},[&](int id){executed=id;},[&](const std::wstring& path){require(path==original.wstring(),"wrong shelf target");require(addReference(settings,path),"reference rejected");save(root/L"settings.json",settings);require(load(root/L"settings.json").paths.size()==1,"reference persistence");settings.paths.clear();++added;return true;});
            Options options;options.applications=full;options.windows=full;options.fuzzy=full;launcher.configure(options);std::cout<<"All providers enabled="<<full<<std::endl;
            launcher.show();auto window=FindWindowW(L"Floatlet.Search",nullptr);require(window!=nullptr,"launcher HWND");auto edit=GetDlgItem(window,401),list=GetDlgItem(window,402);
            require(pump(launcher,[&]{return find(list,L"Fixture command")>=0;}),"empty query commands");MSG enter{edit,WM_KEYDOWN,VK_RETURN};launcher.translate(enter);require(executed==7,"command activation");
            std::vector<double> latency;DWORD handlesBefore=0,handlesAfter=0,gdiBefore=0,gdiAfter=0;
            for(int cycle=0;cycle<101;++cycle){auto start=std::chrono::steady_clock::now();launcher.show();require(GetFocus()==edit,"immediate focus");SetWindowTextW(edit,(root.filename().wstring()+L" Maths").c_str());bool found=pump(launcher,[&]{return find(list,L"Maths Certificate.txt")>=0;});if(!found)std::cout<<"Diagnostic "<<launcher.diagnostics()<<std::endl;require(found,"indexed fixture missing");if(cycle==0){auto settled=GetTickCount64()+600;pump(launcher,[&]{return GetTickCount64()>=settled;},1000);}auto milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
                SendMessageW(list,LB_SETCURSEL,find(list,L"Maths Certificate.txt"),0);SendMessageW(window,WM_COMMAND,404,0);require(!IsWindowVisible(window),"shelf did not dismiss search");require(std::filesystem::file_size(original)==27,"original content changed");
                if(cycle%25==0){DWORD h=0;GetProcessHandleCount(GetCurrentProcess(),&h);std::cout<<"Cycle "<<cycle<<" handles="<<h<<std::endl;}if(cycle==0){GetProcessHandleCount(GetCurrentProcess(),&handlesBefore);gdiBefore=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);}else latency.push_back(milliseconds);
            }
            GetProcessHandleCount(GetCurrentProcess(),&handlesAfter);std::cout<<"Handles warm="<<handlesBefore<<" after100="<<handlesAfter<<std::endl;gdiAfter=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);require(added==101,"shelf cycle count");require(handlesAfter<=handlesBefore+16,"handle growth");require(gdiAfter<=gdiBefore+8,"GDI growth");
            for(int i=0;i<100;++i){launcher.show();require(GetFocus()==edit,"rapid reopen focus");launcher.show();require(!IsWindowVisible(window),"rapid close");}
            launcher.show();launcher.dismiss();require(!IsWindowVisible(window),"suspend dismissal");
            launcher.show();auto disabled=options;disabled.enabled=false;launcher.configure(disabled);require(!IsWindowVisible(window),"disable dismissal");launcher.configure(options);
            launcher.show();for(int i=0;i<100;++i)SetWindowTextW(edit,i%2?L"obsolete":L"Fixture command");SetWindowTextW(edit,L"Fixture command");require(pump(launcher,[&]{return find(list,L"Fixture command")>=0;}),"latest generation missing");require(find(list,L"Maths Certificate.txt")==-1,"stale result survived");SendMessageW(window,WM_CLOSE,0,0);
            std::sort(latency.begin(),latency.end());PROCESS_MEMORY_COUNTERS_EX memory{};memory.cb=sizeof memory;GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof memory);
            std::cout<<"PASS checks="<<checks<<" shelf_cycles=100 rapid_open_close=100\n"<<"Fixture query plus show milliseconds: median="<<latency[50]<<" p95="<<latency[94]<<" worst="<<latency.back()<<'\n'<<"Handles before="<<handlesBefore<<" after="<<handlesAfter<<" GDI before="<<gdiBefore<<" after="<<gdiAfter<<'\n'<<"Launcher test process private_bytes="<<memory.PrivateUsage<<" working_set="<<memory.WorkingSetSize<<'\n';
        }DestroyWindow(owner);
    }catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';code=1;}
    // This absolute path is the uniquely named directory created by this test.
    if(root.is_absolute()&&root.parent_path()==std::filesystem::temp_directory_path()&&root.filename().wstring().starts_with(L"FloatletLauncherFixture"))std::filesystem::remove_all(root);
    OleUninitialize();return code;
}

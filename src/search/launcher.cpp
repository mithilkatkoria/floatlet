#include "launcher.h"
#include "everything.h"
#include "window_provider.h"
#include "icons.h"
#include "paint.h"
#include "placement.h"
#include "open_cue.h"
#include "ui/monitor_follow.h"
#include "indexed_fallback.h"
#include "local_index.h"
#include <winrt/Windows.UI.ViewManagement.h>
#include "platform/shelf.h"
#include <unordered_set>
#include <shlobj.h>
#include <propkey.h>
#include <shellapi.h>
#include <commctrl.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <ShellScalingApi.h>
#include <winrt/base.h>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <optional>
#include <atomic>
namespace delight::search {
namespace {constexpr UINT Ready=WM_APP+80;constexpr int Edit=401,List=402;}
struct Launcher::Impl {
    HWND owner{},hwnd{},edit{},list{},status{},previous{},heading{},openButton{},addButton{},actionButton{};
    winrt::Windows::UI::ViewManagement::UISettings appearance;ULONGLONG animationStart=0;bool highContrast=false;float textScale=1;COLORREF surface=RGB(32,34,36),inputSurface=RGB(32,34,36),textColor=RGB(245,245,243),secondary=RGB(173,177,181),selection=RGB(53,56,59),accent=RGB(227,231,235);
    Paint painter;HMONITOR display=nullptr;MonitorFollow follower;bool placing=false,opening=false;int focusAttempts=0;HHOOK outsideHook{};inline static Impl* hookOwner=nullptr;HFONT inputFont{},smallFont{},monoFont{};HFONT font{},symbolFont{};HBRUSH background=CreateSolidBrush(RGB(14,18,25)),inputBackground=CreateSolidBrush(RGB(26,33,43));
    std::vector<Result> commands,apps,shown;
    std::function<void(int)> execute;std::function<bool(const std::wstring&)> add;bool actionMenu=false;
    std::mutex mutex;std::condition_variable wake;
    std::optional<std::pair<unsigned,std::wstring>> pending;
    std::vector<Result> delivered;unsigned deliveredGeneration=0;bool deliveredComplete=false;
    std::atomic<unsigned> stage=0,generation=0,fileRevision=0;std::atomic<bool> iconsBusy=false;std::atomic<bool> appWorkerRunning=false;bool stop=false,loaded=false,review=false,navigated=false;std::wstring providerStatus,deliveredStatus,queryText;
    std::condition_variable iconWake;std::optional<std::pair<unsigned,std::vector<Result>>> iconPending;std::vector<Result> iconDelivered;unsigned iconGeneration=0;std::unique_ptr<LocalIndex> localIndex;std::thread worker,appWorker,iconWorker;UINT dpi=96;Options options;ULONGLONG catalogueTime=0;
    Impl(HWND parent,std::vector<Result> c,std::function<void(int)> run,std::function<bool(const std::wstring&)> shelf):owner(parent),commands(std::move(c)),execute(std::move(run)),add(std::move(shelf)) {
        WNDCLASSW cls{};cls.hInstance=GetModuleHandleW(nullptr);cls.lpfnWndProc=proc;cls.lpszClassName=L"Floatlet.Search";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&cls);
        review=(GetWindowLongPtrW(owner,GWL_EXSTYLE)&WS_EX_APPWINDOW)!=0;
        hwnd=CreateWindowExW((review?WS_EX_APPWINDOW:WS_EX_TOOLWINDOW)|WS_EX_TOPMOST|WS_EX_LAYERED,cls.lpszClassName,L"Floatlet Search",WS_POPUP,0,0,660,540,review?nullptr:owner,nullptr,cls.hInstance,this);
        if(!hwnd)winrt::throw_last_error();
        heading=CreateWindowExW(0,L"STATIC",L"Quick actions",WS_CHILD|WS_VISIBLE,0,0,0,0,hwnd,nullptr,cls.hInstance,nullptr);
        edit=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,0,0,0,0,hwnd,reinterpret_cast<HMENU>(INT_PTR(Edit)),cls.hInstance,nullptr);
        SetWindowSubclass(edit,editProc,1,reinterpret_cast<DWORD_PTR>(this));SendMessageW(edit,EM_SETLIMITTEXT,256,0);
        list=CreateWindowExW(0,L"LISTBOX",L"Search results",WS_CHILD|WS_VISIBLE|WS_TABSTOP|LBS_NOTIFY|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS|LBS_NOINTEGRALHEIGHT,0,0,0,0,hwnd,reinterpret_cast<HMENU>(INT_PTR(List)),cls.hInstance,nullptr);
        status=CreateWindowExW(0,L"STATIC",L"Enter  Open     Tab  Actions     Shift+Enter  Add to Floatlet",WS_CHILD|WS_VISIBLE,0,0,0,0,hwnd,nullptr,cls.hInstance,nullptr);
        auto button=[&](const wchar_t* title,int id){return CreateWindowExW(0,L"BUTTON",title,WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,0,0,hwnd,reinterpret_cast<HMENU>(INT_PTR(id)),cls.hInstance,nullptr);};
        openButton=button(L"Open  \u21b5",403);addButton=button(L"Add to tray",404);actionButton=button(L"Actions  \u21e5",405);
        BOOL dark=TRUE;DwmSetWindowAttribute(hwnd,20,&dark,sizeof dark);DWORD corner=2;DwmSetWindowAttribute(hwnd,33,&corner,sizeof corner);
        localIndex=std::make_unique<LocalIndex>([this]{PostMessageW(hwnd,WM_APP+81,0,0);});
        worker=std::thread([this]{work();});iconWorker=std::thread([this]{loadIcons();});
    }
    ~Impl(){hide(false);{std::lock_guard lock(mutex);stop=true;}wake.notify_one();iconWake.notify_one();worker.join();iconWorker.join();if(appWorker.joinable())appWorker.join();localIndex.reset();DestroyWindow(hwnd);DeleteObject(inputFont);DeleteObject(smallFont);DeleteObject(monoFont);DeleteObject(font);DeleteObject(symbolFont);DeleteObject(background);DeleteObject(inputBackground);}
    void catalogue(){
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);
        std::vector<Result> discovered;
        winrt::com_ptr<IShellItem> folder;
        if(FAILED(SHCreateItemInKnownFolder(FOLDERID_AppsFolder,0,nullptr,IID_PPV_ARGS(folder.put()))))return;
        winrt::com_ptr<IEnumShellItems> items;if(FAILED(folder->BindToHandler(nullptr,BHID_EnumItems,IID_PPV_ARGS(items.put()))))return;
        while(discovered.size()<4096){winrt::com_ptr<IShellItem> item;if(items->Next(1,item.put(),nullptr)!=S_OK)break;PWSTR name=nullptr,target=nullptr;
            if(SUCCEEDED(item->GetDisplayName(SIGDN_NORMALDISPLAY,&name))&&SUCCEEDED(item->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING,&target))){Result r;r.id=target;r.title=name;r.target=target;r.detail=L"Application";auto words=tokens(r.title);std::wstring initials;for(auto& word:words)if(!word.empty())initials+=word[0];r.aliases=initials;if(words.size()>1){initials.pop_back();r.aliases+=L" "+initials+words.back();}if(auto rich=item.try_as<IShellItem2>()){PWSTR id=nullptr;if(SUCCEEDED(rich->GetString(PKEY_AppUserModel_ID,&id))&&id){r.activationId=id;CoTaskMemFree(id);}}discovered.push_back(std::move(r));}CoTaskMemFree(name);CoTaskMemFree(target);
        }
        {std::lock_guard lock(mutex);apps=std::move(discovered);loaded=true;catalogueTime=GetTickCount64();}
        PostMessageW(hwnd,WM_APP+83,0,0);
    }
    static LRESULT CALLBACK editProc(HWND control,UINT message,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR data){
        auto p=reinterpret_cast<Impl*>(data);auto result=DefSubclassProc(control,message,w,l);
        if(message==WM_PAINT&&GetWindowTextLengthW(control)==0){
            auto dc=GetDC(control);RECT bounds;GetClientRect(control,&bounds);
            auto old=SelectObject(dc,p->inputFont);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,p->secondary);
            bounds.left=p->px(5);DrawTextW(dc,L"Search apps, files and more",-1,&bounds,DT_LEFT|DT_SINGLELINE|DT_NOPREFIX|DT_END_ELLIPSIS);
            SelectObject(dc,old);ReleaseDC(control,dc);
        }
        return result;
    }
    void refreshApps(){
        bool refresh=false;{std::lock_guard lock(mutex);refresh=!loaded||GetTickCount64()-catalogueTime>=300000;}
        if(refresh&&!appWorkerRunning.exchange(true)){if(appWorker.joinable())appWorker.join();appWorker=std::thread([this]{try{catalogue();}catch(...){}appWorkerRunning=false;});}
    }
    void requestIcons(const std::vector<Result>& results,unsigned version){
        std::lock_guard lock(mutex);if(version!=generation.load())return;
        auto count=std::min(size_t(12),results.size());iconPending=std::pair(version,std::vector<Result>(results.begin(),results.begin()+count));iconWake.notify_one();
    }
    void loadIcons(){
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);{Icons icons;for(;;){std::pair<unsigned,std::vector<Result>> request;
            {std::unique_lock lock(mutex);iconWake.wait(lock,[&]{return stop||iconPending.has_value();});if(stop)break;request=std::move(*iconPending);iconPending.reset();iconsBusy=true;}
            for(auto& result:request.second){if(request.first!=generation.load())break;try{icons.fill(result);}catch(...){} }
            {std::lock_guard lock(mutex);iconsBusy=false;if(stop)break;if(request.first!=generation.load())continue;iconDelivered=std::move(request.second);iconGeneration=request.first;}
            PostMessageW(hwnd,WM_APP+84,0,0);
        }}winrt::uninit_apartment();
    }
    void acceptIcons(){
        std::vector<Result> loadedIcons;{std::lock_guard lock(mutex);if(iconGeneration!=generation.load())return;loadedIcons=std::move(iconDelivered);iconGeneration=0;}
        for(auto& item:shown)for(auto& loadedIcon:loadedIcons)if(item.id==loadedIcon.id&&loadedIcon.icon){item.icon=loadedIcon.icon;break;}
        InvalidateRect(list,nullptr,FALSE);
    }
    void work(){winrt::init_apartment(winrt::apartment_type::single_threaded);{Everything everything;struct CachedFiles{std::wstring query;ULONGLONG time;std::vector<Result> results;};std::vector<CachedFiles> cache;unsigned cacheVersion=0;ULONGLONG providerRetryAt=0;for(;;){std::pair<unsigned,std::wstring> request;Options config;{std::unique_lock lock(mutex);stage=0;wake.wait(lock,[&]{return stop||pending.has_value();});if(stop)break;request=std::move(*pending);pending.reset();config=options;}
        try{if(cacheVersion!=fileRevision.load()){cache.clear();cacheVersion=fileRevision.load();providerRetryAt=0;}std::wstring localStatus=L"Floatlet built-in file index";std::vector<Result> results;stage=2;
        std::vector<Result> appSnapshot;{std::lock_guard lock(mutex);if(config.applications)appSnapshot=apps;}
        for(auto* source:{&commands,&appSnapshot})for(auto r:*source){if(request.first!=generation.load())break;r.score=rank(r,request.second,config.fuzzy);if(r.score)results.push_back(std::move(r));}

        auto publish=[&](bool complete=false){sort(results);{std::lock_guard lock(mutex);if(request.first!=generation.load())return;delivered=results;deliveredGeneration=request.first;deliveredComplete=complete;deliveredStatus=config.provider==1?localStatus:everything.status;}PostMessageW(hwnd,Ready,0,0);};
        publish();
        requestIcons(results,request.first);
        if(config.windows&&!tokens(request.second).empty()&&request.first==generation.load()){for(auto r:windows()){r.score=rank(r,request.second,config.fuzzy);if(r.score)results.push_back(std::move(r));}publish();}
        if(config.files&&config.provider==1&&!tokens(request.second).empty()&&request.first==generation.load()){stage=4;auto files=localIndex->query(request.second,request.first,generation,localStatus);results.insert(results.end(),std::make_move_iterator(files.begin()),std::make_move_iterator(files.end()));publish();}
        if(config.files&&config.provider==0&&!tokens(request.second).empty()&&request.first==generation.load()){
            std::unordered_set<std::wstring> seen;for(auto& r:results)seen.insert(r.id);
            auto merge=[&](std::vector<Result> found){for(auto& r:found){if(request.first!=generation.load())break;r.score=rank(r,request.second,config.fuzzy);if(r.score&&seen.insert(r.id).second)results.push_back(std::move(r));}};
            auto hit=std::find_if(cache.begin(),cache.end(),[&](auto& c){return c.query==request.second&&GetTickCount64()-c.time<5000;});
            if(hit!=cache.end())merge(hit->results);
            else{
                // Apps render immediately. Wait briefly for typing to settle before requesting
                // an external filesystem search; superseded IPC queries still cost the provider work.
                {std::unique_lock lock(mutex);if(wake.wait_for(lock,std::chrono::milliseconds(90),[&]{return stop||request.first!=generation.load();}))continue;}
                if(GetTickCount64()<providerRetryAt){everything.status=L"Everything is busy. Try again shortly, or press Ctrl+R to retry.";publish(true);continue;}
                stage=3;auto files=everything.query(request.second,false,request.first,generation,false,2000);
                // Publish direct names before the more expensive ancestor-path expansion.
                merge(files);publish();
                requestIcons(results,request.first);
                if(everything.ready&&request.first==generation.load()){
                    auto paths=everything.query(request.second,true,request.first,generation,false,4000);
                    files.insert(files.end(),std::make_move_iterator(paths.begin()),std::make_move_iterator(paths.end()));
                }
                if(!everything.ready&&!everything.present&&request.first==generation.load()){stage=4;files=indexedFallback(request.second,request.first,generation,everything.status);}
                std::unordered_set<std::wstring> uniqueFiles;std::erase_if(files,[&](auto& r){return !uniqueFiles.insert(r.id).second;});
                for(auto& r:files)r.score=rank(r,request.second,config.fuzzy);sort(files);
                size_t bytes=0;for(auto& r:files)bytes+=(r.title.size()+r.detail.size()+r.target.size()+r.id.size())*sizeof(wchar_t);
                if(!files.empty()&&bytes<128*1024&&request.first==generation.load()){if(cache.size()>=8)cache.erase(cache.begin());cache.push_back({request.second,GetTickCount64(),files});}
                merge(std::move(files));
            }
            if(everything.present&&!everything.ready&&request.first==generation.load())providerRetryAt=GetTickCount64()+5000;
            publish();
            if(config.fuzzy&&everything.ready&&results.size()<8&&request.first==generation.load()){merge(everything.query(request.second,false,request.first,generation,true,2000));publish();}

        }
        publish(true);requestIcons(results,request.first);
        }catch(...){if(request.first==generation.load()){{std::lock_guard lock(mutex);delivered.clear();deliveredGeneration=request.first;deliveredComplete=true;deliveredStatus=L"A search provider could not finish. Try another query.";}PostMessageW(hwnd,Ready,0,0);}}
    }}winrt::uninit_apartment();}
    void query(){navigated=false;wchar_t text[257]{};GetWindowTextW(edit,text,257);if(queryText!=text){queryText=text;SetWindowTextW(status,L"Searching on your PC...");}SetWindowTextW(heading,queryText.empty()?L"Quick actions":L"Search results");auto current=++generation;{std::lock_guard lock(mutex);pending=std::pair(current,std::wstring(text));}wake.notify_one();}
    void accept(){bool complete=false;std::vector<Result> results;{std::lock_guard lock(mutex);if(deliveredGeneration!=generation.load())return;results=std::move(delivered);providerStatus=deliveredStatus;complete=deliveredComplete;deliveredGeneration=0;}for(auto& result:results)if(!result.icon)for(auto& old:shown)if(old.id==result.id){result.icon=old.icon;break;}bool sameRows=results.size()==shown.size();if(sameRows)for(size_t i=0;i<results.size();++i)if(results[i].id!=shown[i].id||results[i].title!=shown[i].title||results[i].detail!=shown[i].detail){sameRows=false;break;}if(sameRows&&!results.empty()){if(options.provider==1)SetWindowTextW(status,providerStatus.c_str());shown=std::move(results);fitResults();InvalidateRect(list,nullptr,FALSE);updateActions();return;}std::wstring selected;int index=int(SendMessageW(list,LB_GETCURSEL,0,0));if(navigated&&index>=0&&index<int(shown.size()))selected=shown[index].id;
        SetWindowTextW(heading,queryText.empty()?L"Quick actions":results.empty()?(complete?L"No matches":L"Searching..."):L"Search results");if(complete&&results.empty()&&!queryText.empty()&&(providerStatus==L"Everything connected"||providerStatus.empty()))providerStatus=L"Try a shorter name, or check your search locations in Preferences.";if(navigated){std::vector<Result> stable;for(auto& old:shown){auto it=std::find_if(results.begin(),results.end(),[&](auto& r){return r.id==old.id;});if(it!=results.end()){stable.push_back(*it);results.erase(it);}}for(auto& r:results)stable.push_back(std::move(r));results=std::move(stable);}shown=std::move(results);SendMessageW(list,WM_SETREDRAW,FALSE,0);SendMessageW(list,LB_RESETCONTENT,0,0);int select=0;for(size_t i=0;i<shown.size();++i){auto label=shown[i].title+L", "+shown[i].detail;SendMessageW(list,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));if(shown[i].id==selected)select=int(i);}SendMessageW(list,LB_SETCURSEL,select,0);SendMessageW(list,WM_SETREDRAW,TRUE,0);fitResults();InvalidateRect(list,nullptr,TRUE);
        SetWindowTextW(status,(shown.empty()||options.provider==1)?providerStatus.c_str():L"\u2191 \u2193 to navigate   \u00b7   Enter to open   \u00b7   Tab for actions");updateActions();
    }
    void fitResults(){
        if(!IsWindowVisible(hwnd)||placing)return;
        RECT bounds;GetWindowRect(hwnd,&bounds);MONITORINFO monitor{sizeof monitor};if(!GetMonitorInfoW(display,&monitor))return;
        int rows=std::clamp(int(shown.size()),1,6),header=std::max(72,int(42+33*textScale));
        int desired=px(header+int(20*textScale+14)+int(52*textScale)*rows+78);
        desired=std::min(desired,std::max(1,int(monitor.rcWork.bottom-bounds.top)-px(12)));
        if(bounds.bottom-bounds.top!=desired){SetWindowPos(hwnd,nullptr,0,0,bounds.right-bounds.left,desired,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);arrange();}
    }
    void updateActions(){int i=int(SendMessageW(list,LB_GETCURSEL,0,0));bool selected=i>=0&&i<int(shown.size());EnableWindow(openButton,selected);EnableWindow(actionButton,selected);EnableWindow(addButton,selected&&(shown[i].kind==Kind::File||shown[i].kind==Kind::Folder));}
    int px(int n)const{return MulDiv(n,int(dpi),96);}
    void arrange(){
        RECT r;GetClientRect(hwnd,&r);
        int header=std::max(72,int(42+33*textScale)),top=px(header+int(20*textScale+14)),footer=px(72);
        int rowHeight=px(int(52*textScale)),listSpace=std::max(1,int(r.bottom)-top-footer);
        MoveWindow(edit,px(62),px(24),std::max(1,int(r.right)-px(124)),px(int(33*textScale)),TRUE);
        MoveWindow(heading,px(24),px(header+10),std::max(1,int(r.right)-px(48)),px(int(18*textScale)),TRUE);
        int listHeight=listSpace>=rowHeight?(listSpace/rowHeight)*rowHeight:listSpace;
        MoveWindow(list,px(10),top,std::max(1,int(r.right)-px(20)),listHeight,TRUE);
        MoveWindow(status,px(24),r.bottom-px(63),std::max(1,int(r.right)-px(48)),px(20),TRUE);
        int buttonY=r.bottom-px(35),buttonHeight=px(26);
        MoveWindow(openButton,r.right-px(306),buttonY,px(76),buttonHeight,TRUE);
        MoveWindow(addButton,r.right-px(222),buttonY,px(104),buttonHeight,TRUE);
        MoveWindow(actionButton,r.right-px(110),buttonY,px(94),buttonHeight,TRUE);
        SendMessageW(list,LB_SETITEMHEIGHT,0,rowHeight);updateActions();InvalidateRect(hwnd,nullptr,FALSE);
    }
    void layout(){auto oldDpi=dpi;auto oldScale=textScale;auto oldSurface=surface;auto oldTextColor=textColor;dpi=GetDpiForWindow(hwnd);HIGHCONTRASTW hc{sizeof hc};SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof hc,&hc,0);highContrast=(hc.dwFlags&HCF_HIGHCONTRASTON)!=0;surface=highContrast?GetSysColor(COLOR_WINDOW):RGB(32,34,36);inputSurface=highContrast?surface:RGB(32,34,36);textColor=highContrast?GetSysColor(COLOR_WINDOWTEXT):RGB(245,245,243);secondary=highContrast?textColor:RGB(173,177,181);selection=highContrast?GetSysColor(COLOR_HIGHLIGHT):RGB(53,56,59);accent=highContrast?textColor:RGB(227,231,235);DeleteObject(background);background=CreateSolidBrush(surface);DeleteObject(inputBackground);inputBackground=CreateSolidBrush(inputSurface);
        try{textScale=float(appearance.TextScaleFactor());}catch(...){textScale=1;}textScale=std::clamp(textScale,1.f,2.25f);if(font&&oldDpi==dpi&&oldScale==textScale&&oldSurface==surface&&oldTextColor==textColor){arrange();return;}
        DeleteObject(symbolFont);symbolFont=CreateFontW(-px(23),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI Symbol");auto old=font;font=CreateFontW(-px(int(15*textScale)),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI Variable");RECT r;GetClientRect(hwnd,&r);painter.formats.clear();
        DeleteObject(inputFont);DeleteObject(smallFont);DeleteObject(monoFont);
        inputFont=CreateFontW(-px(int(23*textScale)),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI Variable");
        smallFont=CreateFontW(-px(int(11*textScale)),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI Variable");
        monoFont=CreateFontW(-px(int(11*textScale)),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Cascadia Code");
        arrange();
        for(auto control:{heading,edit,list,status,openButton,addButton,actionButton})SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
        SendMessageW(edit,WM_SETFONT,reinterpret_cast<WPARAM>(inputFont),TRUE);SendMessageW(heading,WM_SETFONT,reinterpret_cast<WPARAM>(smallFont),TRUE);SendMessageW(status,WM_SETFONT,reinterpret_cast<WPARAM>(smallFont),TRUE);
        if(old)DeleteObject(old);
    }
    void placeOn(HMONITOR monitor){MONITORINFO info{sizeof info};if(!GetMonitorInfoW(monitor,&info))return;placing=true;UINT x=96,y=96;GetDpiForMonitor(monitor,MDT_EFFECTIVE_DPI,&x,&y);dpi=x;auto box=searchPlacement(info.rcWork.left,info.rcWork.top,info.rcWork.right,info.rcWork.bottom,x);SetWindowPos(hwnd,HWND_TOPMOST,box.x,box.y,box.width,box.height,SWP_NOACTIVATE);display=monitor;layout();placing=false;fitResults();}
    void follow(){if(!IsWindowVisible(hwnd)||actionMenu||GetCapture()||(GetAsyncKeyState(VK_LBUTTON)&0x8000)){follower.reset();return;}POINT point{};if(!GetCursorPos(&point)){follower.reset();return;}auto next=MonitorFromPoint(point,MONITOR_DEFAULTTONEAREST);if(follower.update(reinterpret_cast<uintptr_t>(display),reinterpret_cast<uintptr_t>(next),GetTickCount64(),false))placeOn(next);}
    static LRESULT CALLBACK outsideClick(int code,WPARAM event,LPARAM data){auto p=hookOwner;if(code==HC_ACTION&&p&&(event==WM_LBUTTONDOWN||event==WM_RBUTTONDOWN||event==WM_MBUTTONDOWN)&&!p->actionMenu){auto point=reinterpret_cast<MSLLHOOKSTRUCT*>(data)->pt;auto hit=WindowFromPoint(point);if(hit!=p->hwnd&&!IsChild(p->hwnd,hit))PostMessageW(p->hwnd,WM_APP+82,0,0);}return CallNextHookEx(nullptr,code,event,data);}

    bool focusInput(){auto foreground=GetForegroundWindow();auto sourceThread=GetWindowThreadProcessId(foreground,nullptr),searchThread=GetCurrentThreadId();bool joined=sourceThread&&sourceThread!=searchThread&&AttachThreadInput(searchThread,sourceThread,TRUE);BringWindowToTop(hwnd);SetForegroundWindow(hwnd);SetActiveWindow(hwnd);SetFocus(edit);if(joined)AttachThreadInput(searchThread,sourceThread,FALSE);return GetForegroundWindow()==hwnd&&GetFocus()==edit;}
    void show(){if(IsWindowVisible(hwnd)){hide(true);return;}refreshApps();previous=GetForegroundWindow();POINT point{};GetCursorPos(&point);placeOn(MonitorFromPoint(point,MONITOR_DEFAULTTONEAREST));follower.reset();if(GetSystemMetrics(SM_CMONITORS)>1)SetCoalescableTimer(hwnd,92,120,nullptr,30);SetWindowTextW(edit,L"");BOOL animate=TRUE;SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&animate,0);bool motion;{std::lock_guard lock(mutex);motion=animate&&!options.reduceMotion&&!highContrast;}SetLayeredWindowAttributes(hwnd,0,motion?210:255,LWA_ALPHA);opening=true;ShowWindow(hwnd,SW_SHOWNORMAL);focusAttempts=0;if(focusInput())opening=false;else SetTimer(hwnd,93,45,nullptr);if(!review){hookOwner=this;outsideHook=SetWindowsHookExW(WH_MOUSE_LL,outsideClick,GetModuleHandleW(nullptr),0);if(!outsideHook)hookOwner=nullptr;}SendMessageW(edit,EM_SETSEL,0,-1);query();playOpenCue();if(motion){animationStart=GetTickCount64();SetTimer(hwnd,91,16,nullptr);}}
    void hide(bool restore){opening=false;KillTimer(hwnd,92);KillTimer(hwnd,93);if(outsideHook){UnhookWindowsHookEx(outsideHook);outsideHook=nullptr;}if(hookOwner==this)hookOwner=nullptr;follower.reset();KillTimer(hwnd,91);++generation;ShowWindow(hwnd,SW_HIDE);if(restore&&IsWindow(previous))SetForegroundWindow(previous);}
    void open(){int index=int(SendMessageW(list,LB_GETCURSEL,0,0));if(index<0||index>=int(shown.size()))return;auto result=shown[index];hide(false);if(result.kind==Kind::Window){if(!switchWindow(result)){show();SetWindowTextW(status,L"Window changed or Windows could not focus it. Search again.");}return;}if(result.kind==Kind::Command){execute(result.command);return;}if(result.kind==Kind::File||result.kind==Kind::Folder){SHELLEXECUTEINFOW launch{sizeof launch};launch.fMask=SEE_MASK_ASYNCOK;launch.hwnd=owner;launch.lpVerb=L"open";launch.lpFile=result.target.c_str();launch.nShow=SW_SHOWNORMAL;if(!ShellExecuteExW(&launch)){show();SetWindowTextW(status,L"Windows could not open this item. It may have moved or be offline.");}return;}PIDLIST_ABSOLUTE pidl=nullptr;bool opened=false;if(!result.activationId.empty()&&result.activationId.find(L'!')!=std::wstring::npos){winrt::com_ptr<IApplicationActivationManager> manager;if(SUCCEEDED(CoCreateInstance(CLSID_ApplicationActivationManager,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(manager.put())))){DWORD process=0;opened=SUCCEEDED(manager->ActivateApplication(result.activationId.c_str(),nullptr,AO_NONE,&process));}}if(opened)return;auto target=applicationTarget(result.target);if(SUCCEEDED(SHParseDisplayName(target.c_str(),nullptr,&pidl,0,nullptr))){SHELLEXECUTEINFOW launch{sizeof launch};launch.fMask=SEE_MASK_INVOKEIDLIST|SEE_MASK_NOASYNC;launch.hwnd=owner;launch.lpVerb=L"open";launch.lpIDList=pidl;launch.nShow=SW_SHOWNORMAL;opened=ShellExecuteExW(&launch)!=FALSE;CoTaskMemFree(pidl);}if(!opened){show();SetWindowTextW(status,L"Windows could not open this application. Try again.");}}
    void act(int action){int index=int(SendMessageW(list,LB_GETCURSEL,0,0));if(index<0||index>=int(shown.size()))return;auto r=shown[index];if(action==1){open();return;}if(r.kind!=Kind::File&&r.kind!=Kind::Folder)return;
        if(GetFileAttributesW(r.target.c_str())==INVALID_FILE_ATTRIBUTES){SetWindowTextW(status,L"This original is unavailable. It may have moved or be offline.");return;}
        if(action==5||action==6){if(add(r.target)){if(action==5)hide(true);else SetWindowTextW(status,L"Added to Floatlet. Keep searching to collect another item.");}else SetWindowTextW(status,L"The tray is full or this item could not be added.");return;}
        if(action==2){PIDLIST_ABSOLUTE pidl=nullptr;if(SUCCEEDED(SHParseDisplayName(r.target.c_str(),nullptr,&pidl,0,nullptr))){auto hr=SHOpenFolderAndSelectItems(pidl,0,nullptr,0);CoTaskMemFree(pidl);if(SUCCEEDED(hr))hide(false);else SetWindowTextW(status,L"Windows could not reveal this item.");}return;}
        if(action==3){IDataObject* data=nullptr;HRESULT hr=createFileData(r.target,&data);if(SUCCEEDED(hr)){hr=OleSetClipboard(data);if(SUCCEEDED(hr))hr=OleFlushClipboard();data->Release();}SetWindowTextW(status,SUCCEEDED(hr)?L"File copied. Paste it into another app.":L"The clipboard is busy. Try again.");return;}
        if(action==4){auto memory=GlobalAlloc(GMEM_MOVEABLE,(r.target.size()+1)*sizeof(wchar_t));if(!memory)return;auto data=GlobalLock(memory);if(!data){GlobalFree(memory);return;}memcpy(data,r.target.c_str(),(r.target.size()+1)*sizeof(wchar_t));GlobalUnlock(memory);if(OpenClipboard(hwnd)){EmptyClipboard();if(SetClipboardData(CF_UNICODETEXT,memory))memory=nullptr;CloseClipboard();}if(memory){GlobalFree(memory);SetWindowTextW(status,L"The clipboard is busy. Try again.");}else SetWindowTextW(status,L"Path copied.");}
    }
    void actions(std::optional<POINT> at={}){int index=int(SendMessageW(list,LB_GETCURSEL,0,0));if(index<0||index>=int(shown.size()))return;auto menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING,1,L"Open    Enter");auto kind=shown[index].kind;if(kind==Kind::File||kind==Kind::Folder){AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,2,L"Open file location in Explorer");AppendMenuW(menu,MF_STRING,4,L"Copy full path");AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,5,L"Add to Floatlet    Shift+Enter");AppendMenuW(menu,MF_STRING,6,L"Collect and keep searching    Ctrl+Enter");if(kind==Kind::File)AppendMenuW(menu,MF_STRING,3,L"Copy file");}RECT r;GetWindowRect(hwnd,&r);POINT point=at.value_or(POINT{r.right-px(240),r.bottom-px(52)});actionMenu=true;int action=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,point.x,point.y,0,hwnd,nullptr);actionMenu=false;DestroyMenu(menu);if(action)act(action);}
    bool key(MSG& message){if(message.hwnd!=hwnd&&!IsChild(hwnd,message.hwnd))return false;if(message.message!=WM_KEYDOWN)return false;switch(message.wParam){case 'A':if(message.hwnd==edit&&(GetKeyState(VK_CONTROL)&0x8000)){SendMessageW(edit,EM_SETSEL,0,-1);return true;}return false;case 'R':if(GetKeyState(VK_CONTROL)&0x8000){++fileRevision;localIndex->refresh(true);query();return true;}return false;case VK_ESCAPE:hide(true);return true;case VK_RETURN:if(GetFocus()==actionButton){actions();return true;}if(GetFocus()==addButton){act(5);return true;}act((GetKeyState(VK_SHIFT)&0x8000)?5:(GetKeyState(VK_CONTROL)&0x8000)?6:1);return true;case VK_TAB:if(GetKeyState(VK_SHIFT)&0x8000){SetFocus(GetFocus()==edit?actionButton:GetFocus()==actionButton?addButton:GetFocus()==addButton?openButton:edit);}else actions();return true;case VK_HOME:case VK_END:if(GetFocus()==list){navigated=true;SendMessageW(list,LB_SETCURSEL,message.wParam==VK_HOME?0:std::max(0,int(shown.size())-1),0);updateActions();return true;}return false;case VK_DOWN:case VK_UP:case VK_NEXT:case VK_PRIOR:{navigated=true;int count=int(shown.size()),i=int(SendMessageW(list,LB_GETCURSEL,0,0));int delta=message.wParam==VK_DOWN?1:message.wParam==VK_UP?-1:message.wParam==VK_NEXT?7:-7;if(count)SendMessageW(list,LB_SETCURSEL,std::clamp(i+delta,0,count-1),0);updateActions();return true;}}return false;}
    static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){auto p=reinterpret_cast<Impl*>(GetWindowLongPtrW(h,GWLP_USERDATA));if(m==WM_NCCREATE){p=static_cast<Impl*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(p));}if(!p)return DefWindowProcW(h,m,w,l);
        switch(m){case WM_TIMER:if(w==92){p->follow();return 0;}if(w==93){if(p->focusInput()){p->opening=false;KillTimer(h,93);}else if(++p->focusAttempts>=6){p->opening=false;KillTimer(h,93);SetWindowTextW(p->status,L"Click the search field to type");}return 0;}if(w==91){float t=std::min(1.f,float(GetTickCount64()-p->animationStart)/110.f);float eased=1-(1-t)*(1-t)*(1-t);SetLayeredWindowAttributes(h,0,BYTE(210+45*eased),LWA_ALPHA);if(t>=1)KillTimer(h,91);}return 0;case WM_COMMAND:if(LOWORD(w)==403)p->act(1);if(LOWORD(w)==404)p->act(5);if(LOWORD(w)==405)p->actions();if(LOWORD(w)==Edit&&HIWORD(w)==EN_CHANGE)p->query();if(LOWORD(w)==Edit&&HIWORD(w)==EN_SETFOCUS&&p->focusAttempts>=6){p->focusAttempts=0;SetWindowTextW(p->status,L"\u2191 \u2193 to navigate   \u00b7   Enter to open   \u00b7   Tab for actions");}if(LOWORD(w)==List&&HIWORD(w)==LBN_SELCHANGE){p->navigated=true;p->updateActions();}if(LOWORD(w)==List&&HIWORD(w)==LBN_DBLCLK)p->open();return 0;case WM_APP+81:if(IsWindowVisible(h))p->query();return 0;case WM_APP+83:if(IsWindowVisible(h))p->query();return 0;case WM_APP+84:p->acceptIcons();return 0;case WM_APP+82:if(IsWindowVisible(h))p->hide(false);return 0;case Ready:p->accept();return 0;case WM_ACTIVATE:if(LOWORD(w)!=WA_INACTIVE&&IsWindowVisible(h)&&GetFocus()!=p->edit)SetFocus(p->edit);else if(!p->review&&!p->actionMenu&&!p->opening&&LOWORD(w)==WA_INACTIVE)p->hide(false);return 0;case WM_CLOSE:p->hide(true);return 0;case WM_SETTINGCHANGE:p->layout();InvalidateRect(h,nullptr,TRUE);return 0;case WM_DISPLAYCHANGE:{KillTimer(h,92);p->follower.reset();if(IsWindowVisible(h)&&GetSystemMetrics(SM_CMONITORS)>1)SetCoalescableTimer(h,92,120,nullptr,30);POINT point{};GetCursorPos(&point);p->placeOn(MonitorFromPoint(point,MONITOR_DEFAULTTONEAREST));return 0;}case WM_DPICHANGED:{if(p->placing)return 0;auto r=reinterpret_cast<RECT*>(l);SetWindowPos(h,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);p->layout();return 0;}
        case WM_ERASEBKGND:return 1;
        case WM_PAINT:{PAINTSTRUCT ps;auto dc=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);try{auto& g=p->painter;g.begin(dc,r);g.clear(p->surface);float scale=p->dpi/96.f,width=float(r.right),height=float(r.bottom),header=std::max(72.f,42+33*p->textScale)*scale;
            auto edge=p->highContrast?p->textColor:RGB(75,78,81);auto divider=p->highContrast?p->textColor:RGB(54,57,60);
            g.outline(.5f,.5f,width-1,height-1,14*scale,edge);
            g.searchIcon(0,26*scale,30*scale,22*scale,p->secondary);
            g.line(1,header,width-1,header,divider);
            g.outline(width-48*scale,31*scale,30*scale,20*scale,5*scale,divider);
            g.text(L"esc",width-43*scale,32*scale,25*scale,18*scale,10*scale,p->secondary);
            g.line(16*scale,height-72*scale,width-16*scale,height-72*scale,divider);
            g.text(L"Floatlet",24*scale,height-30*scale,110*scale,22*scale,11*scale,p->secondary,true);
            g.end();}catch(...){}EndPaint(h,&ps);return 0;}
        case WM_CTLCOLOREDIT:{auto dc=reinterpret_cast<HDC>(w);SetTextColor(dc,p->textColor);SetBkColor(dc,p->inputSurface);return reinterpret_cast<LRESULT>(p->inputBackground);}
        case WM_CTLCOLORSTATIC:case WM_CTLCOLORLISTBOX:SetTextColor(reinterpret_cast<HDC>(w),p->secondary);SetBkColor(reinterpret_cast<HDC>(w),p->surface);return reinterpret_cast<LRESULT>(p->background);
        case WM_CONTEXTMENU:if(reinterpret_cast<HWND>(w)==p->list){POINT point{GET_X_LPARAM(l),GET_Y_LPARAM(l)};if(point.x==-1&&point.y==-1){int selected=int(SendMessageW(p->list,LB_GETCURSEL,0,0));if(selected<0)return 0;RECT item{};if(SendMessageW(p->list,LB_GETITEMRECT,selected,reinterpret_cast<LPARAM>(&item))==LB_ERR)return 0;point={item.left+p->px(60),item.bottom};ClientToScreen(p->list,&point);}else{POINT local=point;ScreenToClient(p->list,&local);auto hit=SendMessageW(p->list,LB_ITEMFROMPOINT,0,MAKELPARAM(local.x,local.y));if(HIWORD(hit)||LOWORD(hit)>=p->shown.size())return 0;SendMessageW(p->list,LB_SETCURSEL,LOWORD(hit),0);p->navigated=true;p->updateActions();}int selected=int(SendMessageW(p->list,LB_GETCURSEL,0,0));if(selected>=0&&selected<int(p->shown.size())&&(p->shown[selected].kind==Kind::File||p->shown[selected].kind==Kind::Folder))SetWindowTextW(p->status,p->shown[selected].target.c_str());p->actions(point);return 0;}return DefWindowProcW(h,m,w,l);
        case WM_DRAWITEM:{auto d=reinterpret_cast<DRAWITEMSTRUCT*>(l);float scale=p->dpi/96.f;
            if(d->CtlType==ODT_BUTTON){try{auto& g=p->painter;g.begin(d->hDC,d->rcItem);g.clear(p->surface);float width=float(d->rcItem.right-d->rcItem.left),height=float(d->rcItem.bottom-d->rcItem.top);bool enabled=(d->itemState&ODS_DISABLED)==0,pressed=(d->itemState&ODS_SELECTED)!=0,focused=(d->itemState&ODS_FOCUS)!=0;
                auto fill=pressed?p->selection:(p->highContrast?p->surface:RGB(43,46,49));g.round(0,0,width,height,6*scale,fill);
                if(focused)g.outline(1,1,width-2,height-2,5*scale,p->accent);
                wchar_t label[80]{};GetWindowTextW(d->hwndItem,label,80);auto color=enabled?p->textColor:(p->highContrast?p->secondary:RGB(145,149,153));
                g.text(label,10*scale,4*scale,width-16*scale,height-4*scale,11*scale,color);g.end();}catch(...){}return TRUE;}
            if(d->CtlID!=List)return FALSE;
            try{auto& g=p->painter;g.begin(d->hDC,d->rcItem);g.clear(p->surface);if(d->itemID<p->shown.size()){auto& result=p->shown[d->itemID];float width=float(d->rcItem.right-d->rcItem.left),height=float(d->rcItem.bottom-d->rcItem.top);bool selected=(d->itemState&ODS_SELECTED)!=0;
                if(selected){g.round(0,1,width,height-2,8*scale,p->selection);g.outline(.5f,1.5f,width-1,height-3,8*scale,p->highContrast?p->textColor:RGB(69,73,77));}
                auto foreground=selected&&p->highContrast?GetSysColor(COLOR_HIGHLIGHTTEXT):p->textColor;auto muted=selected&&p->highContrast?foreground:p->secondary;
                g.text(result.title,58*scale,6*scale,width-108*scale,25*p->textScale*scale,14*p->textScale*scale,foreground,true);
                g.text(result.detail,58*scale,(8+20*p->textScale)*scale,width-92*scale,20*p->textScale*scale,11*p->textScale*scale,muted);
                if(selected){g.round(width-34*scale,(height-22*scale)/2,24*scale,22*scale,5*scale,p->highContrast?p->selection:RGB(64,68,72));g.text(L"\u21b5",width-29*scale,(height-20*scale)/2,20*scale,20*scale,13*scale,foreground);}
                if(!result.icon){unsigned icon=8;if(result.kind==Kind::Command){auto title=fold(result.title);icon=title.find(L"alarm")!=title.npos?1:title.find(L"calendar")!=title.npos?2:title.find(L"tray")!=title.npos?3:title.find(L"music")!=title.npos?4:title.find(L"preferences")!=title.npos?5:6;}else if(result.kind==Kind::Folder)icon=9;else if(result.kind==Kind::File)icon=7;g.searchIcon(icon,18*scale,(height-24*scale)/2,24*scale,muted);}
            }g.end();if(d->itemID<p->shown.size()&&p->shown[d->itemID].icon){int size=p->px(32);DrawIconEx(d->hDC,d->rcItem.left+p->px(14),d->rcItem.top+(d->rcItem.bottom-d->rcItem.top-size)/2,static_cast<HICON>(p->shown[d->itemID].icon.get()),size,size,0,nullptr,DI_NORMAL);}}catch(...){}return TRUE;}
        }
        return DefWindowProcW(h,m,w,l);
    }
};
Launcher::Launcher(HWND owner,std::vector<Result> commands,std::function<void(int)> command,std::function<bool(const std::wstring&)> add):impl(std::make_unique<Impl>(owner,std::move(commands),std::move(command),std::move(add))){}
Launcher::~Launcher()=default;
void Launcher::show(){impl->show();}
void Launcher::dismiss(){impl->hide(false);std::lock_guard lock(impl->mutex);impl->pending.reset();}
void Launcher::configure(Options options){bool local=options.enabled&&options.files&&options.provider==1;impl->localIndex->configure(local,local?LocalIndex::locations(options.scope):std::vector<std::wstring>{},local?LocalIndex::cacheFile(options.scope):std::filesystem::path{});{std::lock_guard lock(impl->mutex);impl->options=options;}if(!options.enabled)dismiss();else{if(options.applications)impl->refreshApps();if(IsWindowVisible(impl->hwnd))impl->query();}}
std::string Launcher::diagnostics()const{std::lock_guard lock(impl->mutex);return "generation="+std::to_string(impl->generation.load())+" stage="+std::to_string(impl->stage.load())+" apps="+(impl->loaded?"ready":"loading")+" icons="+((impl->iconsBusy.load()||impl->iconPending.has_value())?"loading":"ready");}
bool Launcher::translate(MSG& message){return impl->key(message);}
}

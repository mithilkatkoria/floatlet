#include "launcher.h"
#include "everything.h"
#include "window_provider.h"
#include "icons.h"
#include "paint.h"
#include "placement.h"
#include "ui/monitor_follow.h"
#include "indexed_fallback.h"
#include <winrt/Windows.UI.ViewManagement.h>
#include "platform/shelf.h"
#include <unordered_set>
#include <shlobj.h>
#include <propkey.h>
#include <shellapi.h>
#include <commctrl.h>
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
    winrt::Windows::UI::ViewManagement::UISettings appearance;ULONGLONG animationStart=0;bool highContrast=false;float textScale=1;COLORREF surface=RGB(19,21,27),textColor=RGB(240,239,245),secondary=RGB(159,158,173),selection=RGB(43,49,63);
    Paint painter;HMONITOR display=nullptr;MonitorFollow follower;bool placing=false;HFONT inputFont{},smallFont{};HFONT font{},symbolFont{};HBRUSH background=CreateSolidBrush(RGB(19,21,27));
    std::vector<Result> commands,apps,shown;
    std::function<void(int)> execute;std::function<bool(const std::wstring&)> add;bool actionMenu=false;
    std::mutex mutex;std::condition_variable wake;
    std::optional<std::pair<unsigned,std::wstring>> pending;
    std::vector<Result> delivered;unsigned deliveredGeneration=0;
    std::atomic<unsigned> stage=0,generation=0;bool stop=false,loaded=false,review=false,navigated=false;std::wstring providerStatus,deliveredStatus,queryText;
    std::thread worker;UINT dpi=96;Options options;ULONGLONG catalogueTime=0;
    Impl(HWND parent,std::vector<Result> c,std::function<void(int)> run,std::function<bool(const std::wstring&)> shelf):owner(parent),commands(std::move(c)),execute(std::move(run)),add(std::move(shelf)) {
        WNDCLASSW cls{};cls.hInstance=GetModuleHandleW(nullptr);cls.lpfnWndProc=proc;cls.lpszClassName=L"Floatlet.Search";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&cls);
        review=(GetWindowLongPtrW(owner,GWL_EXSTYLE)&WS_EX_APPWINDOW)!=0;
        hwnd=CreateWindowExW((review?WS_EX_APPWINDOW:WS_EX_TOOLWINDOW)|WS_EX_TOPMOST|WS_EX_LAYERED,cls.lpszClassName,L"Floatlet Search",WS_POPUP,0,0,660,540,review?nullptr:owner,nullptr,cls.hInstance,this);
        if(!hwnd)winrt::throw_last_error();
        heading=CreateWindowExW(0,L"STATIC",L"RESULTS",WS_CHILD|WS_VISIBLE,0,0,0,0,hwnd,nullptr,cls.hInstance,nullptr);
        edit=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,0,0,0,0,hwnd,reinterpret_cast<HMENU>(INT_PTR(Edit)),cls.hInstance,nullptr);
        SetWindowSubclass(edit,editPaint,1,reinterpret_cast<DWORD_PTR>(this));
        SendMessageW(edit,EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(L"Search your PC"));SendMessageW(edit,EM_SETLIMITTEXT,256,0);
        list=CreateWindowExW(0,L"LISTBOX",L"Search results",WS_CHILD|WS_VISIBLE|WS_TABSTOP|LBS_NOTIFY|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS|LBS_NOINTEGRALHEIGHT,0,0,0,0,hwnd,reinterpret_cast<HMENU>(INT_PTR(List)),cls.hInstance,nullptr);
        status=CreateWindowExW(0,L"STATIC",L"Enter  Open     Tab  Actions     Shift+Enter  Add to Floatlet",WS_CHILD|WS_VISIBLE,0,0,0,0,hwnd,nullptr,cls.hInstance,nullptr);
        auto button=[&](const wchar_t* title,int id){return CreateWindowExW(0,L"BUTTON",title,WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,0,0,hwnd,reinterpret_cast<HMENU>(INT_PTR(id)),cls.hInstance,nullptr);};
        openButton=button(L"Open  \u21b5",403);addButton=button(L"Add to Floatlet",404);actionButton=button(L"Actions  \u21e5",405);
        BOOL dark=TRUE;DwmSetWindowAttribute(hwnd,20,&dark,sizeof dark);DWORD corner=2;DwmSetWindowAttribute(hwnd,33,&corner,sizeof corner);
        worker=std::thread([this]{work();});
    }
    ~Impl(){hide(false);{std::lock_guard lock(mutex);stop=true;}wake.notify_one();worker.join();DestroyWindow(hwnd);DeleteObject(inputFont);DeleteObject(smallFont);DeleteObject(font);DeleteObject(symbolFont);DeleteObject(background);}
    bool catalogue(unsigned expected){
        std::vector<Result> discovered;
        winrt::com_ptr<IShellItem> folder;
        if(FAILED(SHCreateItemInKnownFolder(FOLDERID_AppsFolder,0,nullptr,IID_PPV_ARGS(folder.put()))))return false;
        winrt::com_ptr<IEnumShellItems> items;if(FAILED(folder->BindToHandler(nullptr,BHID_EnumItems,IID_PPV_ARGS(items.put()))))return false;
        while(discovered.size()<4096){if(expected!=generation.load())return false;winrt::com_ptr<IShellItem> item;if(items->Next(1,item.put(),nullptr)!=S_OK)break;PWSTR name=nullptr,target=nullptr;
            if(SUCCEEDED(item->GetDisplayName(SIGDN_NORMALDISPLAY,&name))&&SUCCEEDED(item->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING,&target))){Result r;r.id=target;r.title=name;r.target=target;r.detail=L"Application";auto words=tokens(r.title);std::wstring initials;for(auto& word:words)if(!word.empty())initials+=word[0];r.aliases=initials;if(words.size()>1){initials.pop_back();r.aliases+=L" "+initials+words.back();}if(auto rich=item.try_as<IShellItem2>()){PWSTR id=nullptr;if(SUCCEEDED(rich->GetString(PKEY_AppUserModel_ID,&id))&&id){r.activationId=id;CoTaskMemFree(id);}}discovered.push_back(std::move(r));}CoTaskMemFree(name);CoTaskMemFree(target);
        }
        if(expected!=generation.load())return false;apps=std::move(discovered);catalogueTime=GetTickCount64();return true;
    }
    void work(){winrt::init_apartment(winrt::apartment_type::single_threaded);{Everything everything;Icons icons;struct CachedFiles{std::wstring query;ULONGLONG time;std::vector<Result> results;};std::vector<CachedFiles> cache;for(;;){std::pair<unsigned,std::wstring> request;Options config;{std::unique_lock lock(mutex);stage=0;wake.wait(lock,[&]{return stop||pending.has_value();});if(stop)break;request=std::move(*pending);pending.reset();config=options;}
        try{std::vector<Result> results;stage=2;
        for(auto* source:{&commands,&apps})for(auto r:*source){if(source==&apps&&!config.applications)break;if(request.first!=generation.load())break;r.score=rank(r,request.second,config.fuzzy);if(r.score)results.push_back(std::move(r));}
        if(config.windows&&!tokens(request.second).empty())for(auto r:windows()){r.score=rank(r,request.second,config.fuzzy);if(r.score)results.push_back(std::move(r));}
        auto publish=[&]{sort(results);{std::lock_guard lock(mutex);if(request.first!=generation.load())return;delivered=results;deliveredGeneration=request.first;deliveredStatus=everything.status;}PostMessageW(hwnd,Ready,0,0);};
        publish();
        if(config.applications&&(!loaded||GetTickCount64()-catalogueTime>=300000)){
            stage=1;loaded=catalogue(request.first);
            std::erase_if(results,[](const Result& r){return r.kind==Kind::Application;});
            for(auto r:apps){if(request.first!=generation.load())break;r.score=rank(r,request.second,config.fuzzy);if(r.score)results.push_back(std::move(r));}publish();
        }
        stage=5;for(size_t i=0;i<std::min(size_t(12),results.size())&&request.first==generation.load();++i)icons.fill(results[i]);if(request.first==generation.load())publish();
        if(config.files&&!tokens(request.second).empty()&&request.first==generation.load()){
            std::unordered_set<std::wstring> seen;for(auto& r:results)seen.insert(r.id);
            auto merge=[&](std::vector<Result> found){for(auto& r:found){if(request.first!=generation.load())break;r.score=rank(r,request.second,config.fuzzy);if(r.score&&seen.insert(r.id).second)results.push_back(std::move(r));}};
            auto hit=std::find_if(cache.begin(),cache.end(),[&](auto& c){return c.query==request.second&&GetTickCount64()-c.time<1000;});
            if(hit!=cache.end())merge(hit->results);
            else{
                stage=3;auto files=everything.query(request.second,false,request.first,generation);
                // Publish direct names before the more expensive ancestor-path expansion.
                merge(files);publish();
                stage=5;for(size_t i=0;i<std::min(size_t(12),results.size())&&request.first==generation.load();++i)icons.fill(results[i]);if(request.first==generation.load())publish();
                if(everything.ready&&request.first==generation.load()){
                    auto paths=everything.query(request.second,true,request.first,generation);
                    files.insert(files.end(),std::make_move_iterator(paths.begin()),std::make_move_iterator(paths.end()));
                }
                if(!everything.ready&&!everything.present&&request.first==generation.load()){stage=4;files=indexedFallback(request.second,request.first,generation,everything.status);}
                std::unordered_set<std::wstring> uniqueFiles;std::erase_if(files,[&](auto& r){return !uniqueFiles.insert(r.id).second;});
                for(auto& r:files)r.score=rank(r,request.second,config.fuzzy);sort(files);
                size_t bytes=0;for(auto& r:files)bytes+=(r.title.size()+r.detail.size()+r.target.size()+r.id.size())*sizeof(wchar_t);
                if(!files.empty()&&bytes<128*1024&&request.first==generation.load()){if(cache.size()>=8)cache.erase(cache.begin());cache.push_back({request.second,GetTickCount64(),files});}
                merge(std::move(files));
            }
            publish();
            if(config.fuzzy&&everything.ready&&results.size()<8&&request.first==generation.load()){merge(everything.query(request.second,false,request.first,generation,true));publish();}

        }
        stage=5;for(size_t i=0;i<std::min(size_t(12),results.size())&&request.first==generation.load();++i)icons.fill(results[i]);if(request.first==generation.load())publish();
        }catch(...){if(request.first==generation.load()){{std::lock_guard lock(mutex);delivered.clear();deliveredGeneration=request.first;deliveredStatus=L"A search provider could not finish. Try another query.";}PostMessageW(hwnd,Ready,0,0);}}
    }}winrt::uninit_apartment();}
    void query(){navigated=false;wchar_t text[257]{};GetWindowTextW(edit,text,257);if(queryText!=text){queryText=text;shown.clear();SendMessageW(list,LB_RESETCONTENT,0,0);SetWindowTextW(status,L"Searching on your PC...");updateActions();}auto current=++generation;{std::lock_guard lock(mutex);pending=std::pair(current,std::wstring(text));}wake.notify_one();}
    void accept(){std::vector<Result> results;{std::lock_guard lock(mutex);if(deliveredGeneration!=generation.load())return;results=std::move(delivered);providerStatus=deliveredStatus;deliveredGeneration=0;}bool sameRows=results.size()==shown.size();if(sameRows)for(size_t i=0;i<results.size();++i)if(results[i].id!=shown[i].id||results[i].title!=shown[i].title||results[i].detail!=shown[i].detail){sameRows=false;break;}if(sameRows&&!results.empty()){shown=std::move(results);InvalidateRect(list,nullptr,FALSE);updateActions();return;}std::wstring selected;int index=int(SendMessageW(list,LB_GETCURSEL,0,0));if(navigated&&index>=0&&index<int(shown.size()))selected=shown[index].id;
        if(navigated){std::vector<Result> stable;for(auto& old:shown){auto it=std::find_if(results.begin(),results.end(),[&](auto& r){return r.id==old.id;});if(it!=results.end()){stable.push_back(*it);results.erase(it);}}for(auto& r:results)stable.push_back(std::move(r));results=std::move(stable);}shown=std::move(results);SendMessageW(list,WM_SETREDRAW,FALSE,0);SendMessageW(list,LB_RESETCONTENT,0,0);int select=0;for(size_t i=0;i<shown.size();++i){auto label=shown[i].title+L", "+shown[i].detail;SendMessageW(list,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));if(shown[i].id==selected)select=int(i);}SendMessageW(list,LB_SETCURSEL,select,0);SendMessageW(list,WM_SETREDRAW,TRUE,0);InvalidateRect(list,nullptr,TRUE);
        SetWindowTextW(status,shown.empty()?providerStatus.c_str():L"Enter  Open     Shift+Enter  Add to tray     Ctrl+Enter  Collect");updateActions();
    }
    void updateActions(){int i=int(SendMessageW(list,LB_GETCURSEL,0,0));bool selected=i>=0&&i<int(shown.size());EnableWindow(openButton,selected);EnableWindow(actionButton,selected);EnableWindow(addButton,selected&&(shown[i].kind==Kind::File||shown[i].kind==Kind::Folder));}
    int px(int n)const{return MulDiv(n,int(dpi),96);}
    void layout(){dpi=GetDpiForWindow(hwnd);HIGHCONTRASTW hc{sizeof hc};SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof hc,&hc,0);highContrast=(hc.dwFlags&HCF_HIGHCONTRASTON)!=0;surface=highContrast?GetSysColor(COLOR_WINDOW):RGB(19,21,27);textColor=highContrast?GetSysColor(COLOR_WINDOWTEXT):RGB(240,239,245);secondary=highContrast?textColor:RGB(159,158,173);selection=highContrast?GetSysColor(COLOR_HIGHLIGHT):RGB(43,49,63);DeleteObject(background);background=CreateSolidBrush(surface);
        try{textScale=float(appearance.TextScaleFactor());}catch(...){textScale=1;}textScale=std::clamp(textScale,1.f,2.25f);
        DeleteObject(symbolFont);symbolFont=CreateFontW(-px(23),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI Symbol");auto old=font;font=CreateFontW(-px(int(15*textScale)),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI Variable");RECT r;GetClientRect(hwnd,&r);painter.formats.clear();
        DeleteObject(inputFont);DeleteObject(smallFont);
        inputFont=CreateFontW(-px(int(25*textScale)),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI Variable");
        smallFont=CreateFontW(-px(int(11*textScale)),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI Variable");
        int queryHeight=px(int(38*textScale)),top=px(12)+queryHeight+px(50),footer=px(int(58*textScale)),statusHeight=px(int(23*textScale));
        MoveWindow(edit,px(30),px(24),r.right-px(60),queryHeight,TRUE);
        MoveWindow(heading,px(30),px(32)+queryHeight,r.right-px(60),px(int(18*textScale)),TRUE);
        int rowHeight=px(int(60*textScale)),listSpace=std::max(1,int(r.bottom)-top-footer-statusHeight-px(11));
        int listHeight=listSpace>=rowHeight?(listSpace/rowHeight)*rowHeight:listSpace;
        MoveWindow(list,px(14),top,std::max(1,int(r.right)-px(28)),listHeight,TRUE);
        MoveWindow(status,px(30),r.bottom-footer-statusHeight,r.right-px(60),statusHeight,TRUE);
        int buttonY=r.bottom-footer+px(8),buttonHeight=footer-px(20),available=r.right-px(60),unit=available/3;
        MoveWindow(openButton,px(22),buttonY,unit-px(8),buttonHeight,TRUE);
        MoveWindow(addButton,px(30)+unit,buttonY,unit-px(8),buttonHeight,TRUE);
        MoveWindow(actionButton,px(38)+unit*2,buttonY,unit-px(8),buttonHeight,TRUE);
        for(auto control:{heading,edit,list,status,openButton,addButton,actionButton})SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
        SendMessageW(edit,WM_SETFONT,reinterpret_cast<WPARAM>(inputFont),TRUE);for(auto c:{heading,status})SendMessageW(c,WM_SETFONT,reinterpret_cast<WPARAM>(smallFont),TRUE);
        if(old)DeleteObject(old);SendMessageW(list,LB_SETITEMHEIGHT,0,px(int(60*textScale)));updateActions();
    }
    void placeOn(HMONITOR monitor){MONITORINFO info{sizeof info};if(!GetMonitorInfoW(monitor,&info))return;placing=true;UINT x=96,y=96;GetDpiForMonitor(monitor,MDT_EFFECTIVE_DPI,&x,&y);dpi=x;auto box=searchPlacement(info.rcWork.left,info.rcWork.top,info.rcWork.right,info.rcWork.bottom,x);SetWindowPos(hwnd,HWND_TOPMOST,box.x,box.y,box.width,box.height,SWP_NOACTIVATE);display=monitor;layout();placing=false;}
    void follow(){if(!IsWindowVisible(hwnd)||actionMenu||GetCapture()||(GetAsyncKeyState(VK_LBUTTON)&0x8000)){follower.reset();return;}POINT point{};if(!GetCursorPos(&point)){follower.reset();return;}auto next=MonitorFromPoint(point,MONITOR_DEFAULTTONEAREST);if(follower.update(reinterpret_cast<uintptr_t>(display),reinterpret_cast<uintptr_t>(next),GetTickCount64(),false))placeOn(next);}

    void show(){if(IsWindowVisible(hwnd)){hide(true);return;}previous=GetForegroundWindow();POINT point{};GetCursorPos(&point);placeOn(MonitorFromPoint(point,MONITOR_DEFAULTTONEAREST));follower.reset();if(GetSystemMetrics(SM_CMONITORS)>1)SetCoalescableTimer(hwnd,92,120,nullptr,30);SetWindowTextW(edit,L"");BOOL animate=TRUE;SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&animate,0);bool motion;{std::lock_guard lock(mutex);motion=animate&&!options.reduceMotion&&!highContrast;}SetLayeredWindowAttributes(hwnd,0,motion?80:255,LWA_ALPHA);ShowWindow(hwnd,SW_SHOW);SetForegroundWindow(hwnd);SetFocus(edit);query();if(motion){animationStart=GetTickCount64();SetTimer(hwnd,91,16,nullptr);}}
    void hide(bool restore){KillTimer(hwnd,92);follower.reset();KillTimer(hwnd,91);++generation;ShowWindow(hwnd,SW_HIDE);if(restore&&IsWindow(previous))SetForegroundWindow(previous);}
    void open(){int index=int(SendMessageW(list,LB_GETCURSEL,0,0));if(index<0||index>=int(shown.size()))return;auto result=shown[index];hide(false);if(result.kind==Kind::Window){if(!switchWindow(result)){show();SetWindowTextW(status,L"Window changed or Windows could not focus it. Search again.");}return;}if(result.kind==Kind::Command){execute(result.command);return;}PIDLIST_ABSOLUTE pidl=nullptr;bool opened=false;if(!result.activationId.empty()&&result.activationId.find(L'!')!=std::wstring::npos){winrt::com_ptr<IApplicationActivationManager> manager;if(SUCCEEDED(CoCreateInstance(CLSID_ApplicationActivationManager,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(manager.put())))){DWORD process=0;opened=SUCCEEDED(manager->ActivateApplication(result.activationId.c_str(),nullptr,AO_NONE,&process));}}if(opened)return;auto target=result.kind==Kind::Application?applicationTarget(result.target):result.target;if(SUCCEEDED(SHParseDisplayName(target.c_str(),nullptr,&pidl,0,nullptr))){SHELLEXECUTEINFOW launch{sizeof launch};launch.fMask=SEE_MASK_INVOKEIDLIST|SEE_MASK_NOASYNC;launch.hwnd=owner;launch.lpVerb=L"open";launch.lpIDList=pidl;launch.nShow=SW_SHOWNORMAL;opened=ShellExecuteExW(&launch)!=FALSE;CoTaskMemFree(pidl);}if(!opened){show();SetWindowTextW(status,L"Windows could not open this application. Try again.");}}
    void act(int action){int index=int(SendMessageW(list,LB_GETCURSEL,0,0));if(index<0||index>=int(shown.size()))return;auto r=shown[index];if(action==1){open();return;}if(r.kind!=Kind::File&&r.kind!=Kind::Folder)return;
        if(GetFileAttributesW(r.target.c_str())==INVALID_FILE_ATTRIBUTES){SetWindowTextW(status,L"This original is unavailable. It may have moved or be offline.");return;}
        if(action==5||action==6){if(add(r.target)){if(action==5)hide(true);else SetWindowTextW(status,L"Added to Floatlet. Keep searching to collect another item.");}else SetWindowTextW(status,L"The tray is full or this item could not be added.");return;}
        if(action==2){PIDLIST_ABSOLUTE pidl=nullptr;if(SUCCEEDED(SHParseDisplayName(r.target.c_str(),nullptr,&pidl,0,nullptr))){auto hr=SHOpenFolderAndSelectItems(pidl,0,nullptr,0);CoTaskMemFree(pidl);if(SUCCEEDED(hr))hide(false);else SetWindowTextW(status,L"Windows could not reveal this item.");}return;}
        if(action==3){IDataObject* data=nullptr;HRESULT hr=createFileData(r.target,&data);if(SUCCEEDED(hr)){hr=OleSetClipboard(data);if(SUCCEEDED(hr))hr=OleFlushClipboard();data->Release();}SetWindowTextW(status,SUCCEEDED(hr)?L"File copied. Paste it into another app.":L"The clipboard is busy. Try again.");return;}
        if(action==4){auto memory=GlobalAlloc(GMEM_MOVEABLE,(r.target.size()+1)*sizeof(wchar_t));if(!memory)return;auto data=GlobalLock(memory);if(!data){GlobalFree(memory);return;}memcpy(data,r.target.c_str(),(r.target.size()+1)*sizeof(wchar_t));GlobalUnlock(memory);if(OpenClipboard(hwnd)){EmptyClipboard();if(SetClipboardData(CF_UNICODETEXT,memory))memory=nullptr;CloseClipboard();}if(memory){GlobalFree(memory);SetWindowTextW(status,L"The clipboard is busy. Try again.");}else SetWindowTextW(status,L"Path copied.");}
    }
    void actions(){int index=int(SendMessageW(list,LB_GETCURSEL,0,0));if(index<0||index>=int(shown.size()))return;auto menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING,1,L"Open    Enter");auto kind=shown[index].kind;if(kind==Kind::File||kind==Kind::Folder){AppendMenuW(menu,MF_STRING,5,L"Add to Floatlet    Shift+Enter");AppendMenuW(menu,MF_STRING,6,L"Collect and keep searching    Ctrl+Enter");AppendMenuW(menu,MF_STRING,2,L"Reveal in File Explorer");AppendMenuW(menu,MF_STRING,3,L"Copy file");AppendMenuW(menu,MF_STRING,4,L"Copy path");}RECT r;GetWindowRect(hwnd,&r);actionMenu=true;int action=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,r.right-px(240),r.bottom-px(52),0,hwnd,nullptr);actionMenu=false;DestroyMenu(menu);if(action)act(action);}
    bool key(MSG& message){if(message.hwnd!=hwnd&&!IsChild(hwnd,message.hwnd))return false;if(message.message!=WM_KEYDOWN)return false;switch(message.wParam){case VK_ESCAPE:hide(true);return true;case VK_RETURN:if(GetFocus()==actionButton){actions();return true;}if(GetFocus()==addButton){act(5);return true;}act((GetKeyState(VK_SHIFT)&0x8000)?5:(GetKeyState(VK_CONTROL)&0x8000)?6:1);return true;case VK_TAB:if(GetKeyState(VK_SHIFT)&0x8000){SetFocus(GetFocus()==edit?actionButton:GetFocus()==actionButton?addButton:GetFocus()==addButton?openButton:edit);}else actions();return true;case VK_HOME:case VK_END:if(GetFocus()==list){navigated=true;SendMessageW(list,LB_SETCURSEL,message.wParam==VK_HOME?0:std::max(0,int(shown.size())-1),0);updateActions();return true;}return false;case VK_DOWN:case VK_UP:case VK_NEXT:case VK_PRIOR:{navigated=true;int count=int(shown.size()),i=int(SendMessageW(list,LB_GETCURSEL,0,0));int delta=message.wParam==VK_DOWN?1:message.wParam==VK_UP?-1:message.wParam==VK_NEXT?7:-7;if(count)SendMessageW(list,LB_SETCURSEL,std::clamp(i+delta,0,count-1),0);updateActions();return true;}}return false;}
    static LRESULT CALLBACK editPaint(HWND h,UINT m,WPARAM w,LPARAM l,UINT_PTR id,DWORD_PTR data){
        if(m==WM_NCDESTROY){RemoveWindowSubclass(h,editPaint,id);return DefSubclassProc(h,m,w,l);}
        auto result=DefSubclassProc(h,m,w,l);
        if(m==WM_PAINT&&GetWindowTextLengthW(h)==0){auto p=reinterpret_cast<Impl*>(data);auto dc=GetDC(h);if(dc){RECT r;GetClientRect(h,&r);auto old=SelectObject(dc,p->inputFont);SetTextColor(dc,p->secondary);SetBkMode(dc,TRANSPARENT);DrawTextW(dc,L"Search your PC",-1,&r,DT_LEFT|DT_TOP|DT_SINGLELINE|DT_NOPREFIX);SelectObject(dc,old);ReleaseDC(h,dc);}}
        return result;
    }
    static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){auto p=reinterpret_cast<Impl*>(GetWindowLongPtrW(h,GWLP_USERDATA));if(m==WM_NCCREATE){p=static_cast<Impl*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(p));}if(!p)return DefWindowProcW(h,m,w,l);
        switch(m){case WM_TIMER:if(w==92){p->follow();return 0;}if(w==91){float t=std::min(1.f,float(GetTickCount64()-p->animationStart)/160.f);float eased=1-(1-t)*(1-t)*(1-t);SetLayeredWindowAttributes(h,0,BYTE(80+175*eased),LWA_ALPHA);if(t>=1)KillTimer(h,91);}return 0;case WM_COMMAND:if(LOWORD(w)==403)p->act(1);if(LOWORD(w)==404)p->act(5);if(LOWORD(w)==405)p->actions();if(LOWORD(w)==Edit&&HIWORD(w)==EN_CHANGE)p->query();if(LOWORD(w)==List&&HIWORD(w)==LBN_SELCHANGE){p->navigated=true;p->updateActions();}if(LOWORD(w)==List&&HIWORD(w)==LBN_DBLCLK)p->open();return 0;case Ready:p->accept();return 0;case WM_ACTIVATE:if(!p->review&&!p->actionMenu&&LOWORD(w)==WA_INACTIVE)p->hide(false);return 0;case WM_CLOSE:p->hide(true);return 0;case WM_SETTINGCHANGE:p->layout();InvalidateRect(h,nullptr,TRUE);return 0;case WM_DISPLAYCHANGE:{KillTimer(h,92);p->follower.reset();if(IsWindowVisible(h)&&GetSystemMetrics(SM_CMONITORS)>1)SetCoalescableTimer(h,92,120,nullptr,30);POINT point{};GetCursorPos(&point);p->placeOn(MonitorFromPoint(point,MONITOR_DEFAULTTONEAREST));return 0;}case WM_DPICHANGED:{if(p->placing)return 0;auto r=reinterpret_cast<RECT*>(l);SetWindowPos(h,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);p->layout();return 0;}
        case WM_ERASEBKGND:{RECT r;GetClientRect(h,&r);FillRect(reinterpret_cast<HDC>(w),&r,p->background);return 1;}
        case WM_CTLCOLOREDIT:case WM_CTLCOLORSTATIC:case WM_CTLCOLORLISTBOX:SetTextColor(reinterpret_cast<HDC>(w),p->textColor);SetBkColor(reinterpret_cast<HDC>(w),p->surface);return reinterpret_cast<LRESULT>(p->background);
        case WM_DRAWITEM:{auto d=reinterpret_cast<DRAWITEMSTRUCT*>(l);auto rect=d->rcItem;float scale=p->dpi/96.f,wid=float(rect.right-rect.left),height=float(rect.bottom-rect.top);auto& paint=p->painter;try{paint.begin(d->hDC,rect);paint.clear(p->surface);
            if(d->CtlType==ODT_BUTTON){bool disabled=(d->itemState&ODS_DISABLED)!=0;paint.round(0,0,wid,height,10*scale,p->highContrast?GetSysColor(COLOR_BTNFACE):d->CtlID==403?RGB(48,60,80):RGB(29,32,39));wchar_t label[80]{};GetWindowTextW(d->hwndItem,label,80);paint.text(label,16*scale,(height-19*scale*p->textScale)/2,wid-24*scale,height,12*scale*p->textScale,disabled?p->secondary:p->highContrast?GetSysColor(COLOR_BTNTEXT):p->textColor);paint.end();if(d->itemState&ODS_FOCUS){InflateRect(&rect,-4,-4);DrawFocusRect(d->hDC,&rect);}return TRUE;}
            if(d->itemID>=p->shown.size()){paint.end();return TRUE;}auto& item=p->shown[d->itemID];bool selected=(d->itemState&ODS_SELECTED)!=0;auto fg=selected&&p->highContrast?GetSysColor(COLOR_HIGHLIGHTTEXT):p->textColor;auto muted=selected&&p->highContrast?fg:p->secondary;
            if(selected)paint.round(0,2*scale,wid,height-4*scale,12*scale,p->selection);
            paint.text(item.title,62*scale,9*scale,wid-100*scale,27*scale*p->textScale,15*scale*p->textScale,fg,true);
            paint.text(item.detail,62*scale,9*scale+24*scale*p->textScale,wid-82*scale,23*scale*p->textScale,11.5f*scale*p->textScale,muted);
            if(selected)paint.text(L"\u21b5",wid-28*scale,(height-20*scale)/2,22*scale,22*scale,15*scale,muted);
            if(!item.icon){const wchar_t* symbol=item.kind==Kind::Command?(item.title.find(L"Calendar")!=std::wstring::npos?L"\u25a6":item.title.find(L"Music")!=std::wstring::npos?L"\u266b":item.title.find(L"Preferences")!=std::wstring::npos?L"\u2699":item.title.find(L"Alarm")!=std::wstring::npos?L"\u25f7":L"\u229e"):L"\u25a1";paint.text(symbol,18*scale,(height-29*scale)/2,32*scale,32*scale,24*scale,fg);}
            paint.end();if(item.icon)DrawIconEx(d->hDC,rect.left+p->px(16),rect.top+(int(height)-p->px(32))/2,static_cast<HICON>(item.icon.get()),p->px(32),p->px(32),0,nullptr,DI_NORMAL);return TRUE;
        }catch(...){return TRUE;}}}

        return DefWindowProcW(h,m,w,l);
    }
};
Launcher::Launcher(HWND owner,std::vector<Result> commands,std::function<void(int)> command,std::function<bool(const std::wstring&)> add):impl(std::make_unique<Impl>(owner,std::move(commands),std::move(command),std::move(add))){}
Launcher::~Launcher()=default;
void Launcher::show(){impl->show();}
void Launcher::dismiss(){impl->hide(false);std::lock_guard lock(impl->mutex);impl->pending.reset();}
void Launcher::configure(Options options){{std::lock_guard lock(impl->mutex);impl->options=options;}if(!options.enabled)dismiss();else if(IsWindowVisible(impl->hwnd))impl->query();}
std::string Launcher::diagnostics()const{return "generation="+std::to_string(impl->generation.load())+" stage="+std::to_string(impl->stage.load());}
bool Launcher::translate(MSG& message){return impl->key(message);}
}

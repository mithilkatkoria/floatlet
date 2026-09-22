#pragma once
#include <windows.h>
#include <commctrl.h>
#include <optional>
#include "options.h"
#include "everything.h"
#include <shellapi.h>
#include <thread>
#include <mutex>
namespace delight::search {
struct Preferences {
    Options options;std::optional<Options> result;bool registered=false;
    std::thread probe;std::atomic<unsigned> probeGeneration=0;std::mutex probeMutex;
    std::wstring connection;Everything::Installation installed;bool canStart=false;
    ~Preferences(){++probeGeneration;if(probe.joinable())probe.join();}
    void check(HWND hwnd){
        if(probe.joinable())probe.join();EnableWindow(GetDlgItem(hwnd,113),FALSE);EnableWindow(GetDlgItem(hwnd,114),FALSE);SetDlgItemTextW(hwnd,112,L"Checking Everything connection...");auto current=++probeGeneration;
        probe=std::thread([this,hwnd,current]{try{Everything provider;auto setup=Everything::installation();provider.query(L"FloatletConnectionProbeNoResultsExpected",false,current,probeGeneration);std::wstring message;
            if(provider.ready)message=L"Everything connected. Indexed file and folder search is available.";
            else if(setup.lite)message=L"Everything Lite does not support IPC. Install the full edition for indexed search.";
            else if(!provider.present&&!setup.executable.empty())message=L"Everything is installed but its IPC window was not found. Start it or check its instance settings.";
            else if(!provider.present)message=L"Enable Everything integration for full-drive search. A portable or custom instance may need to be started manually.";
            else message=provider.status;
            {std::lock_guard lock(probeMutex);connection=std::move(message);installed=std::move(setup);canStart=!provider.present&&!installed.lite&&!installed.executable.empty();}}catch(...){std::lock_guard lock(probeMutex);connection=L"Could not check Everything. You can retry without changing its settings.";canStart=false;}if(current==probeGeneration.load())PostMessageW(hwnd,WM_APP+93,0,0);
        });
    }
    static INT_PTR CALLBACK proc(HWND hwnd,UINT msg,WPARAM w,LPARAM l){
        auto self=reinterpret_cast<Preferences*>(GetWindowLongPtrW(hwnd,DWLP_USER));
        if(msg==WM_INITDIALOG){self=reinterpret_cast<Preferences*>(l);SetWindowLongPtrW(hwnd,DWLP_USER,l);UINT dpi=GetDpiForWindow(hwnd);auto px=[&](int n){return MulDiv(n,int(dpi),96);};auto make=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int width,int height,int id){auto h=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,px(x),px(y),px(width),px(height),hwnd,reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);return h;};
            const wchar_t* labels[]={L"Enable Floatlet Search",L"Applications",L"Indexed files and folders",L"Open windows",L"Approximate spelling"};bool values[]={self->options.enabled,self->options.applications,self->options.files,self->options.windows,self->options.fuzzy};
            for(int i=0;i<5;++i){auto h=make(L"BUTTON",labels[i],WS_TABSTOP|BS_AUTOCHECKBOX,22,20+i*32,400,28,100+i);SendMessageW(h,BM_SETCHECK,values[i]?BST_CHECKED:BST_UNCHECKED,0);}
            make(L"STATIC",L"Search shortcut",0,22,190,160,24,0);auto hotkey=make(HOTKEY_CLASSW,L"",WS_TABSTOP|WS_BORDER,190,188,200,28,110);auto modifiers=self->options.modifiers;WORD hk=WORD(((modifiers&MOD_CONTROL)?HOTKEYF_CONTROL:0)|((modifiers&MOD_ALT)?HOTKEYF_ALT:0)|((modifiers&MOD_SHIFT)?HOTKEYF_SHIFT:0));SendMessageW(hotkey,HKM_SETHOTKEY,MAKEWORD(self->options.key,hk),0);
            make(L"STATIC",self->registered?L"Shortcut is available.":L"Shortcut unavailable or disabled. Search remains in the tray menu.",0,22,226,400,42,111);
            make(L"STATIC",L"Queries stay on this PC. Search history is not recorded.\nEverything keeps ownership of its index and settings.",0,22,275,400,46,0);
            make(L"STATIC",L"Everything connection",0,22,330,400,55,112);
            make(L"BUTTON",L"Check connection",WS_TABSTOP,22,392,145,32,113);make(L"BUTTON",L"Start Everything",WS_TABSTOP,177,392,145,32,114);
            make(L"BUTTON",L"Save",WS_TABSTOP|BS_DEFPUSHBUTTON,222,449,90,32,IDOK);make(L"BUTTON",L"Cancel",WS_TABSTOP,322,449,90,32,IDCANCEL);
            RECT r{0,0,px(440),px(500)};AdjustWindowRectEx(&r,GetWindowLongW(hwnd,GWL_STYLE),FALSE,GetWindowLongW(hwnd,GWL_EXSTYLE));SetWindowPos(hwnd,nullptr,0,0,r.right-r.left,r.bottom-r.top,SWP_NOMOVE|SWP_NOZORDER);self->check(hwnd);return TRUE;
        }
        if(msg==WM_APP+93&&self){if(self->probe.joinable())self->probe.join();std::lock_guard lock(self->probeMutex);SetDlgItemTextW(hwnd,112,self->connection.c_str());EnableWindow(GetDlgItem(hwnd,113),TRUE);EnableWindow(GetDlgItem(hwnd,114),self->canStart);return TRUE;}
        if(msg==WM_COMMAND&&self){
            if(LOWORD(w)==113){self->check(hwnd);return TRUE;}
            if(LOWORD(w)==114){std::wstring executable;{std::lock_guard lock(self->probeMutex);if(self->canStart)executable=self->installed.executable;}if(!executable.empty()){auto opened=ShellExecuteW(hwnd,L"open",executable.c_str(),nullptr,nullptr,SW_SHOWNORMAL);SetDlgItemTextW(hwnd,112,reinterpret_cast<INT_PTR>(opened)>32?L"Everything started. Check connection after its index is ready.":L"Windows could not start Everything. Open it from Start.");}return TRUE;}
if(LOWORD(w)==IDCANCEL){EndDialog(hwnd,0);return TRUE;}if(LOWORD(w)==IDOK){auto o=self->options;o.enabled=IsDlgButtonChecked(hwnd,100)==BST_CHECKED;o.applications=IsDlgButtonChecked(hwnd,101)==BST_CHECKED;o.files=IsDlgButtonChecked(hwnd,102)==BST_CHECKED;o.windows=IsDlgButtonChecked(hwnd,103)==BST_CHECKED;o.fuzzy=IsDlgButtonChecked(hwnd,104)==BST_CHECKED;auto hot=SendDlgItemMessageW(hwnd,110,HKM_GETHOTKEY,0,0);o.key=LOBYTE(hot);auto hk=HIBYTE(hot);o.modifiers=((hk&HOTKEYF_CONTROL)?MOD_CONTROL:0)|((hk&HOTKEYF_ALT)?MOD_ALT:0)|((hk&HOTKEYF_SHIFT)?MOD_SHIFT:0);
            if(!o.key||!o.modifiers||o.key==VK_F12||(o.key==VK_TAB&&o.modifiers==MOD_ALT)){SetDlgItemTextW(hwnd,111,L"Choose a modified shortcut other than Alt+Tab or F12.");return TRUE;}self->result=o;EndDialog(hwnd,IDOK);return TRUE;}}
        return FALSE;
    }
    static std::optional<Options> show(HWND owner,Options options,bool registered){INITCOMMONCONTROLSEX ic{sizeof ic,ICC_WIN95_CLASSES};InitCommonControlsEx(&ic);struct Template {DLGTEMPLATE dialog;WORD menu=0,cls=0;wchar_t title[32]=L"Floatlet Search preferences";};Template t{};t.dialog.style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|DS_CENTER;t.dialog.cx=280;t.dialog.cy=250;Preferences p;p.options=options;p.registered=registered;DialogBoxIndirectParamW(GetModuleHandleW(nullptr),&t.dialog,owner,proc,reinterpret_cast<LPARAM>(&p));return p.result;}
};
}

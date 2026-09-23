#pragma once
#include <windows.h>
#include <commctrl.h>
#include <optional>
#include "options.h"
#include "shortcut.h"
#include "everything.h"
#include <shellapi.h>
#include <thread>
#include <mutex>
namespace delight::search {
struct Preferences {
    Options options;std::optional<Options> result;bool registered=false,recording=false;HFONT font=nullptr;HHOOK captureHook=nullptr;HWND captureWindow=nullptr;inline static Preferences* activeCapture=nullptr;
    std::thread probe;std::atomic<unsigned> probeGeneration=0;std::mutex probeMutex;
    std::wstring connection;Everything::Installation installed;bool canStart=false;
    ~Preferences(){stopCapture();++probeGeneration;if(probe.joinable())probe.join();if(font)DeleteObject(font);}
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
    void stopCapture(){recording=false;if(captureHook){UnhookWindowsHookEx(captureHook);captureHook=nullptr;}if(activeCapture==this)activeCapture=nullptr;}
    static LRESULT CALLBACK capture(int code,WPARAM message,LPARAM parameter){auto self=activeCapture;if(code==HC_ACTION&&self&&self->recording&&GetForegroundWindow()==self->captureWindow){auto key=reinterpret_cast<KBDLLHOOKSTRUCT*>(parameter);if(message==WM_KEYDOWN||message==WM_SYSKEYDOWN){if(key->vkCode==VK_ESCAPE){PostMessageW(self->captureWindow,WM_APP+95,0,0);return 1;}auto sides=normalizeSides(heldSides());Options candidate=self->options;candidate.key=key->vkCode;candidate.sides=sides;candidate.modifiers=sideModifiers(sides);candidate.extended=(key->flags&LLKHF_EXTENDED)!=0;if(validShortcut(candidate)&&!(GetAsyncKeyState(VK_LWIN)&0x8000)&&!(GetAsyncKeyState(VK_RWIN)&0x8000)){PostMessageW(self->captureWindow,WM_APP+94,candidate.key,candidate.sides|(candidate.extended?64:0));return 1;}}}return CallNextHookEx(nullptr,code,message,parameter);}
    void label(HWND hwnd){SetDlgItemTextW(hwnd,110,shortcutLabel(options).c_str());SetDlgItemTextW(hwnd,115,recording?L"Press your shortcut...":L"Record shortcut");}
    static LRESULT CALLBACK record(HWND h,UINT m,WPARAM w,LPARAM l,UINT_PTR id,DWORD_PTR data){auto self=reinterpret_cast<Preferences*>(data);
        if(m==WM_NCDESTROY){RemoveWindowSubclass(h,record,id);return DefSubclassProc(h,m,w,l);}
        if(self->recording&&m==WM_GETDLGCODE)return DLGC_WANTALLKEYS;
        if(self->recording&&(m==WM_KEYDOWN||m==WM_SYSKEYDOWN))return 0;
        return DefSubclassProc(h,m,w,l);
    }
    void provider(HWND hwnd){bool local=SendDlgItemMessageW(hwnd,116,CB_GETCURSEL,0,0)==1;EnableWindow(GetDlgItem(hwnd,117),local);EnableWindow(GetDlgItem(hwnd,113),!local);EnableWindow(GetDlgItem(hwnd,114),!local&&canStart);if(local)SetDlgItemTextW(hwnd,112,L"Built-in mode uses Floatlet's local index. Ctrl+R refreshes it.");else{std::wstring message;{std::lock_guard lock(probeMutex);message=connection;}SetDlgItemTextW(hwnd,112,message.empty()?L"Check Everything connection to see its current state.":message.c_str());}SetDlgItemTextW(hwnd,118,local?L"Floatlet indexes filenames locally, without Everything. Starts when you search. No file contents are read. Maximum 100,000 items with a 16 MiB metadata budget. Larger drives may need Everything. Network folders and linked subfolders are excluded.":L"Everything supplies its existing index. If unavailable, Floatlet tries the Windows Search index. Neither index is changed by Floatlet.");}
    static INT_PTR CALLBACK proc(HWND hwnd,UINT msg,WPARAM w,LPARAM l){
        auto self=reinterpret_cast<Preferences*>(GetWindowLongPtrW(hwnd,DWLP_USER));
        if(msg==WM_INITDIALOG){self=reinterpret_cast<Preferences*>(l);SetWindowLongPtrW(hwnd,DWLP_USER,l);UINT dpi=GetDpiForWindow(hwnd);auto px=[&](int n){return MulDiv(n,int(dpi),96);};self->font=CreateFontW(-px(14),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");auto make=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int width,int height,int id){auto h=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,px(x),px(y),px(width),px(height),hwnd,reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(self->font),TRUE);return h;};
            const wchar_t* labels[]={L"Enable Floatlet Search",L"Applications",L"Files and folders",L"Open windows",L"Approximate app spelling"};bool values[]={self->options.enabled,self->options.applications,self->options.files,self->options.windows,self->options.fuzzy};
            for(int i=0;i<5;++i){auto h=make(L"BUTTON",labels[i],WS_TABSTOP|BS_AUTOCHECKBOX,24+(i%2)*260,20+(i/2)*32,250,28,100+i);SendMessageW(h,BM_SETCHECK,values[i]?BST_CHECKED:BST_UNCHECKED,0);}
            make(L"STATIC",L"File search engine",0,24,130,170,24,0);auto engine=make(L"COMBOBOX",L"",WS_TABSTOP|CBS_DROPDOWNLIST,204,125,316,180,116);for(auto text:{L"Everything",L"Floatlet built-in"})SendMessageW(engine,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(text));SendMessageW(engine,CB_SETCURSEL,self->options.provider,0);
            make(L"STATIC",L"Built-in index scope",0,24,170,170,24,0);auto scope=make(L"COMBOBOX",L"",WS_TABSTOP|CBS_DROPDOWNLIST,204,165,316,180,117);for(auto text:{L"Personal folders",L"Local fixed drives"})SendMessageW(scope,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(text));SendMessageW(scope,CB_SETCURSEL,self->options.scope,0);
            make(L"STATIC",L"",0,24,210,496,94,118);
            make(L"STATIC",L"Search shortcut",0,24,315,150,24,0);make(L"STATIC",L"",0,180,315,340,25,110);auto recorder=make(L"BUTTON",L"Record shortcut",WS_TABSTOP,24,350,190,34,115);SetWindowSubclass(recorder,record,1,reinterpret_cast<DWORD_PTR>(self));
            make(L"BUTTON",L"Use Right Alt + Space",WS_TABSTOP,228,350,210,34,119);
            make(L"STATIC",L"Record a modifier plus a key. Left and Right keys are distinct. Escape cancels recording. Windows-reserved chords are excluded.",0,24,395,496,42,0);
            make(L"STATIC",self->registered?L"Shortcut is active.":L"Shortcut unavailable or disabled. Search remains in the tray menu.",0,24,447,496,40,111);
            make(L"STATIC",L"Everything connection",0,24,490,496,40,112);
            make(L"BUTTON",L"Check Everything",WS_TABSTOP,24,535,165,32,113);make(L"BUTTON",L"Start Everything",WS_TABSTOP,200,535,155,32,114);
            make(L"BUTTON",L"Save",WS_TABSTOP|BS_DEFPUSHBUTTON,324,590,90,34,IDOK);make(L"BUTTON",L"Cancel",WS_TABSTOP,430,590,90,34,IDCANCEL);
            RECT r{0,0,px(544),px(646)};AdjustWindowRectEx(&r,GetWindowLongW(hwnd,GWL_STYLE),FALSE,GetWindowLongW(hwnd,GWL_EXSTYLE));SetWindowPos(hwnd,nullptr,0,0,r.right-r.left,r.bottom-r.top,SWP_NOMOVE|SWP_NOZORDER);self->label(hwnd);self->provider(hwnd);if(self->options.provider==0)self->check(hwnd);else SetDlgItemTextW(hwnd,112,L"Built-in mode uses Floatlet's local index. Ctrl+R refreshes it.");return TRUE;
        }
        if(msg==WM_APP+94&&self&&self->recording){auto o=self->options;o.key=unsigned(w);o.sides=unsigned(l)&63;o.extended=(l&64)!=0;o.modifiers=sideModifiers(o.sides);self->stopCapture();if(validShortcut(o)){self->options=o;self->label(hwnd);SetDlgItemTextW(hwnd,111,L"Shortcut recorded. Press Save to apply it.");}return TRUE;}
        if(msg==WM_APP+95&&self&&self->recording){self->stopCapture();self->label(hwnd);SetDlgItemTextW(hwnd,111,L"Recording cancelled. Your previous shortcut is unchanged.");return TRUE;}
        if(msg==WM_ACTIVATE&&self&&LOWORD(w)==WA_INACTIVE&&self->recording){self->stopCapture();self->label(hwnd);return FALSE;}
        if(msg==WM_APP+93&&self){if(self->probe.joinable())self->probe.join();EnableWindow(GetDlgItem(hwnd,113),TRUE);self->provider(hwnd);return TRUE;}
        if(msg==WM_COMMAND&&self){
            if(LOWORD(w)==116&&HIWORD(w)==CBN_SELCHANGE){self->provider(hwnd);return TRUE;}
            if(LOWORD(w)==115){self->stopCapture();self->captureWindow=hwnd;activeCapture=self;self->captureHook=SetWindowsHookExW(WH_KEYBOARD_LL,capture,GetModuleHandleW(nullptr),0);if(!self->captureHook){activeCapture=nullptr;SetDlgItemTextW(hwnd,111,L"Windows could not start shortcut recording. Use the preset or try again.");return TRUE;}self->recording=true;self->label(hwnd);SetDlgItemTextW(hwnd,111,L"Hold your modifier keys, then press the final key. Nothing is saved until Save.");SetFocus(GetDlgItem(hwnd,115));return TRUE;}
            if(LOWORD(w)==119){self->stopCapture();self->options.key=VK_SPACE;self->options.modifiers=MOD_ALT;self->options.sides=2;self->options.extended=false;self->label(hwnd);SetDlgItemTextW(hwnd,111,L"Right Alt + Space selected. Left Alt is left available to other apps.");return TRUE;}
            if(LOWORD(w)==113){self->check(hwnd);return TRUE;}
            if(LOWORD(w)==114){std::wstring executable;{std::lock_guard lock(self->probeMutex);if(self->canStart)executable=self->installed.executable;}if(!executable.empty()){auto opened=ShellExecuteW(hwnd,L"open",executable.c_str(),nullptr,nullptr,SW_SHOWNORMAL);SetDlgItemTextW(hwnd,112,reinterpret_cast<INT_PTR>(opened)>32?L"Everything started. Check connection after its index is ready.":L"Windows could not start Everything. Open it from Start.");}return TRUE;}
if(LOWORD(w)==IDCANCEL){self->stopCapture();EndDialog(hwnd,0);return TRUE;}if(LOWORD(w)==IDOK){self->stopCapture();auto o=self->options;o.enabled=IsDlgButtonChecked(hwnd,100)==BST_CHECKED;o.applications=IsDlgButtonChecked(hwnd,101)==BST_CHECKED;o.files=IsDlgButtonChecked(hwnd,102)==BST_CHECKED;o.windows=IsDlgButtonChecked(hwnd,103)==BST_CHECKED;o.fuzzy=IsDlgButtonChecked(hwnd,104)==BST_CHECKED;o.provider=SendDlgItemMessageW(hwnd,116,CB_GETCURSEL,0,0)==1?1:0;o.scope=SendDlgItemMessageW(hwnd,117,CB_GETCURSEL,0,0)==1?1:0;
            if(!validShortcut(o)){SetDlgItemTextW(hwnd,111,L"Record a modifier and a supported key first.");return TRUE;}
            if(o.enabled&&!o.sides){if(!RegisterHotKey(hwnd,99,o.modifiers|MOD_NOREPEAT,o.key)){SetDlgItemTextW(hwnd,111,L"That shortcut is already in use. Record another, or choose Right Alt + Space.");return TRUE;}UnregisterHotKey(hwnd,99);}
            self->result=o;EndDialog(hwnd,IDOK);return TRUE;}}
        return FALSE;
    }
    static std::optional<Options> show(HWND owner,Options options,bool registered){INITCOMMONCONTROLSEX ic{sizeof ic,ICC_WIN95_CLASSES};InitCommonControlsEx(&ic);struct Template {DLGTEMPLATE dialog;WORD menu=0,cls=0;wchar_t title[32]=L"Floatlet Search preferences";};Template t{};t.dialog.style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|DS_CENTER;t.dialog.cx=280;t.dialog.cy=250;Preferences p;p.options=options;p.registered=registered;DialogBoxIndirectParamW(GetModuleHandleW(nullptr),&t.dialog,owner,proc,reinterpret_cast<LPARAM>(&p));return p.result;}
};
}

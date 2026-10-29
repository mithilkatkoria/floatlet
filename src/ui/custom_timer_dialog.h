#pragma once
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <algorithm>
#include <optional>
#include <string>
#include "search/paint.h"

namespace delight {
class CustomTimerDialog {
    HWND window{},owner{};HBRUSH input=CreateSolidBrush(RGB(36,34,44));HFONT font{};search::Paint paint;std::optional<unsigned> result;bool invalid=false;
    static constexpr int Hours=601,Minutes=602,Seconds=603;
    static LRESULT CALLBACK procedure(HWND h,UINT m,WPARAM w,LPARAM l){
        auto self=reinterpret_cast<CustomTimerDialog*>(GetWindowLongPtrW(h,GWLP_USERDATA));
        if(m==WM_NCCREATE){self=static_cast<CustomTimerDialog*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);self->window=h;SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
        if(!self)return DefWindowProcW(h,m,w,l);
        switch(m){
        case WM_CREATE:{
            self->font=CreateFontW(-22,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI Variable");
            const int ids[]={Hours,Minutes,Seconds};for(int i=0;i<3;++i){auto edit=CreateWindowExW(0,L"EDIT",L"0",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_NUMBER|ES_CENTER,36+i*135,141,112,43,h,reinterpret_cast<HMENU>(INT_PTR(ids[i])),GetModuleHandleW(nullptr),nullptr);SendMessageW(edit,WM_SETFONT,reinterpret_cast<WPARAM>(self->font),TRUE);SendMessageW(edit,EM_SETLIMITTEXT,2,0);}
            CreateWindowExW(0,L"BUTTON",L"Cancel",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,36,246,175,42,h,reinterpret_cast<HMENU>(INT_PTR(IDCANCEL)),GetModuleHandleW(nullptr),nullptr);
            CreateWindowExW(0,L"BUTTON",L"Set duration",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,229,246,175,42,h,reinterpret_cast<HMENU>(INT_PTR(IDOK)),GetModuleHandleW(nullptr),nullptr);
            return 0;}
        case WM_ERASEBKGND:return 1;
        case WM_PAINT:{PAINTSTRUCT ps{};BeginPaint(h,&ps);RECT r{};GetClientRect(h,&r);try{auto& p=self->paint;p.begin(ps.hdc,r);p.clear(RGB(22,22,29));p.round(23,19,5,27,2,RGB(188,164,235));p.text(L"Set an exact timer",37,20,363,36,23,RGB(247,245,252),true);p.text(L"Choose hours, minutes and seconds.",37,62,350,25,12,RGB(168,164,180));const wchar_t* names[]={L"HOURS",L"MINUTES",L"SECONDS"};for(int i=0;i<3;++i){int x=36+i*135;p.text(names[i],float(x),113,112,23,10,RGB(183,163,229),true,true);p.outline(float(x),140,112,45,10,RGB(77,71,91));}p.text(self->invalid?L"Enter 1 second to 24 hours. Minutes and seconds must be under 60.":L"Your timer begins after you press Start in the island.",36,199,368,23,11,self->invalid?RGB(246,146,146):RGB(160,156,172));p.end();}catch(...){}EndPaint(h,&ps);return 0;}
        case WM_CTLCOLOREDIT:{auto dc=reinterpret_cast<HDC>(w);SetTextColor(dc,RGB(251,249,255));SetBkColor(dc,RGB(36,34,44));return reinterpret_cast<LRESULT>(self->input);}
        case WM_DRAWITEM:{auto item=reinterpret_cast<DRAWITEMSTRUCT*>(l);RECT r=item->rcItem;bool start=item->CtlID==IDOK;try{self->paint.begin(item->hDC,r);self->paint.round(0,0,float(r.right-r.left),float(r.bottom-r.top),10,start?RGB(187,168,233):RGB(47,45,57));self->paint.text(start?L"Set duration":L"Cancel",start?46.f:68.f,10,float(r.right-r.left)-60,27,13,start?RGB(23,21,31):RGB(240,238,247),true);self->paint.end();}catch(...){}return TRUE;}
        case WM_COMMAND:
            if(LOWORD(w)==IDCANCEL){DestroyWindow(h);return 0;}
            if(LOWORD(w)==IDOK){unsigned parts[3]{};const int ids[]={Hours,Minutes,Seconds};bool valid=true;for(int i=0;i<3;++i){wchar_t text[8]{};GetDlgItemTextW(h,ids[i],text,8);if(!*text){parts[i]=0;continue;}for(auto cursor=text;*cursor;++cursor)if(*cursor<L'0'||*cursor>L'9')valid=false;parts[i]=unsigned(wcstoul(text,nullptr,10));}
                unsigned total=parts[0]*3600+parts[1]*60+parts[2];if(parts[1]>=60||parts[2]>=60||!total||total>86400)valid=false;
                if(valid){self->result=total;DestroyWindow(h);}else{self->invalid=true;InvalidateRect(h,nullptr,FALSE);}return 0;}
            return 0;
        case WM_CLOSE:DestroyWindow(h);return 0;
        case WM_NCDESTROY:self->window=nullptr;return DefWindowProcW(h,m,w,l);
        }return DefWindowProcW(h,m,w,l);
    }
public:
    ~CustomTimerDialog(){if(window)DestroyWindow(window);if(font)DeleteObject(font);DeleteObject(input);}
    static std::optional<unsigned> show(HWND owner,unsigned seconds){CustomTimerDialog dialog;dialog.owner=owner;
        WNDCLASSW cls{};cls.hInstance=GetModuleHandleW(nullptr);cls.lpfnWndProc=procedure;cls.lpszClassName=L"Floatlet.CustomTimer";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&cls);
        RECT parent{};GetWindowRect(owner,&parent);MONITORINFO monitor{sizeof monitor};GetMonitorInfoW(MonitorFromWindow(owner,MONITOR_DEFAULTTONEAREST),&monitor);
        int x=std::clamp((parent.left+parent.right-440)/2,monitor.rcWork.left,monitor.rcWork.right-440);
        int y=std::clamp(parent.bottom+14,monitor.rcWork.top,monitor.rcWork.bottom-330);
        dialog.window=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW,cls.lpszClassName,L"Custom timer",WS_POPUP|WS_CAPTION|WS_SYSMENU,x,y,440,330,owner,nullptr,cls.hInstance,&dialog);
        if(!dialog.window)return {};
        BOOL dark=TRUE;DwmSetWindowAttribute(dialog.window,20,&dark,sizeof dark);DWORD corners=2;DwmSetWindowAttribute(dialog.window,33,&corners,sizeof corners);
        SetDlgItemInt(dialog.window,Hours,seconds/3600,FALSE);SetDlgItemInt(dialog.window,Minutes,(seconds/60)%60,FALSE);SetDlgItemInt(dialog.window,Seconds,seconds%60,FALSE);
        EnableWindow(owner,FALSE);ShowWindow(dialog.window,SW_SHOW);SetForegroundWindow(dialog.window);SetFocus(GetDlgItem(dialog.window,Minutes));
        MSG message{};while(dialog.window){int status=GetMessageW(&message,nullptr,0,0);if(status<=0){if(status==0)PostQuitMessage(int(message.wParam));break;}if(message.message==WM_KEYDOWN&&message.wParam==VK_ESCAPE){DestroyWindow(dialog.window);continue;}if(message.message==WM_KEYDOWN&&message.wParam==VK_RETURN){SendMessageW(dialog.window,WM_COMMAND,IDOK,0);continue;}if(!IsDialogMessageW(dialog.window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}}
        if(IsWindow(owner)){EnableWindow(owner,TRUE);SetForegroundWindow(owner);}return dialog.result;
    }
};
}

#pragma once
#include <windows.h>
#include <shellapi.h>
#include <optional>
#include <string>
#include "services/calendar_feed.h"
namespace delight {
struct SubscriptionDialog {
    std::optional<std::wstring> result;
    std::vector<std::wstring> links;
    void update(HWND hwnd){auto list=GetDlgItem(hwnd,105);SendMessageW(list,LB_RESETCONTENT,0,0);for(size_t i=0;i<links.size();++i){auto label=L"Calendar "+std::to_wstring(i+1)+L" (private link saved)";SendMessageW(list,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));}SetWindowTextW(GetDlgItem(hwnd,106),(std::to_wstring(links.size())+L" of 8 calendars. Select one to remove it.").c_str());}

    static INT_PTR CALLBACK proc(HWND hwnd,UINT message,WPARAM w,LPARAM l){
        auto self=reinterpret_cast<SubscriptionDialog*>(GetWindowLongPtrW(hwnd,DWLP_USER));
        if(message==WM_INITDIALOG){self=reinterpret_cast<SubscriptionDialog*>(l);SetWindowLongPtrW(hwnd,DWLP_USER,l);auto font=GetStockObject(DEFAULT_GUI_FONT);auto child=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int width,int height,int id){auto h=CreateWindowExW(wcscmp(cls,L"EDIT")==0?WS_EX_CLIENTEDGE:0,cls,text,WS_CHILD|WS_VISIBLE|style,x,y,width,height,hwnd,reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);return h;};
            child(L"STATIC",L"Connect once. Floatlet checks for changes every minute.",0,18,16,445,25,0);
            auto edit=child(L"EDIT",L"",WS_TABSTOP|ES_AUTOHSCROLL|ES_PASSWORD,18,148,330,26,101);SendMessageW(edit,EM_SETLIMITTEXT,4096,0);
            child(L"BUTTON",L"1. Open Google Calendar settings",WS_TABSTOP,18,47,280,28,104);child(L"STATIC",L"2. Select your calendar under Settings for my calendars.\n   Open Integrate calendar. Copy Secret address in iCal format.\n3. Paste below. This private link stays encrypted on your PC.",0,18,84,445,58,0);child(L"BUTTON",L"Add calendar",WS_TABSTOP,358,148,105,26,107);
            child(L"LISTBOX",L"",WS_TABSTOP|WS_BORDER|WS_VSCROLL|LBS_NOTIFY,18,208,445,100,105);
            child(L"STATIC",L"",0,18,182,445,22,106);
            child(L"BUTTON",L"Remove selected",WS_TABSTOP,18,323,145,28,103);child(L"BUTTON",L"Save",WS_TABSTOP|BS_DEFPUSHBUTTON,245,323,105,28,IDOK);child(L"BUTTON",L"Cancel",WS_TABSTOP,360,323,103,28,IDCANCEL);self->update(hwnd);RECT bounds{0,0,484,373},position{};AdjustWindowRectEx(&bounds,GetWindowLongW(hwnd,GWL_STYLE),FALSE,GetWindowLongW(hwnd,GWL_EXSTYLE));GetWindowRect(hwnd,&position);int width=bounds.right-bounds.left,height=bounds.bottom-bounds.top;SetWindowPos(hwnd,nullptr,position.left+(position.right-position.left-width)/2,position.top+(position.bottom-position.top-height)/2,width,height,SWP_NOZORDER|SWP_NOACTIVATE);SetFocus(edit);return FALSE;
        }
        if(message==WM_COMMAND&&self){int id=LOWORD(w);if(id==IDCANCEL){EndDialog(hwnd,0);return TRUE;}if(id==103){auto index=SendDlgItemMessageW(hwnd,105,LB_GETCURSEL,0,0);if(index!=LB_ERR&&size_t(index)<self->links.size()){self->links.erase(self->links.begin()+index);self->update(hwnd);}return TRUE;}if(id==104){ShellExecuteW(hwnd,L"open",L"https://calendar.google.com/calendar/u/0/r/settings",nullptr,nullptr,SW_SHOWNORMAL);return TRUE;}if(id==IDOK||id==107){auto edit=GetDlgItem(hwnd,101);int length=GetWindowTextLengthW(edit);std::wstring value(size_t(length)+1,L'\0');GetWindowTextW(edit,value.data(),length+1);value.resize(length);if(value.empty()&&id==IDOK){self->result=CalendarFeed::joinLinks(self->links);EndDialog(hwnd,IDOK);return TRUE;}if(!CalendarFeed::validUrl(value)){MessageBoxW(hwnd,L"Paste an HTTPS iCal subscription address from calendar.google.com.",L"Check calendar address",MB_OK);return TRUE;}if(std::find(self->links.begin(),self->links.end(),value)==self->links.end()){if(self->links.size()>=CalendarFeed::MaxCalendars){MessageBoxW(hwnd,L"You can connect up to eight calendars. Remove one first.",L"Calendar limit",MB_OK);return TRUE;}self->links.push_back(value);}SetWindowTextW(edit,L"");self->update(hwnd);if(id==IDOK){self->result=CalendarFeed::joinLinks(self->links);EndDialog(hwnd,IDOK);}return TRUE;}}
        return FALSE;
    }
    static std::optional<std::wstring> show(HWND owner,std::vector<std::wstring> links={}){struct Template{DLGTEMPLATE dialog;WORD menu=0,windowClass=0;wchar_t title[32]=L"Manage Google Calendars";};Template t{};t.dialog.style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|DS_CENTER;t.dialog.cx=330;t.dialog.cy=260;SubscriptionDialog state;state.links=std::move(links);DialogBoxIndirectParamW(GetModuleHandleW(nullptr),&t.dialog,owner,proc,reinterpret_cast<LPARAM>(&state));return state.result;}
};
}

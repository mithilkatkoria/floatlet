#pragma once
#include <windows.h>
#include <shellapi.h>
#include <optional>
#include <string>
#include "services/calendar_feed.h"
namespace delight {
struct SubscriptionDialog {
    std::optional<std::wstring> result;
    static INT_PTR CALLBACK proc(HWND hwnd,UINT message,WPARAM w,LPARAM l){
        auto self=reinterpret_cast<SubscriptionDialog*>(GetWindowLongPtrW(hwnd,DWLP_USER));
        if(message==WM_INITDIALOG){self=reinterpret_cast<SubscriptionDialog*>(l);SetWindowLongPtrW(hwnd,DWLP_USER,l);auto font=GetStockObject(DEFAULT_GUI_FONT);auto child=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int width,int height,int id){auto h=CreateWindowExW(wcscmp(cls,L"EDIT")==0?WS_EX_CLIENTEDGE:0,cls,text,WS_CHILD|WS_VISIBLE|style,x,y,width,height,hwnd,reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);return h;};
            child(L"STATIC",L"Paste your Google Calendar secret iCal address.\nThe link is encrypted for your Windows account.",0,18,16,445,40,0);
            auto edit=child(L"EDIT",L"",WS_TABSTOP|ES_AUTOHSCROLL|ES_PASSWORD,18,68,445,26,101);SendMessageW(edit,EM_SETLIMITTEXT,4096,0);
            child(L"BUTTON",L"Where is my link?",WS_TABSTOP,18,112,125,28,104);child(L"BUTTON",L"Disconnect",WS_TABSTOP,154,112,95,28,103);child(L"BUTTON",L"Connect",WS_TABSTOP|BS_DEFPUSHBUTTON,260,112,95,28,IDOK);child(L"BUTTON",L"Cancel",WS_TABSTOP,367,112,95,28,IDCANCEL);SetFocus(edit);return FALSE;
        }
        if(message==WM_COMMAND&&self){int id=LOWORD(w);if(id==IDCANCEL){EndDialog(hwnd,0);return TRUE;}if(id==103){self->result=L"";EndDialog(hwnd,IDOK);return TRUE;}if(id==104){ShellExecuteW(hwnd,L"open",L"https://support.google.com/calendar/answer/37648",nullptr,nullptr,SW_SHOWNORMAL);return TRUE;}if(id==IDOK){auto edit=GetDlgItem(hwnd,101);int length=GetWindowTextLengthW(edit);std::wstring value(size_t(length)+1,L'\0');GetWindowTextW(edit,value.data(),length+1);value.resize(length);if(!CalendarFeed::validUrl(value)){MessageBoxW(hwnd,L"Paste an HTTPS iCal subscription address from calendar.google.com.",L"Check calendar address",MB_OK);return TRUE;}self->result=std::move(value);EndDialog(hwnd,IDOK);return TRUE;}}
        return FALSE;
    }
    static std::optional<std::wstring> show(HWND owner){struct Template{DLGTEMPLATE dialog;WORD menu=0,windowClass=0;wchar_t title[32]=L"Connect Google Calendar";};Template t{};t.dialog.style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|DS_CENTER;t.dialog.cx=330;t.dialog.cy=122;SubscriptionDialog state;DialogBoxIndirectParamW(GetModuleHandleW(nullptr),&t.dialog,owner,proc,reinterpret_cast<LPARAM>(&state));return state.result;}
};
}

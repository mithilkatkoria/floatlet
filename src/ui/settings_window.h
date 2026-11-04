#pragma once
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <algorithm>
#include <functional>
#include <string>
#include <vector>
#include "storage/settings.h"
#include "search/paint.h"
#include "search/shortcut.h"
#include "ui/fonts.h"

namespace delight {
class SettingsWindow {
public:
    enum Action { IconOnly=1, FollowPointer, Touch, ReduceMotion, Calls, AlignLeft, AlignCenter, AlignRight,
        SearchEnabled, SearchProvider, SearchApps, SearchFiles, SearchWindows,
        SearchFuzzy, SearchOptions, Calendar, Startup, Website, SignOut, Portfolio, ActivityAuto=30, ActivityMusic, ActivityTimer, ActivityStopwatch, ActivityCall, IncomingAlerts };
    SettingsWindow(HWND,Settings& source,std::function<void(int)> action,std::wstring identity={}):settings(source),perform(std::move(action)),accountEmail(std::move(identity)){}
    ~SettingsWindow(){if(window)DestroyWindow(window);}
    HWND native()const{return window;}
    void showSearch(){page=2;scroll=0;show();}
    void show(){
        if(!window){
            WNDCLASSW cls{};cls.hInstance=GetModuleHandleW(nullptr);cls.lpfnWndProc=procedure;cls.lpszClassName=L"Floatlet.Settings";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.hIcon=LoadIconW(cls.hInstance,MAKEINTRESOURCEW(101));RegisterClassW(&cls);
            // An owned window inherits the topmost island's stacking constraints.
            // Settings must be an independent, ordinary taskbar window.
            window=CreateWindowExW(WS_EX_APPWINDOW,L"Floatlet.Settings",L"Floatlet",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1080,780,nullptr,nullptr,cls.hInstance,this);
            if(!window)return;
            DWORD corners=2;DwmSetWindowAttribute(window,33,&corners,sizeof corners);
            BOOL dark=FALSE;DwmSetWindowAttribute(window,20,&dark,sizeof dark);
            COLORREF chrome=RGB(245,244,240),caption=RGB(35,34,32);DwmSetWindowAttribute(window,35,&chrome,sizeof chrome);DwmSetWindowAttribute(window,36,&caption,sizeof caption);
            POINT cursor{};GetCursorPos(&cursor);MONITORINFO monitor{sizeof monitor};GetMonitorInfoW(MonitorFromPoint(cursor,MONITOR_DEFAULTTONEAREST),&monitor);
            int width=std::min(int(1080*scale()),int(monitor.rcWork.right-monitor.rcWork.left-24));
            int height=std::min(int(780*scale()),int(monitor.rcWork.bottom-monitor.rcWork.top-24));
            SetWindowPos(window,HWND_NOTOPMOST,monitor.rcWork.left+(monitor.rcWork.right-monitor.rcWork.left-width)/2,monitor.rcWork.top+(monitor.rcWork.bottom-monitor.rcWork.top-height)/2,width,height,SWP_NOACTIVATE);
            paint.typography(InterfaceFonts::get().collection.get());
        }
        ShowWindow(window,SW_RESTORE);SetForegroundWindow(window);refresh();
    }
    void refresh(){if(window)InvalidateRect(window,nullptr,FALSE);}
private:
    struct Hit {float x,y,w,h;int action;};
    HWND window{};Settings& settings;std::function<void(int)> perform;std::wstring accountEmail;search::Paint paint;
    std::vector<Hit> hits;int page=0,hover=-1,focus=-1;float scroll=0;
    static constexpr COLORREF ink=RGB(37,36,34),muted=RGB(117,114,108),edge=RGB(233,230,224),paper=RGB(255,255,254),warm=RGB(245,244,240),selected=RGB(237,234,226),accent=RGB(111,89,158);
    float scale()const{return window?GetDpiForWindow(window)/96.f:1.f;}
    float height()const{RECT r{};GetClientRect(window,&r);return r.bottom/scale();}
    float maxScroll()const{return std::max(0.f,(page==0?710.f:page==2?750.f:page==4?810.f:720.f)-height());}
    int at(float x,float y)const{for(size_t i=0;i<hits.size();++i){auto item=hits[i];if(item.x>190&&(y<12||y>=height()-12))continue;if(x>=item.x&&x<item.x+item.w&&y>=item.y&&y<item.y+item.h)return int(i);}return -1;}
    void hit(float x,float y,float w,float h,int action,bool fixed=false){hits.push_back({x,y-(fixed?0:scroll),w,h,action});}
    void activate(int index){if(index<0||index>=int(hits.size()))return;int action=hits[index].action;if(action>=100){page=action-100;scroll=0;focus=-1;hover=-1;refresh();return;}perform(action);refresh();}
    static LRESULT CALLBACK procedure(HWND h,UINT m,WPARAM w,LPARAM l){
        auto self=reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(h,GWLP_USERDATA));
        if(m==WM_NCCREATE){self=static_cast<SettingsWindow*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);self->window=h;SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
        if(!self)return DefWindowProcW(h,m,w,l);
        switch(m){
        case WM_ERASEBKGND:return 1;
        case WM_PAINT:{PAINTSTRUCT ps{};BeginPaint(h,&ps);self->draw(ps.hdc);EndPaint(h,&ps);return 0;}
        case WM_LBUTTONUP:self->focus=-1;self->activate(self->at(float(GET_X_LPARAM(l))/self->scale(),float(GET_Y_LPARAM(l))/self->scale()));return 0;
        case WM_MOUSEMOVE:{int next=self->at(float(GET_X_LPARAM(l))/self->scale(),float(GET_Y_LPARAM(l))/self->scale());if(next!=self->hover){self->hover=next;self->refresh();}TRACKMOUSEEVENT tracking{sizeof tracking,TME_LEAVE,h,0};TrackMouseEvent(&tracking);return 0;}
        case WM_MOUSELEAVE:self->hover=-1;self->refresh();return 0;
        case WM_SETCURSOR:if(LOWORD(l)==HTCLIENT&&self->hover>=0){SetCursor(LoadCursorW(nullptr,IDC_HAND));return TRUE;}break;
        case WM_MOUSEWHEEL:self->scroll=std::clamp(self->scroll-GET_WHEEL_DELTA_WPARAM(w)/float(WHEEL_DELTA)*60.f,0.f,self->maxScroll());self->hover=-1;self->refresh();return 0;
        case WM_KEYDOWN:
            if(w==VK_ESCAPE){DestroyWindow(h);return 0;}
            if((w==VK_TAB||w==VK_DOWN||w==VK_UP)&&!self->hits.empty()){
                int direction=(w==VK_UP||(w==VK_TAB&&(GetKeyState(VK_SHIFT)&0x8000)))?-1:1;
                self->focus=(self->focus+direction+int(self->hits.size()))%int(self->hits.size());auto item=self->hits[self->focus];
                if(item.x>190){if(item.y<20)self->scroll=std::max(0.f,self->scroll+item.y-20);if(item.y+item.h>self->height()-20)self->scroll=std::min(self->maxScroll(),self->scroll+item.y+item.h-self->height()+20);}
                self->refresh();return 0;
            }
            if(w==VK_RETURN||w==VK_SPACE){self->activate(self->focus);return 0;}
            break;
        case WM_SIZE:self->scroll=std::clamp(self->scroll,0.f,self->maxScroll());self->refresh();return 0;
        case WM_GETMINMAXINFO:{auto info=reinterpret_cast<MINMAXINFO*>(l);info->ptMinTrackSize={LONG(760*self->scale()),LONG(480*self->scale())};return 0;}
        case WM_DPICHANGED:{auto rect=reinterpret_cast<RECT*>(l);SetWindowPos(h,nullptr,rect->left,rect->top,rect->right-rect->left,rect->bottom-rect->top,SWP_NOZORDER|SWP_NOACTIVATE);self->refresh();return 0;}
        case WM_CLOSE:DestroyWindow(h);return 0;
        case WM_NCDESTROY:self->paint=search::Paint{};self->window=nullptr;return DefWindowProcW(h,m,w,l);
        }return DefWindowProcW(h,m,w,l);
    }
    void text(const std::wstring& value,float x,float y,float w,float size,COLORREF colour=ink,bool bold=false,bool serif=false){float s=scale();paint.text(value,x*s,y*s,w*s,(size*1.5f+4)*s,size*s,colour,bold,false,serif);}
    void box(float x,float y,float w,float h,float r,COLORREF c){float s=scale();paint.round(x*s,y*s,w*s,h*s,r*s,c);}
    void line(float x,float y,float w,COLORREF c=edge){float s=scale();paint.line(x*s,y*s,(x+w)*s,y*s,c,s);}
    void icon(int kind,float x,float y,COLORREF c){
        float s=scale();auto stroke=[&](float a,float b,float u,float v){paint.line((x+a)*s,(y+b)*s,(x+u)*s,(y+v)*s,c,1.6f*s);};
        if(kind==0){stroke(2,9,10,2);stroke(10,2,18,9);stroke(4,8,4,18);stroke(4,18,16,18);stroke(16,18,16,8);stroke(8,18,8,12);stroke(8,12,12,12);stroke(12,12,12,18);}
        if(kind==1){paint.outline((x+2)*s,(y+4)*s,16*s,12*s,5*s,c,1.6f*s);paint.ellipse((x+7)*s,(y+10)*s,2*s,2*s,c);stroke(12,8,15,8);stroke(12,12,15,12);}
        if(kind==2){paint.outline((x+2)*s,(y+2)*s,12*s,12*s,6*s,c,1.6f*s);stroke(12,12,18,18);}
        if(kind==3){paint.outline((x+1)*s,(y+5)*s,11*s,7*s,3.5f*s,c,1.6f*s);paint.outline((x+8)*s,(y+10)*s,11*s,7*s,3.5f*s,c,1.6f*s);}
    }
    void row(float x,float y,float w,const wchar_t* title,const std::wstring& detail,int action,bool enabled=false,bool arrow=false){
        int index=int(hits.size());box(x,y,w,64,10,index==hover?RGB(248,247,243):paper);
        text(title,x+18,y+10,w-110,14,ink,true);text(detail,x+18,y+34,w-110,12,muted);
        if(arrow)text(L"›",x+w-36,y+13,22,25,muted);
        else{box(x+w-62,y+20,40,24,12,enabled?ink:RGB(219,217,210));float s=scale();paint.ellipse((x+w-(enabled?34:50))*s,(y+32)*s,9*s,9*s,paper);}
        hit(x,y,w,64,action);line(x+18,y+64,w-36);
    }
    void draw(HDC dc){
        RECT bounds{};GetClientRect(window,&bounds);float s=scale(),w=bounds.right/s,h=bounds.bottom/s;hits.clear();
        try{
            paint.begin(dc,bounds);paint.clear(warm);box(190,10,w-200,h-20,22,paper);
            box(24,26,15,28,6,ink);box(24,26,27,11,5,ink);box(27,39,19,8,4,ink);paint.ellipse(51*s,54*s,4*s,4*s,accent);
            text(L"Floatlet",65,28,112,21,ink,true);text(L"Your desktop companion",24,76,150,11,muted);
            const wchar_t* tabs[]={L"Home",L"Appearance",L"Search",L"Connections",L"Island activity"};
            for(int i=0;i<5;++i){float y=126.f+i*48.f;bool active=page==i;if(active||hover==i)box(12,y,166,40,9,active?selected:RGB(239,237,231));icon(i==4?1:i,26,y+10,active?ink:muted);text(tabs[i],60,y+10,111,14,active?ink:muted,active);hit(12,y,166,40,100+i,true);}
            line(24,h-116,142);text(L"Free. Open source.",24,h-94,150,12,ink);const bool creditHovered = hover == static_cast<int>(hits.size());
            text(L"made by draey.dev",24,h-66,150,11,creditHovered ? accent : muted);
            if(creditHovered) line(24,h-49,96,accent);
            hit(18,h-70,112,24,Portfolio,true);
            text(L"Floatlet for Windows",24,h-40,150,10,muted);
            paint.target->PushAxisAlignedClip(D2D1::RectF(194*s,12*s,(w-12)*s,(h-12)*s),D2D1_ANTIALIAS_MODE_ALIASED);
            paint.target->SetTransform(D2D1::Matrix3x2F::Translation(0,-scroll*s));
            float x=226,cw=w-x-38;
            const wchar_t* titles[]={L"Welcome to Floatlet",L"Appearance",L"Search",L"Connections",L"Island activity"};
            const wchar_t* descriptions[]={L"A little less switching. A little more room to focus.",L"Make the island comfortable for the way you work.",L"Your apps, files and windows, one shortcut away.",L"Choose what belongs in your everyday setup.",L"Keep the activity you care about in the resting island."};
            text(titles[page],x,40,cw,25,ink,true);text(descriptions[page],x,81,cw,14,muted);
            if(page==0){
                bool wide=cw>=690;float main=wide?cw-236:cw;
                box(x,132,main,225,16,RGB(38,35,33));
                text(L"A home for the",x+26,151,main-42,34,RGB(255,250,238),false,true);
                text(L"little things.",x+26,192,main-42,34,RGB(255,250,238),false,true);
                text(L"Keep music, files and timers within reach.",x+27,250,main-50,13,RGB(216,210,199));
                box(x+26,300,177,35,8,RGB(248,243,230));text(L"Personalise your island",x+40,309,153,12,ink,true);hit(x+26,300,177,35,101);
                if(wide){float sx=x+main+18;box(sx,132,218,225,16,warm);text(L"Your setup",sx+21,148,175,26,ink,false,true);
                    text(L"Search shortcut",sx+21,199,175,11,muted);text(search::shortcutLabel(settings.search),sx+21,219,175,15,ink,true);line(sx+21,253,176);
                    text(L"File search",sx+21,268,175,11,muted);text(settings.search.provider?L"Floatlet index":L"Everything",sx+21,290,175,15,ink,true);}
                text(L"Make yourself at home",x,391,cw,19,ink,true);
                const wchar_t* names[]={L"Your island",L"Your search",L"Your connections"};
                const wchar_t* details[]={L"Position, display and motion",L"Sources, matching and shortcuts",L"Calendar and Windows startup"};
                for(int i=0;i<3;++i){float y=434.f+i*73.f;int index=int(hits.size());box(x,y,cw,66,11,index==hover?warm:paper);icon(i+1,x+16,y+23,ink);text(names[i],x+54,y+10,cw-100,14,ink,true);text(details[i],x+54,y+35,cw-100,12,muted);text(L"›",x+cw-30,y+17,25,22,muted);line(x+54,y+66,cw-54);hit(x,y,cw,66,101+i);}
                text(L"Small on your desktop. Ready when you need it.",x,674,cw,12,muted);
            }else if(page==1){
                box(x,132,cw,111,14,warm);text(L"The island",x+20,150,200,12,muted);text(settings.iconOnly?L"Artwork at a glance":L"A little space for what is playing",x+20,178,cw-250,16,ink,true);
                float px=x+cw-207;box(px,165,185,47,24,RGB(12,12,12));box(px+8,173,31,31,8,RGB(220,195,151));text(L"♪",px+16,174,23,21,RGB(58,44,28));if(!settings.iconOnly){text(L"Your music",px+50,170,103,11,paper,true);text(L"Playing now",px+50,189,103,9,RGB(168,166,161));}text(L"▶",px+160,179,20,14,paper);
                text(L"Display and behaviour",x,270,cw,17,ink,true);
                row(x,306,cw,L"Icon-only music",L"Keep just the artwork when the island rests",IconOnly,settings.iconOnly);
                row(x,371,cw,L"Follow your pointer",L"Move the island to the screen you are using",FollowPointer,settings.followPointer);
                row(x,436,cw,L"Larger touch targets",L"Give the controls more room",Touch,settings.touch);
                row(x,501,cw,L"Reduce motion",L"Use simpler transitions",ReduceMotion,settings.reduceMotion);
                row(x,566,cw,L"Show calls",L"Show supported calls and microphone activity",Calls,settings.callControls);
                text(L"Island position",x+18,654,200,14,ink,true);
                const wchar_t* positions[]={L"Left",L"Centre",L"Right"};float start=x+cw-240;
                box(start,645,230,36,9,warm);for(int i=0;i<3;++i){bool active=settings.alignment==i;float bx=start+3+i*76;if(active)box(bx,648,72,30,7,paper);text(positions[i],bx+15,655,67,12,active?ink:muted,active);hit(bx,645,76,36,AlignLeft+i);}
            }else if(page==2){
                box(x,132,cw,87,13,warm);icon(2,x+20,163,ink);text(L"Open search with",x+56,146,cw-100,12,muted);text(search::shortcutLabel(settings.search),x+56,170,cw-220,20,ink,true);text(L"Edit shortcut  ›",x+cw-140,167,126,12,accent,true);hit(x,132,cw,87,SearchOptions);
                text(L"Search preferences",x,247,cw,17,ink,true);
                row(x,279,cw,L"Floatlet Search",L"Open the launcher on your active screen",SearchEnabled,settings.search.enabled);
                row(x,344,cw,L"File search engine",settings.search.provider?L"Floatlet built-in index":L"Everything, with Windows fallback",SearchProvider,false,true);
                row(x,409,cw,L"Applications",L"Find installed desktop and Store apps",SearchApps,settings.search.applications);
                row(x,474,cw,L"Files and folders",L"Search filenames and paths",SearchFiles,settings.search.files);
                row(x,539,cw,L"Open windows",L"Switch to a running app",SearchWindows,settings.search.windows);
                row(x,604,cw,L"Fuzzy matching",L"Include close matches when you mistype",SearchFuzzy,settings.search.fuzzy);
                text(L"Advanced preferences and shortcut recording",x+18,689,cw-30,13,accent,true);hit(x,679,cw,35,SearchOptions);
            }else if(page==4){
                text(L"Prefer on the island",x,132,cw,18,ink,true);
                text(L"When several activities are running, your choice stays in front.",x,164,cw,13,muted);
                const wchar_t* names[]={L"Automatic",L"Music",L"Timer",L"Stopwatch",L"Calls"};
                const wchar_t* detail[]={L"Calls, then timers, stopwatch and music",L"Keep your track visible, even during a call",L"Keep the remaining time within sight",L"Keep elapsed time in the resting island",L"Keep call and microphone controls in front"};
                for(int i=0;i<5;++i){float y=207.f+i*72.f;bool active=int(settings.preferredActivity)==i;
                    box(x,y,cw,64,12,active?selected:(hover==int(hits.size())?warm:paper));
                    paint.outline((x+22)*s,(y+23)*s,18*s,18*s,9*s,active?ink:edge,1.5f*s);
                    if(active)paint.ellipse((x+31)*s,(y+32)*s,5*s,5*s,ink);
                    text(names[i],x+55,y+10,cw-76,14,ink,true);text(detail[i],x+55,y+34,cw-76,12,muted);hit(x,y,cw,64,ActivityAuto+i);
                }
                text(L"When your choice stops, another active item takes its place.",x+4,582,cw-8,12,muted);
                row(x,621,cw,L"Incoming call alerts",L"Let a ringing call take priority",IncomingAlerts,settings.incomingCallAlerts);
                text(L"Finished timers and alarms always alert you.",x+18,712,cw-36,13,ink,true);
                text(L"These preferences change the display, not the running activities.",x+18,742,cw-36,12,muted);
            }else{
                box(x,132,cw,111,14,warm);text(L"Connected, by choice.",x+22,147,cw-44,29,ink,false,true);text(L"Your settings and file references stay on this PC.",x+23,200,cw-46,13,muted);
                text(L"Your connections",x,278,cw,17,ink,true);
                row(x,319,cw,L"Google Calendar",L"Connect a private calendar link",Calendar,false,true);
                row(x,391,cw,L"Start with Windows",L"Manage Floatlet in Windows startup settings",Startup,false,true);
                row(x,463,cw,L"Website and downloads",L"Visit floatlet.vercel.app",Website,false,true);
                if(!accountEmail.empty())row(x,550,cw,L"Sign out of Floatlet",accountEmail,SignOut,false,true);
                else{text(L"Made for your everyday desktop.",x+18,576,cw-36,24,ink,false,true);text(L"Free and open source, with settings stored locally.",x+18,620,cw-36,13,muted);}
            }
            paint.target->SetTransform(D2D1::Matrix3x2F::Identity());paint.target->PopAxisAlignedClip();
            if(focus>=0&&focus<int(hits.size())){auto item=hits[focus];paint.outline(item.x*s,item.y*s,item.w*s,item.h*s,9*s,accent,1.5f*s);}
            if(maxScroll()>0){float bar=(h-44)*h/(h+maxScroll());box(w-18,22+(h-44-bar)*scroll/maxScroll(),3,bar,2,RGB(212,208,198));}
            paint.end();
        }catch(...){}
    }
};
}

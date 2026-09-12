#include <array>
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>
#include <wtsapi32.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <winrt/Windows.Networking.Connectivity.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <winrt/Windows.Foundation.h>
#include "ui/model.h"
#include "ui/monitor_follow.h"
#include "ui/renderer.h"
#include "storage/settings.h"
#include "platform/shelf.h"
#include "platform/window_geometry.h"
#include "platform/capture_guard.h"
#include "services/media.h"
#include "services/audio.h"
#include "services/system_controls.h"
#include "ui/calendar.h"
#include "ui/hover_controls.h"
#include "ui/countdown.h"
#include "ui/time_tools.h"
#include "services/chime.h"
#include "services/calendar_feed.h"
#include "services/calendar_reminders.h"
#include "services/microphone.h"
#include "platform/subscription_dialog.h"
#include <thread>
#include <condition_variable>
#include <atomic>
#include <sstream>
#include <iomanip>
using namespace delight;
namespace {
constexpr UINT Tray=WM_APP+1;
HWND captureNotifyWindow=nullptr;
void CALLBACK foregroundChanged(HWINEVENTHOOK,DWORD,HWND window,LONG,LONG,DWORD,DWORD){if(captureNotifyWindow)PostMessageW(captureNotifyWindow,WM_APP+8,0,reinterpret_cast<LPARAM>(window));}
enum Command { Show=1,Hide,Quit,Music,Files,Controls,Pin,Add,Touch,Left,Center,Right,OtherMonitor,SettingsHelp,Play,Previous,Next,VolDown,VolUp,Mute,Wifi,Bluetooth,Display,Sound,Power,FollowPointer,IconOnly,TimerPanel,PreferencesPanel,TimerStart,TimerPause,TimerCancel,TimerLess,TimerMore,Timer1,Timer5,Timer10,Timer25,ReduceMotion,CallControls,CallPanel,MicrophoneMute,CalendarConnect,AgendaPanel,CalendarRefresh,StartupSettings,CalendarPanel,ScreenshotsPanel,NightLight,Mirror,WifiSettings,WifiPanel,PreviousMonth,NextMonth,Today,NextYear,DismissAlert,RemoveBase=1000,RevealBase=2000 };
struct Writer {
    std::mutex mutex;std::condition_variable cv;std::optional<Settings> pending;bool finish=false;std::atomic_bool failed=false;
    std::filesystem::path file;
    std::thread worker;
    explicit Writer(std::filesystem::path f):file(std::move(f)),worker([this]{
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        for(;;){std::optional<Settings> value;{std::unique_lock lock(mutex);cv.wait(lock,[&]{return finish||pending.has_value();});if(!pending&&finish)return;value=std::move(pending);pending.reset();}try{save(file,*value);}catch(...){failed=true;}}
    }){}
    void put(const Settings& s){{std::scoped_lock lock(mutex);pending=s;}cv.notify_one();}
    ~Writer(){{std::scoped_lock lock(mutex);finish=true;}cv.notify_one();worker.join();}
};
std::uint64_t value(FILETIME t){return (std::uint64_t(t.dwHighDateTime)<<32)|t.dwLowDateTime;}
struct App {
    HWND hwnd=nullptr,previousFocus=nullptr;Model model;Settings settings;std::unique_ptr<Writer> writer;
    std::wstring notice,noticeDetail=L"Floatlet";CalendarReminders reminders;Glyph noticeGlyph=Glyph::Camera;ULONGLONG noticeUntil=0;std::array<float,9> levels{};bool visualTicking=false;
    Audio* audio=nullptr;std::unique_ptr<SystemControls> system;ControlSnapshot controls;Calendar calendar;
    std::unique_ptr<MicrophoneMonitor> microphone;MicrophoneSnapshot mic;std::unique_ptr<CalendarFeed> feed;AgendaSnapshot agenda;int agendaOffset=0;
    Countdown countdown;Stopwatch stopwatch;Alarm alarm;Chime chime;TimeMode timeMode=TimeMode::Timer;bool clockTicking=false;unsigned timerMinutes=5;ULONGLONG lastBrightnessWrite=0;int pendingBrightness=-1;
    int shelfOffset=0,listOffset=0,slider=0;float sliderValue=0;ULONGLONG lastSliderPaint=0;std::wstring airpodsNotice;
    std::unique_ptr<Renderer> renderer;std::shared_ptr<Media> media;DropTarget* drop=nullptr;
    UINT taskbar=RegisterWindowMessageW(L"TaskbarCreated");HMONITOR monitor=nullptr;float dpi=96;Size size{224,36};
    bool review=false;bool interactive=false,tracking=false,hotkey=false,emergency=false,suspended=false;State beforeSuspend=State::Collapsed;
    std::wstring message;std::optional<double> cpuLoad;MEMORYSTATUSEX memory{sizeof(memory)};
    std::uint64_t lastIdle=0,lastKernel=0,lastUser=0;ULONGLONG lastSample=0;
    winrt::event_token networkToken{};winrt::Windows::UI::ViewManagement::UISettings uiSettings;
    std::shared_ptr<std::atomic<HWND>> callbackWindow=std::make_shared<std::atomic<HWND>>(nullptr);
    POINT down{};int downItem=-1;HPOWERNOTIFY powerNotify=nullptr;
    MonitorFollow follower;
    bool menuOpen=false;
    WindowGeometry geometry;
    bool captureFrozen=false,layoutQueued=false;
    HWINEVENTHOOK foregroundHook=nullptr;
    bool captureActive(){return captureFrozen||((GetAsyncKeyState(VK_LWIN)&0x8000)&&(GetAsyncKeyState(VK_SHIFT)&0x8000)&&(GetAsyncKeyState('S')&0x8000));}
    void syncCanvas(){if(renderer)renderer->canvas(float(geometry.bounds.w),float(geometry.bounds.h));}
    void followTimer(){
        KillTimer(hwnd,5);follower.reset();
        if(settings.followPointer&&!captureFrozen&&!suspended&&model.state!=State::Hidden&&model.state!=State::Unavailable&&GetSystemMetrics(SM_CMONITORS)>1)SetCoalescableTimer(hwnd,5,100,nullptr,20);
    }
    void followMonitor(){
        POINT point{};if(!GetCursorPos(&point)){follower.reset();return;}
        auto next=MonitorFromPoint(point,MONITOR_DEFAULTTONULL);
        bool blocked=captureActive()||menuOpen||!IsWindowEnabled(hwnd)||suspended||GetCapture()!=nullptr||(GetAsyncKeyState(VK_LBUTTON)&0x8000)||model.state==State::DragOver||model.state==State::DraggingOut;
        MONITORINFO mi{sizeof(mi)};bool interior=next&&GetMonitorInfoW(next,&mi)&&point.x>mi.rcMonitor.left+24&&point.x<mi.rcMonitor.right-24&&point.y>mi.rcMonitor.top+24&&point.y<mi.rcMonitor.bottom-24;
        if(!blocked&&interior&&next!=monitor){follower.candidate=reinterpret_cast<std::uintptr_t>(next);follower.since=GetTickCount64()-200;}
        if(follower.update(reinterpret_cast<std::uintptr_t>(monitor),reinterpret_cast<std::uintptr_t>(next),GetTickCount64(),blocked)){
            KillTimer(hwnd,1);KillTimer(hwnd,2);tracking=false;monitor=next;
            if(model.state==State::Peek)model.state=State::Collapsed;
            layout();
        }
    }
    void error(const wchar_t* text){message=text;render();}
    void tray(bool remove=false){if(review)return;NOTIFYICONDATAW n{sizeof(n)};n.hWnd=hwnd;n.uID=1;n.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;n.uCallbackMessage=Tray;n.hIcon=LoadIconW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(101));wcscpy_s(n.szTip,hotkey?L"Floatlet | Ctrl+Shift+F12":L"Floatlet | Click to open");Shell_NotifyIconW(remove?NIM_DELETE:NIM_ADD,&n);}
    void dirty(){SetTimer(hwnd,3,500,nullptr);}
    void focus(){if(interactive)return;previousFocus=GetForegroundWindow();interactive=true;SetWindowLongPtrW(hwnd,GWL_EXSTYLE,GetWindowLongPtrW(hwnd,GWL_EXSTYLE)&~WS_EX_NOACTIVATE);SetForegroundWindow(hwnd);SetFocus(hwnd);}
    void unfocus(){bool restore=interactive&&GetForegroundWindow()==hwnd;interactive=false;SetWindowLongPtrW(hwnd,GWL_EXSTYLE,GetWindowLongPtrW(hwnd,GWL_EXSTYLE)|WS_EX_NOACTIVATE);if(restore&&IsWindow(previousFocus))SetForegroundWindow(previousFocus);previousFocus=nullptr;}
    void event(Event e){if(e==Event::Escape||e==Event::Hide){notice.clear();KillTimer(hwnd,11);}auto old=model.state;model.send(e);if(old!=model.state)layout();}
    void layout(){
        if(geometry.applying)return;if(captureFrozen&&model.state!=State::Hidden&&model.state!=State::Unavailable)return;
        KillTimer(hwnd,6);
        KillTimer(hwnd,4);
        followTimer();syncTimeTick();syncVisualTick();
        if(model.state==State::Hidden||model.state==State::Unavailable){ShowWindow(hwnd,SW_HIDE);return;}
        BOOL motion=TRUE;SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&motion,0);
        auto desired=sizeFor(model.state,settings.touch);auto currentMedia=media?media->get():MediaSnapshot{};bool activity=(mic.active&&settings.callControls)||countdown.active()||countdown.finished||alarm.ringing||stopwatch.running;
        if(!activity&&!currentMedia.play&&airpodsNotice.empty()&&(model.state==State::Collapsed||model.state==State::Peek))desired={model.state==State::Peek?96.f:84.f,settings.touch?44.f:model.state==State::Peek?32.f:28.f};
        else if(model.state==State::Collapsed&&settings.iconOnly&&!activity)desired.w=84;
        if(!notice.empty()&&model.state==State::Peek)desired={300,68};
        geometry.configure(monitor,desired,settings.alignment,motion!=FALSE&&!settings.reduceMotion);
        monitor=geometry.monitor;dpi=geometry.dpi;size=geometry.target;syncCanvas();
        ShowWindow(hwnd,SW_SHOWNOACTIVATE);render(true);
        if(geometry.animating)SetTimer(hwnd,6,15,nullptr);
        if(model.state==State::Controls&&!suspended){sample();SetTimer(hwnd,4,1000,nullptr);}else{lastSample=0;cpuLoad.reset();}
    }
    void render(bool animate=false){
        if(!renderer||captureFrozen||suspended||model.state==State::Hidden||model.state==State::Unavailable)return;
        if(writer&&writer->failed.exchange(false)) message=L"Settings could not be saved. Check disk access.";
        std::vector<Line> lines;std::vector<Mark> marks;auto add=[&](std::wstring t,float x,float y,float font=14,bool secondary=false,float width=0,bool centered=false){lines.push_back({std::move(t),x,y,font,secondary,width,centered});};
        auto icon=[&](Glyph g,float x,float y,float s,bool active=false,bool disabled=false){marks.push_back({g,x,y,s,active,disabled});};
        auto m=media?media->get():MediaSnapshot{};
        if(model.state==State::Peek&&!notice.empty()){marks.push_back({noticeGlyph,18,19,28,true,false,28,noticeGlyph==Glyph::AlarmBell&&!settings.reduceMotion?sinf(GetTickCount64()*.035f)*9.f:0.f});add(notice,61,14,15,false,220);add(noticeDetail,61,37,10,true,220);}
        else if((model.state==State::Collapsed||model.state==State::Peek)&&mic.active&&settings.callControls){icon(Glyph::Microphone,12,(size.h-24)/2,24,mic.muted);add(mic.app,48,(size.h-20)/2,13,false,size.w-94);icon(Glyph::Microphone,size.w-33,(size.h-18)/2,18,mic.muted);}
        else if((model.state==State::Collapsed||model.state==State::Peek)&&alarm.ringing){marks.push_back({Glyph::AlarmBell,12,(size.h-24)/2,24,true,false,24,!settings.reduceMotion&&GetTickCount64()%4000<800?sinf(GetTickCount64()*.035f)*9.f:0.f});add(L"Alarm",48,(size.h-21)/2,15);icon(Glyph::Close,size.w-32,(size.h-16)/2,16);}
        else if((model.state==State::Collapsed||model.state==State::Peek)&&(countdown.active()||countdown.finished)){
            marks.push_back({countdown.finished?Glyph::AlarmBell:Glyph::Timer,12,(size.h-24)/2,24,true,false,24,countdown.finished?(!settings.reduceMotion&&GetTickCount64()%4000<800?sinf(GetTickCount64()*.035f)*9.f:0.f):countdown.fraction(GetTickCount64())});add(countdown.finished?L"Timer complete":countdown.text(GetTickCount64()),48,(size.h-21)/2,15,false,143);icon(countdown.finished?Glyph::Close:countdown.running?Glyph::Pause:Glyph::Play,size.w-32,(size.h-14)/2,14);
        }
        else if((model.state==State::Collapsed||model.state==State::Peek)&&stopwatch.running){icon(Glyph::Timer,13,(size.h-22)/2,22,true);add(stopwatch.text(GetTickCount64()),47,(size.h-22)/2,16,false,130);icon(Glyph::Pause,size.w-33,(size.h-18)/2,18);}
        else if((model.state==State::Collapsed||model.state==State::Peek)&&!m.play&&airpodsNotice.empty()){ /* Resting pill intentionally has no text or disabled transport. */ }
        else if(model.state==State::Collapsed){icon(m.artwork?Glyph::Disc:Glyph::Logo,12,(size.h-23)/2,23);if(!settings.iconOnly)add(m.title,44,(size.h-19)/2,12.5f,false,140);if(m.playing){float left=settings.iconOnly?51.f:185.f;for(int i=0;i<9;++i)marks.push_back({Glyph::Wavebar,left+i*(settings.iconOnly?2.f:2.4f),(size.h-12)/2,settings.iconOnly?1.2f:1.5f,true,false,12,levels[i]*(.45f+.55f*sinf((i+1)*.314159f))});}else icon(Glyph::Play,settings.iconOnly?61.f:195.f,(size.h-15)/2,15,false,!m.play);}
        else if(model.state==State::Peek&&!airpodsNotice.empty()){icon(Glyph::AirPods,16,11,34);add(L"AirPods",64,9,14);add(L"Connected for audio",64,31,11,true);}
        else if(model.state==State::Peek){icon(Glyph::Disc,12,10,36);if(m.spotify)icon(Glyph::Spotify,37,34,12);add(m.title,60,9,13,false,197);add(m.play?m.artist:L"Your music",60,32,11,true,88);icon(Glyph::Previous,155,31,20,false,!m.previous);icon(m.playing?Glyph::Pause:Glyph::Play,193,31,20,false,!m.play);icon(Glyph::Next,231,31,20,false,!m.next);}
        else {
            int tab=model.state==State::Timer?3:model.state==State::Music||model.state==State::Calendar||model.state==State::Agenda?0:model.state==State::Controls||model.state==State::Wifi||model.state==State::Preferences?2:1;
            icon(Glyph::Chip,10+tab*64.f,8,60,true);add(L"Music",24,14,11,tab!=0,45);add(L"Tray",94,14,11,tab!=1,40);add(L"Controls",148,14,11,tab!=2,56);add(L"Clock",216,14,11,tab!=3,45);icon(Glyph::Gear,size.w-62,13,16,model.state==State::Preferences);icon(Glyph::Pin,size.w-34,13,16,model.pinned);
            if(model.state==State::Music){
                icon(Glyph::Disc,16,52,64);if(m.spotify)icon(Glyph::Spotify,67,103,16);
                add(m.title,94,54,14,false,150);add(m.artist,94,77,12,true,150);
                icon(Glyph::Previous,100,113,18,false,!m.previous);icon(m.playing?Glyph::Pause:Glyph::Play,144,112,20,false,!m.play);icon(Glyph::Next,189,113,18,false,!m.next);
                SYSTEMTIME date;GetLocalTime(&date);wchar_t day[32]{},month[32]{};GetDateFormatEx(LOCALE_NAME_USER_DEFAULT,0,&date,L"ddd",day,32,nullptr);GetDateFormatEx(LOCALE_NAME_USER_DEFAULT,0,&date,L"MMM",month,32,nullptr);
                icon(Glyph::Rule,241,57,65);add(day,288,53,11,true,40);add(std::to_wstring(date.wDay),286,71,24,false,42);add(month,289,109,11,true,40);
            }
            else if(model.state==State::Shelf||model.state==State::DraggingOut){
                if(settings.paths.empty()){icon(Glyph::Folder,163,54,30);add(L"Keep it in your tray",116,100,14);add(L"Drop files here. Originals stay put.",91,124,11,true);}
                else {for(int row=0;row<3&&shelfOffset+row<int(settings.paths.size());++row){int i=shelfOffset+row;icon(Glyph::Folder,18,55+float(row)*30,18);add(std::filesystem::path(settings.paths[i]).filename().wstring(),47,54+float(row)*30,12,false,252);icon(Glyph::Close,315,54+float(row)*30,18);}}
                add(std::to_wstring(settings.paths.size())+L" files",18,168,11,true);add(L"Screenshots",94,168,11,true,100);icon(Glyph::Plus,219,166,17);add(L"Add files",242,168,11);icon(Glyph::More,321,166,18);
            }else if(model.state==State::DragOver){icon(Glyph::Folder,160,55,32);add(L"Add to your tray",126,103,15);add(L"Release to keep a reference",111,133,11,true);}
            else if(model.state==State::Call){
                icon(Glyph::Microphone,22,56,32,mic.muted);add(mic.active?mic.app:L"No supported microphone session",70,53,16,false,250);add(mic.active?(mic.muted?L"Microphone muted":L"Microphone active"):L"Voice activity appears automatically",70,79,11,true,250);
                icon(Glyph::Chip,22,114,120);add(mic.muted?L"Unmute microphone":L"Mute microphone",31,120,11,!mic.active,115);add(L"Applies to this microphone in all apps.",22,155,10,true,299);
            }
            else if(model.state==State::Timer && (alarm.ringing||countdown.finished)){
                marks.push_back({Glyph::AlarmBell,142,79,60,true,false,60,!settings.reduceMotion&&GetTickCount64()%4000<800?sinf(GetTickCount64()*.035f)*9.f:0.f});add(alarm.ringing&&countdown.finished?L"Alarm and timer":alarm.ringing?L"Alarm ringing":L"Time is up",16,159,24,false,312,true);add(L"Stays active until you dismiss it.",16,202,11,true,312,true);marks.push_back({Glyph::Card,88,248,168,true,false,42});add(L"Dismiss",88,260,14,false,168,true);
            }else if(model.state==State::Timer){
                const wchar_t* modes[]={L"Alarm",L"Timer",L"Stopwatch"};for(int i=0;i<3;++i){if(int(timeMode)==i)icon(Glyph::Chip,16+i*104.f,45,104);add(modes[i],16+i*104.f,51,11,int(timeMode)!=i,104,true);}
                if(timeMode==TimeMode::Timer){
                    marks.push_back({Glyph::Timer,118,80,108,true,false,108,countdown.finished?0:countdown.active()?countdown.fraction(GetTickCount64()):1});
                    add(countdown.finished?L"0:00":countdown.active()?countdown.text(GetTickCount64()):std::to_wstring(timerMinutes)+L":00",112,116,23,false,120,true);
                    add(countdown.finished?L"Time is up":countdown.running?L"Counting down":countdown.active()?L"Paused":L"Quick timer",16,194,11,true,312,true);
                    const wchar_t* presets[]={L"1 min",L"5 min",L"10 min",L"25 min"};for(int i=0;i<4;++i){icon(Glyph::Chip,16+i*80.f,223,70);add(presets[i],16+i*80.f,229,11,false,70,true);}
                    add(L"-",22,270,20);add(L"+",60,270,20);icon(Glyph::Chip,98,267,108);add(countdown.finished?L"Dismiss":countdown.active()?(countdown.running?L"Pause":L"Resume"):L"Start",98,273,12,false,108,true);add(L"Reset",246,273,12,true,72,true);
                }else if(timeMode==TimeMode::Alarm){
                    add(alarm.text(),16,102,38,false,312,true);add(alarm.ringing?L"Alarm ringing":alarm.armed?L"Set for the next occurrence":L"Choose a time (24-hour)",16,161,11,true,312,true);
                    icon(Glyph::Chip,24,201,136);icon(Glyph::Chip,184,201,136);add(L"-    Hour    +",24,207,12,false,136,true);add(L"-   Minute   +",184,207,12,false,136,true);
                    icon(Glyph::Chip,88,248,168);add(alarm.ringing?L"Dismiss":alarm.armed?L"Cancel alarm":L"Set alarm",88,254,12,false,168,true);add(L"While the app is open. No wake from sleep.",16,289,10,true,312,true);
                }else{
                    add(stopwatch.preciseText(GetTickCount64()),16,112,38,false,312,true);add(stopwatch.running?L"Running":stopwatch.held?L"Paused":L"Ready when you are",16,181,11,true,312,true);
                    icon(Glyph::RoundButton,70,228,64,!stopwatch.running);icon(Glyph::RoundButton,218,228,64);add(stopwatch.running?L"Stop":stopwatch.held?L"Resume":L"Start",70,249,12,false,64,true);add(L"Reset",218,249,12,true,64,true);
                }
            }
            else if(model.state==State::Preferences){
                add(L"Make it yours",20,48,18);add(L"Floatlet",20,74,11,true);
                const wchar_t* labels[]={L"Icon-only music bar",L"Follow your pointer",L"Larger touch targets",L"Reduce motion",L"Microphone controls"};bool values[]={settings.iconOnly,settings.followPointer,settings.touch,settings.reduceMotion,settings.callControls};
                for(int i=0;i<5;++i){marks.push_back({Glyph::Card,12,99+i*33.f,332,false,false,30});add(labels[i],24,105+i*33.f,12);icon(Glyph::Toggle,294,104+i*33.f,34,values[i]);}
                add(agenda.connected?L"Google Calendar connected":L"Connect Google Calendar",24,278,12);add(L">",318,277,14);add(L"Start with Windows",24,308,12);add(L">",318,307,14);add(hotkey?L"Ctrl+Shift+F12 to open. F10 for all actions.":L"Shortcut in use. Click the island to open.",24,331,9,true);lines.push_back({L"made by draey.dev",20,354,11,true,316,true,true});
            }
            else if(model.state==State::Agenda){
                add(L"< Calendar",20,50,12,true,100);add(L"Refresh",272,50,11,true,60);
                SYSTEMTIME d{};d.wYear=WORD(calendar.year);d.wMonth=WORD(calendar.month);d.wDay=WORD(calendar.selected);wchar_t heading[100]{};GetDateFormatEx(LOCALE_NAME_USER_DEFAULT,0,&d,L"d MMMM yyyy",heading,100,nullptr);add(heading,20,77,17,false,302);
                bool matches=agenda.year==calendar.year&&agenda.month==calendar.month&&agenda.day==calendar.selected;
                auto time=[](int minute){minute=std::clamp(minute,0,1440);return (minute/60<10?L"0":L"")+std::to_wstring(minute/60)+L":"+(minute%60<10?L"0":L"")+std::to_wstring(minute%60);};
                if(matches){for(int row=0;row<4&&agendaOffset+row<int(agenda.agenda.entries.size());++row){auto& entry=agenda.agenda.entries[agendaOffset+row];marks.push_back({Glyph::Card,12,111+row*43.f,320,false,false,39});add(std::wstring(winrt::to_hstring(entry.title)),23,115+row*43.f,12,false,294);auto label=entry.allDay?std::wstring(L"All day"):time(entry.minute)+L" - "+time(entry.endMinute);if(!entry.location.empty())label+=L"  "+std::wstring(winrt::to_hstring(entry.location));add(label,23,131+row*43.f,10,true,294);}
                    if(agenda.agenda.entries.empty())add(!agenda.connected?L"Connect a calendar in Preferences":agenda.status.starts_with(L"Could")?L"Calendar unavailable":L"No events for this day",23,121,13,true,296);
                }else add(agenda.connected?L"Loading your day...":L"Connect a calendar in Preferences",23,121,13,true,296);
                add(agenda.status,20,294,10,true,305);
            }
            else if(model.state==State::Calendar){
                SYSTEMTIME d{};d.wYear=WORD(calendar.year);d.wMonth=WORD(calendar.month);d.wDay=1;wchar_t header[80]{};GetDateFormatEx(LOCALE_NAME_USER_DEFAULT,0,&d,L"MMMM yyyy",header,80,nullptr);
                add(header,20,50,17,false,230);add(L"<",270,51,18);add(L">",309,51,18);
                const wchar_t* days[]={L"M",L"T",L"W",L"T",L"F",L"S",L"S"};for(int i=0;i<7;++i)add(days[i],26+i*44.f,86,11,true,25);
                SYSTEMTIME now;GetLocalTime(&now);
                for(int cell=0;cell<42;++cell){auto day=calendar.dayAt(cell);if(!day)continue;float x=17+(cell%7)*44.f,y=108+(cell/7)*27.f;if(day==calendar.selected)icon(Glyph::Chip,x,y-3,34,true);add(std::to_wstring(day),x+8,y,12,!(day==now.wDay&&calendar.month==now.wMonth&&calendar.year==now.wYear),30);}
                add(L"Today",20,291,11,true,60);add(L"Next year",102,291,11,true,90);add(std::to_wstring(calendar.selected)+L" / "+std::to_wstring(calendar.month)+L" / "+std::to_wstring(calendar.year),221,291,11,false,109);
            }
            else if(model.state==State::Screenshots||model.state==State::Wifi){
                bool shots=model.state==State::Screenshots;add(shots?L"Recent screenshots":L"Nearby Wi-Fi networks",18,48,16);
                add(shots?L"Click to add a reference to your tray":L"Connect nearby. Scroll for more networks.",18,73,11,true);
                int count=int(shots?controls.screenshots.size():controls.wifi.size());
                for(int row=0;row<4&&listOffset+row<count;++row){int i=listOffset+row;marks.push_back({Glyph::Card,12,101+row*29.f,332,false,false,27});add(shots?std::filesystem::path(controls.screenshots[i]).filename().wstring():controls.wifi[i].name+(controls.wifi[i].connected?L"  Connected":L"  "+std::to_wstring(controls.wifi[i].signal)+L"%"),22,105+row*29.f,12,false,292);add(shots?L"+":L">",320,104+row*29.f,13);}
                if(!count){add(shots?L"No saved screenshots found":L"No nearby networks available",22,115,13);add(shots?L"Enable automatic saving in Snipping Tool.":L"Windows may require location permission.",22,141,11,true);}
                add(L"Refresh",18,237,11,true,60);add(shots?L"Add files...":L"Windows Wi-Fi...",211,237,11,true,127);
            }
            else if(model.state==State::Controls){
                auto card=[&](float x,float y,float w,float h){marks.push_back({Glyph::Card,x,y,w,false,false,h});};
                card(12,45,103,48);card(121,45,103,48);card(230,45,114,48);
                add(L"CPU",24,52,10,true);add(cpuLoad?std::to_wstring(int(std::lround(*cpuLoad)))+L"%":L"N/A",24,68,16);
                add(L"Memory",133,52,10,true);add(memory.ullTotalPhys?std::to_wstring(memory.dwMemoryLoad)+L"%":L"N/A",133,68,16);
                SYSTEM_POWER_STATUS power{};if(!GetSystemPowerStatus(&power))power.BatteryLifePercent=255;
                add(power.ACLineStatus==1?L"Plugged in":L"Battery",242,52,10,true);add(power.BatteryLifePercent==255?L"N/A":std::to_wstring(power.BatteryLifePercent)+L"%",242,68,16);
                card(12,101,161,47);card(181,101,163,47);
                add(L"Wi-Fi",25,109,12);std::wstring network=L"Nearby networks";try{auto profile=winrt::Windows::Networking::Connectivity::NetworkInformation::GetInternetConnectionProfile();if(profile&&profile.IsWlanConnectionProfile())network=profile.ProfileName().c_str();}catch(...){}add(network,25,127,10,true,133);
                add(L"Bluetooth",194,109,12);add(audio&&audio->airpods()?L"AirPods audio active":L"Devices in Windows",194,127,10,true,136);
                card(12,156,332,64);icon(Glyph::Sun,24,181,20);add(L"Built-in display",24,163,10,true,200);add(controls.brightness>=0?std::to_wstring(slider==2?int(sliderValue*100):pendingBrightness>=0?pendingBrightness:controls.brightness)+L"%":L"Windows >",261,163,10,true,75);
                marks.push_back({Glyph::Slider,56,184,272,false,controls.brightness<0,24,slider==2?sliderValue:std::max(0,pendingBrightness>=0?pendingBrightness:controls.brightness)/100.f});
                card(12,228,332,64);icon(Glyph::Speaker,24,255,20,audio&&audio->muted,!audio||!audio->available);add(audio&&audio->available?audio->name:L"No audio output",24,235,10,true,234);add(audio&&audio->available?std::to_wstring(int(audio->level*100))+L"%":L"N/A",284,235,10,true,44);
                marks.push_back({Glyph::Slider,56,256,272,false,!audio||!audio->available,24,slider==1?sliderValue:audio?audio->level:0});
                card(12,300,103,40);card(121,300,103,40);card(230,300,114,40);
                add(L"Night light",25,307,11);add(L"Windows >",25,323,9,true);add(L"Mirror",134,307,11);add(L"Choose display >",134,323,9,true);add(L"Energy saver",243,307,11);add(L"Windows >",243,323,9,true);
            }
        }
        if(!message.empty()&&(model.state==State::Shelf||model.state==State::Wifi||model.state==State::Screenshots)) {lines.erase(std::remove_if(lines.begin(),lines.end(),[&](auto& l){return l.y>size.h-58&&l.y<size.h-35;}),lines.end());add(message,20,size.h-53,10,true,size.w-40);}
        HIGHCONTRASTW high{sizeof(high)};SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(high),&high,0);
        BOOL motion=TRUE;SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&motion,0);
        try{renderer->draw(size.w,size.h,dpi,lines,animate&&motion&&!settings.reduceMotion,(high.dwFlags&HCF_HIGHCONTRASTON)!=0,marks,m.artwork);}catch(...){
            // Rebuild all D3D/D2D/composition resources once after device loss.
            try{renderer=std::make_unique<Renderer>();renderer->initialize(hwnd);syncCanvas();renderer->draw(size.w,size.h,dpi,lines,false,(high.dwFlags&HCF_HIGHCONTRASTON)!=0,marks,m.artwork);}catch(...){ShowWindow(hwnd,SW_HIDE);message=L"Graphics unavailable. Use tray Show to retry.";}
        }
    }
    void sample(){FILETIME i{},k{},u{};ULONGLONG now=GetTickCount64();if(GetSystemTimes(&i,&k,&u)){auto iv=value(i),kv=value(k),uv=value(u);if(lastSample&&now-lastSample<3000&&iv>=lastIdle&&kv>=lastKernel&&uv>=lastUser)cpuLoad=cpu(iv-lastIdle,kv-lastKernel,uv-lastUser);else cpuLoad.reset();lastIdle=iv;lastKernel=kv;lastUser=uv;lastSample=now;}GlobalMemoryStatusEx(&memory);render();}
    void add(std::vector<std::wstring> paths){unsigned count=0;for(auto& p:paths)if(addReference(settings,p))++count;message=std::to_wstring(count)+L" added; "+std::to_wstring(paths.size()-count)+L" duplicate, invalid or over limit";dirty();render();}
    void volume(int id){if(audio){bool ok=id==Mute?audio->toggle():audio->set(audio->level+(id==VolUp?.05f:-.05f));if(!ok)message=L"Audio endpoint unavailable";}render();}
    float localX(int x) const{return x*96.f/dpi-(geometry.bounds.w*96.f/dpi-size.w)/2;}
    void sliderTo(int x,bool commit){sliderValue=std::clamp((localX(x)-68)/248.f,0.f,1.f);if(slider==1&&audio)audio->set(sliderValue);if(slider==2&&system&&(commit||GetTickCount64()-lastBrightnessWrite>=80)){pendingBrightness=int(std::lround(sliderValue*100));lastBrightnessWrite=GetTickCount64();system->setBrightness(pendingBrightness);}render();}
    void dateToday(){SYSTEMTIME d;GetLocalTime(&d);calendar.year=d.wYear;calendar.month=d.wMonth;calendar.selected=d.wDay;}
    void popNotice(std::wstring text,unsigned duration=2400,bool important=false,Glyph glyph=Glyph::Camera,std::wstring detail=L"Floatlet"){if(suspended||captureFrozen||model.state==State::Hidden||model.state==State::Unavailable||(model.pinned&&!important))return;noticeDetail=std::move(detail);noticeGlyph=glyph;notice=std::move(text);noticeUntil=GetTickCount64()+duration;model.state=State::Peek;SetTimer(hwnd,11,duration,nullptr);layout();}
    void syncVisualTick(){auto m=media?media->get():MediaSnapshot{};bool wanted=!settings.reduceMotion&&!suspended&&!captureFrozen&&((model.state==State::Collapsed&&m.playing&&!stopwatch.running&&!countdown.active()&&!countdown.finished&&!alarm.ringing&&!(mic.active&&settings.callControls)&&notice.empty())||((alarm.ringing||countdown.finished)&&(model.state==State::Collapsed||model.state==State::Peek||model.state==State::Timer)&&GetTickCount64()%4000<800)||(model.state==State::Timer&&timeMode==TimeMode::Stopwatch&&stopwatch.running));if(wanted&&!visualTicking){SetTimer(hwnd,10,33,nullptr);visualTicking=true;}else if(!wanted&&visualTicking){KillTimer(hwnd,10);visualTicking=false;}}
    void syncTimeTick(){syncVisualTick();bool needed=countdown.running||countdown.finished||alarm.armed||alarm.ringing||(stopwatch.running&&(model.state==State::Collapsed||model.state==State::Peek||(model.state==State::Timer&&timeMode==TimeMode::Stopwatch)));if(needed&&!clockTicking)clockTicking=SetTimer(hwnd,8,1000,nullptr)!=0;else if(!needed&&clockTicking){KillTimer(hwnd,8);clockTicking=false;}}
    void dismissTimeAlert(){bool wasRinging=alarm.ringing||countdown.finished;chime.stop();alarm.ringing=false;if(countdown.finished)countdown.cancel();syncTimeTick();if(wasRinging)popNotice(L"Alert dismissed",1600,true,Glyph::AlarmBell);}
    void timerTick(){auto now=GetTickCount64();bool timerDone=countdown.tick(now),alarmDone=alarm.tick(std::chrono::system_clock::now());if(timerDone||alarmDone){chime.start();NOTIFYICONDATAW n{sizeof(n)};n.hWnd=hwnd;n.uID=1;n.uFlags=NIF_INFO;n.dwInfoFlags=NIIF_INFO|NIIF_NOSOUND|NIIF_RESPECT_QUIET_TIME;wcscpy_s(n.szInfoTitle,alarmDone?L"Alarm":L"Timer complete");wcscpy_s(n.szInfo,L"Open Clock and press Dismiss to stop the alert.");Shell_NotifyIconW(NIM_MODIFY,&n);popNotice(alarmDone?L"Alarm ringing":L"Timer complete",5000,true,Glyph::AlarmBell);layout();}syncTimeTick();if(model.state==State::Timer||model.state==State::Collapsed||model.state==State::Peek)render();}
    void launchSettings(const wchar_t* uri){auto result=reinterpret_cast<INT_PTR>(ShellExecuteW(hwnd,L"open",uri,nullptr,nullptr,SW_SHOWNORMAL));if(result<=32){message=L"Windows could not open this setting";render();}}
    void command(int id){message.clear();if(id>=RevealBase&&id<RevealBase+100){auto index=id-RevealBase;if(index<int(settings.paths.size())){PIDLIST_ABSOLUTE pidl=nullptr;if(SUCCEEDED(SHParseDisplayName(settings.paths[index].c_str(),nullptr,&pidl,0,nullptr))){SHOpenFolderAndSelectItems(pidl,0,nullptr,0);CoTaskMemFree(pidl);}else error(L"Original unavailable. Reference kept.");}return;}
        if(id>=RemoveBase&&id<RemoveBase+100){auto index=id-RemoveBase;if(index<int(settings.paths.size())){settings.paths.erase(settings.paths.begin()+index);shelfOffset=std::clamp(shelfOffset,0,std::max(0,int(settings.paths.size())-3));dirty();render();}return;}
        switch(id){
        case PreferencesPanel:case SettingsHelp:focus();model.panel(State::Preferences);layout();break;
        case TimerPanel:focus();model.panel(State::Timer);layout();break;
        case Timer1:case Timer5:case Timer10:case Timer25:if(alarm.ringing||countdown.finished)break;timerMinutes=id==Timer1?1:id==Timer5?5:id==Timer10?10:25;if(!countdown.active())countdown.finished=false;render();break;
        case TimerLess:timerMinutes=std::max(1u,timerMinutes-1);render();break;case TimerMore:timerMinutes=std::min(1440u,timerMinutes+1);render();break;
        case TimerStart:if(alarm.ringing||countdown.finished)break;countdown.start(timerMinutes*60,GetTickCount64());syncTimeTick();layout();break;
        case TimerPause:timerTick();if(countdown.finished)break;if(countdown.running)countdown.pause(GetTickCount64());else countdown.resume(GetTickCount64());syncTimeTick();render();break;
        case DismissAlert:dismissTimeAlert();layout();break;case TimerCancel:if(alarm.ringing||countdown.finished)break;countdown.cancel();syncTimeTick();layout();break;
        case ReduceMotion:settings.reduceMotion=!settings.reduceMotion;dirty();layout();break;
        case CallControls:settings.callControls=!settings.callControls;microphone->enable(settings.callControls);dirty();render();break;
        case CallPanel:focus();model.panel(State::Call);layout();break;
        case MicrophoneMute:if(mic.active)microphone->mute();break;
        case StartupSettings:launchSettings(L"ms-settings:startupapps");break;
        case Show:event(Event::Show);break;case Hide:unfocus();event(Event::Hide);break;case Quit:DestroyWindow(hwnd);break;
        case CalendarPanel:focus();dateToday();model.panel(State::Calendar);feed->request(calendar.year,calendar.month,calendar.selected);layout();break;
        case AgendaPanel:agendaOffset=0;model.panel(State::Agenda);feed->request(calendar.year,calendar.month,calendar.selected);layout();break;
        case CalendarRefresh:feed->request(calendar.year,calendar.month,calendar.selected,true);break;
        case CalendarConnect:focus();if(auto link=SubscriptionDialog::show(hwnd)){try{feed->connect(*link);agenda=feed->get();feed->request(calendar.year,calendar.month,calendar.selected);render();}catch(...){MessageBoxW(hwnd,L"The calendar subscription could not be saved.",L"Floatlet",MB_OK);}}break;
        case PreviousMonth:calendar.move(-1);feed->request(calendar.year,calendar.month,calendar.selected);render();break;case NextMonth:calendar.move(1);feed->request(calendar.year,calendar.month,calendar.selected);render();break;case Today:dateToday();feed->request(calendar.year,calendar.month,calendar.selected);render();break;case NextYear:calendar.move(12);feed->request(calendar.year,calendar.month,calendar.selected);render();break;
        case ScreenshotsPanel:case WifiPanel:focus();listOffset=0;model.panel(id==ScreenshotsPanel?State::Screenshots:State::Wifi);system->refresh(id==ScreenshotsPanel?2:1);if(id==WifiPanel)SetTimer(hwnd,9,2500,nullptr);layout();break;
        case IconOnly:settings.iconOnly=!settings.iconOnly;dirty();layout();break;
        case Music:case Files:case Controls:if(id==Controls){system->refresh(0);if(audio)audio->read();}focus();model.panel(id==Music?State::Music:id==Files?State::Shelf:State::Controls);layout();break;
        case Pin:model.pinned=!model.pinned;render();break;
        case Add:focus();try{add(pickFiles(hwnd));}catch(...){error(L"Could not add files");}break;
        case Touch:settings.touch=!settings.touch;dirty();layout();break;
        case FollowPointer:settings.followPointer=!settings.followPointer;dirty();followTimer();break;
        case Left:case Center:case Right:settings.alignment=id-Left;dirty();layout();break;
        case OtherMonitor:{std::vector<HMONITOR> monitors;EnumDisplayMonitors(nullptr,nullptr,[](HMONITOR m,HDC,LPRECT,LPARAM p)->BOOL{reinterpret_cast<std::vector<HMONITOR>*>(p)->push_back(m);return TRUE;},reinterpret_cast<LPARAM>(&monitors));auto it=std::find(monitors.begin(),monitors.end(),monitor);if(!monitors.empty())monitor=monitors[(it==monitors.end()?0:(it-monitors.begin()+1))%monitors.size()];layout();break;}
        case Play:media->command(0);break;case Previous:media->command(1);break;case Next:media->command(2);break;
        case VolDown:case VolUp:case Mute:volume(id);break;
        case Mirror:{wchar_t folder[MAX_PATH]{};GetSystemDirectoryW(folder,MAX_PATH);auto exe=std::filesystem::path(folder)/L"DisplaySwitch.exe";ShellExecuteW(hwnd,L"open",exe.c_str(),nullptr,nullptr,SW_SHOWNORMAL);break;}
        case NightLight:case WifiSettings:case Wifi:case Bluetooth:case Display:case Sound:case Power:{const wchar_t* uri=id==NightLight?L"ms-settings:nightlight":(id==Wifi||id==WifiSettings)?L"ms-settings:network-wifi":id==Bluetooth?L"ms-settings:bluetooth":id==Display?L"ms-settings:display":id==Sound?L"ms-settings:sound":L"ms-settings:batterysaver";ShellExecuteW(hwnd,L"open",uri,nullptr,nullptr,SW_SHOWNORMAL);break;}
        }
    }
    void menu(){focus();HMENU menu=CreatePopupMenu();auto item=[&](int id,const wchar_t* label,UINT flags=0){AppendMenuW(menu,MF_STRING|flags,id,label);};
        item(Show,L"Show");item(Hide,emergency?L"Hide (Ctrl+Alt+H)":L"Hide");item(Music,L"Music");item(Files,L"Tray");item(Controls,L"Controls");item(CallPanel,L"Microphone controls");item(TimerPanel,L"Alarm, timer and stopwatch");item(PreferencesPanel,L"Preferences");item(Pin,L"Keep expanded",model.pinned?MF_CHECKED:0);item(Add,L"Add files...");
        auto m=media->get();item(Play,m.playing?L"Pause":L"Play",m.play?0:MF_GRAYED);item(Previous,L"Previous track",m.previous?0:MF_GRAYED);item(Next,L"Next track",m.next?0:MF_GRAYED);
        item(VolDown,L"Volume down");item(Mute,L"Toggle mute");item(VolUp,L"Volume up");
        if(!settings.paths.empty()){HMENU files=CreatePopupMenu();for(std::size_t i=0;i<settings.paths.size();++i){HMENU entry=CreatePopupMenu();AppendMenuW(entry,MF_STRING,RevealBase+i,L"Reveal in Explorer");AppendMenuW(entry,MF_STRING,RemoveBase+i,L"Remove reference (keep original)");auto name=std::filesystem::path(settings.paths[i]).filename().wstring();AppendMenuW(files,MF_POPUP,reinterpret_cast<UINT_PTR>(entry),name.c_str());}AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(files),L"Tray actions");}
        item(IconOnly,L"Icon-only resting capsule",settings.iconOnly?MF_CHECKED:0);item(CalendarPanel,L"Calendar");item(ScreenshotsPanel,L"Recent saved screenshots");item(WifiPanel,L"Nearby Wi-Fi networks");item(NightLight,L"Night light settings");item(Mirror,L"Screen mirroring controls");item(Power,L"Energy saver settings");
        item(FollowPointer,L"Follow pointer between displays",settings.followPointer?MF_CHECKED:0);item(Touch,L"Larger touch capsule",settings.touch?MF_CHECKED:0);item(Left,L"Place left");item(Center,L"Place centre");item(Right,L"Place right");item(OtherMonitor,L"Move to next display");item(SettingsHelp,L"Preferences");item(Quit,L"Quit");
        POINT p;GetCursorPos(&p);menuOpen=true;int id=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY|TPM_RIGHTBUTTON,p.x,p.y,0,hwnd,nullptr);menuOpen=false;DestroyMenu(menu);if(id)command(id);if(!tracking&&!model.pinned)SetTimer(hwnd,2,220,nullptr);
    }
    void click(float x,float y){if(!notice.empty()){notice.clear();KillTimer(hwnd,11);if(alarm.ringing||countdown.finished){timeMode=alarm.ringing?TimeMode::Alarm:TimeMode::Timer;command(TimerPanel);}else {model.state=State::Collapsed;layout();}return;}x-=(geometry.bounds.w*96.f/dpi-size.w)/2;if((model.state==State::Collapsed||model.state==State::Peek)&&(alarm.ringing||countdown.finished)){timeMode=alarm.ringing?TimeMode::Alarm:TimeMode::Timer;command(TimerPanel);return;}if((model.state==State::Collapsed||model.state==State::Peek)&&mic.active&&settings.callControls){command(x>size.w-45?MicrophoneMute:CallPanel);return;}if((model.state==State::Collapsed||model.state==State::Peek)&&(alarm.ringing||countdown.finished)){timeMode=alarm.ringing?TimeMode::Alarm:TimeMode::Timer;command(TimerPanel);return;}if((model.state==State::Collapsed||model.state==State::Peek)&&(countdown.active()||countdown.finished)){if(x>size.w-45&&countdown.active())command(TimerPause);else command(TimerPanel);return;}if((model.state==State::Collapsed||model.state==State::Peek)&&stopwatch.running){if(x>size.w-45){stopwatch.toggle(GetTickCount64());layout();}else{timeMode=TimeMode::Stopwatch;command(TimerPanel);}return;}if(model.state==State::Peek&&airpodsNotice.empty()&&media->get().play){int hit=hoverTransport(x,y);if(hit>=0){auto m=media->get();if(hit==0&&m.previous)command(Previous);else if(hit==1&&m.play)command(Play);else if(hit==2&&m.next)command(Next);return;}}if(model.state==State::Collapsed||model.state==State::Peek){focus();event(Event::Open);return;}if(y<40){if(x>size.w-43)command(Pin);else if(x>size.w-73)command(PreferencesPanel);else if(x<266)command(x<74?Music:x<138?Files:x<202?Controls:TimerPanel);return;}
        if(model.state==State::Call){if(y>=110&&y<146&&x<156)command(MicrophoneMute);return;}
        if(model.state==State::Timer){
            if(alarm.ringing||countdown.finished){if(y>=248&&y<290&&x>=88&&x<256)command(DismissAlert);return;}
            if(y>=45&&y<76){timeMode=TimeMode(std::clamp(int((x-16)/104),0,2));syncTimeTick();render();return;}
            if(timeMode==TimeMode::Timer){if(y>=223&&y<256&&!countdown.active()){int i=std::clamp(int((x-16)/80),0,3);command(Timer1+i);}else if(y>=267&&y<309){if(countdown.finished){command(TimerCancel);}else if(x<48){if(!countdown.active())command(TimerLess);}else if(x<90){if(!countdown.active())command(TimerMore);}else if(x<215)command(countdown.active()?TimerPause:TimerStart);else command(TimerCancel);}}
            else if(timeMode==TimeMode::Alarm){if(y>=201&&y<234){alarm.cancel();dismissTimeAlert();if(x>=24&&x<160)alarm.hour=(alarm.hour+(x<92?23:1))%24;else if(x>=184&&x<320)alarm.minute=(alarm.minute+(x<252?59:1))%60;}else if(y>=248&&y<280&&x>=88&&x<256){if(alarm.ringing){dismissTimeAlert();}else if(alarm.armed)alarm.cancel();else try{alarm.arm(std::chrono::system_clock::now());}catch(...){message=L"Alarm time could not be set.";}}syncTimeTick();render();}
            else if(y>=228&&y<292){if(x>=70&&x<134){stopwatch.toggle(GetTickCount64());if(stopwatch.running)popNotice(L"Stopwatch running",1800,false,Glyph::AlarmBell);}else if(x>=218&&x<282)stopwatch.reset();syncTimeTick();render();}return;
        }
        if(model.state==State::Preferences){if(y>=99&&y<264){int row=int((y-99)/33);int actions[]={IconOnly,FollowPointer,Touch,ReduceMotion,CallControls};command(actions[row]);render();}else if(y>=271&&y<301)command(CalendarConnect);else if(y>=302&&y<331)command(StartupSettings);return;}
        if(model.state==State::Music){if(x>266&&y>=46){command(CalendarPanel);return;}if(y>100&&y<147){if(x>=88&&x<130)command(Previous);else if(x>=132&&x<177)command(Play);else if(x>=178&&x<220)command(Next);}return;}
        if(model.state==State::Agenda){if(y>=44&&y<70){if(x<144){model.panel(State::Calendar);layout();}else if(x>250)command(CalendarRefresh);}return;}
        if(model.state==State::Calendar){if(y>=45&&y<82&&x>=260)command(x<299?PreviousMonth:NextMonth);else if(y>=108&&y<270&&x>=17&&x<325){auto day=calendar.dayAt(int((y-108)/27)*7+int((x-17)/44));if(day){calendar.selected=day;feed->request(calendar.year,calendar.month,calendar.selected);render();}}else if(y>=282){if(x<90)command(Today);else if(x<203)command(NextYear);}return;}
        if(model.state==State::Shelf){if(y>=50&&y<140&&x>=303){int row=int((y-50)/30)+shelfOffset;command(RemoveBase+row);}else if(y>156){if(x>=207&&x<303)command(Add);else if(x>=303)menu();else if(x>=86)command(ScreenshotsPanel);}return;}
        if(model.state==State::Screenshots||model.state==State::Wifi){bool shots=model.state==State::Screenshots;if(y>=101&&y<217){int row=int((y-101)/29)+listOffset;if(shots&&row<int(controls.screenshots.size())){add({controls.screenshots[row]});}else if(!shots&&row<int(controls.wifi.size())){message=L"Connecting...";if(controls.wifi[row].profile.empty())command(WifiSettings);else{system->connect(controls.wifi[row]);SetTimer(hwnd,9,2500,nullptr);render();}}}else if(y>=231){if(x<110){message.clear();system->refresh(shots?2:1);if(!shots)SetTimer(hwnd,9,2500,nullptr);}else if(x>200)command(shots?Add:WifiSettings);}return;}
        if(model.state==State::Controls){if(y>=101&&y<148)command(x<177?WifiPanel:Bluetooth);else if(y>=156&&y<220&&controls.brightness<0)command(Display);else if(y>=251&&y<287&&x<50)command(Mute);else if(y>=300)command(x<118?NightLight:x<227?Mirror:Power);}
    }

    LRESULT handle(UINT msg,WPARAM w,LPARAM l){
        if(msg==taskbar){tray();return 0;}
        switch(msg){
        case WM_MOUSEACTIVATE:return interactive?MA_ACTIVATE:MA_NOACTIVATE;
        case WM_ERASEBKGND:return 1;
        case WM_PAINT:{PAINTSTRUCT ps;BeginPaint(hwnd,&ps);EndPaint(hwnd,&ps);return 0;}
        case WM_MOUSEMOVE:
            if(slider){auto now=GetTickCount64();if(now-lastSliderPaint>=16){lastSliderPaint=now;sliderTo(GET_X_LPARAM(l),false);}return 0;}
            if(downItem>=0&&(w&MK_LBUTTON)&&(abs(GET_X_LPARAM(l)-down.x)>GetSystemMetrics(SM_CXDRAG)||abs(GET_Y_LPARAM(l)-down.y)>GetSystemMetrics(SM_CYDRAG))){auto path=settings.paths[downItem];downItem=-1;ReleaseCapture();event(Event::DragStart);auto result=dragCopy(hwnd,path);event(Event::DragEnd);message=result==DRAGDROP_S_DROP?L"Copy drag completed. Reference kept.":L"Drag cancelled or unavailable. Reference kept.";render();return 0;}
            KillTimer(hwnd,2);if(!tracking){tracking=true;TRACKMOUSEEVENT t{sizeof(t),TME_LEAVE,hwnd,0};TrackMouseEvent(&t);SetTimer(hwnd,1,120,nullptr);}return 0;
        case WM_MOUSELEAVE:tracking=false;KillTimer(hwnd,1);SetTimer(hwnd,2,220,nullptr);return 0;
        case WM_LBUTTONDBLCLK:{float x=localX(GET_X_LPARAM(l)),y=GET_Y_LPARAM(l)*96.f/dpi;if(model.state==State::Calendar&&y>=108&&y<270&&x>=17&&x<325){auto day=calendar.dayAt(int((y-108)/27)*7+int((x-17)/44));if(day){calendar.selected=day;command(AgendaPanel);}}return 0;}
        case WM_LBUTTONDOWN:{down={GET_X_LPARAM(l),GET_Y_LPARAM(l)};float x=localX(down.x),y=down.y*96.f/dpi;if((model.state==State::Collapsed||model.state==State::Peek)&&x>size.w-45&&(mic.active||countdown.active()))return 0;if(model.state==State::Peek&&airpodsNotice.empty()&&media->get().play&&hoverTransport(x,y)>=0)return 0;focus();
            if(model.state==State::Controls&&x>=56&&x<=328){if(y>=251&&y<=286&&audio&&audio->available)slider=1;else if(y>=179&&y<=214&&controls.brightness>=0)slider=2;if(slider){SetCapture(hwnd);sliderTo(down.x,false);return 0;}}
            if(model.state==State::Shelf&&x<303&&y>=50&&y<140){int row=int((y-50)/30)+shelfOffset;if(row<int(settings.paths.size())){downItem=row;SetCapture(hwnd);}}return 0;}
        case WM_LBUTTONUP:if(slider){sliderTo(GET_X_LPARAM(l),true);slider=0;ReleaseCapture();return 0;}ReleaseCapture();if(downItem>=0){downItem=-1;return 0;}click(GET_X_LPARAM(l)*96.f/dpi,GET_Y_LPARAM(l)*96.f/dpi);return 0;
        case WM_CAPTURECHANGED:slider=0;downItem=-1;return 0;
        case WM_MOUSEWHEEL:{int delta=GET_WHEEL_DELTA_WPARAM(w)>0?-1:1;if(model.state==State::Shelf)shelfOffset=std::clamp(shelfOffset+delta,0,std::max(0,int(settings.paths.size())-3));else if(model.state==State::Screenshots||model.state==State::Wifi){auto count=model.state==State::Screenshots?controls.screenshots.size():controls.wifi.size();listOffset=std::clamp(listOffset+delta,0,std::max(0,int(count)-4));}else if(model.state==State::Agenda)agendaOffset=std::clamp(agendaOffset+delta,0,std::max(0,int(agenda.agenda.entries.size())-4));else if(model.state==State::Calendar){calendar.move(delta);feed->request(calendar.year,calendar.month,calendar.selected);}render();return 0;}
        case WM_CONTEXTMENU:menu();return 0;
        case WM_SYSKEYDOWN:if(w==VK_F10){menu();return 0;}break;
        case WM_KEYDOWN:if(w==VK_ESCAPE){unfocus();event(Event::Escape);}else if(w==VK_F10||w==VK_APPS||w==VK_TAB)menu();else if(w=='1')command(Music);else if(w=='2')command(Files);else if(w=='3')command(Controls);else if(w=='4')command(TimerPanel);else if(w==VK_RETURN||w==VK_SPACE)event(Event::Open);return 0;
        case WM_HOTKEY:if(w==2)command(model.state==State::Hidden?Show:Hide);else{focus();event(Event::Show);event(Event::Open);}return 0;
        case WM_CLIPBOARDUPDATE:if(IsClipboardFormatAvailable(CF_DIB)||IsClipboardFormatAvailable(CF_BITMAP))popNotice(L"Image copied");return 0;
        case WM_TIMER:if(w==11){KillTimer(hwnd,11);notice.clear();if(model.state==State::Peek){model.state=model.pinned?model.last:State::Collapsed;layout();}}if(w==10){syncVisualTick();if(visualTicking){float peak=audio&&!alarm.ringing&&!countdown.finished?audio->peak():0;for(int i=8;i>0;--i)levels[i]=levels[i]*.5f+levels[i-1]*.5f;float target=std::min(.78f,sqrtf(std::max(0.f,peak))*.8f);levels[0]+= (target-levels[0])*(target>levels[0]?.35f:.12f);render();}}if(w==9){KillTimer(hwnd,9);if(model.state==State::Wifi)system->refresh(3);}if(w==8)timerTick();if(w==7){KillTimer(hwnd,7);airpodsNotice.clear();if(model.state==State::Peek)event(Event::Leave);}if(w==1){KillTimer(hwnd,1);if(tracking&&!captureActive())event(Event::Hover);}if(w==2){KillTimer(hwnd,2);if(notice.empty()&&!model.pinned&&!menuOpen&&IsWindowEnabled(hwnd)&&GetCapture()!=hwnd&&!captureActive()){unfocus();event(Event::Leave);}}if(w==3){KillTimer(hwnd,3);writer->put(settings);}if(w==4)sample();if(w==5)followMonitor();if(w==6){if(!geometry.tick())KillTimer(hwnd,6);syncCanvas();}return 0;
        case WM_DPICHANGED:if(!geometry.applying&&!layoutQueued){layoutQueued=true;PostMessageW(hwnd,WM_APP+6,0,0);}return 0;
        case WM_APP+9:if(audio){auto old=audio->id;audio->bind();if(audio->id!=old&&audio->airpods()&&!captureActive()&&!suspended&&model.state==State::Collapsed){airpodsNotice=audio->name;model.state=State::Peek;layout();SetTimer(hwnd,7,4000,nullptr);}else render();}return 0;
        case WM_APP+10:if(audio)audio->read();if(model.state==State::Controls)render();return 0;
        case WM_APP+11:if(system){controls=system->get();if(pendingBrightness>=0&&(controls.brightness==pendingBrightness||FAILED(controls.brightnessError)))pendingBrightness=-1;if(model.state==State::Wifi)message=controls.status;if(model.state==State::Controls||model.state==State::Wifi||model.state==State::Screenshots)render();}return 0;
        case WM_APP+14:if(microphone){mic=microphone->get();if(model.state==State::Collapsed||model.state==State::Peek)layout();else if(model.state==State::Call)render();}return 0;
        case WM_APP+13:if(feed){agenda=feed->get();if(agenda.connected&&!agenda.status.starts_with(L"Offline")&&!agenda.status.starts_with(L"Could")&&!suspended&&!captureFrozen&&!captureActive()&&notice.empty()&&!alarm.ringing&&!countdown.finished&&model.state!=State::Hidden&&model.state!=State::Unavailable){auto now=std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());if(auto next=reminders.next(agenda.upcoming,now)){auto mins=((next->start-now).count()+59)/60;popNotice(std::wstring(winrt::to_hstring(next->title)),7000,true,Glyph::CalendarIcon,L"Starts in "+std::to_wstring(mins)+L" minute"+(mins==1?L"":L"s"));}}if(model.state==State::Agenda||model.state==State::Calendar||model.state==State::Preferences)render();}return 0;
        case WM_APP+12:DestroyWindow(hwnd);return 0;
        case WM_APP+6:layoutQueued=false;layout();return 0;
        case WM_APP+8:{bool active=captureWindow(reinterpret_cast<HWND>(l));if(active&&!captureFrozen){captureFrozen=true;KillTimer(hwnd,1);KillTimer(hwnd,2);KillTimer(hwnd,4);KillTimer(hwnd,5);KillTimer(hwnd,6);follower.reset();}else if(!active&&captureFrozen){captureFrozen=false;geometry.width.last=geometry.height.last=WindowGeometry::now();layout();}return 0;}
        case WM_DISPLAYCHANGE:monitor=MonitorFromWindow(hwnd,MONITOR_DEFAULTTOPRIMARY);layout();return 0;
        case WM_SETTINGCHANGE:layout();return 0;
        case WM_ACTIVATE:if(LOWORD(w)==WA_INACTIVE&&interactive){interactive=false;SetWindowLongPtrW(hwnd,GWL_EXSTYLE,GetWindowLongPtrW(hwnd,GWL_EXSTYLE)|WS_EX_NOACTIVATE);if(!captureActive()&&!captureWindow(reinterpret_cast<HWND>(l)))SetTimer(hwnd,2,220,nullptr);}return 0;
        case WM_WTSSESSION_CHANGE:if(w==WTS_SESSION_LOCK){beforeSuspend=model.state;suspended=true;event(Event::Suspend);}else if(w==WTS_SESSION_UNLOCK){suspended=false;model.state=beforeSuspend;layout();media->command(3);}return 0;
        case WM_POWERBROADCAST:if(w==PBT_APMSUSPEND){beforeSuspend=model.state;suspended=true;event(Event::Suspend);}else if(w==PBT_APMRESUMEAUTOMATIC){suspended=false;model.state=beforeSuspend;layout();media->command(3);}else if(w==PBT_POWERSETTINGCHANGE){auto p=reinterpret_cast<POWERBROADCAST_SETTING*>(l);if(p->PowerSetting==GUID_CONSOLE_DISPLAY_STATE&&p->DataLength==4){DWORD state=0;memcpy(&state,p->Data,4);suspended=state==0;if(suspended){KillTimer(hwnd,4);KillTimer(hwnd,5);KillTimer(hwnd,6);follower.reset();}else layout();}}else render();return TRUE;
        case WM_APP+2:if(model.state==State::Collapsed||model.state==State::Peek)layout();else render();return 0;case WM_APP+3:media->command(3);return 0;case WM_APP+4:media->command(4);return 0;
        case Tray:if(l==WM_RBUTTONUP||l==WM_CONTEXTMENU)menu();else if(l==WM_LBUTTONUP)command(model.state==State::Hidden?Show:Music);return 0;
        case WM_CLOSE:command(Hide);return 0;
        case WM_DESTROY:RemoveClipboardFormatListener(hwnd);chime.stop();microphone.reset();feed.reset();if(audio){audio->stop();audio->Release();audio=nullptr;}system.reset();captureNotifyWindow=nullptr;if(foregroundHook)UnhookWinEvent(foregroundHook);callbackWindow->store(nullptr);media->stop();try{winrt::Windows::Networking::Connectivity::NetworkInformation::NetworkStatusChanged(networkToken);}catch(...){}RevokeDragDrop(hwnd);if(drop){drop->Release();drop=nullptr;}UnregisterHotKey(hwnd,1);UnregisterHotKey(hwnd,2);WTSUnRegisterSessionNotification(hwnd);if(powerNotify)UnregisterPowerSettingNotification(powerNotify);tray(true);writer->put(settings);PostQuitMessage(0);return 0;
        }return DefWindowProcW(hwnd,msg,w,l);
    }
};
LRESULT CALLBACK proc(HWND hwnd,UINT msg,WPARAM w,LPARAM l){auto app=reinterpret_cast<App*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));if(msg==WM_NCCREATE){app=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);app->hwnd=hwnd;SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(app));}if(app)try{return app->handle(msg,w,l);}catch(...){return DefWindowProcW(hwnd,msg,w,l);}return DefWindowProcW(hwnd,msg,w,l);}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR args,int){
    bool review=args&&wcscmp(args,L"--review")==0;
    if(args&&(wcscmp(args,L"--quit")==0||wcscmp(args,L"--quit-review")==0)){if(auto h=FindWindowW(wcscmp(args,L"--quit-review")==0?L"DelightIsland.Review":L"DelightIsland.Window",nullptr))PostMessageW(h,WM_APP+12,0,0);return 0;}
    HANDLE single=CreateMutexW(nullptr,FALSE,review?L"Local\\DelightIsland.Review":L"Local\\DelightIsland.0.1");if(!single)return 1;if(GetLastError()==ERROR_ALREADY_EXISTS){CloseHandle(single);return 0;}
    HRESULT ole=OleInitialize(nullptr);if(FAILED(ole)){CloseHandle(single);return 2;}
    int result=0;
    try{
        App app;app.review=review;PWSTR local=nullptr;winrt::check_hresult(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&local));auto file=std::filesystem::path(local)/L"DelightIsland"/(review?L"review-settings.json":L"settings.json");CoTaskMemFree(local);
        try{app.settings=load(file);}catch(...){app.message=L"Settings unreadable. Existing file preserved.";file.replace_filename(L"recovered-settings.json");try{app.settings=load(file);}catch(...){app.message=L"Recovery settings also unreadable. Changes will replace recovery file.";}}
        if(review){app.settings.alignment=0;app.settings.followPointer=false;}
        app.writer=std::make_unique<Writer>(file);
        WNDCLASSW cls{};cls.hInstance=instance;cls.style=CS_DBLCLKS;cls.lpszClassName=review?L"DelightIsland.Review":L"DelightIsland.Window";cls.lpfnWndProc=proc;cls.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(101));cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&cls);
        auto hwnd=CreateWindowExW(WS_EX_TOPMOST|(review?WS_EX_APPWINDOW:WS_EX_TOOLWINDOW)|WS_EX_NOACTIVATE|WS_EX_NOREDIRECTIONBITMAP,cls.lpszClassName,review?L"Floatlet review":L"Floatlet",WS_POPUP,0,0,224,36,nullptr,nullptr,instance,&app);if(!hwnd)winrt::throw_last_error();
        app.geometry.hwnd=hwnd;app.renderer=std::make_unique<Renderer>();app.renderer->initialize(hwnd);
        app.feed=std::make_unique<CalendarFeed>(hwnd,review?file.parent_path()/L"review":file.parent_path());app.agenda=app.feed->get();app.dateToday();app.feed->request(app.calendar.year,app.calendar.month,app.calendar.selected);
        app.microphone=std::make_unique<MicrophoneMonitor>(hwnd,app.settings.callControls);
        app.audio=new Audio;app.audio->start(hwnd);app.system=std::make_unique<SystemControls>(hwnd);
        app.media=std::make_shared<Media>(hwnd);app.media->start();
        app.drop=new DropTarget;app.drop->hovering=[&](bool yes){app.event(yes?Event::DragEnter:Event::DragLeave);};app.drop->accepted=[&](auto paths){app.model.panel(State::Shelf);app.add(std::move(paths));app.layout();};app.drop->rejected=[&]{app.error(L"Unsupported drop. Use real files with copy allowed.");};winrt::check_hresult(RegisterDragDrop(hwnd,app.drop));
        if(!review){app.hotkey=RegisterHotKey(hwnd,1,MOD_CONTROL|MOD_SHIFT|MOD_NOREPEAT,VK_F12);app.emergency=RegisterHotKey(hwnd,2,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,'H');
        // Optional hotkeys may belong to another app. Keep startup silent; preferences report availability.
        }
        WTSRegisterSessionNotification(hwnd,NOTIFY_FOR_THIS_SESSION);app.powerNotify=RegisterPowerSettingNotification(hwnd,&GUID_CONSOLE_DISPLAY_STATE,DEVICE_NOTIFY_WINDOW_HANDLE);
        app.callbackWindow->store(hwnd);auto destination=app.callbackWindow;
        app.networkToken=winrt::Windows::Networking::Connectivity::NetworkInformation::NetworkStatusChanged([destination](auto&&){if(auto window=destination->load())PostMessageW(window,WM_APP+2,0,0);});
        if(!review)AddClipboardFormatListener(hwnd);captureNotifyWindow=hwnd;app.foregroundHook=SetWinEventHook(EVENT_SYSTEM_FOREGROUND,EVENT_SYSTEM_FOREGROUND,nullptr,foregroundChanged,0,0,WINEVENT_OUTOFCONTEXT);app.tray();app.layout();if(review){app.settings.followPointer=false;app.command(Controls);app.model.pinned=true;}MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}app.renderer.reset();app.media.reset();
    }catch(const winrt::hresult_error& e){MessageBoxW(nullptr,e.message().c_str(),L"Floatlet could not start",MB_OK|MB_ICONERROR);result=3;}catch(...){MessageBoxW(nullptr,L"An unexpected startup error occurred.",L"Floatlet",MB_OK|MB_ICONERROR);result=4;}
    OleUninitialize();CloseHandle(single);return result;
}

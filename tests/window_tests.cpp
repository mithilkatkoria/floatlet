#include <windows.h>
#include "platform/window_geometry.h"
#include <iostream>
#include <vector>
#include <thread>
using namespace delight;
unsigned checks=0,dpiMessages=0;
void require(bool condition){++checks;if(!condition)throw std::runtime_error("Window assertion failed");}
LRESULT CALLBACK testProc(HWND w,UINT m,WPARAM wp,LPARAM lp){if(m==WM_DPICHANGED){++dpiMessages;return 0;}return DefWindowProcW(w,m,wp,lp);}
int main(){
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if(HWND live=FindWindowW(L"DelightIsland.Window",nullptr)){
        auto style=GetWindowLongPtrW(live,GWL_EXSTYLE);RECT r{};GetWindowRect(live,&r);
        HRGN region=CreateRectRgn(0,0,0,0);GetWindowRgn(live,region);RECT box{};GetRgnBox(region,&box);DeleteObject(region);
        std::cout<<"Installed window: visible="<<IsWindowVisible(live)<<" toolWindow="<<bool(style&WS_EX_TOOLWINDOW)<<" appWindow="<<bool(style&WS_EX_APPWINDOW)<<" dpi="<<GetDpiForWindow(live)<<" bounds="<<r.left<<","<<r.top<<","<<r.right<<","<<r.bottom<<" region="<<box.left<<","<<box.top<<","<<box.right<<","<<box.bottom<<"\n";
    }
    WNDCLASSW cls{};cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"DelightIsland.GeometryTest";cls.lpfnWndProc=testProc;RegisterClassW(&cls);
    HWND w=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,cls.lpszClassName,L"Delight Island geometry test",WS_POPUP,0,0,224,36,nullptr,nullptr,cls.hInstance,nullptr);
    WindowGeometry g;g.hwnd=w;
    try{
        std::vector<HMONITOR> monitors;EnumDisplayMonitors(nullptr,nullptr,[](HMONITOR m,HDC,LPRECT,LPARAM p)->BOOL{reinterpret_cast<std::vector<HMONITOR>*>(p)->push_back(m);return TRUE;},reinterpret_cast<LPARAM>(&monitors));
        std::cout<<"Connected monitors: "<<monitors.size()<<"\n";
        for(auto m:monitors){MONITORINFOEXW info{};info.cbSize=sizeof(info);GetMonitorInfoW(m,&info);DEVMODEW mode{};mode.dmSize=sizeof(mode);EnumDisplaySettingsW(info.szDevice,ENUM_CURRENT_SETTINGS,&mode);std::wcout<<info.szDevice<<L" bounds "<<info.rcMonitor.left<<L","<<info.rcMonitor.top<<L","<<info.rcMonitor.right<<L","<<info.rcMonitor.bottom<<L" nominal Hz "<<mode.dmDisplayFrequency<<L"\n";}
        for(int cycle=0;cycle<100;++cycle){
            for(auto m:monitors){
                for(auto state:{State::Collapsed,State::Music,State::Shelf,State::Controls,State::Calendar,State::Screenshots,State::Wifi,State::Peek,State::Timer,State::Preferences,State::Agenda,State::Call}){
                    g.configure(m,sizeFor(state),1,false);
                    RECT rect{};GetWindowRect(w,&rect);
                    require(rect.right-rect.left==g.bounds.w&&rect.bottom-rect.top==g.bounds.h);
                    require(GetDpiForWindow(w)==UINT(g.dpi));
                    HRGN region=CreateRectRgn(0,0,0,0);require(GetWindowRgn(w,region)!=ERROR);RECT box{};GetRgnBox(region,&box);DeleteObject(region);
                    require(box.left>=0&&box.top==0&&box.right<=g.bounds.w+1&&box.bottom<=g.bounds.h+1);
                    require(box.bottom>=g.bounds.h-1);
                }
            }
        }
        for(int cycle=0;cycle<100;++cycle){g.configure(monitors.front(),sizeFor(cycle%2?State::Music:State::Collapsed),1,true);std::this_thread::sleep_for(std::chrono::milliseconds(2));g.tick();require(g.width.position>0&&g.height.position>0);}
        std::cout<<checks<<" real HWND assertions passed; "<<dpiMessages<<" DPI messages observed\n";
        DestroyWindow(w);return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<" check "<<checks<<"\n";DestroyWindow(w);return 1;}
}

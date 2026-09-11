#pragma once
#include <windows.h>
#include <shellscalingapi.h>
#include <chrono>
#include "ui/model.h"
#include "ui/spring.h"
namespace delight {
// Only the rounded region changes per animation frame. HWND bounds change at
// transition boundaries, never in a continuous resize loop. The actual region
// is both the visible shell mask and Windows' input boundary.
struct WindowGeometry {
    HWND hwnd=nullptr;HMONITOR monitor=nullptr;
    Rect bounds{};Size target{};float dpi=96;
    Spring width,height;bool applying=false,animating=false,initialized=false;
    int alignment=1;
    static double now(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
    void mask(){
        int w=std::max(1,int(std::lround(width.position*dpi/96))),h=std::max(1,int(std::lround(height.position*dpi/96)));
        w=std::min(w,bounds.w);h=std::min(h,bounds.h);
        int x=(bounds.w-w)/2;
        int r=std::min(h/2,int(std::lround(22*dpi/96)));
        auto region=CreateRoundRectRgn(x,0,x+w+1,h+1,2*r,2*r);
        if(!SetWindowRgn(hwnd,region,TRUE))DeleteObject(region);
    }
    void position(Rect r){
        applying=true;bounds=r;
        SetWindowPos(hwnd,HWND_TOPMOST,r.x,r.y,r.w,r.h,SWP_NOACTIVATE);
        applying=false;
    }
    void configure(HMONITOR destination,Size requested,int align,bool motion){
        double time=now();width.sample(time);height.sample(time);
        MONITORINFO info{sizeof(info)};
        if(!GetMonitorInfoW(destination,&info)){destination=MonitorFromWindow(hwnd,MONITOR_DEFAULTTOPRIMARY);GetMonitorInfoW(destination,&info);}
        UINT dx=96,dy=96;GetDpiForMonitor(destination,MDT_EFFECTIVE_DPI,&dx,&dy);
        bool changedDisplay=!initialized||destination!=monitor||dpi!=float(dx);
        monitor=destination;dpi=float(dx);alignment=align;
        Rect work{info.rcWork.left,info.rcWork.top,info.rcWork.right-info.rcWork.left,info.rcWork.bottom-info.rcWork.top};
        auto endpoint=place(work,requested,dpi/96,align);
        target={endpoint.w*96.f/dpi,endpoint.h*96.f/dpi};
        if(changedDisplay||!motion){width.snap(target.w,time);height.snap(target.h,time);animating=false;position(endpoint);}
        else {
            width.retarget(target.w,time);height.retarget(target.h,time);
            animating=!width.settled()||!height.settled();
            if(animating){
                Size envelope{std::max({target.w,float(width.position),bounds.w*96.f/dpi}),std::max({target.h,float(height.position),bounds.h*96.f/dpi})};
                position(place(work,envelope,dpi/96,align));
            }else position(endpoint);
        }
        initialized=true;mask();
    }
    bool tick(){
        if(!animating)return false;
        double time=now();width.sample(time);height.sample(time);mask();
        if(width.settled()&&height.settled()){
            width.snap(target.w,time);height.snap(target.h,time);animating=false;
            MONITORINFO info{sizeof(info)};if(GetMonitorInfoW(monitor,&info))position(place({info.rcWork.left,info.rcWork.top,info.rcWork.right-info.rcWork.left,info.rcWork.bottom-info.rcWork.top},target,dpi/96,alignment));
            mask();
        }
        return animating;
    }
};
}

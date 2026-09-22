#pragma once
#include <windows.h>
#include <shellscalingapi.h>
#include <chrono>
#include "ui/model.h"
#include "ui/spring.h"
namespace delight {
// Animate the visible centre and rounded input region together.
// The canvas envelope stays stable until the transition settles.
struct WindowGeometry {
    HWND hwnd=nullptr;HMONITOR monitor=nullptr;
    Rect bounds{};Size target{};float dpi=96;
    Spring width,height,left,top;bool applying=false,animating=false,initialized=false;
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
        double time=now();width.sample(time);height.sample(time);left.sample(time);top.sample(time);
        MONITORINFO info{sizeof(info)};
        if(!GetMonitorInfoW(destination,&info)){destination=MonitorFromWindow(hwnd,MONITOR_DEFAULTTOPRIMARY);GetMonitorInfoW(destination,&info);}
        UINT dx=96,dy=96;GetDpiForMonitor(destination,MDT_EFFECTIVE_DPI,&dx,&dy);
        bool changedDisplay=!initialized||destination!=monitor||dpi!=float(dx);
        monitor=destination;dpi=float(dx);alignment=align;
        Rect work{info.rcWork.left,info.rcWork.top,info.rcWork.right-info.rcWork.left,info.rcWork.bottom-info.rcWork.top};
        auto endpoint=place(work,requested,dpi/96,align);
        target={endpoint.w*96.f/dpi,endpoint.h*96.f/dpi};
        if(changedDisplay||!motion){width.snap(target.w,time);height.snap(target.h,time);animating=false;left.snap(endpoint.x+endpoint.w/2.0,time);top.snap(endpoint.y,time);position(endpoint);}
        else {
            left.retarget(endpoint.x+endpoint.w/2.0,time);top.retarget(endpoint.y,time);width.retarget(target.w,time);height.retarget(target.h,time);
            animating=!width.settled()||!height.settled()||!left.settled()||!top.settled();
            if(animating){
                Size envelope{std::max({target.w,float(width.position),bounds.w*96.f/dpi}),std::max({target.h,float(height.position),bounds.h*96.f/dpi})};
                auto frame=place(work,envelope,dpi/96,align);frame.x=int(std::lround(left.position-frame.w/2.0));frame.y=int(top.position);position(frame);
            }else position(endpoint);
        }
        initialized=true;mask();
    }
    bool tick(){
        if(!animating)return false;
        double time=now();width.sample(time);height.sample(time);left.sample(time);top.sample(time);auto frame=bounds;frame.x=int(std::lround(left.position-frame.w/2.0));frame.y=int(std::lround(top.position));position(frame);mask();
        if(width.settled()&&height.settled()&&left.settled()&&top.settled()){
            width.snap(target.w,time);height.snap(target.h,time);animating=false;
            MONITORINFO info{sizeof(info)};if(GetMonitorInfoW(monitor,&info))position(place({info.rcWork.left,info.rcWork.top,info.rcWork.right-info.rcWork.left,info.rcWork.bottom-info.rcWork.top},target,dpi/96,alignment));
            mask();
        }
        return animating;
    }
};
}

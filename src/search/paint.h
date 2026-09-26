#pragma once
#include <d2d1.h>
#include <dwrite.h>
#include <winrt/base.h>
#include <map>
namespace delight::search {
// Native, event-driven DirectWrite rendering. No web view or animation loop.
struct Paint {
    winrt::com_ptr<ID2D1Factory> factory;winrt::com_ptr<IDWriteFactory> write;
    winrt::com_ptr<ID2D1DCRenderTarget> target;winrt::com_ptr<ID2D1SolidColorBrush> brush;
    std::map<int,winrt::com_ptr<IDWriteTextFormat>> formats;
    static D2D1_COLOR_F color(COLORREF c){return D2D1::ColorF(GetRValue(c)/255.f,GetGValue(c)/255.f,GetBValue(c)/255.f);}
    void begin(HDC dc,const RECT& bounds){
        if(!factory){winrt::check_hresult(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,factory.put()));winrt::check_hresult(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(write.put())));}
        if(!target){auto properties=D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_IGNORE),96,96);winrt::check_hresult(factory->CreateDCRenderTarget(&properties,target.put()));winrt::check_hresult(target->CreateSolidColorBrush(color(RGB(255,255,255)),brush.put()));}
        winrt::check_hresult(target->BindDC(dc,&bounds));target->BeginDraw();target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    }
    void clear(COLORREF c){target->Clear(color(c));}
    void round(float x,float y,float w,float h,float radius,COLORREF c){brush->SetColor(color(c));target->FillRoundedRectangle(D2D1::RoundedRect({x,y,x+w,y+h},radius,radius),brush.get());}
    void outline(float x,float y,float w,float h,float radius,COLORREF c,float width=1){brush->SetColor(color(c));target->DrawRoundedRectangle(D2D1::RoundedRect({x,y,x+w,y+h},radius,radius),brush.get(),width);}
    void text(const std::wstring& text,float x,float y,float w,float h,float size,COLORREF c,bool strong=false,bool mono=false){
        int key=int(size*10)*4+int(strong)*2+int(mono);auto& format=formats[key];if(!format){winrt::check_hresult(write->CreateTextFormat(mono?L"Cascadia Code":L"Segoe UI Variable",nullptr,strong?DWRITE_FONT_WEIGHT_SEMI_BOLD:DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"en-GB",format.put()));format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);DWRITE_TRIMMING trim{DWRITE_TRIMMING_GRANULARITY_CHARACTER};winrt::com_ptr<IDWriteInlineObject> ellipsis;write->CreateEllipsisTrimmingSign(format.get(),ellipsis.put());format->SetTrimming(&trim,ellipsis.get());}
        brush->SetColor(color(c));target->DrawTextW(text.c_str(),UINT32(text.size()),format.get(),{x,y,x+w,y+h},brush.get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
    void end(){auto hr=target->EndDraw();if(hr==D2DERR_RECREATE_TARGET){brush=nullptr;target=nullptr;}else winrt::check_hresult(hr);}
};
}

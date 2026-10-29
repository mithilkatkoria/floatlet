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
    winrt::com_ptr<IDWriteFontCollection> interfaceFonts;
    void typography(IDWriteFontCollection* fonts){interfaceFonts.copy_from(fonts);formats.clear();}
    static D2D1_COLOR_F color(COLORREF c){return D2D1::ColorF(GetRValue(c)/255.f,GetGValue(c)/255.f,GetBValue(c)/255.f);}
    void begin(HDC dc,const RECT& bounds){
        if(!factory){winrt::check_hresult(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,factory.put()));winrt::check_hresult(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(write.put())));}
        if(!target){auto properties=D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_IGNORE),96,96);winrt::check_hresult(factory->CreateDCRenderTarget(&properties,target.put()));winrt::check_hresult(target->CreateSolidColorBrush(color(RGB(255,255,255)),brush.put()));}
        winrt::check_hresult(target->BindDC(dc,&bounds));target->BeginDraw();target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    }
    void clear(COLORREF c){target->Clear(color(c));}
    void round(float x,float y,float w,float h,float radius,COLORREF c){brush->SetColor(color(c));target->FillRoundedRectangle(D2D1::RoundedRect({x,y,x+w,y+h},radius,radius),brush.get());}
    void gradient(float x,float y,float w,float h,float radius,COLORREF first,COLORREF last){
        D2D1_GRADIENT_STOP stops[2]{{0,color(first)},{1,color(last)}};
        winrt::com_ptr<ID2D1GradientStopCollection> collection;
        winrt::check_hresult(target->CreateGradientStopCollection(stops,2,collection.put()));
        winrt::com_ptr<ID2D1LinearGradientBrush> fill;
        auto properties=D2D1::LinearGradientBrushProperties({x,y},{x+w,y+h});
        winrt::check_hresult(target->CreateLinearGradientBrush(properties,collection.get(),fill.put()));
        target->FillRoundedRectangle(D2D1::RoundedRect({x,y,x+w,y+h},radius,radius),fill.get());
    }
    void ellipse(float x,float y,float rx,float ry,COLORREF c){brush->SetColor(color(c));target->FillEllipse(D2D1::Ellipse({x,y},rx,ry),brush.get());}
    void line(float x1,float y1,float x2,float y2,COLORREF c,float width=1){brush->SetColor(color(c));target->DrawLine({x1,y1},{x2,y2},brush.get(),width);}
    void outline(float x,float y,float w,float h,float radius,COLORREF c,float width=1){brush->SetColor(color(c));target->DrawRoundedRectangle(D2D1::RoundedRect({x,y,x+w,y+h},radius,radius),brush.get(),width);}
    // Small outline symbols remain crisp at every monitor scale without font fallback.
    void searchIcon(unsigned kind,float x,float y,float size,COLORREF c){
        const float u=size/24,stroke=1.6f*u;
        auto segment=[&](float a,float b,float d,float e){line(x+a*u,y+b*u,x+d*u,y+e*u,c,stroke);};
        auto ring=[&](float a,float b,float radius){brush->SetColor(color(c));target->DrawEllipse(D2D1::Ellipse({x+a*u,y+b*u},radius*u,radius*u),brush.get(),stroke);};
        auto box=[&](float a,float b,float w,float h,float radius=2){outline(x+a*u,y+b*u,w*u,h*u,radius*u,c,stroke);};
        switch(kind){
        case 0:ring(10,10,7);segment(15,15,21,21);break;
        case 1:ring(12,12,9);segment(12,5,12,12);segment(12,12,17,15);break;
        case 2:box(3,5,18,16);segment(3,10,21,10);segment(8,2,8,7);segment(16,2,16,7);for(int row=0;row<2;++row)for(int col=0;col<3;++col)ellipse(x+(7+col*5)*u,y+(14+row*4)*u,.8f*u,.8f*u,c);break;
        case 3:box(3,5,18,15);segment(3,13,8,13);segment(8,13,9,16);segment(9,16,15,16);segment(15,16,16,13);segment(16,13,21,13);break;
        case 4:segment(9,17,9,5);segment(9,5,20,3);segment(20,3,20,15);ellipse(x+6*u,y+18*u,3*u,2.3f*u,c);ellipse(x+17*u,y+16*u,3*u,2.3f*u,c);break;
        case 5:ring(12,12,6.5f);ring(12,12,2.3f);segment(12,1,12,5);segment(12,19,12,23);segment(1,12,5,12);segment(19,12,23,12);segment(4,4,7,7);segment(17,17,20,20);segment(4,20,7,17);segment(17,7,20,4);break;
        case 6:segment(3,5,21,5);segment(3,12,21,12);segment(3,19,21,19);ring(9,5,2);ring(16,12,2);ring(8,19,2);break;
        case 7:box(5,2,14,20);segment(8,9,16,9);segment(8,13,16,13);segment(8,17,13,17);break;
        case 9:box(2,7,20,14);segment(2,7,2,4);segment(2,4,9,4);segment(9,4,12,7);break;
        default:box(3,3,7,7);box(14,3,7,7);box(3,14,7,7);box(14,14,7,7);break;
        }
    }
    void text(const std::wstring& text,float x,float y,float w,float h,float size,COLORREF c,bool strong=false,bool mono=false,bool editorial=false){
        int key=int(size*10)*8+int(strong)*4+int(mono)*2+int(editorial);auto& format=formats[key];if(!format){bool custom=interfaceFonts&&!mono;const wchar_t* family=mono?L"Cascadia Code":custom?(editorial?L"EB Garamond":L"Figtree"):L"Segoe UI Variable";winrt::check_hresult(write->CreateTextFormat(family,custom?interfaceFonts.get():nullptr,strong?DWRITE_FONT_WEIGHT_SEMI_BOLD:DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"en-GB",format.put()));format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);DWRITE_TRIMMING trim{DWRITE_TRIMMING_GRANULARITY_CHARACTER};winrt::com_ptr<IDWriteInlineObject> ellipsis;write->CreateEllipsisTrimmingSign(format.get(),ellipsis.put());format->SetTrimming(&trim,ellipsis.get());}
        brush->SetColor(color(c));target->DrawTextW(text.c_str(),UINT32(text.size()),format.get(),{x,y,x+w,y+h},brush.get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
    void end(){auto hr=target->EndDraw();if(hr==D2DERR_RECREATE_TARGET){brush=nullptr;target=nullptr;}else winrt::check_hresult(hr);}
};
}

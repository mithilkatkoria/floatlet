#include "renderer.h"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dispatcherqueue.h>
#include <windows.ui.composition.interop.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.h>
using namespace winrt;
using namespace Windows::UI::Composition;
namespace delight {
void Renderer::initialize(HWND hwnd) {
    if(!Windows::System::DispatcherQueue::GetForCurrentThread()) {
        DispatcherQueueOptions options{sizeof(options),DQTYPE_THREAD_CURRENT,DQTAT_COM_STA};
        check_hresult(CreateDispatcherQueueController(options,reinterpret_cast<ABI::Windows::System::IDispatcherQueueController**>(put_abi(queue))));
    }
    compositor=Compositor();
    auto interop=compositor.as<ABI::Windows::UI::Composition::Desktop::ICompositorDesktopInterop>();
    check_hresult(interop->CreateDesktopWindowTarget(hwnd,FALSE,reinterpret_cast<ABI::Windows::UI::Composition::Desktop::IDesktopWindowTarget**>(put_abi(target))));
    root=compositor.CreateContainerVisual(); target.Root(root);shell=compositor.CreateRoundedRectangleGeometry();root.Clip(compositor.CreateGeometricClip(shell));
    background=compositor.CreateSpriteVisual();background.Brush(compositor.CreateColorBrush(Windows::UI::Color{255,9,10,12}));root.Children().InsertAtBottom(background);
    content=compositor.CreateSpriteVisual(); root.Children().InsertAtTop(content);
    com_ptr<ID3D11Device> d3d;
    auto hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,d3d.put(),nullptr,nullptr);
    if(FAILED(hr)) check_hresult(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,d3d.put(),nullptr,nullptr));
    com_ptr<ID2D1Factory1> factory;
    check_hresult(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,__uuidof(ID2D1Factory1),nullptr,factory.put_void()));
    check_hresult(factory->CreateDevice(d3d.as<IDXGIDevice>().get(),device.put()));
    check_hresult(compositor.as<ABI::Windows::UI::Composition::ICompositorInterop>()->CreateGraphicsDevice(device.get(),reinterpret_cast<ABI::Windows::UI::Composition::ICompositionGraphicsDevice**>(put_abi(graphics))));
    check_hresult(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(write.put())));
}
void Renderer::roundShell(float width,float height,float scale){float w=std::max(1.f,width*scale-2),h=std::max(1.f,height*scale-2);shell.Offset({(canvasW-w)/2,1});shell.Size({w,h});float radius=std::min(h/2,22*scale);shell.CornerRadius({radius,radius});}
void Renderer::canvas(float w,float h){canvasW=w;canvasH=h;background.Size({w,h});content.Offset({(w-paintedW)/2,0,0});}
void Renderer::draw(float w,float h,float dpi,const std::vector<Line>& lines,bool animate,bool highContrast,const std::vector<Mark>& marks,std::shared_ptr<const Artwork> artwork) {
    float scale=dpi/96.f;
    bool fresh=!surface||surfaceW!=w*scale||surfaceH!=h*scale;
    if(fresh){surfaceW=w*scale;surfaceH=h*scale;surface=graphics.CreateDrawingSurface({surfaceW,surfaceH},Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,Windows::Graphics::DirectX::DirectXAlphaMode::Premultiplied);}
    com_ptr<ID2D1DeviceContext> dc; POINT offset{};
    auto interop=surface.as<ABI::Windows::UI::Composition::ICompositionDrawingSurfaceInterop>();
    check_hresult(interop->BeginDraw(nullptr,__uuidof(ID2D1DeviceContext),dc.put_void(),&offset));
    dc->SetDpi(dpi,dpi);
    dc->SetTransform(D2D1::Matrix3x2F::Translation(offset.x/scale,offset.y/scale));
    auto bg=highContrast?GetSysColor(COLOR_WINDOW):RGB(9,10,12);
    auto fg=highContrast?GetSysColor(COLOR_WINDOWTEXT):RGB(244,244,242);
    auto color=[](COLORREF c) {return D2D1::ColorF(GetRValue(c)/255.f,GetGValue(c)/255.f,GetBValue(c)/255.f);};
    dc->Clear(D2D1::ColorF(0,0,0,0));
    if(backgroundColor!=bg){background.Brush(compositor.CreateColorBrush(Windows::UI::Color{255,GetRValue(bg),GetGValue(bg),GetBValue(bg)}));backgroundColor=bg;}
    com_ptr<ID2D1SolidColorBrush> brush; dc->CreateSolidColorBrush(color(fg),brush.put());
    dc->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    // Original vector geometry, rasterized only when content changes.
    for(auto& mark:marks){
        float x=mark.x,y=mark.y,s=mark.size;
        brush->SetColor(color(fg));brush->SetOpacity(mark.disabled?.22f:mark.active?1.f:.72f);
        auto stroke=[&](float ax,float ay,float bx,float by){dc->DrawLine({x+ax*s,y+ay*s},{x+bx*s,y+by*s},brush.get(),1.6f);};
        auto ellipse=[&](float cx,float cy,float r){dc->DrawEllipse(D2D1::Ellipse({x+cx*s,y+cy*s},r*s,r*s),brush.get(),1.4f);};
        auto triangle=[&](bool left,float offset){
            winrt::com_ptr<ID2D1Factory> f;dc->GetFactory(f.put());winrt::com_ptr<ID2D1PathGeometry> g;f->CreatePathGeometry(g.put());winrt::com_ptr<ID2D1GeometrySink> sink;g->Open(sink.put());
            sink->BeginFigure({x+(offset+(left?.70f:.28f))*s,y+.20f*s},D2D1_FIGURE_BEGIN_FILLED);
            sink->AddLine({x+(offset+(left?.28f:.70f))*s,y+.50f*s});sink->AddLine({x+(offset+(left?.70f:.28f))*s,y+.80f*s});sink->EndFigure(D2D1_FIGURE_END_CLOSED);sink->Close();dc->FillGeometry(g.get(),brush.get());
        };
        switch(mark.glyph){
        case Glyph::Headphones:{
            winrt::com_ptr<ID2D1Factory> factory;dc->GetFactory(factory.put());winrt::com_ptr<ID2D1PathGeometry> path;check_hresult(factory->CreatePathGeometry(path.put()));winrt::com_ptr<ID2D1GeometrySink> sink;check_hresult(path->Open(sink.put()));sink->BeginFigure({x+s*.18f,y+s*.68f},D2D1_FIGURE_BEGIN_HOLLOW);sink->AddBezier(D2D1::BezierSegment({x+s*.05f,y-s*.03f},{x+s*.95f,y-s*.03f},{x+s*.82f,y+s*.68f}));sink->EndFigure(D2D1_FIGURE_END_OPEN);check_hresult(sink->Close());dc->DrawGeometry(path.get(),brush.get(),1.7f);dc->DrawRoundedRectangle(D2D1::RoundedRect({x+s*.12f,y+s*.48f,x+s*.31f,y+s*.84f},s*.07f,s*.07f),brush.get(),1.7f);dc->DrawRoundedRectangle(D2D1::RoundedRect({x+s*.69f,y+s*.48f,x+s*.88f,y+s*.84f},s*.07f,s*.07f),brush.get(),1.7f);break;}
        case Glyph::Microphone:dc->DrawRoundedRectangle(D2D1::RoundedRect({x+s*.34f,y+s*.1f,x+s*.66f,y+s*.61f},s*.16f,s*.16f),brush.get(),1.7f);stroke(.22f,.43f,.22f,.60f);stroke(.22f,.60f,.35f,.75f);stroke(.35f,.75f,.65f,.75f);stroke(.65f,.75f,.78f,.60f);stroke(.78f,.60f,.78f,.43f);stroke(.5f,.75f,.5f,.91f);stroke(.33f,.91f,.67f,.91f);if(mark.active)stroke(.08f,.12f,.92f,.9f);break;
        case Glyph::Gear:ellipse(.5f,.5f,.26f);ellipse(.5f,.5f,.09f);for(int i=0;i<8;++i){float a=i*3.14159265f/4;stroke(.5f+.28f*cosf(a),.5f+.28f*sinf(a),.5f+.43f*cosf(a),.5f+.43f*sinf(a));}break;
        case Glyph::Toggle:brush->SetOpacity(mark.active?1.f:.16f);if(mark.active&&!highContrast)brush->SetColor(D2D1::ColorF(.35f,.62f,.98f));dc->FillRoundedRectangle(D2D1::RoundedRect({x,y,x+s,y+20},10,10),brush.get());brush->SetColor(color(fg));brush->SetOpacity(1);dc->FillEllipse(D2D1::Ellipse({x+(mark.active?s-10:10),y+10},7,7),brush.get());break;
        case Glyph::Timer:{brush->SetOpacity(.16f);ellipse(.5f,.5f,.42f);brush->SetOpacity(1);if(!highContrast)brush->SetColor(D2D1::ColorF(1.f,.64f,.28f));float fraction=std::clamp(mark.value,0.f,1.f);
            if(fraction>=.9999f)dc->DrawEllipse(D2D1::Ellipse({x+s*.5f,y+s*.5f},s*.42f,s*.42f),brush.get(),std::max(2.f,s*.045f));
            else if(fraction>0){winrt::com_ptr<ID2D1Factory> f;dc->GetFactory(f.put());winrt::com_ptr<ID2D1PathGeometry> path;check_hresult(f->CreatePathGeometry(path.put()));winrt::com_ptr<ID2D1GeometrySink> sink;check_hresult(path->Open(sink.put()));sink->BeginFigure({x+s*.5f,y+s*.08f},D2D1_FIGURE_BEGIN_HOLLOW);float a=-1.5707963f+6.2831853f*fraction;sink->AddArc(D2D1::ArcSegment({x+s*(.5f+.42f*cosf(a)),y+s*(.5f+.42f*sinf(a))},{s*.42f,s*.42f},0,D2D1_SWEEP_DIRECTION_CLOCKWISE,fraction>.5f?D2D1_ARC_SIZE_LARGE:D2D1_ARC_SIZE_SMALL));sink->EndFigure(D2D1_FIGURE_END_OPEN);check_hresult(sink->Close());dc->DrawGeometry(path.get(),brush.get(),std::max(2.f,s*.045f));}break;}
        case Glyph::AlarmBell:{D2D1_MATRIX_3X2_F before;dc->GetTransform(&before);dc->SetTransform(D2D1::Matrix3x2F::Rotation(mark.value,{x+s*.5f,y+s*.55f})*before);if(!highContrast)brush->SetColor(D2D1::ColorF(1.f,.68f,.32f));ellipse(.5f,.55f,.32f);stroke(.5f,.55f,.5f,.34f);stroke(.5f,.55f,.66f,.65f);stroke(.22f,.12f,.07f,.27f);stroke(.78f,.12f,.93f,.27f);stroke(.3f,.84f,.21f,.95f);stroke(.7f,.84f,.79f,.95f);dc->SetTransform(before);break;}
        case Glyph::Camera:dc->DrawRoundedRectangle(D2D1::RoundedRect({x+s*.08f,y+s*.25f,x+s*.92f,y+s*.82f},s*.09f,s*.09f),brush.get(),1.6f);ellipse(.5f,.53f,.17f);stroke(.3f,.25f,.38f,.13f);stroke(.38f,.13f,.62f,.13f);stroke(.62f,.13f,.7f,.25f);break;
        case Glyph::CalendarIcon:dc->DrawRoundedRectangle(D2D1::RoundedRect({x+s*.12f,y+s*.2f,x+s*.88f,y+s*.88f},s*.08f,s*.08f),brush.get(),1.6f);stroke(.12f,.4f,.88f,.4f);stroke(.32f,.1f,.32f,.3f);stroke(.68f,.1f,.68f,.3f);stroke(.3f,.59f,.45f,.59f);stroke(.55f,.72f,.7f,.72f);break;
        case Glyph::Wavebar:{if(!highContrast)brush->SetColor(D2D1::ColorF(.53f,.48f,.72f));float barHeight=std::max(2.f,std::min(12.f,mark.height)*std::clamp(mark.value,0.f,1.f));dc->FillRoundedRectangle(D2D1::RoundedRect({x,y+(mark.height-barHeight)/2,x+s,y+(mark.height+barHeight)/2},s/2,s/2),brush.get());break;}
        case Glyph::RoundButton:{if(!highContrast)brush->SetColor(mark.active?D2D1::ColorF(.16f,.46f,.29f):D2D1::ColorF(.22f,.23f,.24f));brush->SetOpacity(.6f);dc->FillEllipse(D2D1::Ellipse({x+s/2,y+s/2},s/2,s/2),brush.get());brush->SetOpacity(.35f);dc->DrawEllipse(D2D1::Ellipse({x+s/2,y+s/2},s/2-3,s/2-3),brush.get(),1);break;}
        case Glyph::Card:brush->SetOpacity(highContrast?.18f:.055f);dc->FillRoundedRectangle(D2D1::RoundedRect({x,y,x+s,y+mark.height},12,12),brush.get());break;
        case Glyph::Slider:{brush->SetOpacity(.10f);dc->FillRoundedRectangle(D2D1::RoundedRect({x,y,x+s,y+24},12,12),brush.get());float width=12+(s-24)*std::clamp(mark.value,0.f,1.f);brush->SetOpacity(mark.disabled?.1f:.75f);dc->FillRoundedRectangle(D2D1::RoundedRect({x,y,x+width+12,y+24},12,12),brush.get());brush->SetOpacity(mark.disabled?.3f:1);dc->FillEllipse(D2D1::Ellipse({x+width,y+12},10,10),brush.get());break;}
        case Glyph::Close:stroke(.25f,.25f,.75f,.75f);stroke(.75f,.25f,.25f,.75f);break;
        case Glyph::Speaker:stroke(.15f,.4f,.35f,.4f);stroke(.35f,.4f,.6f,.2f);stroke(.6f,.2f,.6f,.8f);stroke(.6f,.8f,.35f,.6f);stroke(.35f,.6f,.15f,.6f);stroke(.15f,.6f,.15f,.4f);if(mark.active){stroke(.72f,.3f,.95f,.7f);stroke(.95f,.3f,.72f,.7f);}else{stroke(.78f,.3f,.85f,.5f);stroke(.85f,.5f,.78f,.7f);}break;
        case Glyph::Sun:ellipse(.5f,.5f,.18f);for(int a=0;a<8;++a){float angle=a*3.14159265f/4;stroke(.5f+.31f*cosf(angle),.5f+.31f*sinf(angle),.5f+.43f*cosf(angle),.5f+.43f*sinf(angle));}break;
        case Glyph::AirPods:for(float a:{.18f,.65f}){dc->FillEllipse(D2D1::Ellipse({x+s*(a+.08f),y+s*.3f},s*.15f,s*.17f),brush.get());dc->FillRoundedRectangle(D2D1::RoundedRect({x+s*a,y+s*.28f,x+s*(a+.12f),y+s*.86f},s*.05f,s*.05f),brush.get());}break;
        case Glyph::Logo:dc->FillRoundedRectangle(D2D1::RoundedRect({x+s*.08f,y+s*.28f,x+s*.92f,y+s*.72f},s*.22f,s*.22f),brush.get());brush->SetColor(color(bg));dc->FillEllipse(D2D1::Ellipse({x+s*.7f,y+s*.5f},s*.10f,s*.10f),brush.get());break;
        case Glyph::Chip:brush->SetOpacity(highContrast?.2f:.08f);dc->FillRoundedRectangle(D2D1::RoundedRect({x,y,x+s,y+28},14,14),brush.get());break;
        case Glyph::Disc:
            if(artwork&&artwork->pixels.size()==artwork->width*artwork->height*4){
                winrt::com_ptr<ID2D1Bitmap> bitmap;
                auto properties=D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED));
                check_hresult(dc->CreateBitmap({artwork->width,artwork->height},artwork->pixels.data(),artwork->width*4,properties,bitmap.put()));
                winrt::com_ptr<ID2D1Factory> f;dc->GetFactory(f.put());winrt::com_ptr<ID2D1RoundedRectangleGeometry> clip;
                check_hresult(f->CreateRoundedRectangleGeometry(D2D1::RoundedRect({x,y,x+s,y+s},s*.17f,s*.17f),clip.put()));
                dc->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(),clip.get()),nullptr);dc->DrawBitmap(bitmap.get(),{x,y,x+s,y+s},1,D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);dc->PopLayer();break;
            }
            brush->SetOpacity(.065f);dc->FillRoundedRectangle(D2D1::RoundedRect({x,y,x+s,y+s},16,16),brush.get());
            brush->SetOpacity(.12f);ellipse(.5f,.5f,.34f);ellipse(.5f,.5f,.25f);brush->SetOpacity(.75f);ellipse(.5f,.5f,.07f);break;
        case Glyph::AppleMusic:brush->SetOpacity(1);brush->SetColor(highContrast?color(fg):D2D1::ColorF(.98f,.18f,.34f));dc->FillRoundedRectangle(D2D1::RoundedRect({x,y,x+s,y+s},s*.23f,s*.23f),brush.get());brush->SetColor(highContrast?color(bg):D2D1::ColorF(1,1,1));x+=s*.2f;y+=s*.17f;s*=.63f;[[fallthrough]];
        case Glyph::Music:stroke(.42f,.75f,.42f,.18f);stroke(.42f,.18f,.8f,.1f);stroke(.8f,.1f,.8f,.64f);ellipse(.28f,.78f,.13f);ellipse(.66f,.68f,.13f);break;
        case Glyph::Play:case Glyph::Pause:
            if(mark.active){dc->FillEllipse(D2D1::Ellipse({x+s/2,y+s/2},s*.85f,s*.85f),brush.get());brush->SetColor(color(bg));}
            if(mark.glyph==Glyph::Play)triangle(false,.02f);else{dc->FillRoundedRectangle(D2D1::RoundedRect({x+s*.25f,y+s*.19f,x+s*.41f,y+s*.81f},1,1),brush.get());dc->FillRoundedRectangle(D2D1::RoundedRect({x+s*.59f,y+s*.19f,x+s*.75f,y+s*.81f},1,1),brush.get());}break;
        case Glyph::Previous:triangle(true,0);stroke(.2f,.2f,.2f,.8f);break;
        case Glyph::Next:triangle(false,0);stroke(.8f,.2f,.8f,.8f);break;
        case Glyph::Folder:stroke(.12f,.28f,.12f,.82f);stroke(.12f,.82f,.88f,.82f);stroke(.88f,.82f,.88f,.32f);stroke(.88f,.32f,.48f,.32f);stroke(.48f,.32f,.38f,.19f);stroke(.38f,.19f,.12f,.19f);stroke(.12f,.19f,.12f,.28f);break;
        case Glyph::Pin:stroke(.32f,.18f,.68f,.18f);stroke(.38f,.18f,.38f,.48f);stroke(.62f,.18f,.62f,.48f);stroke(.38f,.48f,.22f,.63f);stroke(.22f,.63f,.78f,.63f);stroke(.78f,.63f,.62f,.48f);stroke(.5f,.63f,.5f,.92f);break;
        case Glyph::Plus:stroke(.5f,.2f,.5f,.8f);stroke(.2f,.5f,.8f,.5f);break;
        case Glyph::More:for(float a:{.2f,.5f,.8f})dc->FillEllipse(D2D1::Ellipse({x+a*s,y+s*.5f},1.5f,1.5f),brush.get());break;
        case Glyph::Rule:brush->SetOpacity(.12f);stroke(.5f,0,.5f,1);break;
        case Glyph::Spotify:{
            brush->SetOpacity(1);brush->SetColor(highContrast?color(fg):D2D1::ColorF(.12f,.84f,.38f));dc->FillEllipse(D2D1::Ellipse({x+s/2,y+s/2},s/2,s/2),brush.get());brush->SetColor(color(bg));
            winrt::com_ptr<ID2D1Factory> f;dc->GetFactory(f.put());
            for(int row=0;row<3;++row){float y0=.34f+row*.17f;winrt::com_ptr<ID2D1PathGeometry> path;f->CreatePathGeometry(path.put());winrt::com_ptr<ID2D1GeometrySink> sink;path->Open(sink.put());sink->BeginFigure({x+s*.23f,y+s*y0},D2D1_FIGURE_BEGIN_HOLLOW);sink->AddBezier({{x+s*.40f,y+s*(y0-.08f)},{x+s*.63f,y+s*(y0-.03f)},{x+s*(.79f-row*.045f),y+s*(y0+.06f)}});sink->EndFigure(D2D1_FIGURE_END_OPEN);sink->Close();dc->DrawGeometry(path.get(),brush.get(),std::max(1.f,s*.065f));}break;}
        }
    }
    brush->SetColor(color(fg));
    for(auto& line:lines) {
        auto& format=formats[line.size];
        if(!format){check_hresult(write->CreateTextFormat(L"Segoe UI Variable",nullptr,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,line.size,L"en-GB",format.put()));
        format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        DWRITE_TRIMMING trim{DWRITE_TRIMMING_GRANULARITY_CHARACTER,0,0};
        com_ptr<IDWriteInlineObject> ellipsis; write->CreateEllipsisTrimmingSign(format.get(),ellipsis.put()); format->SetTrimming(&trim,ellipsis.get());}
        format->SetTextAlignment(line.centered?DWRITE_TEXT_ALIGNMENT_CENTER:DWRITE_TEXT_ALIGNMENT_LEADING);
        brush->SetColor(color(fg));
        if(line.glow&&!highContrast){brush->SetColor(D2D1::ColorF(.78f,.71f,.97f));brush->SetOpacity(.07f);for(auto shift:{-1.f,1.f})dc->DrawTextW(line.text.c_str(),static_cast<UINT32>(line.text.size()),format.get(),D2D1::RectF(line.x+shift,line.y+shift,line.x+line.width+shift,line.y+line.size*1.6f+shift),brush.get());}
        brush->SetOpacity(line.secondary?.62f:1.f);
        dc->DrawTextW(line.text.c_str(),static_cast<UINT32>(line.text.size()),format.get(),D2D1::RectF(line.x,line.y,line.width>0?line.x+line.width:w-18,line.y+line.size*1.6f),brush.get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
    check_hresult(interop->EndDraw());
    content.Size({w*scale,h*scale});
    paintedW=w*scale;if(canvasW<=0)canvas(w*scale,h*scale);else canvas(canvasW,canvasH);
    if(fresh){auto b=compositor.CreateSurfaceBrush(surface); b.Stretch(CompositionStretch::None); content.Brush(b);}
    if(animate) {
        auto a=compositor.CreateScalarKeyFrameAnimation(); a.InsertKeyFrame(1,1); a.Duration(std::chrono::milliseconds(110));
        // Implicit starting value retargets an in-flight opacity animation.
        content.Opacity(0); content.StartAnimation(L"Opacity",a);
    } else { content.StopAnimation(L"Opacity"); content.Opacity(1); }
}
}

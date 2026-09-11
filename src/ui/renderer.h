#pragma once
#include <windows.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Composition.Desktop.h>
#include <winrt/Windows.System.h>
#include <string>
#include <vector>
#include <map>
#include "services/artwork.h"
namespace delight {
struct Line { std::wstring text; float x,y,size; bool secondary=false; float width=0; };
enum class Glyph { Music, Play, Pause, Previous, Next, Folder, Pin, Plus, More, Disc, Chip, Spotify, Rule, Card, Slider, Close, Speaker, Sun, AirPods, Logo, Gear, Timer, Toggle, Microphone };
struct Mark { Glyph glyph; float x,y,size; bool active=false; bool disabled=false; float height=28,value=0; };
class Renderer {
    winrt::Windows::System::DispatcherQueueController queue{nullptr};
    winrt::Windows::UI::Composition::Compositor compositor{nullptr};
    winrt::Windows::UI::Composition::Desktop::DesktopWindowTarget target{nullptr};
    winrt::Windows::UI::Composition::ContainerVisual root{nullptr};
    winrt::Windows::UI::Composition::SpriteVisual content{nullptr};
    winrt::Windows::UI::Composition::SpriteVisual background{nullptr};
    winrt::Windows::UI::Composition::CompositionGraphicsDevice graphics{nullptr};
    winrt::com_ptr<ID2D1Device> device;
    winrt::com_ptr<IDWriteFactory> write;
    COLORREF backgroundColor=0xffffffff;
    float canvasW=0,canvasH=0,paintedW=0,surfaceW=0,surfaceH=0;
    winrt::Windows::UI::Composition::CompositionDrawingSurface surface{nullptr};
    std::map<float,winrt::com_ptr<IDWriteTextFormat>> formats;
public:
    void initialize(HWND hwnd);
    void canvas(float w,float h);
    void draw(float w,float h,float dpi,const std::vector<Line>& lines,bool animate,bool highContrast,const std::vector<Mark>& marks={},std::shared_ptr<const Artwork> artwork={});
};
}

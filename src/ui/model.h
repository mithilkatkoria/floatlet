#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace delight {
enum class State { Collapsed, Peek, Music, Shelf, Controls, Timer, Preferences, Call, Agenda, Calendar, Screenshots, Wifi, DragOver, DraggingOut, Hidden, Unavailable };
enum class Event { Hover, Leave, Open, Escape, Hide, Show, DragEnter, DragLeave, DragStart, DragEnd, Suspend, Resume };
struct Model {
    State state = State::Collapsed, last = State::Music, returnState = State::Collapsed;
    bool pinned = false;
    bool expanded() const { return state == State::Music || state == State::Shelf || state == State::Controls || state == State::Calendar || state == State::Screenshots || state == State::Wifi || state == State::Timer || state == State::Preferences || state == State::Agenda || state == State::Call; }
    void panel(State s) { if (s == State::Music || s == State::Shelf || s == State::Controls || s == State::Calendar || s == State::Screenshots || s == State::Wifi || s == State::Timer || s == State::Preferences || s == State::Agenda || s == State::Call) state = last = s; }
    void send(Event e) {
        if (e == Event::Hide) { state = State::Hidden; pinned = false; return; }
        if (e == Event::Suspend) { state = State::Unavailable; return; }
        if (e == Event::Show || e == Event::Resume) { if(state != State::Hidden || e == Event::Show) state = State::Collapsed; return; }
        if(state == State::Hidden || state == State::Unavailable) return;
        if(e == Event::DragEnter || e == Event::DragStart) {
            if(state != State::DragOver && state != State::DraggingOut) returnState = state;
            state = e == Event::DragEnter ? State::DragOver : State::DraggingOut; return;
        }
        if(e == Event::DragLeave || e == Event::DragEnd) { if(state == State::DragOver || state == State::DraggingOut) state = returnState; return; }
        if(state == State::DragOver || state == State::DraggingOut) return;
        switch(e) {
        case Event::Hover: if(state == State::Collapsed) state = State::Peek; break;
        case Event::Leave: if(!pinned) {state = State::Collapsed; last=State::Music;} break;
        case Event::Open: state = pinned?last:State::Music; break;
        case Event::Escape: pinned = false; state = State::Collapsed; last=State::Music; break;
        default: break;
        }
    }
};
struct Size { float w, h; };
inline Size sizeFor(State s, bool touch = false) {
    switch(s) {
    case State::Peek: return {272,56};
    case State::Music: return {344,154};
    case State::Shelf: case State::DragOver: case State::DraggingOut: return {356,196};
    case State::Call: return {344,184};
    case State::Timer: return {344,320};
    case State::Preferences: return {356,380};
    case State::Agenda: return {344,328};
    case State::Controls: return {356,354};
    case State::Calendar: return {344,328};
    case State::Wifi: case State::Screenshots: return {356,276};
    default: return {224,touch ? 44.f : 36.f};
    }
}
struct Rect { int x,y,w,h; };
inline Rect place(Rect work, Size size, float scale, int alignment = 1) {
    int w = std::min(work.w, static_cast<int>(std::lround(size.w * scale)));
    int h = std::min(work.h, static_cast<int>(std::lround(size.h * scale)));
    int inset = static_cast<int>(std::lround(8 * scale));
    if(alignment==3)return {work.x+work.w-w,std::clamp(work.y+(work.h-h)/2,work.y,work.y+work.h-h),w,h};
    int x = alignment == 0 ? work.x + inset : alignment == 2 ? work.x + work.w - w - inset : work.x + (work.w-w)/2;
    return {std::clamp(x,work.x,work.x+work.w-w),std::clamp(work.y+inset,work.y,work.y+work.h-h),w,h};
}
inline std::optional<double> cpu(std::uint64_t idle, std::uint64_t kernel, std::uint64_t user) {
    auto total = kernel+user;
    if(!total || idle > total) return {};
    return 100.0 * static_cast<double>(total-idle) / static_cast<double>(total);
}
// Monotone cubic retargeting is continuous in position. Velocity matching is not claimed.
struct Motion {
    float from=0, to=0; double start=0, duration=.3;
    float at(double t) const { float p=static_cast<float>(std::clamp((t-start)/duration,0.0,1.0)); return from+(to-from)*(1-(1-p)*(1-p)*(1-p)); }
    void target(float v, double now) { from=at(now); to=v; start=now; }
};
constexpr std::size_t ShelfLimit=100;
inline std::wstring pathKey(std::wstring p) {
    std::replace(p.begin(),p.end(),L'/',L'\\');
    while(p.size()>3 && p.back()==L'\\') p.pop_back();
    // Case folding happens via CompareStringOrdinal in Windows shelf storage.
    return p;
}
}

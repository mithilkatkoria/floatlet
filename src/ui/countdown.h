#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include "duration_editor.h"
namespace delight {
struct Countdown {
    std::uint64_t deadline=0,held=0,total=0;bool running=false,finished=false;
    std::uint64_t left(std::uint64_t now)const{return running?(deadline>now?deadline-now:0):held;}
    bool active()const{return running||held>0;}
    void start(unsigned seconds,std::uint64_t now){total=std::clamp(seconds,1u,24u*60u*60u)*1000ull;deadline=now+total;held=0;running=true;finished=false;}
    void pause(std::uint64_t now){if(running){if(tick(now))return;held=left(now);running=false;}}
    void resume(std::uint64_t now){if(held){deadline=now+held;held=0;running=true;}}
    void cancel(){deadline=held=total=0;running=finished=false;}
    bool tick(std::uint64_t now){if(running&&now>=deadline){running=false;held=0;finished=true;return true;}return false;}
    float fraction(std::uint64_t now)const{return total?float(left(now))/float(total):0;}
    std::wstring text(std::uint64_t now)const{return durationText(unsigned((left(now)+999)/1000));}
};
}

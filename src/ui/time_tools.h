#pragma once
#include <chrono>
#include <cstdint>
#include <string>
namespace delight {
enum class TimeMode { Alarm, Timer, Stopwatch };
inline std::wstring clockText(unsigned hours,unsigned minutes){return (hours<10?L"0":L"")+std::to_wstring(hours)+L":"+(minutes<10?L"0":L"")+std::to_wstring(minutes);}
struct Stopwatch {
    std::uint64_t started=0,held=0;bool running=false;
    std::uint64_t elapsed(std::uint64_t now)const{return held+(running&&now>=started?now-started:0);}
    void toggle(std::uint64_t now){if(running){held=elapsed(now);running=false;}else{started=now;running=true;}}
    void reset(){started=held=0;running=false;}
    std::wstring preciseText(std::uint64_t now)const{auto ms=elapsed(now);auto cent=(ms/10)%100;return text(now)+L"."+(cent<10?L"0":L"")+std::to_wstring(cent);}
    std::wstring text(std::uint64_t now)const{auto seconds=elapsed(now)/1000;return clockText(unsigned(seconds/60),unsigned(seconds%60));}
};
struct Alarm {
    unsigned hour=7,minute=0;std::chrono::system_clock::time_point due{};bool armed=false,ringing=false;
    void arm(std::chrono::system_clock::time_point now){using namespace std::chrono;auto zone=current_zone();auto local=zone->to_local(now);auto day=floor<days>(local);auto target=local_seconds{day}+hours{hour}+minutes{minute};if(target<=local)target+=days{1};due=zone->to_sys(target,choose::latest);armed=true;ringing=false;}
    bool tick(std::chrono::system_clock::time_point now){if(armed&&now>=due){armed=false;ringing=true;return true;}return false;}
    void cancel(){armed=ringing=false;}
    std::wstring text()const{return clockText(hour,minute);}
};
}

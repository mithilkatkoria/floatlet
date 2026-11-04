#pragma once
#include <algorithm>
#include <array>
#include <string>
namespace delight {
inline std::wstring durationText(unsigned seconds) {
    auto two=[](unsigned v){return (v<10?L"0":L"")+std::to_wstring(v);};
    if(seconds>=3600)return std::to_wstring(seconds/3600)+L":"+two(seconds/60%60)+L":"+two(seconds%60);
    return std::to_wstring(seconds/60)+L":"+two(seconds%60);
}
struct DurationEditor {
    std::array<unsigned,3> parts{};int field=1;bool open=false,typing=false;
    void begin(unsigned seconds){seconds=std::clamp(seconds,1u,86400u);parts={seconds/3600,seconds/60%60,seconds%60};field=1;typing=false;open=true;}
    unsigned seconds()const{return parts[0]*3600+parts[1]*60+parts[2];}
    void select(int next){field=(next+3)%3;typing=false;}
    void normalize(){if(parts[0]==24)parts[1]=parts[2]=0;}
    void step(int delta){typing=false;auto max=field==0?24:59;parts[field]=unsigned(std::clamp(int(parts[field])+delta,0,max));if(field>0&&parts[field]&&parts[0]==24)parts[0]=23;normalize();}
    void digit(unsigned value){unsigned next=typing?parts[field]*10+value:value;unsigned max=field==0?24u:59u;if(next>max)next=value;parts[field]=next;if(field>0&&next&&parts[0]==24)parts[0]=23;normalize();typing=!typing;}
    void erase(){parts[field]=0;typing=false;}
    std::wstring text(int index)const{return (parts[index]<10?L"0":L"")+std::to_wstring(parts[index]);}
};
}

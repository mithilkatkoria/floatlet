#pragma once
#include <windows.h>
#include <mmsystem.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <thread>

namespace delight::search {
inline void playOpenCue() {
    std::thread([] {
        constexpr int rate=22050, count=rate*11/100;
        std::array<std::uint8_t,44+count*2> wave{};
        auto put16=[&](int offset,std::uint16_t value){wave[offset]=std::uint8_t(value);wave[offset+1]=std::uint8_t(value>>8);};
        auto put32=[&](int offset,std::uint32_t value){for(int i=0;i<4;++i)wave[offset+i]=std::uint8_t(value>>(i*8));};
        for(int i=0;i<4;++i){wave[i]="RIFF"[i];wave[8+i]="WAVE"[i];wave[12+i]="fmt "[i];wave[36+i]="data"[i];}
        put32(4,std::uint32_t(wave.size()-8));put32(16,16);put16(20,1);put16(22,1);put32(24,rate);put32(28,rate*2);put16(32,2);put16(34,16);put32(40,count*2);
        constexpr double pi=3.141592653589793;
        double phase=0;
        for(int i=0;i<count;++i){double t=double(i)/rate,attack=std::min(1.0,t/.009),release=std::pow(std::max(0.0,1-t/.11),2.3);phase+=2*pi*(640+340*t/.11)/rate;auto sample=std::int16_t(std::sin(phase)*attack*release*1700);put16(44+i*2,std::uint16_t(sample));}
        PlaySoundA(reinterpret_cast<const char*>(wave.data()),nullptr,SND_MEMORY|SND_SYNC|SND_NODEFAULT);
    }).detach();
}
}

#pragma once
#include <windows.h>
#include <mmsystem.h>
#include <vector>
#include <cmath>
#include <cstdint>
#include <cstring>
namespace delight {
// Original four-note bell phrase. PCM exists only while an alert is sounding.
inline std::vector<std::uint8_t> makeChime(){
    constexpr unsigned rate=22050,count=rate*4;std::vector<std::uint8_t> data(44+count*2);
    auto put16=[&](unsigned p,unsigned v){data[p]=std::uint8_t(v);data[p+1]=std::uint8_t(v>>8);};
    auto put32=[&](unsigned p,unsigned v){put16(p,v);put16(p+2,v>>16);};
    std::memcpy(data.data(),"RIFF",4);put32(4,unsigned(data.size()-8));std::memcpy(data.data()+8,"WAVEfmt ",8);put32(16,16);put16(20,1);put16(22,1);put32(24,rate);put32(28,rate*2);put16(32,2);put16(34,16);std::memcpy(data.data()+36,"data",4);put32(40,count*2);
    const double notes[]={523.251,659.255,783.991,1046.502};
    for(unsigned i=0;i<count;++i){double t=double(i)/rate,sample=0;for(unsigned n=0;n<4;++n){double age=t-.38*n;if(age>=0&&age<2.3){double envelope=(1-std::exp(-age*80))*std::exp(-age*3.3);double phase=6.283185307179586*notes[n]*age;sample+=envelope*(std::sin(phase)+.18*std::sin(phase*2)+.06*std::sin(phase*3));}}auto value=std::int16_t(sample*6500);put16(44+i*2,std::uint16_t(value));}return data;
}
class Chime {
    using Play=BOOL(WINAPI*)(LPCWSTR,HMODULE,DWORD);HMODULE library=nullptr;Play play=nullptr;std::vector<std::uint8_t> wave;
public:
    ~Chime(){stop();if(library)FreeLibrary(library);}
    void start(){stop();if(!library){library=LoadLibraryExW(L"winmm.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(library)play=reinterpret_cast<Play>(GetProcAddress(library,"PlaySoundW"));}if(play){wave=makeChime();play(reinterpret_cast<LPCWSTR>(wave.data()),nullptr,SND_MEMORY|SND_ASYNC|SND_LOOP|SND_NODEFAULT);}}
    void stop(){if(play)play(nullptr,nullptr,0);std::vector<std::uint8_t>().swap(wave);}
};
}

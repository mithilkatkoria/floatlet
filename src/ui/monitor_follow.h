#pragma once
#include <cstdint>
namespace delight {
// Two stable observations prevent edge chatter without high-frequency input hooks.
struct MonitorFollow {
    std::uintptr_t candidate=0;
    std::uint64_t since=0;
    void reset(){candidate=0;since=0;}
    bool update(std::uintptr_t current,std::uintptr_t underPointer,std::uint64_t now,bool blocked){
        if(blocked||!underPointer||underPointer==current){reset();return false;}
        if(candidate!=underPointer){candidate=underPointer;since=now;return false;}
        if(now-since<200)return false;
        reset();return true;
    }
};
}

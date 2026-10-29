#pragma once
#include <cstdint>

namespace delight {
class CallLatch {
    std::uint64_t holdUntil=0;
    bool armed=false;
public:
    static constexpr std::uint64_t holdMs=30000;
    bool update(bool observed,bool active,bool windowAlive,bool enabled,std::uint64_t now) {
        if(!enabled||!windowAlive||active){armed=false;return false;}
        if(observed){holdUntil=now+holdMs;armed=true;return true;}
        if(armed&&now<holdUntil)return true;
        armed=false;
        return false;
    }
};
}

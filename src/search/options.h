#pragma once
#include <string>
namespace delight::search {
struct Options {
    bool enabled=true,applications=true,files=true,windows=true,fuzzy=true,reduceMotion=false;
    unsigned modifiers=1,key=32;
    // 0: Everything (with Windows Search fallback), 1: Floatlet's own index.
    unsigned provider=0,scope=0,sides=0;bool extended=false;
};
}

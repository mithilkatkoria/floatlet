#pragma once
namespace delight::search {
struct Options {
    bool enabled=true,applications=true,files=true,windows=true,fuzzy=true,reduceMotion=false;
    unsigned modifiers=1,key=32;
};
}

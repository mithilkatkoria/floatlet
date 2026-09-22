#pragma once
#include <algorithm>
namespace delight::search {
struct Placement { int x,y,width,height; };
inline Placement searchPlacement(int left,int top,int right,int bottom,unsigned dpi) {
    auto px=[&](int value){return int((value*dpi+48)/96);};
    int availableWidth=std::max(1,right-left),availableHeight=std::max(1,bottom-top);
    int width=std::min(px(660),std::max(1,availableWidth-px(24)));
    int height=std::min(px(552),std::max(1,availableHeight-px(24)));
    return {left+(availableWidth-width)/2,top+(availableHeight-height)/3,width,height};
}
}

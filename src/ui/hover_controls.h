#pragma once
namespace delight {
// Shared hit geometry for passive mouse-down and transport dispatch.
// Previous / play-pause / next. Gaps and metadata open the full panel.
inline int hoverTransport(float x,float y){
    if(y<24||y>=56)return -1;
    if(x>=148&&x<184)return 0;
    if(x>=186&&x<222)return 1;
    if(x>=224&&x<260)return 2;
    return -1;
}
}

#pragma once
namespace delight {
// Shared hit geometry for passive mouse-down and transport dispatch.
// Previous / play-pause / next. Gaps and metadata open the full panel.
inline int hoverTransport(float x,float y){
    if(y<27||y>=55)return -1;
    if(x>=150&&x<182)return 0;
    if(x>=188&&x<220)return 1;
    if(x>=226&&x<258)return 2;
    return -1;
}
}

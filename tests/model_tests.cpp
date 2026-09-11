#include "ui/model.h"
#include "ui/monitor_follow.h"
#include "ui/spring.h"
#include "ui/hover_controls.h"
#include "services/artwork.h"
#include <iostream>
#include <cstdlib>
using namespace delight;
int checks=0;
void require(bool ok) { ++checks; if(!ok) { std::cerr<<"Failed check "<<checks<<'\n'; std::exit(1); } }
int main() {
    require(hoverTransport(166,41)==0);require(hoverTransport(204,41)==1);require(hoverTransport(242,41)==2);
    require(hoverTransport(149,41)==-1);require(hoverTransport(182,41)==-1);require(hoverTransport(220,41)==-1);require(hoverTransport(258,41)==-1);require(hoverTransport(166,26)==-1);require(hoverTransport(166,55)==-1);
    Spring spring;spring.snap(224,0);
    for(int i=0;i<100;++i){double t=i*.009;spring.sample(t);auto p=spring.position,v=spring.velocity;spring.retarget(i%2?224:344,t);require(spring.position==p&&spring.velocity==v);}
    spring.sample(5);require(spring.settled());
    require(!boundedArtwork(0,20));require(!boundedArtwork(4096,4096));require(boundedArtwork(128,128));
    MonitorFollow follow;
    require(!follow.update(1,2,0,false));
    require(!follow.update(1,2,199,false));
    require(follow.update(1,2,200,false));
    require(!follow.update(2,1,220,false));
    require(!follow.update(2,2,240,false));
    require(!follow.update(2,1,260,false));
    require(!follow.update(2,1,500,true));
    require(!follow.update(2,1,700,false));
    require(follow.update(2,1,900,false));
    require(!follow.update(1,0,1000,false));
    Model m;
    for(auto panel:{State::Shelf,State::Controls,State::Calendar,State::Timer,State::Preferences}){m.panel(panel);m.send(Event::Leave);require(m.state==State::Collapsed);m.send(Event::Open);require(m.state==State::Music);m.panel(panel);m.pinned=true;m.send(Event::Leave);require(m.state==panel);m.send(Event::Open);require(m.state==panel);m.send(Event::Escape);}
    m.send(Event::Hover); require(m.state==State::Peek);
    m.send(Event::Open); require(m.state==State::Music);
    m.panel(State::Shelf); m.send(Event::DragEnter); m.send(Event::Leave); require(m.state==State::DragOver);
    m.send(Event::DragLeave); require(m.state==State::Shelf);
    m.pinned=true; m.send(Event::Leave); require(m.state==State::Shelf);
    m.send(Event::Escape); require(m.state==State::Collapsed && !m.pinned);
    for(int i=0;i<100;++i) { m.send(Event::Hover); m.send(Event::Leave); require(m.state==State::Collapsed); }
    m.send(Event::Hide); m.send(Event::Hover); require(m.state==State::Hidden);
    m.send(Event::Resume); require(m.state==State::Hidden);
    m.send(Event::Show); require(m.state==State::Collapsed);
    for(float dpi : {1.f,1.25f,1.5f,2.f,3.f}) {
        for(auto work : {Rect{-1920,0,1920,1080},Rect{0,-1920,1280,1920},Rect{0,0,200,100}}) {
            auto r=place(work,sizeFor(State::Controls),dpi);
            require(r.x>=work.x && r.y>=work.y && r.x+r.w<=work.x+work.w && r.y+r.h<=work.y+work.h);
        }
    }
    require(place({0,0,2880,1920},sizeFor(State::Collapsed),2).w==448);
    require(!cpu(0,0,0)); require(!cpu(20,10,0)); require(cpu(50,70,30).value()==50);
    Motion a; for(int i=0;i<100;++i) { double t=i*.017; float before=a.at(t); a.target(i%2?0.f:1.f,t); require(std::abs(a.at(t)-before)<.0001f); }
    require(pathKey(L"C:/example/")==L"C:\\example");
    std::cout<<checks<<" deterministic checks passed\n";
}

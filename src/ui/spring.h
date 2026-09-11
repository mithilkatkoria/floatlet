#pragma once
#include <cmath>
namespace delight {
// Analytic critically damped spring. Retargets preserve position and velocity.
struct Spring {
    double position=0,velocity=0,target=0,last=0;
    void snap(double p,double now){position=target=p;velocity=0;last=now;}
    void sample(double now){
        double dt=now-last;if(dt<=0)return;last=now;
        constexpr double omega=26;
        double delta=position-target,b=velocity+omega*delta,e=std::exp(-omega*dt);
        position=target+(delta+b*dt)*e;velocity=(velocity-omega*b*dt)*e;
    }
    void retarget(double p,double now){sample(now);target=p;}
    bool settled() const{return std::abs(position-target)<.12&&std::abs(velocity)<1;}
};
}

#define NOMINMAX
#pragma once
#include <algorithm>
#include <cmath>
namespace BotShotSpread {
struct Direction { float x,y,z; };
inline Direction Sample(Direction a,float deviationDegrees,float u,float v) {
    const float length=std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z);
    if (!std::isfinite(length) || length<1e-6f || !std::isfinite(deviationDegrees) || deviationDegrees<0.f || deviationDegrees>90.f)
        return {0,0,0};
    a={a.x/length,a.y/length,a.z/length};
    Direction right=std::fabs(a.z)<.99f ? Direction{-a.y,a.x,0} : Direction{0,-a.z,a.y};
    const float r=std::sqrt(right.x*right.x+right.y*right.y+right.z*right.z);
    right={right.x/r,right.y/r,right.z/r};
    const Direction up{a.y*right.z-a.z*right.y,a.z*right.x-a.x*right.z,a.x*right.y-a.y*right.x};
    constexpr float pi=3.14159265358979323846f;
    const float cosTheta=1.f-std::clamp(u,0.f,1.f)*(1.f-std::cos(deviationDegrees*pi/360.f));
    const float sinTheta=std::sqrt(std::max(0.f,1.f-cosTheta*cosTheta));
    const float phi=std::clamp(v,0.f,1.f)*2.f*pi;
    const float x=std::cos(phi)*sinTheta,y=std::sin(phi)*sinTheta;
    return {a.x*cosTheta+right.x*x+up.x*y,a.y*cosTheta+right.y*x+up.y*y,a.z*cosTheta+right.z*x+up.z*y};
}
}

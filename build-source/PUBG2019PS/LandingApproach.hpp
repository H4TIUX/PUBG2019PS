#pragma once
#define NOMINMAX
#include <algorithm>
#include <cmath>

namespace LandingApproach {
inline float Wrap(float degrees) {
    degrees=std::fmod(degrees+180.f,360.f);
    if (degrees<0.f) degrees+=360.f;
    return degrees-180.f;
}
inline float Turn(float current,float desired,float seconds,bool parachute) {
    const float limit=(parachute ? 45.f : 60.f)*std::clamp(seconds,0.f,1.f);
    return Wrap(current+std::clamp(Wrap(desired-current),-limit,limit));
}
struct Input {float forward; bool hold;};
inline Input Approach(float distance,float speed,bool held,bool parachute) {
    if (!std::isfinite(distance) || !std::isfinite(speed) || distance<0.f) return {0.f,true};
    // Decelerate within a few metres under canopy. The former 40--120 m
    // dead band prevented reaching an otherwise valid settlement target.
    const float stop=std::clamp(std::max(0.f,speed)*(parachute ? 0.6f : 0.4f),300.f,parachute ? 1500.f : 3000.f);
    const float resume=stop+500.f;
    const bool hold=held ? distance<resume : distance<=stop;
    return {hold ? 0.f : std::clamp((distance-stop)/(parachute ? 6000.f : 10000.f),0.15f,1.f),hold};
}
}

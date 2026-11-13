#pragma once
#include <algorithm>
#include <cmath>
namespace DoorTraversalPolicy {
enum class Action {Abort, Approach, RequestOpen, Wait, Cross, Complete};
constexpr float ProbeDistance=450.f;
constexpr float ApproachAcceptance=15.f;
// Native EDoorState values are verified separately from reflected metadata.
inline Action Decide(int nativeState,float distance,float range,bool crossed,bool expired) {
    if (expired || nativeState<0 || nativeState>3 || !std::isfinite(distance) || !std::isfinite(range)) return Action::Abort;
    if (crossed) return Action::Complete;
    if (nativeState==2) return Action::Cross;
    if (nativeState==1 || nativeState==3) return Action::Wait;
    return distance>range ? Action::Approach : Action::RequestOpen;
}
inline float RequestRange(float nativeRange) {
    if (!std::isfinite(nativeRange) || nativeRange<70.f || nativeRange>500.f) return 180.f;
    return nativeRange-10.f;
}
inline float StandOff(float range,float height,float lateral) {
    const float available=range*range-height*height-lateral*lateral;
    if (!std::isfinite(available) || available<70.f*70.f) return -1.f;
    // The entire acceptance circle must fit inside native interaction range.
    return std::clamp(std::sqrt(available)-ApproachAcceptance-20.f,55.f,150.f);
}
}

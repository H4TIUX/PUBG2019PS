#pragma once

// Distances are measured in Unreal centimetres from the respective centres.
namespace ZoneBoostPolicy {
inline bool FarAndInBlue(float safeDistance, float safeRadius, float blueDistance, float blueRadius) {
    return safeDistance >= safeRadius + 50000.f && blueDistance > blueRadius;
}
// Begin before the edge and keep travelling farther inside the circle.
// Proportional caps keep the policy sensible in the final small circles.
inline bool ShouldStartOrContinueTravel(float safeDistance, float safeRadius, bool travelling) {
    const float cap = travelling ? 25000.f : 10000.f;
    const float fraction = travelling ? 0.30f : 0.15f;
    const float margin = safeRadius * fraction < cap ? safeRadius * fraction : cap;
    return safeDistance > safeRadius - margin;
}
inline bool GroundTravel(bool walking, bool falling, bool boosted, double sinceWalking, bool skyOrChute) {
    return walking || (falling && boosted && sinceWalking >= 0.0 && sinceWalking <= 1.5 && !skyOrChute);
}
// A bot can move in circles without approaching the safe zone. Recover
// after two slow samples, or immediately when essentially stationary.
inline bool NeedsNavigationRecovery(float moved, float previousGap, float currentGap, int& noProgressSamples) {
    const float improvement=previousGap-currentGap;
    if (improvement > 500.f || improvement < -20000.f)
        noProgressSamples=0;
    else ++noProgressSamples;
    const bool recover=moved < 300.f || noProgressSamples >= 2;
    if (recover) noProgressSamples=0;
    return recover;
}
inline bool WithinFirstThreeClosings(bool observed, int started, bool releasing) {
    return observed && started >= 0 && (started < 3 || (started == 3 && releasing));
}
}

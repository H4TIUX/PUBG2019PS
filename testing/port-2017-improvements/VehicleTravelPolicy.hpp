#pragma once

namespace VehicleTravelPolicy {
// Do not drive away from a nearby circle centre just to reach 300 m.
inline bool AtCircleCentre(float distanceCm, float travelledCm, float initialDistanceCm) {
    return distanceCm < 10000.f &&
        (travelledCm >= 30000.f || initialDistanceCm <= 30000.f || distanceCm < 1500.f);
}
inline bool LeaveBlockedInBlue(bool inBlue, double blockedSince, double now) {
    return inBlue && blockedSince > 0.0 && now - blockedSince >= 2.0;
}
}

#pragma once
#include <cmath>

namespace VehicleObstaclePolicy {
// A near-horizontal, unpenetrated landscape contact can be the road beneath
// a horizontal probe. Walls, objects, steep ground and penetration remain
// obstacles; the caller still checks a raised probe before proceeding.
inline bool TraversableGround(bool landscape, bool penetrating, float normalZ) {
    return landscape && !penetrating && std::isfinite(normalZ) && normalZ >= 0.90f;
}
inline bool IsRejectedVehicle(int index, int serial, double until, double now,
                              int candidateIndex, int candidateSerial) {
    return index >= 0 && now < until && index == candidateIndex && serial == candidateSerial;
}
}

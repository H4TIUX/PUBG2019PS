#pragma once
#include <cmath>

namespace HumanRockRecovery {
struct Evidence {
    bool overlappingTerrain;
    bool tryingToMove;
    bool movementEligible;
    bool destinationClear;
    double secondsStuck;
    double safeAge;
    double cooldownRemaining;
    float travelled;
    float distanceToSafe;
};
inline bool ShouldRecover(const Evidence& e) {
    return e.overlappingTerrain && e.tryingToMove && e.movementEligible && e.destinationClear &&
        std::isfinite(e.secondsStuck) && e.secondsStuck >= 4.0 &&
        std::isfinite(e.safeAge) && e.safeAge >= 0.0 && e.safeAge <= 12.0 &&
        std::isfinite(e.cooldownRemaining) && e.cooldownRemaining <= 0.0 &&
        std::isfinite(e.travelled) && e.travelled >= 0.f && e.travelled <= 35.f &&
        std::isfinite(e.distanceToSafe) && e.distanceToSafe >= 20.f && e.distanceToSafe <= 250.f;
}
}

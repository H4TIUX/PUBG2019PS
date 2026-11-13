#pragma once
#include <algorithm>
#include <cmath>
namespace WeaponDamagePolicy {
// Unreal world coordinates are centimetres; trajectory asset range fields
// (e.g. rifles' 1000 and shotguns' 150) are in metres.
inline float MetresFromWorldCentimetres(float centimetres) { return centimetres * 0.01f; }
// Both humans and bots pass this raw damage and the weapon's own damage type
// to the engine. Body-part and armor handling remains in the native damage path.
inline float Raw(float base, float rangeModifier, float referenceDistance, float distance, float maximumDistance) {
    if (!std::isfinite(base) || base<=0.f || base>10000.f || !std::isfinite(distance) || distance<0.f) return 0.f;
    if (std::isfinite(maximumDistance) && maximumDistance>0.f && distance>maximumDistance) return 0.f;
    if (!std::isfinite(rangeModifier) || rangeModifier<=0.f || rangeModifier>1.f) return 0.f;
    if (rangeModifier==1.f) return base;
    if (!std::isfinite(referenceDistance) || referenceDistance<=0.f) return 0.f;
    const float damage=base*std::pow(rangeModifier,distance/referenceDistance);
    return std::isfinite(damage) ? std::clamp(damage,0.f,base) : 0.f;
}
}

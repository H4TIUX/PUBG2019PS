#pragma once
#include "../SDK.hpp"
#include "WeaponDamagePolicy.hpp"
namespace SharedWeaponDamage {
inline bool Live(const SDK::UObject* object, int32_t index) {
    auto* array=SDK::UObject::GObjects.GetTypedPtr();
    if (!object || !array || index<0 || index>=array->Num()) return false;
    auto* items=array->GetDecrytedObjPtr();
    if (!items || items[index].Object!=object) return false;
    const int32_t flags=*reinterpret_cast<const int32_t*>(&items[index].Pad_8[0]);
    return !(flags & ((1<<29)|(1<<28)));
}
inline SDK::UObject* Weak(const SDK::FWeakObjectPtr& ref) {
    auto* array=SDK::UObject::GObjects.GetTypedPtr();
    if (!array || ref.ObjectIndex<0 || ref.ObjectIndex>=array->Num() || ref.ObjectSerialNumber<=0) return nullptr;
    auto* items=array->GetDecrytedObjPtr();
    if (!items || *reinterpret_cast<const int32_t*>(&items[ref.ObjectIndex].Pad_8[8])!=ref.ObjectSerialNumber) return nullptr;
    auto* object=items[ref.ObjectIndex].Object;
    return Live(object,ref.ObjectIndex) ? object : nullptr;
}
inline bool Resolve(SDK::ATslWeapon_Gun* gun, float distance, float& damage, SDK::TSubclassOf<SDK::UDamageType>& type) {
    if (!gun || !gun->IsA(SDK::ATslWeapon_Trajectory::StaticClass())) return false;
    auto* asset=static_cast<SDK::ATslWeapon_Trajectory*>(gun)->WeaponTrajectoryData;
    if (!asset || !Live(asset,asset->GetIndex())) return false;
    const auto& config=asset->TrajectoryConfig;
    const float distanceMetres=WeaponDamagePolicy::MetresFromWorldCentimetres(distance);
    damage=WeaponDamagePolicy::Raw(float(config.HitDamage),config.RangeModifier,config.ReferenceDistance,distanceMetres,config.TravelDistanceMax);
    type=config.DamageType;
    return damage>0.f && type!=nullptr;
}
}

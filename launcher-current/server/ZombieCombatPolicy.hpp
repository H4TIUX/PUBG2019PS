#pragma once
namespace ZombieCombatPolicy {
// -1 preserves the game's decision, 0 prevents damage, 1 permits damage.
inline int DamageDecision(int attackerRole, int victimRole, bool battleRoyale, int teamSize) {
    constexpr int Human=1, Zombie=3;
    if (attackerRole==Zombie && victimRole==Zombie) return 0;
    if ((attackerRole==Zombie && victimRole==Human) || (attackerRole==Human && victimRole==Zombie)) return 1;
    if (battleRoyale && teamSize==1 && attackerRole==Human && victimRole==Human) return 1;
    return -1;
}
}

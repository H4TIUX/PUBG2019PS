#include "ZombieCombatPolicy.hpp"
#include <cassert>
int main() {
    using ZombieCombatPolicy::DamageDecision;
    assert(DamageDecision(3,3,true,1)==0);
    assert(DamageDecision(3,1,true,1)==1);
    assert(DamageDecision(1,3,true,1)==1);
    assert(DamageDecision(1,1,true,1)==1);
    assert(DamageDecision(1,1,true,2)==-1);
    assert(DamageDecision(1,1,false,1)==-1);
    assert(DamageDecision(0,1,true,1)==-1);
}

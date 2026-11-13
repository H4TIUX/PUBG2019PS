#include "ZombieHumanCountPolicy.hpp"
#include <cassert>
#include <cstdio>
int main() {
    using ZombieHumanCountPolicy::RecoverLiveHuman;
    // An alive connected human remains alive while the lobby pawn is handed to the aircraft.
    assert(RecoverLiveHuman(false, true, true, false, false, true));
    assert(!RecoverLiveHuman(true, true, true, false, false, true));
    assert(!RecoverLiveHuman(false, false, true, false, false, true));
    assert(!RecoverLiveHuman(false, true, false, false, false, true));
    assert(!RecoverLiveHuman(false, true, true, true, false, true));
    assert(!RecoverLiveHuman(false, true, true, false, true, true));
    assert(!RecoverLiveHuman(false, true, true, false, false, false));
    using ZombieHumanCountPolicy::DisplayZombieCount;
    assert(DisplayZombieCount(0,0,3,false)==3);
    assert(DisplayZombieCount(0,3,3,true)==3);
    assert(DisplayZombieCount(2,1,3,true)==3);
    assert(DisplayZombieCount(2,0,3,true)==2);
    assert(DisplayZombieCount(0,0,3,true)==0);
    using ZombieHumanCountPolicy::TotalAliveForHud;
    assert(TotalAliveForHud(1,95)-95==1);
    assert(TotalAliveForHud(2,3)-3==2);
    assert(TotalAliveForHud(0,0)==0);
    assert(TotalAliveForHud(-1,3)==3);
    using ZombieHumanCountPolicy::HoldBattleRoyale;
    using ZombieHumanCountPolicy::PublishZombieCount;
    assert(HoldBattleRoyale(1, 1, 0, 95, true, 5.0));
    assert(HoldBattleRoyale(1, 1, 3, 0, true, 120.0));
    assert(HoldBattleRoyale(2, 2, 0, 0, true, 120.0)); // solo humans still compete
    assert(!HoldBattleRoyale(2, 1, 0, 0, true, 120.0)); // one surviving human squad
    assert(!HoldBattleRoyale(0, 0, 95, 0, true, 120.0));
    assert(!HoldBattleRoyale(1, 1, 0, 0, true, 31.0));
    assert(PublishZombieCount(0, 3, 0));
    assert(PublishZombieCount(3, 3, 0));
    assert(!PublishZombieCount(3, 3, 3));
    std::puts("PASS: connected living humans, handoff, end conditions and scoreboard resynchronization");
}

#pragma once
namespace ZombieHumanCountPolicy {
// A living observed player pawn can survive the native lobby-to-aircraft
// handoff even while its PlayerState temporarily has bKilled set.
inline bool RecoverLiveHuman(bool isBot, bool alivePawn, bool humanRole,
                             bool quit, bool spectatorOnly, bool remoteConnection) {
    return !isBot && alivePawn && humanRole && !quit && !spectatorOnly && remoteConnection;
}
inline bool HoldBattleRoyale(int humans, int humanTeams, int zombies, int pending, bool started, double elapsed) {
    return humans > 0 && (humanTeams > 1 || zombies > 0 || pending > 0 || (started && elapsed < 30.0));
}
// Count scheduled zombies during initial spawn so the native zombie HUD and
// win condition do not interpret its startup delay as an empty opposing team.
inline int DisplayZombieCount(int living, int pending, int configured, bool matchStarted) {
    if (living < 0) living = 0;
    if (pending < 0) pending = 0;
    if (configured < 0) configured = 0;
    return matchStarted ? living + pending : (living > configured ? living : configured);
}
// The native zombie HUD computes humans as NumAlivePlayers minus zombies.
// Publish the combined total there; retain the separate human count for rules.
inline int TotalAliveForHud(int humans, int zombies) {
    return (humans > 0 ? humans : 0) + (zombies > 0 ? zombies : 0);
}
inline bool PublishZombieCount(int previous, int actual, int observed) {
    return previous != actual || observed != actual;
}
}

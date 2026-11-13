#pragma once

namespace HumanNameTeamPolicy {
// Only an explicitly linked player-state may identify a member of a squad.
inline bool ExplicitMatch(bool authorizedMatches, bool cachedMatches) {
    return authorizedMatches || cachedMatches;
}
inline bool DirectMatch(bool authorizedPresent, bool authorizedMatches,
                        bool cachedPresent, bool cachedMatches) {
    return (!authorizedPresent || authorizedMatches) && (!cachedPresent || cachedMatches);
}
// A detached identity-less actor is safe to associate by team number only
// when the connected player is the only member of that team.
inline bool DetachedSoloMatch(int memberCount, bool authorizedPresent,
                              bool cachedPresent, bool characterPresent) {
    return memberCount == 1 && !authorizedPresent && !cachedPresent && !characterPresent;
}
}

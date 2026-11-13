#pragma once
#include <algorithm>
#include <cctype>
#include <string>
namespace BotWeaponPolicy {
// This version's supply-drop-exclusive guns. Test both the concrete item and
// weapon class name so aliases do not bypass the bot-only rule.
inline bool IsCarePackageWeapon(std::string name) {
    std::transform(name.begin(), name.end(), name.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    static const char* const names[] = {"awm", "groza", "aug", "mk14", "m249", "dbs", "dp12"};
    for (const char* token : names) if (name.find(token) != std::string::npos) return true;
    return false;
}
}

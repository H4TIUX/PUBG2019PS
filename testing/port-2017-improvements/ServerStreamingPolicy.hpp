#pragma once
#include <string_view>

namespace ServerStreamingPolicy {
// Audited runtime IDC layers: 0..3 Training, 0..7 Sanhok, 0..15 Miramar/Classic
// contains no blocking primitive, replicated actor, door or loot component.
// Keep the engine's own requests for these decal-data layers instead of
// forcing every tile resident. Never unload a level or change terrain LODs.
inline bool KeepNativeStreaming(std::string_view Map, std::string_view Package) {
    if ((Map != "Desert_Main" && Map != "Erangel_Main" && Map != "Savage_Main" && Map != "Range_Main") || Package.substr(0, 5) != "IDC_x") return false;
    size_t At = 5;
    auto Tile = [&]() {
        const size_t Begin = At;
        unsigned Value = 0;
        while (At < Package.size() && Package[At] >= '0' && Package[At] <= '9') {
            if (At - Begin >= 2) return false;
            Value = Value * 10 + unsigned(Package[At++] - '0');
        }
        return At > Begin && Value < (Map == "Range_Main" ? 4u : Map == "Savage_Main" ? 8u : 16u) && (At - Begin == 1 || Package[Begin] != '0');
    };
    if (!Tile() || Package.substr(At, 2) != "_y") return false;
    At += 2;
    return Tile() && At == Package.size();
}
}

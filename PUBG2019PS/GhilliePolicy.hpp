#pragma once
#include <algorithm>
#include <cctype>
#include <string>
namespace GhilliePolicy {
inline std::string Lower(std::string s) {
    std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    return s;
}
inline bool IsGhillie(const std::string& item) { return Lower(item).find("item_ghillie_") != std::string::npos; }
inline bool Forbidden(const std::string& map, const std::string& item) {
    const std::string m=Lower(map), i=Lower(item);
    if (m.find("desert_main")!=std::string::npos) return i.find("item_ghillie_04_")!=std::string::npos;
    if (m.find("dihorotok_main")!=std::string::npos) return i.find("item_ghillie_01_")!=std::string::npos || i.find("item_ghillie_02_")!=std::string::npos;
    return false;
}
}

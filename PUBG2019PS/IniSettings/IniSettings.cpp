#include "IniSettings.h"
#include <algorithm>
#include <cctype>

bool Loaded = false;
CSimpleIniA ini;
static std::string IniPath; // remembered so the ini can be reloaded at runtime

void LoadIni(HMODULE hModule)
{
    if (Loaded)
        return;
    std::string PathAndName;
    const int BUFSIZE = 4096;
    char buffer[BUFSIZE];
    if (GetModuleFileNameA(hModule, buffer, BUFSIZE - 1) <= 0)
        return;
    PathAndName = buffer;
    size_t found = PathAndName.find_last_of("/\\");
    PathAndName = PathAndName.substr(0, found);
    std::string str{ PathAndName.c_str() };
    str.append("\\settings.ini");
    IniPath = str;
    ini.SetUnicode();

    SI_Error rc = ini.LoadFile(str.c_str());
    if (rc < 0)
        return;
    Loaded = true;
}

void ReloadIni()
{
    if (IniPath.empty())
        return;
    ini.Reset();
    ini.SetUnicode();
    Loaded = ini.LoadFile(IniPath.c_str()) >= 0;
}

int GetWaitTime()
{
    return GetIniInt("Lobby", "WaitTime", 60);
}

std::string GetIniString(const char* Section, const char* Key, const char* Default)
{
    if (!Loaded)
        return Default;
    const char* Value = ini.GetValue(Section, Key, Default);
    return Value ? std::string(Value) : std::string(Default);
}

int GetIniInt(const char* Section, const char* Key, int Default)
{
    std::string Value = GetIniString(Section, Key, "");
    if (Value.empty())
        return Default;
    try { return std::stoi(Value); }
    catch (...) { return Default; }
}

float GetIniFloat(const char* Section, const char* Key, float Default)
{
    std::string Value = GetIniString(Section, Key, "");
    if (Value.empty())
        return Default;
    try { return std::stof(Value); }
    catch (...) { return Default; }
}

bool GetIniBool(const char* Section, const char* Key, bool Default)
{
    std::string Value = GetIniString(Section, Key, "");
    if (Value.empty())
        return Default;
    std::transform(Value.begin(), Value.end(), Value.begin(),
        [](unsigned char c) { return (char)std::tolower(c); });
    return Value == "1" || Value == "true" || Value == "yes" || Value == "on";
}

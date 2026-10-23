#pragma once
// Minimal stand-in for the PUBG2017PS Common.h, used when building PUBG_Bots.dll standalone.
// Bots/BotManager.cpp and Config/IniSettings.cpp compile unchanged against this.

#include <Windows.h>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>
#include "../simpleini/SimpleIni.h"
#include "SDK.hpp"


using namespace SDK;

void CUSTOMLOG(std::string LOG);
SDK::UClass* Get_PlayerPawn_Class();

void LoadIni(HMODULE hModule);
void ReloadIni();

int GetWaitTime();

// Generic getters (return Default if the ini or key is missing)
std::string GetIniString(const char* Section, const char* Key, const char* Default);
int         GetIniInt(const char* Section, const char* Key, int Default);
float       GetIniFloat(const char* Section, const char* Key, float Default);
bool        GetIniBool(const char* Section, const char* Key, bool Default);

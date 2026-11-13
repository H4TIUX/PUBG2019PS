#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <TlHelp32.h>

#include "MinHook.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "SDK/Engine_parameters.hpp"
#include "SDK/Engine_classes.hpp"
#include "SDK/TslGame_parameters.hpp"
#include "SDK/PhysXVehicles_parameters.hpp"
#include "IniSettings.h"
#include "SDK.hpp"

#include "BotLogic.h"
#include "NameTeamPolicy.hpp"
#include "ZoneBoostPolicy.hpp"
#include "VehicleTravelPolicy.hpp"
#include "VehicleObstaclePolicy.hpp"
#include "HumanRockRecovery.hpp"
#include "ZombieHumanCountPolicy.hpp"
#include "LootSpatial.hpp"
#include "LandingApproach.hpp"
#include "SharedWeaponDamage.hpp"
#include "BotWeaponPolicy.hpp"
#include "GhilliePolicy.hpp"
#include "KillDistancePolicy.hpp"
#include "DoorTraversalPolicy.hpp"
#include "BotShotSpread.hpp"

#include "../minhook_src/hde/hde64.h"

using namespace SDK;

// =================================================================================
//  Settings  ([Bots] section of settings.ini - every key is optional)
// =================================================================================
struct FBotSettings
{
    bool        Enabled = true;
    bool        HumanlikeGoals = true; // false preserves the previous hunter/roam behavior
    bool        UseADS = true;
    int         AutoBurstShotsMin = 7, AutoBurstShotsMax = 10;
    int         TeamSize = 1;
    int         CameraMode = 0; // 0 = TPP/FPP, 1 = FPP only
    bool        BotsInTeam = false;
    int         Count = 20;      // bots spawned by "bots" / F6 / match start
    int         MaxBots = 99;
    bool        SpawnOnMatchStart = true;
    int         SpawnPerTick = 2;       // spawn throttle (per AI think)
    std::string PawnClass = "random"; // random = PlayerFemale_A_C / male class like real players | auto | a class name
    std::string Weapon;                       // comma list matched against weapon/item class names, empty = any non-pistol gun
    std::string NamePrefix = "Bot_";
    float       SpawnMinX = 150000.f, SpawnMaxX = 550000.f; // same box the server uses for players
    float       SpawnMinY = 150000.f, SpawnMaxY = 500000.f;
    float       MinSpawnZ = -1000000.f;  // raise this if bots spawn on the sea floor

    int         ThinkHz = 10;
    float       ViewDistance = 30000.f; // cm - normal sight range (300 m)
    float       AlertDistance = 50000.f; // cm - sight range for a few seconds after being shot / hearing shots
    float       HearingDistance = 30000.f; // cm - bots hear player gunfire this far and go check it out
    bool        HuntPlayers = true;    // roaming drifts toward the nearest player
    float       HuntDistance = 200000.f; // cm - only players within this range pull bots in (2 km)
    float       HuntChance = 0.6f;    // share of a HUNTER's roam picks that head toward its player
    float       DriftChance = 0.f;
    float       LateZoneShare = 0.4f;    // share of bots that stay out in the blue until the gas gets close, then run in
    float       LateZoneMargin = 25000.f; // cm - late bots start running in when the gas is this close (250 m)
    float       HunterPickRange = 150000.f; // cm - hunters are picked from bots within this range of a player (no teleporting)     // share of a normal bot's roam picks that drift toward a player (0 = stay in their own area)
    int         MaxBotsPerPlayer = 1;       // most bots fighting one player at once (a bot that gets shot can always fight back)
    int         MaxResponders = 2;       // how many nearby bots come to check out a burst of gunfire
    float       ZoneLeg = 30000.f; // cm - how far a bot runs per leg when it is outside the safe zone
    bool        AvoidObstacles = true;    // probe ahead while walking: go around walls, hop small stuff, avoid water
    float       FovDegrees = 130.f;
    float       ReactionTime = 0.2f;    // seconds between first seeing someone and shooting
    bool        FightEachOther = true;

    bool        OwnHitRoll = true;    // resolve native weapon deviation and collision traces, then ApplyPointDamage
    float       LongRangeDist = 10000.f; // cm - beyond this bots fire less: shorter bursts, longer pauses, later / fewer hits
    float       CloseFightDist = 4000.f;  // cm - bots this close to each other fight even during BotPeaceTime
    float       EvadeTime = 4.f;     // seconds of dodging (crouch spam + side steps) after a player hits it
    bool        EvadeCrouch = true;
    float       MaxFireDistance = 20000.f; // cm - bots only shoot players closer than this (200 m); further away they close in first
    float       PlayerLandingGrace = 10.f;    // seconds after a player lands before bots may shoot them (never while in the air)
    bool        CombatLog = true;    // log who bots spot / hit / why a nearby bot isn't shooting
    float       Damage = 14.f;    // per body hit
    float       HeadshotChance = 0.08f;
    float       HeadshotMultiplier = 1.0f;    // the game applies its own head multiplier on top
    std::string DamageTypeClass;              // optional UDamageType class name, empty = engine default
    float       BurstMin = 0.25f, BurstMax = 0.8f;
    float       BurstPauseMin = 0.8f, BurstPauseMax = 2.0f;
    bool        InfiniteAmmo = true;

    float       RoamRadius = 12000.f; // cm
    int         HuntersPerPlayer = 3;       // bots that always know where each player is and track them down
    float       HunterRefillDelay = 150.f;   // seconds before a dead hunter is replaced
    bool        AiLod = true;    // bots far from every player think less often (big CPU saver with many bots)
    bool        HunterCatchUpReplacements = false; // replacement hunters may also teleport near the player
    bool        DressBots = true;    // give bots random clothes
    bool        CopyAppearance = false;   // old workaround (copy a player's look) - superseded by BotUpdateAppearance
    bool        BotUpdateAppearance = true;   // run the game's own UpdateAppearence on bots after dressing them
    bool        BotBodyFix = true;    // copy a connected player's face / hair / skin values into bots' appearance record
    std::string BotGender = "match"; // match = only genders a connected player has | random | female | male
    bool        BodyServerRebuild = false;   // also rebuild the bot's look on the server from the fixed record (experimental)
    bool        MarkAsBot = false;   // set PlayerState.bIsABot on bots (clients then draw them as test bots: red ghosts / headless)     // bots get a real player's face / hair / body setup (fixes red-ghost / headless bots on some PCs)
    bool        UnarmedInLobby = true;    // bots get their gun only once the match starts (not in the lobby / plane)
    bool        LobbyPunch = true;    // lobby antics include throwing punches
    bool        LobbyAntics = true;    // lobby: bots walk circles, bunny-hop, crouch-spam, spin instead of standing still
    bool        ShowBotShots = true;    // players see / hear bot gunfire (muzzle flash, sound, tracers)
    bool        HunterCatchUp = false;    // hunters that are far away or stuck at water get moved near their player
    float       HunterCatchUpDist = 150000.f; // cm - farther than this from the player = catch up (1.5 km)
    float       HunterCatchUpMin = 25000.f; // cm - catch-up lands them at least this far from the player
    float       HunterCatchUpMax = 45000.f; // cm - and at most this far
    bool        DressPlayers = true;    // give players random clothes in empty slots
    bool        FixKillFeedCount = true;    // kill feed "X left" / "match finished" uses players + bots
    float       HunterReplan = 3.f;     // seconds between hunters re-reading the player's position
    float       HunterStandoff = 1500.f;  // cm - hunters aim for a spot this far from the player (flanking)
    float       AcceptRadius = 120.f;
    bool        FollowZone = true;
    bool        Sprint = true;
    bool        UseNavMesh = true;    // auto-detected; set 0 to force straight-line moves

    bool        VerboseLog = false;
    bool        CountAsJoined = true;    // bump the lobby "JOINED" counter (TslGameState::NumJoinPlayers) per bot
    float       SeaLevelZ = -1e9f;   // ground below SeaLevelZ + 150 is never used for spawns; -1e9 = learn it
    bool        RegisterTeams = true;    // give AI bots a PUBG ATeam (+ GameMode Teams entry)
    bool        BotsInPlayerCount = true;    // keep JOINED / "X left" / alive teams = humans + bots
    bool        KeepMatchWhileBotsAlive = true; // don't let the game end the match while bots are still alive
    bool        HoldLobbyWhenEmpty = true; // lobby countdown stays at full until a real player is in the lobby
    float       EndWhenPlayersDead = 5.f;  // seconds after the last real player dies (or leaves) before the match ends (0 = off)
    float       ExitAfterMatch = 5.f;  // seconds after the match ends before the server process closes itself (0 = off)
    bool        SuperheroDrop = true;    // jump only over land, then land instantly (no freefall / chute / fall damage)
    float       JumpSpread = 90.f;    // once the plane is over land, each bot jumps within this many seconds
    float       DropSpread = 150000.f; // cm - land up to this far from the point under the plane (1.5 km)
    float       VisibleFall = 5.f;     // seconds a bot is seen skydiving after jumping before it "vanishes" into its simulated drop
    float       ClaimHold = 20.f;    // seconds a player stays "taken" by the bot fighting them after it last saw them
    float       DropTimeMin = 45.f;    // seconds from jumping to being on the ground (like a player's skydive + chute)
    float       DropTimeMax = 100.f;
    float       LandingGrace = 25.f;    // seconds after landing before a bot starts shooting
    float       BotPeaceTime = 600.f;   // seconds after the match starts before bots fight EACH OTHER
    float       PlaneKickTime = 150.f;   // seconds after match start: everyone still in the plane is thrown out (0 = off)
    float       PlaneMinZ = 100000.f;
    float       PlaneZ = 150000.f;
    float       EjectMin = 15.f;    // seconds after boarding the plane
    float       EjectMax = 75.f;

    // [Plane]
    bool  PlaneRoute = true;
    float MapSize = 816000.f;       // Erangel, cm
    float CrossRadius = 120000.f;   // the route passes within this of the map centre (cm)
    float EdgeMin = 30000.f;        // route starts / ends at this box (just off the coast)
    float EdgeMax = 786000.f;
    float Altitude = 150000.f;      // cm (1500 m)
    float SpeedKmh = 0.f;           // 0 = keep the plane's own speed
    bool  EjectAtEnd = true;
    float EjectBeforeEnd = 80000.f; // cm before the end of the route
    bool  OpenJump = true;          // make sure the plane's jump (F) is open once it's on the new route
    float PlaneNetRate = 30.f;      // plane network updates per second (0 = leave the game's own)
    bool  JumpAssist = true;        // a player in the plane presses F (interact) and is still aboard -> jump them out
    float JumpAssistDelay = 0.3f;   // seconds to let the game do it itself first
    bool  JumpLog = true;           // log what players' games send the server while in the plane
    bool  PlaneLog = true;
    bool  SplineAbsolute = true;    // pin the path to the map, not to the (moving) plane
};
FBotSettings S;
 
namespace
{
    void LoadSettings()
    {
        const char* B = "Bots";
        S.Enabled = GetIniBool(B, "Enabled", S.Enabled);
        S.HumanlikeGoals = GetIniBool(B, "HumanlikeGoals", S.HumanlikeGoals);
        S.UseADS = GetIniBool(B, "UseADS", S.UseADS);
        S.AutoBurstShotsMin=(std::clamp)(GetIniInt(B,"AutoBurstShotsMin",7),1,30);
        S.AutoBurstShotsMax=(std::clamp)(GetIniInt(B,"AutoBurstShotsMax",10),S.AutoBurstShotsMin,30);
        S.TeamSize = GetIniInt(B, "TeamSize", S.TeamSize);
        if (S.TeamSize != 1 && S.TeamSize != 2 && S.TeamSize != 4) S.TeamSize = 1;
        S.CameraMode = GetIniInt(B, "CameraMode", 0) == 1 ? 1 : 0;
        S.BotsInTeam = GetIniBool(B, "BotsInTeam", S.BotsInTeam);
        S.Count = GetIniInt(B, "Count", S.Count);
        S.MaxBots = GetIniInt(B, "MaxBots", S.MaxBots);
        S.SpawnOnMatchStart = GetIniBool(B, "SpawnOnMatchStart", S.SpawnOnMatchStart);
        S.SpawnPerTick = (std::max)(1, GetIniInt(B, "SpawnPerTick", S.SpawnPerTick));
        S.PawnClass = GetIniString(B, "PawnClass", S.PawnClass.c_str());
        S.Weapon = GetIniString(B, "Weapon", S.Weapon.c_str());
        S.NamePrefix = GetIniString(B, "NamePrefix", S.NamePrefix.c_str());
        S.SpawnMinX = GetIniFloat(B, "SpawnMinX", S.SpawnMinX);
        S.SpawnMaxX = GetIniFloat(B, "SpawnMaxX", S.SpawnMaxX);
        S.SpawnMinY = GetIniFloat(B, "SpawnMinY", S.SpawnMinY);
        S.SpawnMaxY = GetIniFloat(B, "SpawnMaxY", S.SpawnMaxY);
        S.MinSpawnZ = GetIniFloat(B, "MinSpawnZ", S.MinSpawnZ);
        S.ThinkHz = (std::clamp)(GetIniInt(B, "ThinkHz", S.ThinkHz), 2, 60);
        S.ViewDistance = GetIniFloat(B, "ViewDistance", S.ViewDistance);
        S.AlertDistance = GetIniFloat(B, "AlertDistance", S.AlertDistance);
        S.HearingDistance = GetIniFloat(B, "HearingDistance", S.HearingDistance);
        S.HuntPlayers = GetIniBool(B, "HuntPlayers", S.HuntPlayers);
        S.HuntDistance = GetIniFloat(B, "HuntDistance", S.HuntDistance);
        S.HuntChance = GetIniFloat(B, "HuntChance", S.HuntChance);
        S.DriftChance = GetIniFloat(B, "DriftChance", S.DriftChance);
        S.LateZoneShare = GetIniFloat(B, "LateZoneShare", S.LateZoneShare);
        S.LateZoneMargin = GetIniFloat(B, "LateZoneMargin", S.LateZoneMargin);
        S.HunterPickRange = GetIniFloat(B, "HunterPickRange", S.HunterPickRange);
        S.MaxBotsPerPlayer = (int)GetIniFloat(B, "MaxBotsPerPlayer", (float)S.MaxBotsPerPlayer);
        S.MaxResponders = (int)GetIniFloat(B, "MaxResponders", (float)S.MaxResponders);
        S.ZoneLeg = GetIniFloat(B, "ZoneLeg", S.ZoneLeg);
        S.AvoidObstacles = GetIniBool(B, "AvoidObstacles", S.AvoidObstacles);
        S.FovDegrees = GetIniFloat(B, "FovDegrees", S.FovDegrees);
        S.ReactionTime = GetIniFloat(B, "ReactionTime", S.ReactionTime);
        S.FightEachOther = GetIniBool(B, "FightEachOther", S.FightEachOther);
        S.OwnHitRoll = GetIniBool(B, "OwnHitRoll", S.OwnHitRoll);
        S.MaxFireDistance = GetIniFloat(B, "MaxFireDistance", S.MaxFireDistance);
        S.LongRangeDist = GetIniFloat(B, "LongRangeDist", S.LongRangeDist);
        S.CloseFightDist = GetIniFloat(B, "CloseFightDist", S.CloseFightDist);
        S.EvadeTime = GetIniFloat(B, "EvadeTime", S.EvadeTime);
        S.EvadeCrouch = GetIniBool(B, "EvadeCrouch", S.EvadeCrouch);
        S.PlayerLandingGrace = GetIniFloat(B, "PlayerLandingGrace", S.PlayerLandingGrace);
        S.CombatLog = GetIniBool(B, "CombatLog", S.CombatLog);
        S.Damage = GetIniFloat(B, "Damage", S.Damage);
        S.HeadshotChance = GetIniFloat(B, "HeadshotChance", S.HeadshotChance);
        S.HeadshotMultiplier = GetIniFloat(B, "HeadshotMultiplier", S.HeadshotMultiplier);
        S.DamageTypeClass = GetIniString(B, "DamageTypeClass", S.DamageTypeClass.c_str());
        S.BurstMin = GetIniFloat(B, "BurstMin", S.BurstMin);
        S.BurstMax = GetIniFloat(B, "BurstMax", S.BurstMax);
        S.BurstPauseMin = GetIniFloat(B, "BurstPauseMin", S.BurstPauseMin);
        S.BurstPauseMax = GetIniFloat(B, "BurstPauseMax", S.BurstPauseMax);
        S.InfiniteAmmo = GetIniBool(B, "InfiniteAmmo", S.InfiniteAmmo);
        S.RoamRadius = GetIniFloat(B, "RoamRadius", S.RoamRadius);
        S.HuntersPerPlayer = GetIniInt(B, "HuntersPerPlayer", S.HuntersPerPlayer);
        S.HunterRefillDelay = GetIniFloat(B, "HunterRefillDelay", S.HunterRefillDelay);
        S.AiLod = GetIniBool(B, "AiLod", S.AiLod);
        S.HunterCatchUpReplacements = GetIniBool(B, "HunterCatchUpReplacements", S.HunterCatchUpReplacements);
        S.DressBots = GetIniBool(B, "DressBots", S.DressBots);
        S.CopyAppearance = GetIniBool(B, "CopyAppearance", S.CopyAppearance);
        S.BotUpdateAppearance = GetIniBool(B, "BotUpdateAppearance", S.BotUpdateAppearance);
        S.BotBodyFix = GetIniBool(B, "BotBodyFix", S.BotBodyFix);
        S.BotGender = GetIniString(B, "BotGender", S.BotGender.c_str());
        S.BodyServerRebuild = GetIniBool(B, "BodyServerRebuild", S.BodyServerRebuild);
        S.MarkAsBot = GetIniBool(B, "MarkAsBot", S.MarkAsBot);
        S.LobbyAntics = GetIniBool(B, "LobbyAntics", S.LobbyAntics);
        S.UnarmedInLobby = GetIniBool(B, "UnarmedInLobby", S.UnarmedInLobby);
        S.LobbyPunch = GetIniBool(B, "LobbyPunch", S.LobbyPunch);
        S.ShowBotShots = GetIniBool(B, "ShowBotShots", S.ShowBotShots);
        S.HunterCatchUp = GetIniBool(B, "HunterCatchUp", S.HunterCatchUp);
        S.HunterCatchUpDist = GetIniFloat(B, "HunterCatchUpDist", S.HunterCatchUpDist);
        S.HunterCatchUpMin = GetIniFloat(B, "HunterCatchUpMin", S.HunterCatchUpMin);
        S.HunterCatchUpMax = GetIniFloat(B, "HunterCatchUpMax", S.HunterCatchUpMax);
        S.DressPlayers = GetIniBool(B, "DressPlayers", S.DressPlayers);
        S.FixKillFeedCount = GetIniBool(B, "FixKillFeedCount", S.FixKillFeedCount);
        S.HunterReplan = (std::max)(1.f, GetIniFloat(B, "HunterReplan", S.HunterReplan));
        S.HunterStandoff = GetIniFloat(B, "HunterStandoff", S.HunterStandoff);
        S.AcceptRadius = GetIniFloat(B, "AcceptRadius", S.AcceptRadius);
        S.FollowZone = GetIniBool(B, "FollowZone", S.FollowZone);
        S.Sprint = GetIniBool(B, "Sprint", S.Sprint);
        S.UseNavMesh = GetIniBool(B, "UseNavMesh", S.UseNavMesh);
        S.VerboseLog = GetIniBool(B, "VerboseLog", S.VerboseLog);
        S.CountAsJoined = GetIniBool(B, "CountAsJoined", S.CountAsJoined);
        S.SeaLevelZ = GetIniFloat(B, "SeaLevelZ", -1e9f);
        S.RegisterTeams = GetIniBool(B, "RegisterTeams", S.RegisterTeams);
        S.BotsInPlayerCount = GetIniBool(B, "BotsInPlayerCount", S.BotsInPlayerCount);
        S.KeepMatchWhileBotsAlive = GetIniBool(B, "KeepMatchWhileBotsAlive", S.KeepMatchWhileBotsAlive);
        S.EndWhenPlayersDead = GetIniFloat(B, "EndWhenPlayersDead", S.EndWhenPlayersDead);
        S.HoldLobbyWhenEmpty = GetIniBool(B, "HoldLobbyWhenEmpty", S.HoldLobbyWhenEmpty);
        S.ExitAfterMatch = GetIniFloat(B, "ExitAfterMatch", S.ExitAfterMatch);
        S.SuperheroDrop = GetIniBool(B, "SuperheroDrop", S.SuperheroDrop);
        S.JumpSpread = GetIniFloat(B, "JumpSpread", S.JumpSpread);
        S.DropSpread = GetIniFloat(B, "DropSpread", S.DropSpread);
        S.LandingGrace = GetIniFloat(B, "LandingGrace", S.LandingGrace);
        S.DropTimeMin = GetIniFloat(B, "DropTimeMin", S.DropTimeMin);
        S.VisibleFall = GetIniFloat(B, "VisibleFall", S.VisibleFall);
        S.ClaimHold = GetIniFloat(B, "ClaimHold", S.ClaimHold);
        S.DropTimeMax = GetIniFloat(B, "DropTimeMax", S.DropTimeMax);
        S.BotPeaceTime = GetIniFloat(B, "BotPeaceTime", S.BotPeaceTime);
        S.PlaneKickTime = GetIniFloat(B, "PlaneKickTime", S.PlaneKickTime);
        S.PlaneMinZ = GetIniFloat(B, "PlaneMinZ", S.PlaneMinZ);
        S.PlaneZ = GetIniFloat(B, "PlaneZ", S.PlaneZ);
        S.EjectMin = GetIniFloat(B, "EjectMin", S.EjectMin);
        S.EjectMax = GetIniFloat(B, "EjectMax", S.EjectMax);
        if (S.EjectMax < S.EjectMin) S.EjectMax = S.EjectMin;

        if (S.BurstMax < S.BurstMin) S.BurstMax = S.BurstMin;
        if (S.BurstPauseMax < S.BurstPauseMin) S.BurstPauseMax = S.BurstPauseMin;
    }

    uint64_t EncodeTeam(uint64_t team)
    {
        uint32_t low = static_cast<uint32_t>(team);
        uint32_t high = static_cast<uint32_t>(team >> 32);

        uint32_t encLow = (low ^ 0x95F5A15Bu) + 1524127531u;
        uint32_t encHigh = (high ^ 0xEB85EBC5u) + 301227509u;

        return (static_cast<uint64_t>(encHigh) << 32) | encLow;
    }

    uint64_t DecodeTeam(uint64_t team)
    {
        uint32_t encLow = static_cast<uint32_t>(team);
        uint32_t encHigh = static_cast<uint32_t>(team >> 32);

        uint32_t low = (encLow - 1524127531u) ^ 0x95F5A15Bu;
        uint32_t high = (encHigh - 301227509u) ^ 0xEB85EBC5u;

        return (static_cast<uint64_t>(high) << 32) | low;
    }

    void BLOG(const std::string& Msg) { CUSTOMLOG("[PUBG2019PS] " + Msg); }

    // =================================================================================
    //  Small utilities
    // =================================================================================
    double Now()
    {
        static LARGE_INTEGER Freq = [] { LARGE_INTEGER F; QueryPerformanceFrequency(&F); return F; }();
        LARGE_INTEGER C;
        QueryPerformanceCounter(&C);
        return double(C.QuadPart) / double(Freq.QuadPart);
    }

    std::mt19937& Rng() { static std::mt19937 R{ std::random_device{}() }; return R; }
    float RandF(float Min, float Max) { if (Max <= Min) return Min; std::uniform_real_distribution<float> D(Min, Max); return D(Rng()); }
    int   RandI(int Min, int Max) { if (Max <= Min) return Min; std::uniform_int_distribution<int> D(Min, Max); return D(Rng()); }

    constexpr float kPi = 3.14159265f;

    std::string Lower(std::string In)
    {
        std::transform(In.begin(), In.end(), In.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        return In;
    }

    std::vector<std::string> SplitList(const std::string& In)
    {
        std::vector<std::string> Out;
        std::stringstream SS(In);
        std::string Part;
        while (std::getline(SS, Part, ','))
        {
            Part.erase(0, Part.find_first_not_of(" \t"));
            Part.erase(Part.find_last_not_of(" \t") + 1);
            if (!Part.empty()) Out.push_back(Lower(Part));
        }
        return Out;
    }

    FVector V(float X, float Y, float Z) { FVector R{}; R.X = X; R.Y = Y; R.Z = Z; return R; }
    std::string Pos(const FVector& P);
    FVector Flat(const FVector& In) { return V(In.X, In.Y, 0.f); }
    float   Len(const FVector& In) { return std::sqrt(In.X * In.X + In.Y * In.Y + In.Z * In.Z); }
    FVector SafeNormal(const FVector& In) { float L = Len(In); return L > 0.001f ? In / L : V(0, 0, 0); }

    // =================================================================================
    //  Object validity (memory-safe: goes through GObjects instead of dereferencing)
    // =================================================================================
    constexpr int32 kInternalPendingKill = 1 << 29; // EInternalObjectFlags::PendingKill  (UE 4.16)
    constexpr int32 kInternalUnreachable = 1 << 28; // EInternalObjectFlags::Unreachable

    FUObjectItem* GetItem(int32 Index)
    {
        TUObjectArray* Arr = UObject::GObjects.GetTypedPtr();
        if (!Arr || Index < 0 || Index >= Arr->Num())
            return nullptr;
        return &Arr->GetDecrytedObjPtr()[Index];
    }

    template<typename T>
    void SetWeak(TWeakObjectPtr<T>& Weak, const UObject* Obj)
    {
        Weak.ObjectIndex = -1;
        Weak.ObjectSerialNumber = 0;
        if (!Obj)
            return;
        FUObjectItem* Item = GetItem(Obj->GetIndex());
        if (!Item || Item->Object != Obj) return;
        const int32 Serial = *reinterpret_cast<const int32*>(&Item->Pad_8[8]); // FUObjectItem::SerialNumber
        if (Serial == 0)
            return; // never weak-referenced yet; leave null rather than build a bad weak ptr
        Weak.ObjectIndex = Obj->GetIndex();
        Weak.ObjectSerialNumber = Serial;
    }

    // =================================================================================
    //  Game thread detection + command queue
    // =================================================================================
    DWORD GGameThreadId = 0;

    DWORD FindMainThreadId()
    {
        // In UE4 on Windows the game thread is the process main thread = earliest created thread.
        const DWORD Pid = GetCurrentProcessId();
        HANDLE Snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (Snap == INVALID_HANDLE_VALUE)
            return 0;
        THREADENTRY32 TE{};
        TE.dwSize = sizeof(TE);
        ULONGLONG Best = ~0ull;
        DWORD BestId = 0;
        if (Thread32First(Snap, &TE))
        {
            do
            {
                if (TE.th32OwnerProcessID != Pid)
                    continue;
                HANDLE H = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, TE.th32ThreadID);
                if (!H)
                    continue;
                FILETIME C, E, K, U;
                if (GetThreadTimes(H, &C, &E, &K, &U))
                {
                    ULONGLONG T = (ULONGLONG(C.dwHighDateTime) << 32) | C.dwLowDateTime;
                    if (T < Best) { Best = T; BestId = TE.th32ThreadID; }
                }
                CloseHandle(H);
            } while (Thread32Next(Snap, &TE));
        }
        CloseHandle(Snap);
        return BestId;
    }

    enum class EBotState : uint8
    {
        Roam = 1,
        Combat = 2,
        Dead = 3
    };

    struct FBot
    {
        int             Id = 0;
        std::string     Name;
        ATslCharacter* Pawn = nullptr;   int32 PawnIndex = -1;
        AController* Ctrl = nullptr;   int32 CtrlIndex = -1;
        bool            IsPC = false;      // real local player (CreatePlayer) instead of an AIController
        ATeam* TeamActor = nullptr; int32 TeamActorIndex = -1;
        bool            HasTeleport = false;
        FVector         TeleportTo{};
        double          PawnWaitUntil = 0;
        double          NextZombieZoneCheck = 0;
        bool ZombieGroundSpawn = false;
        ATslCharacter* ZombieTarget = nullptr; int32 ZombieTargetIndex = -1; double NextZombieTargetCheck = 0;
        bool            PawnWarned = false;
        double          EjectAt = 0, NextEjectTry = 0;
        float           ReleaseAlt = 0.f;
        double          NextReleaseTry = 0;
        bool            ChuteLogged = false;
        bool            DropPending = false;   // left the plane, superhero landing not done yet
        double          HoldFor = 0;           // simulated skydive time (s) - landed but hidden until JumpedAt + HoldFor
        double          JumpedAt = 0;
        bool            Hidden = false;        // on the ground but "still in the air": invisible, no collision, frozen
        bool            LandTimerSet = false;  // jump time re-rolled once the plane got over land
        double          NoFireUntil = 0;       // landing grace
        int             DropTries = 0;
        double          NextDropTry = 0;
        int             EjectTries = 0;
        bool            SkyPrepareAttempted = false;
        bool            SkyStartAttempted = false;
        bool            NativeSkyActive = false;
        double          NextSkyLandingLog = 0.0;
        bool            SkyDirectionSet = false;
        float           SkyHeading = 0.f;
        double          NextShotGeometryLog = 0;
        bool            HasSkyTarget = false;
        FVector         SkyTarget{};
        bool            SkyTargetIsLoot = false, SkyApproachHold = false;
        float           SkyPlanSide = 0.f;
        double          LastSkySteerAt = 0;
        int             SkyLoggedState = -1;
        double          NextSkySteer = 0;
        double          NextSkyTargetSearch = 0;
        double          NextSkyGroundCheck = 0.0;
        double          NextSkyShoreSearch = 0.0;
        double          NextTerrainCheck = 0.0;
        int             UndergroundChecks = 0;
        double          NextSkyRequest = 0.0;
        double          NextSkyLog = 0.0;
        double          NextBotNameCheck = 0.0;
        double          GroundProbeAt = 0.0;
        FVector         GroundProbePos{};
        EBotState       State = EBotState::Roam;
        bool            Remove = false;

        // combat
        ATslCharacter* Target = nullptr; int32 TargetIndex = -1;
        double          TargetSeenAt = 0, LastSeenTarget = 0;
        FVector         LastKnownTargetLoc{};
        bool            Firing = false;
        ATslWeapon_Gun* FiringGun = nullptr; int32 FiringGunIndex = -1;
        int             BurstRounds=1, BurstRoundsFired=0;
        double          NextTargetingAt=0;
        double          BurstEnd = 0, NextBurst = 0, LastShot = 0, NextStrafe = 0;
        double          AlertUntil = 0;
        float           LastHealth = -1.f;
        ATslCharacter* EngageTarget = nullptr; // who we've been shooting at
        double          EngageStart = 0, LastEngageShot = 0;
        int             ShotsThisEngage = 0, HitsThisBurst = 0;
        int             BurstNo = 0;
        int             BlockedBursts = 0;     // bursts skipped because the target is behind cover
        double          NextWhyLog = 0;
        double          LastHitPlayerAt = -100; // last time this bot landed a hit on a player
        double          SwimResetWindow = 0;    // start of the current run of "stuck swimming" put-backs
        double          LastHurtAt = -100;     // last time this bot took damage (provoked)
        ATslCharacter* ProvokedBy = nullptr;  // the player who last shot it
        double          ProvokedAt = -100;
        double          EvadeUntil = 0, NextCrouchFlip = 0;
        float           EvadeSide = 1.f;
        uint8           LastHitRep = 0;        // LastTakeHitInfo.EnsureReplicationByte last seen

        // movement
        FVector         Dest{};
        float           DestAcceptance=-1.f;
        bool            HasDest = false;
        double          DestSetAt = 0, NextRoamPick = 0;
        bool            UseSteer = false;
        int             MoveFails = 0;
        FVector         ProgressPos{};
        double          ProgressCheckAt = 0;
        int             StuckCount = 0;
        bool            JumpHeld = false;
        bool            Sprinting = false;
        FVector         DetourDir{};           // obstacle avoidance
        double          DetourUntil = 0, NextProbe = 0;
        float           AvoidSide = 0;
        double          AvoidSideUntil = 0;
        bool            InlandNext = false;    // next roam leg heads inland / into the zone
        int             ZoneStyle = -1;        // -1 not rolled, 0 heads into the white circle early, 1 waits in the blue until the gas is close
        FVector         HeardLoc{};            // gunfire / attacker position to investigate
        bool            HeardPending = false;
        bool            HasHome = false;       // normal bots roam around their own area (where they landed)
        FVector         Home{};
        bool            Hunter = false;        // tracks the nearest player all match
        bool            CanCatchUp = false;    // allowed to teleport near its player (first wave only by default)
        double          NextThinkAt = 0;       // AI LOD: far-away bots skip thinks
        int             SwimResets = 0;        // times it had to be put back on land
        // lobby antics
        FVector         LobbyHome{};
        bool            HasLobbyHome = false, Crouched = false;
        int             LobbyMood = 0;
        double          NextMood = 0, NextLobbyAct = 0;
        float           CircleAng = 0.f, CircleR = 400.f, CircleDir = 1.f;
        double          NextCatchUp = 0;
        float           FlankAngle = 0.f;      // which side it comes in from
        double          NextHuntReplan = 0;
        bool            SquadFollowing = false;
        double          NextSquadFollowAt = 0;

        // Persistent plan shared by flight and ground AI.
        bool HasLootArea = false, NeedsEquipment = true;
        FVector LootArea{}, AvoidLootArea{};
        double LootAreaUntil = 0, LootAreaProgressAt = 0, LootAreaAvoidUntil = 0;
        float LootAreaBestDistance = 1e9f;
        double NextAreaSearch = 0, NextAreaMove = 0, NextKitCheck = 0, InvestigateUntil = 0;
        std::string Objective;
        ETargetingType AimType = ETargetingType::Targeting_None;
        double NextAimCheck = 0;
        int EntranceAvoidDoor = -1, EntranceAvoidSerial = 0;
        double EntranceAvoidUntil = 0;

        // Nearby, real ground-item search. Stored object generations avoid stale targets.
        double          NextZoneEmergency = 0, NextZoneProgressLog = 0;
        int32 VehicleTarget = -1, VehicleSerial = 0;
        int32 VehicleRejectedIndex = -1, VehicleRejectedSerial = 0;
        double VehicleRejectedUntil = 0;
        double VehicleScanAt = 0, VehicleAttemptAt = 0, VehicleDeadline = 0, VehicleStuckAt = 0;
        FVector VehicleProgress{};
        FVector VehicleLastPos{};
        double VehicleAvoidUntil = 0, VehicleBlockedSince = 0, VehicleReverseUntil = 0, VehicleReverseCooldown = 0, VehicleLogAt = 0, VehicleStalledUntil = 0;
        int VehicleReverseAttempts = 0;
        float VehicleAvoidTurn = 0.f, VehicleReverseTurn = 0.f, VehicleTravelled = 0.f, VehicleInitialGoalDistance = 0.f;
        int32 BoatTarget=-1, BoatSerial=0;
        double BoatScanAt=0, BoatAttemptAt=0, BoatDeadline=0, BoatStuckAt=0, BoatLogAt=0;
        FVector BoatProgress{};
        bool BoatDriving=false, BoatHolding=false;
#ifdef PUBG2019_TEST_CAR_FIXTURE
        bool TestVehicleAssigned=false;
        FVector TestVehicleGoal{};
#endif
        bool VehicleDriving = false;
        bool            ZoneEmergency = false, ZoneTravel = false;
        bool            ZoneBoosted = false, ZoneBoostRejected = false;
        double          ZoneLastWalking = 0, ZoneProgressCheck = 0;
        FVector         ZoneProgressPos{};
        float           ZonePreviousGap = 0.f;
        int             ZoneStuckCount = 0, ZoneNoProgressSamples = 0;
        float           NormalRunModifier = 0.f, NormalSprintModifier = 0.f;
        double          NextDoorTry = 0, DoorWaitUntil = 0;
        int32 DoorRouteIndex=-1, DoorRouteSerial=0;
        UDoorInteractionComponent* DoorRouteObject=nullptr;
        FVector DoorRouteNear{}, DoorRouteFar{}, DoorRoutePortal{}, DoorRouteDirection{}, DoorResumeDest{};
        double DoorRouteUntil=0, DoorRouteMoveAt=0, DoorRouteRequestAt=0;
        double DoorRouteProgressAt=0, DoorProbeDeferUntil=0;
        float DoorRouteBest=1e9f;
        bool DoorRouteCrossing=false, DoorSawOpen=false;
        int DoorRequestAttempts=0;
        double          WeaponProbeAt = 0, WeaponRelevanceCheckAt = 0;
        double          ChuteDetachAt = 0;
        int32           ChuteDetachIndex = -1, ChuteDetachSerial = 0;
        double          LootStarted = 0, NextLootSearch = 0, NextLootMove = 0;
        double          LootTargetUntil = 0, LootRouteStarted = 0, LootPauseUntil = 0;
        float           LootBestDistance = 1e9f;
        int EntranceStage = 0, EntranceLoot = -1, EntranceLootSerial = 0;
        int EntranceDoor = -1, EntranceDoorSerial = 0;
        FVector EntranceNear{}, EntranceFar{};
        double EntranceSearchAt = 0, EntranceUntil = 0;
        int32           LootTargetIndex = -1, LootTargetSerial = 0;
        int32           LootAvoidIndex = -1, LootAvoidSerial = 0;
        double          LootAvoidUntil = 0;
        bool            Armed = false;
        int             ArmStep = 0;
        int             ReequipTries = 0;
        double          NextArmTry = 0;

        double          DiedAt = 0;
        bool            CountedJoin = false;

        // water
        double          SwimSince = 0;
        double          NextShoreSearch = 0;
        bool            HasShoreTarget = false;
        FVector         ShoreTarget{};
        double          SpawnedAt = 0;
        int             WaterMoves = 0;
        bool            HasDry = false;
        FVector         LastDry{};
        double          NextWaterLog = 0;

        // arming v5
        int             GiveTries = 0;
        int             LastArmMethod = -1;
        bool            ArmFailLogged = false;
    };

    struct FCharInfo
    {
        ATslCharacter* Char;
        int32          Index;
        FVector        Loc;
        bool           IsBot;
    };

    struct FWeaponChoice
    {
        UClass* ItemClass;
        UClass* WeaponClass;
        std::string ItemName;
        std::string WeaponName;
        int32 ItemIndex;
        int32 WeaponIndex;
        int32 ItemSerial;
        int32 WeaponSerial;
    };

    APawn*                     GPawnNearBots;
    bool                       GReady = false;
    bool                       GInTick = false;
    bool                       GFrozen = false;
    bool                       GIsTeamSet = false;
    UWorld* GWorld = nullptr;
    struct FHumanNameRequest { APlayerController* Controller; int32 ControllerIndex; APlayerState* State; int32 StateIndex; std::string Name; };
    std::unordered_map<int32, FHumanNameRequest> GHumanNameRequests;
    double GNextHumanNameCheck = 0.0;
    double GNextServerHostNameCheck = 0.0;
    thread_local APlayerState* GDamageVictimState = nullptr;
    thread_local ATslPlayerState* GDamageBotKillerState = nullptr;
    std::vector<FBot>          GBots;
    ATslCharacter* GServerOnlyZombieTarget=nullptr;
    bool GZombieBrSeeded=false;
    int GZombieAppearanceBudget=0;
    int GZombieDressBudget=0;
    bool GZombieAppearancePhase=false;
    std::vector<FCharInfo>     GChars;
    std::vector<FCharInfo>     GZombieHumans;
    double GNextZombieHumanSnapshot = 0;
    std::vector<FWeaponChoice> GWeapons;
    int                        GPendingSpawns = 0;
    int                        GZombieDisplayCount = 0;
    int                        GPendingNearSpawns = 0;
    bool                       GMatchSpawnDone = false;
    int                        GNextBotId = 1;
    int                        GWorkingArmStep = -1;
    double                     GLastThink = 0, GLastSteer = 0;
    uint64                     GTickCalls = 0;
    double                     GTickRateStart = 0;
    double                     GTickRate = 0;
    UClass* GPawnClass = nullptr;
    TSubclassOf<UDamageType>   GDamageType(nullptr);
    bool                       GGunDamageResolved = false;
    std::unordered_map<int32, double> GHumanAirborneAt;     // char index -> last time seen in the air
    FName                      GHeadBone{}, GSpineBone{};
    bool                       GBonesCached = false;
    int                        GHitsApplied = 0, GHitsNoEffect = 0;
    bool                       GAutoMatchStart = false;
    bool                       GMatchStateKnown = false;
    FName                      GLastMatchState{};
    int32                      GLastRemaining = 0;
    bool                       GWarnedNoDamage = false;
    bool                       GPreStartCleared = false;   // lobby bots removed just before the match started
    int                        GCarryOver = 0;             // how many lobby bots to respawn on land after the start
    double                     GSpawnNotBefore = 0;        // hold spawns until the plane has taken everyone
    double                     GZombieStartWaitSince = 0, GZombieStartNextCheck = 0;
    double                     GMatchStartedAt = 0;        // for BotPeaceTime
    double                     GNextKickTry = 0;
    bool                       GKickAnnounced = false;
    int                        GPendingLobbySpawns = 0;    // player bots created in the lobby (no teleport)
    bool                       GLobbySpawned = false;
    int                        GGroupTeam = -1, GGroupMembers = 0;
    struct FSquadDropPlan { double EjectAt=0, LandEjectAt=0; bool HasTarget=false; FVector Target{}, LootArea{}; };
    std::unordered_map<int32, FSquadDropPlan> GSquadDropPlans;
    bool                       GModeApplied = false;
    int                        GBotTeam = -1;              // shared team when FightEachOther=0
    UFunction* GEndMatchFuncs[8] = {};     // every UFunction named ReadyToEndMatch
    int                        GNumEndMatchFuncs = 0;
    UFunction* GFinishFuncs[8] = {};       // EndMatch / FinishMatch - blocked while players + bots live
    int                        GNumFinishFuncs = 0;
    double                     GLastNoHoldLog = 0;
    // v14: native match-end guard + call trace
    UFunction* GFnEndMatch = nullptr;      // AGameMode::EndMatch
    UFunction* GFnFinishMatch = nullptr;   // ATslGameMode::FinishMatch
    UFunction* GFnBroadcastDeath = nullptr;
    UFunction* GFnOnSetState[4] = {};      // every K2_OnSetMatchState
    int                        GNumOnSetState = 0;
    UFunction* GResultFuncs[12] = {};      // client "match result" RPCs
    int                        GNumResultFuncs = 0;
    double                     GLastResultBlockLog = 0;
    double                     GLastHumanAliveAt = 0;
    double                     GNoHumansSince = 0;         // match running with no living real player since then
    double                     GExitAt = 0;                // close the server process at this time (after the match ended)
    bool                       GMatchOver = false;
    int                        GNatAlive = -1, GNatTeams = -1, GNatJoined = -1; // what the GAME wrote
    int                        GNatAliveLogged = -2, GNatTeamsLogged = -2;
    int                        GLastWroteAlive = -1, GLastWroteTeams = -1;
    std::unordered_set<int32>  GDressed;                   // character object indices already dressed
    std::unordered_map<int32, double> GPlayerDressFirstSeen;
    std::unordered_set<int32>  GAppearanced;               // bot pawns that got an appearance
    std::vector<std::vector<uint8>> GDonorLooks[2];       // InstanceDescriptor blobs from real players, per gender
    std::unordered_set<int32>  GDonorChars;                // characters already used as donors
    double                     GLookWarnAt = 0;
    int                        GStartPlayers = 0, GStartTeams = 0; // players + bots at the start (for "#rank/N")
    UFunction* GFnGameEnded = nullptr;     // PlayerController.ClientGameEnded
    UFunction* GFnRoundEnd = nullptr;      // TslPlayerController.ClientSendRoundEndEvent
    bool                       GTraceDumped = false;
    bool                       GEndPending = false;        // we held an end - finish it ourselves once it's decided
    double                     GEndDecidedAt = 0;
    int                        GHumansAlive = 0, GHumanTeamsAlive = 0, GBotsAlive = 0, GTeamsAlive = 0, GHumanConvertedZombies = 0;
    int                        GLastPublishedZombieCount = -1;
    ATslGameState* GCountGS = nullptr;  int32 GCountGSIndex = -1;
    bool                       GHaveCounts = false, GCountsInMatch = false;
    int32                      GWantJoined = 0, GWantAlive = 0, GWantTeams = 0;
    double                     GLastVetoLog = 0;
    std::vector<int32>         GPlanesSeen;                // aircraft we already logged / fixed
    double                     GNextPlaneCheck = 0;
    bool                       GChuteAltLogged = false;
    int                        GVetoCount = 0;
    unsigned                   GJumpSide = 0;
    double                     GNextSkyShoreSearch = 0;
    double                     GNextBotTerrainTrace = 0;
    struct FLootRef { UDroppedItemInteractionComponent* Component; int32 Index, Serial; FVector Location; };
    struct FDoorRef { UDoorInteractionComponent* Component; int32 Index, Serial; FVector Location; };
    struct FZoneVehicle { ATslWheeledVehicle* Vehicle; int32 Index, Serial; };
    std::vector<FZoneVehicle> GZoneVehicles;
    std::unordered_map<int32, int32> GZoneVehicleIndices;
    struct FZoneBoat { ATslFloatingVehicle* Vehicle; int32 Index, Serial; };
    std::vector<FZoneBoat> GZoneBoats;
    std::unordered_map<int32, int32> GZoneBoatIndices;
    std::vector<FDoorRef> GDoors;
    std::unordered_map<int32, int32> GDoorIndices;
    std::unordered_map<uint64, std::vector<size_t>> GDoorCells;
    std::unordered_map<int32, double> GDoorRetryAfter;
    bool GDoorLayoutChecked = false, GDoorLayoutSafe = false;
    int GDoorStateOffset = -1, GDoorDispatchLogs = 0;
    bool GDoorNativeChecked = false, GDoorNativeSafe = false;
    int GDoorProbeBudget=6;
    std::vector<FLootRef> GLoot;
    std::unordered_map<uint64, std::vector<size_t>> GLootCells;
    std::unordered_map<int32, int32> GLootIndices;
    int32 GLootScanIndex = 0, GLootTailIndex = -1;
    int32 GLootPriorityIndex = -1, GLootPrioritySnapshot = 0;
    double GNextLootSiteRefresh = 0, GNextLootCacheStatus=0;
    double GLootRescanAt = 0;
    bool GLootLayoutChecked = false, GLootLayoutSafe = false;
    int GZoneClosingsStarted = 0;
    bool GZoneReleaseObserved = false, GZonePreviousRelease = false;
    bool GZoneSpeedLayoutChecked = false, GZoneSpeedLayoutSafe = false;
    // A cluster of nearby ground-loot components is a practical target for
    // a settlement even on maps with no ATslBuilding actors in memory.
    std::unordered_map<uint64, int> GLootClusterCounts;
    double GNextSkyTargetSearch = 0;
    std::vector<FLootRef> GLootLandingSites;
    // Reserve landing areas during the aircraft phase so that bots do not
    // all pick the same nearest loot cluster. Reset together with the match.
    std::unordered_map<uint64, std::vector<FVector>> GReservedLandingCells;
    std::vector<FVector>       GBuildingTargets;
    std::unordered_set<int32>  GBuildingIndices;
    int32                      GBuildingScanIndex = 0;
    double                     GBuildingRescanAt = 0;

    bool IsTrainingWorld(UWorld* World)
    {
        return World && World->AuthorityGameMode &&
            World->AuthorityGameMode->IsA(ATslTrainingGameMode::StaticClass());
    }

    // 2017-style registry check adapted to the 2019 object array. The saved
    // index is used without dereferencing the possibly destroyed object.
    bool LiveBotObject(const UObject* Object, int32 Index)
    {
        if (!Object || Index < 0) return false;
        TUObjectArray* Array = UObject::GObjects.GetTypedPtr();
        if (!Array || Index >= Array->Num()) return false;
        FUObjectItem* Items = Array->GetDecrytedObjPtr();
        if (!Items || Items[Index].Object != Object) return false;
        const int32 Flags = *reinterpret_cast<const int32*>(&Items[Index].Pad_8[0]);
        return (Flags & ((1 << 29) | (1 << 28))) == 0;
    }

    int32 BotTeamNumber(const FBot& Bot)
    {
        if (S.TeamSize <= 1 || !LiveBotObject(Bot.Ctrl,Bot.CtrlIndex)) return 0;
        auto* PS=Bot.Ctrl->PlayerState;
        if (!PS || !PS->IsA(ATslPlayerState::StaticClass())) return 0;
        const int32 Team=static_cast<ATslPlayerState*>(PS)->TeamNumber;
        return Team>0 ? Team : 0;
    }
    bool IsSquadmate(const FBot& Bot, ATslCharacter* Other)
    {
        const int32 Team=BotTeamNumber(Bot);
        if (!Team || !Other) return false;
        auto* PS=Other->GetTslPlayerState();
        return PS && PS->TeamNumber==Team;
    }

    // Weak references use -1 for an empty object. The generated SDK Get()
    // indexes without checking it, which caused the captured Vikendi crash.
    UObject* SafeWeakGet(const FWeakObjectPtr& Ref)
    {
        TUObjectArray* Array = UObject::GObjects.GetTypedPtr();
        if (!Array || Ref.ObjectIndex < 0 || Ref.ObjectIndex >= Array->Num()) return nullptr;
        FUObjectItem* Items = Array->GetDecrytedObjPtr();
        if (!Items) return nullptr;
        const FUObjectItem& Item = Items[Ref.ObjectIndex];
        const int32 Serial = *reinterpret_cast<const int32*>(&Item.Pad_8[8]);
        if (Ref.ObjectSerialNumber <= 0 || Serial != Ref.ObjectSerialNumber) return nullptr;
        const int32 Flags = *reinterpret_cast<const int32*>(&Item.Pad_8[0]);
        if (!Item.Object || (Flags & ((1 << 29) | (1 << 28)))) return nullptr;
        return Item.Object;
    }

    AAIController* AIC(const FBot& B)
    {
        return B.IsPC ? nullptr : static_cast<AAIController*>(B.Ctrl);
    }

    // Player bots are removed through the engine (logout), AI bots are just destroyed
    void DestroyBotController(FBot& B)
    {
        if (!LiveBotObject(B.Ctrl, B.CtrlIndex))
            return;

        if (B.IsPC) {
            UGameplayStatics::RemovePlayer(static_cast<APlayerController*>(B.Ctrl), false);
        }
        else {
            B.Ctrl->K2_DestroyActor();
        }
    }

    // =================================================================================
    //  World helpers
    // =================================================================================
    bool TraceDown(UWorld* World, float X, float Y, float TopZ, float BottomZ, FVector& Out)
    {
        FHitResult Hit{};
        TArray<AActor*> Ignore;
        FLinearColor C{};
        bool bHit = UKismetSystemLibrary::LineTraceSingle(World, V(X, Y, TopZ), V(X, Y, BottomZ),
            ETraceTypeQuery::TraceTypeQuery1, false, Ignore, EDrawDebugTrace::None, &Hit, true, C, C, 0.f);
        if (!bHit || !Hit.bBlockingHit)
            return false;
        if (Hit.ImpactNormal.Z < 0.6f) // wall / cliff
            return false;
        Out = V(Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Hit.ImpactPoint.Z);
        return true;
    }

    // Test-only terrain measurement: ignore characters and reject any hit
    // that begins inside an object or is not a stationary world surface.
    bool TraceSkyGround(UWorld* World, ATslCharacter* Me, const FVector& Here, FVector& Out)
    {
        std::vector<AActor*> Ignored;
        Ignored.reserve(GBots.size() + GChars.size() + 1);
        Ignored.push_back(Me);
        for (const FBot& Bot : GBots)
            if (Bot.Pawn != Me && LiveBotObject(Bot.Pawn, Bot.PawnIndex)) Ignored.push_back(Bot.Pawn);
        for (const FCharInfo& Char : GChars)
            if (Char.Char != Me && LiveBotObject(Char.Char, Char.Index)) Ignored.push_back(Char.Char);
        TArray<AActor*> Ignore(Ignored.data(), (int32)Ignored.size(), (int32)Ignored.size());
        FHitResult Hit{}; FLinearColor Color{};
        bool Found = UKismetSystemLibrary::LineTraceSingle(World, V(Here.X, Here.Y, Here.Z - 150.f),
            V(Here.X, Here.Y, -30000.f), ETraceTypeQuery::TraceTypeQuery1, false, Ignore,
            EDrawDebugTrace::None, &Hit, true, Color, Color, 0.f);
        auto* Component = Found ? static_cast<UPrimitiveComponent*>(SafeWeakGet(Hit.Component)) : nullptr;
        if (!Found || !Hit.bBlockingHit || Hit.bStartPenetrating || Hit.ImpactNormal.Z < 0.6f || !Component ||
            Component->Mobility != EComponentMobility::Static) return false;
        Out = V(Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Hit.ImpactPoint.Z);
        return true;
    }

    FVector GroundAt(UWorld* World, const FVector& P)
    {
        FVector G;
        if (TraceDown(World, P.X, P.Y, P.Z + 3000.f, P.Z - 6000.f, G))
            return G;
        return P;
    }

    // Scan a small batch per AI tick so collecting map buildings does not
    // stall the server during the plane sequence. Store locations only.
    void ScanBuildingTargets(double T)
    {
        if (GBuildingTargets.size() >= 4000 || T < GBuildingRescanAt) return;
        TUObjectArray* Array = UObject::GObjects.GetTypedPtr();
        UClass* Building = ATslBuilding::StaticClass();
        UClass* Modular = ATslModularBuilding::StaticClass();
        UClass* Spot = AItemSpot::StaticClass();
        if (!Array || (!Building && !Modular && !Spot)) return;
        const int32 Count = Array->Num();
        for (int n = 0; n < 1000 && GBuildingScanIndex < Count; ++n, ++GBuildingScanIndex)
        {
            UObject* O = Array->GetByIndex(GBuildingScanIndex);
            if (!O || !LiveBotObject(O,GBuildingScanIndex) || GBuildingIndices.count(GBuildingScanIndex) || O->IsDefaultObject()) continue;
            if ((!Building || !O->IsA(Building)) && (!Modular || !O->IsA(Modular)) && (!Spot || !O->IsA(Spot))) continue;
            AActor* Actor = static_cast<AActor*>(O);
            const FVector P = Actor->K2_GetActorLocation();
            if (std::isfinite(P.X) && std::isfinite(P.Y) && std::isfinite(P.Z) &&
                P.X > 0.f && P.Y > 0.f && P.X < 900000.f && P.Y < 900000.f && P.Z > -5000.f)
            { GBuildingTargets.push_back(P); GBuildingIndices.insert(GBuildingScanIndex); }
        }
        if (GBuildingScanIndex >= Count) {
            GBuildingScanIndex = 0; GBuildingRescanAt = T + 30.0;
            BLOG("Landing targets cached: " + std::to_string(GBuildingTargets.size()));
        }
    }

    // Lowest water surface a bot has been found in (the ocean). Ground below it + margin = sea floor.
    float GSeaZ = -1e9f;
    float SeaZ() { return S.SeaLevelZ > -1e8f ? S.SeaLevelZ : GSeaZ; }
    bool  BelowSea(float GroundZ) { const float Sz = SeaZ(); return Sz > -1e8f && GroundZ < Sz + 150.f; }

    // ground height that counts as water: learned sea level if we have it, otherwise a safe guess
    bool IsWaterGround(float GroundZ)
    {
        const float Sz = SeaZ();
        return Sz > -1e8f ? GroundZ < Sz + 150.f : GroundZ < 250.f;
    }

    // ground at (X,Y): trace starts at 1000 m so it can't hit the plane (which flies at 1500 m)
    bool TraceLand(UWorld* World, float X, float Y, FVector& G)
    {
        return TraceDown(World, X, Y, 100000.f, -30000.f, G) && G.Z < 80000.f;
    }

    // land within Radius of (X,Y), widening the search if needed
    // land with no water within ~40 m around it (not a beach, pier or islet)
    bool DrySpot(UWorld* World, const FVector& G)
    {
        for (int i = 0; i < 6; ++i)
        {
            const float A = i * (kPi / 3.f);
            FVector H;
            if (TraceLand(World, G.X + std::cos(A) * 4000.f, G.Y + std::sin(A) * 4000.f, H) && IsWaterGround(H.Z))
                return false;
        }
        return true;
    }

    bool LiveLoot(const FLootRef& L);
    bool FindLootForJump(UWorld* World, const FVector& ExitPos, const FVector& Side, float MaxDistance, float MinSpacing, FVector& Out, FVector& LootArea)
    {
        // Each site comes from a cell with at least three real dropped items.
        // Keep a bounded shortlist before doing validity checks and traces.
        // Penalize previously selected neighbourhoods, while still preferring
        // reachable sites instead of forcing a bot across the whole map.
        std::vector<std::pair<float, size_t>> Candidates;
        for (size_t i=0; i<GLootLandingSites.size(); ++i) {
            const FVector D=Flat(GLootLandingSites[i].Location-ExitPos);
            const float Distance=Len(D);
            if (Distance<6000.f || Distance>MaxDistance) continue;
            const float Alignment=SafeNormal(D).Dot(Side);
            const int32 CX=(int32)std::floor(GLootLandingSites[i].Location.X/15000.f);
            const int32 CY=(int32)std::floor(GLootLandingSites[i].Location.Y/15000.f);
            float CrowdPenalty=0.f;
            bool TooClose=false;
            for (int DX=-1; DX<=1; ++DX) for (int DY=-1; DY<=1; ++DY) {
                const auto It=GReservedLandingCells.find(LootSpatial::Key(CX+DX,CY+DY));
                if (It==GReservedLandingCells.end()) continue;
                for (const FVector& Reserved : It->second) {
                    const float Gap=Len(Flat(GLootLandingSites[i].Location-Reserved));
                    if (Gap<MinSpacing) TooClose=true;
                    if (Gap<15000.f) CrowdPenalty+=30000.f*(1.f-Gap/15000.f);
                }
            }
            if (TooClose) continue;
            // Score the actual distance to each reservation. Counting whole
            // adjacent grid cells made distinct sites look equally crowded.
            const float Score=Distance*(1.f+0.2f*(1.f-Alignment)) + CrowdPenalty;
            auto At=std::lower_bound(Candidates.begin(),Candidates.end(),Score,
                [](const auto& A,float Value){return A.first<Value;});
            Candidates.insert(At,{Score,i});
            if (Candidates.size()>24) Candidates.pop_back();
        }
        const size_t N=Candidates.size();
        const size_t Start=N ? (size_t)RandI(0,(int)(std::min)(N,size_t(3))-1) : 0;
        for (size_t i=0; i<N; ++i) {
            const auto& Site=GLootLandingSites[Candidates[(Start+i)%N].second];
            if (!LiveLoot(Site) || !Site.Component->Item) continue;
            const FVector Towards=SafeNormal(Flat(Site.Location-ExitPos));
            // Aim beside the loot building rather than blindly at its roof.
            for (float Offset : {1000.f,2000.f}) {
                const FVector P=Site.Location-Towards*Offset;
                FVector Ground{};
                if (TraceLand(World,P.X,P.Y,Ground) && !IsWaterGround(Ground.Z) &&
                    std::fabs(Ground.Z-Site.Location.Z)<350.f) { Out=Ground; LootArea=Site.Location; return true; }
            }
        }
        return false;
    }

    void SetAircraftLootPlan(UWorld* World, FBot& B, ATransportAircraftVehicle* Plane, double T)
    {
        if (B.HasSkyTarget) return;
        const int32 Team=BotTeamNumber(B);
        if (Team) {
            auto It=GSquadDropPlans.find(Team);
            if (It!=GSquadDropPlans.end() && It->second.HasTarget) {
                B.SkyTarget=It->second.Target; B.LootArea=It->second.LootArea;
                B.HasSkyTarget=B.SkyTargetIsLoot=B.SkyDirectionSet=B.HasLootArea=true; B.LootAreaUntil=0;
                const float UntilExit=(float)(std::clamp)(B.EjectAt-T,0.0,8.0);
                const FVector Exit=Plane->K2_GetActorLocation()+Plane->GetVelocity()*UntilExit;
                const FVector D=Flat(B.SkyTarget-Exit); B.SkyHeading=std::atan2(D.Y,D.X)*180.f/kPi;
                BLOG("SQUAD DROP " + B.Name + " team=" + std::to_string(Team) + " shared landing"); return;
            }
        }
        if (B.SkyPlanSide==0.f) B.SkyPlanSide=Team ? ((Team & 1) ? -1.f : 1.f) : ((GJumpSide++ & 1u) ? -1.f : 1.f);
        const FVector Forward=SafeNormal(Flat(Plane->GetActorForwardVector()));
        const FVector Side=V(-Forward.Y*B.SkyPlanSide,Forward.X*B.SkyPlanSide,0.f);
        const float UntilExit=(float)(std::clamp)(B.EjectAt-T,0.0,8.0);
        const FVector ExitPos=Plane->K2_GetActorLocation()+Plane->GetVelocity()*UntilExit;
        // If the assigned side has no usable site, try the other side before
        // leaving the plane. The chosen heading stays fixed after the jump.
        if (FindLootForJump(World,ExitPos,Side,30000.f,10000.f,B.SkyTarget,B.LootArea) ||
            FindLootForJump(World,ExitPos,Side,45000.f,10000.f,B.SkyTarget,B.LootArea) ||
            (T>=B.EjectAt && (FindLootForJump(World,ExitPos,Side,30000.f,0.f,B.SkyTarget,B.LootArea) ||
                              FindLootForJump(World,ExitPos,Side,90000.f,0.f,B.SkyTarget,B.LootArea)))) {
            B.HasSkyTarget=true; B.SkyTargetIsLoot=true; B.SkyDirectionSet=true;
            B.HasLootArea=true; B.LootAreaUntil=0;
            const int32 CX=(int32)std::floor(B.LootArea.X/15000.f), CY=(int32)std::floor(B.LootArea.Y/15000.f);
            GReservedLandingCells[LootSpatial::Key(CX,CY)].push_back(B.LootArea);
            const FVector D=Flat(B.SkyTarget-ExitPos);
            B.SkyHeading=std::atan2(D.Y,D.X)*180.f/kPi;
            if (Team) { auto& Plan=GSquadDropPlans[Team]; Plan.Target=B.SkyTarget; Plan.LootArea=B.LootArea; Plan.HasTarget=true; }
            BLOG("SKY PLAN " + B.Name + " aircraft loot-site=" + Pos(B.SkyTarget) +
                " predicted_exit=" + Pos(ExitPos));
        }
    }

    bool FindSkyShore(UWorld* World, const FVector& Here, FVector& Out)
    {
        const float Radii[] = {10000.f, 30000.f, 60000.f};
        for (float Radius : Radii) for (int i = 0; i < 8; ++i) {
            const float A = (float)i * (kPi / 4.f); FVector G;
            if (TraceLand(World, Here.X + std::cos(A)*Radius, Here.Y + std::sin(A)*Radius, G) && !IsWaterGround(G.Z)) { Out = G; return true; }
        }
        return false;
    }

    bool FindLandNear(UWorld* World, float X, float Y, FVector& Out)
    {
        FVector G;
        if (TraceLand(World, X, Y, G) && !IsWaterGround(G.Z) && DrySpot(World, G)) { Out = G; return true; }
        {
            const float Radii[] = { 15000.f, 40000.f, 90000.f };
            for (float R : Radii)
                for (int i = 0; i < 12; ++i)
                {
                    const float A = RandF(0.f, 2.f * kPi), D = RandF(R * 0.3f, R);
                    if (TraceLand(World, X + std::cos(A) * D, Y + std::sin(A) * D, G) && !IsWaterGround(G.Z) && DrySpot(World, G))
                    {
                        Out = G; return true;
                    }
                }
        }
        const float Radii[] = { 15000.f, 40000.f, 90000.f };
        for (float R : Radii)
            for (int i = 0; i < 16; ++i)
            {
                const float A = RandF(0.f, 2.f * kPi), D = RandF(R * 0.3f, R);
                const float PX = X + std::cos(A) * D, PY = Y + std::sin(A) * D;
                if (TraceLand(World, PX, PY, G) && !IsWaterGround(G.Z)) { Out = G; return true; }
            }
        return false;
    }

    bool FindRandomSpawn(UWorld* World, FVector& Out)
    {
        for (int Attempt = 0; Attempt < 40; ++Attempt)
        {
            const float X = RandF(S.SpawnMinX, S.SpawnMaxX);
            const float Y = RandF(S.SpawnMinY, S.SpawnMaxY);
            FVector G;
            if (!TraceLand(World, X, Y, G))
                continue;
            if (G.Z < S.MinSpawnZ || BelowSea(G.Z))
                continue;
            Out = G;
            return true;
        }
        return false;
    }

    bool IsBotChar(const APawn* C)
    {
        for (const FBot& B : GBots)
            if (B.Pawn == C) return true;
        return false;
    }

    bool Readable(const void* P, size_t N);
    using FZombieTakeDamage = float(__fastcall*)(AActor*,float,const FDamageEvent&,AController*,AActor*);
    static FZombieTakeDamage GZombieTakeDamage=nullptr;
    float __fastcall ZombieTakeDamage(AActor* Actor,float Damage,const FDamageEvent& Event,AController* Instigator,AActor* Causer)
    {
        if (Actor && Actor->IsA(ATslCharacter::StaticClass()) && IsTrainingWorld(UWorld::GetWorld()) &&
            Instigator && Instigator->IsA(APlayerController::StaticClass())) {
            auto* Victim=static_cast<ATslCharacter*>(Actor);
            if (Victim->Controller && Victim->Controller->IsA(APlayerController::StaticClass()) && Victim->Controller!=Instigator) return 0.f;
        }
#ifdef PUBG2019_ENABLE_DIAGNOSTICS
        if (Actor && Actor==GServerOnlyZombieTarget) return 0.f;
        char ProtectTestHuman[4]{};
        if (Actor && !IsBotChar(static_cast<APawn*>(Actor)) &&
            GetEnvironmentVariableA("PUBG2019_ZOMBIE_PROTECT_TEST_HUMAN",ProtectTestHuman,sizeof(ProtectTestHuman)) && ProtectTestHuman[0]=='1' &&
            Actor->IsA(ATslCharacter::StaticClass())) {
            auto* Pawn=static_cast<ATslCharacter*>(Actor);
            if (Pawn->Controller && Pawn->Controller->IsA(APlayerController::StaticClass())) {
                static bool Logged=false;
                if (!Logged) {Logged=true;BLOG("[ZOMBIE TEST] Diagnostic: test player damage suppressed for late-zone observation");}
                return 0.f;
            }
        }
#endif
        if (Actor && Actor->IsA(ATslCharacter::StaticClass()) && IsBotChar(static_cast<APawn*>(Actor)) && static_cast<ATslCharacter*>(Actor)->GetTslPlayerState() && static_cast<ATslCharacter*>(Actor)->GetTslPlayerState()->IsA(ATslZombiePlayerState::StaticClass()) && static_cast<ATslZombiePlayerState*>(static_cast<ATslCharacter*>(Actor)->GetTslPlayerState())->GetZombieModePlayerState()==EZombieModePlayerState::Zombie) {
            UClass* Type=Event.DamageTypeClass;
            if (Type && Readable(Type,sizeof(UClass))) {
                std::string Name=Type->GetName();
                std::transform(Name.begin(),Name.end(),Name.begin(),[](unsigned char C){return (char)std::tolower(C);});
                bool Blue=Name.find("bluezone")!=std::string::npos || Name.find("blue_zone")!=std::string::npos ||
                    Name.find("poison")!=std::string::npos || Name.find("gaszone")!=std::string::npos || Name.find("gas_zone")!=std::string::npos;
                auto* Character=static_cast<ATslCharacter*>(Actor);
                auto* Default=Type->DefaultObject;
                if (!Blue && !Instigator && !Causer && Default && Readable(Default,sizeof(UTslDamageType)) &&
                    Default->IsA(UTslDamageType::StaticClass()) && Type!=Character->GroggyDamageType.Get() && Type!=Character->BleedOutDamageType.Get()) {
                    const int Category=(int)static_cast<UTslDamageType*>(Default)->DamageTypeCategory;
                    Blue=Category==(int)EDamageTypeCategory::Damage_BlueZone;
                }
                if (Blue) {
                    static unsigned Count=0; ++Count;
                    if (Count<=3 || (Count&(Count-1))==0) BLOG("[ZOMBIE TEST] AI zombie blue-zone damage suppressed type="+Name+" count="+std::to_string(Count));
                    return 0.f;
                }
            }
        }
        return GZombieTakeDamage ? GZombieTakeDamage(Actor,Damage,Event,Instigator,Causer) : 0.f;
    }

    void InstallZombieDamageGuard(ATslCharacter* Character)
    {
        if (GZombieTakeDamage || !Character || !Readable(Character,sizeof(void*))) return;
        auto** VTable=*reinterpret_cast<void***>(Character);
        if (!VTable || !Readable(VTable,0x588)) return;
        void* Target=VTable[0x580/sizeof(void*)];
        if (!Target || MH_CreateHook(Target,reinterpret_cast<void*>(&ZombieTakeDamage),reinterpret_cast<void**>(&GZombieTakeDamage))!=MH_OK) return;
        if (MH_EnableHook(Target)!=MH_OK) { MH_RemoveHook(Target); GZombieTakeDamage=nullptr; return; }
        BLOG("[ZOMBIE TEST] AI zombie blue-zone damage guard installed");
    }

    void GatherCharacters(UWorld* World)
    {
        GChars.clear();
        int SnapshotPawns = 0;

        if (!World)
            return;

        UClass* CharClass = ATslCharacter::StaticClass();

        ATslGameMode* GM = (ATslGameMode*)UGameplayStatics::GetGameMode(UWorld::GetWorld());
        if (GM)
        {
            static Params::TslGameMode_GetAllPawns PawnQuery{};
            auto* Fn = GM->GetClass()->GetFunction("TslGameMode", "GetAllPawns");
            if (!Fn) return;
            // The native GetAllPawns implementation APPENDS to OutPawns.
            // Reuse its engine allocation, but start a fresh snapshot each time.
            // Without this reset it grows past the 4096 guard and all sight dies.
            PawnQuery.OutPawns.Clear();
            GM->ProcessEvent(Fn, &PawnQuery);
            const auto& Pawns = PawnQuery.OutPawns;
            SnapshotPawns = Pawns.Num();
            if (Pawns.Num()>0 && Pawns.Num()<=4096 && Pawns.IsValid())
            {
                auto* LocalGhost = UGameplayStatics::GetPlayerCharacter(World, 0);
                for (APawn* O : Pawns)
                {
                    if (O && LiveBotObject(O, O->GetIndex()) && O->IsA(CharClass))
                    {
                        ATslCharacter* C = static_cast<ATslCharacter*>(O);
                        if (IsTrainingWorld(World)) {
                            // Training players are invulnerable; this also covers damage
                            // paths which do not consult the game-mode damage hook.
                            C->bCanBeDamaged=false;
                            InstallZombieDamageGuard(C);
                        }
                        if (C->Health <= 0.f) continue;

                        if (C == LocalGhost) continue;

                        const bool bBot = IsBotChar(C);
                        if (bBot && C->bHidden) {
                            continue;   // still "skydiving"
                        }

                        GChars.push_back({ C, C->GetIndex(), C->K2_GetActorLocation(), bBot });
                    }
                    else
                    {
                        continue;
                    }
                }
            }
            else
            {
                return;
            }
        }
        else
        {
            return;
        }

        // Classify the existing pawn snapshot once a second, not once per bot.
        const double ZombieSnapshotNow = Now();
        if (ZombieSnapshotNow >= GNextZombieHumanSnapshot) {
            GNextZombieHumanSnapshot = ZombieSnapshotNow + 1.0;
            GZombieHumans.clear();
            if (World->AuthorityGameMode && World->AuthorityGameMode->IsA(ATslZombieGameMode::StaticClass())) {
                for (const auto& Info : GChars) {
                    if (Info.IsBot || !LiveBotObject(Info.Char, Info.Index) || Info.Char->Health <= 0.f) continue;
                    auto* State = Info.Char->GetTslPlayerState();
                    if (State && State->IsA(ATslZombiePlayerState::StaticClass()) &&
                        static_cast<ATslZombiePlayerState*>(State)->GetZombieModePlayerState() == EZombieModePlayerState::Human)
                        GZombieHumans.push_back(Info);
                }
            }
        }

        static ULONGLONG NextSnapshotLog = 0;
        const ULONGLONG Stamp = GetTickCount64();
        if (S.CombatLog && GMatchSpawnDone && Stamp >= NextSnapshotLog) {
            NextSnapshotLog = Stamp + 15000;
            int Humans = 0;
            for (const auto& C : GChars) if (!C.IsBot) ++Humans;
            BLOG("BOT SIGHT snapshot characters=" + std::to_string(GChars.size()) +
                " humans=" + std::to_string(Humans) + " pawns=" +
                std::to_string(SnapshotPawns));
        }
    }

    // Remote client first. The listen server always has its own local player pawn (the "ghost"),
    // which we only fall back to if nobody else is connected.
    ATslCharacter* FindRealPlayer(UWorld* World)
    {
#ifdef PUBG2019_ENABLE_DIAGNOSTICS
        char ServerOnly[4]{};
        if (GetEnvironmentVariableA("PUBG2019_ZOMBIE_SERVER_ONLY_PROBE",ServerOnly,sizeof(ServerOnly)) && ServerOnly[0]=='1' &&
            GServerOnlyZombieTarget && Readable(GServerOnlyZombieTarget,sizeof(void*)) && GServerOnlyZombieTarget->Health>0.f)
            return GServerOnlyZombieTarget;
#endif
        if (!World || !World->AuthorityGameMode) return nullptr;
        AGameStateBase* GS = World->AuthorityGameMode->GameState;
        if (!GS) return nullptr;
        for (APlayerState* PS : GS->PlayerArray)
        {
            if (!PS || PS->bIsABot) continue;
            auto* Owner = PS->GetOwner();
            if (!Owner || !Owner->IsA(APlayerController::StaticClass())) continue;
            auto* PC = static_cast<APlayerController*>(Owner);
            if (!PC->NetConnection || PC->bIsLocalPlayerController) continue;
            auto* Pawn = PC->K2_GetPawn();
            if (!Pawn || !Pawn->IsA(ATslCharacter::StaticClass()) || IsBotChar(static_cast<ATslCharacter*>(Pawn))) continue;
            auto* Character = static_cast<ATslCharacter*>(Pawn);
            if (Character->Health > 0.f) return Character;
        }
        return nullptr;
    }

    ATslCharacter* FindZombieHumanTarget(FBot& Bot, double T)
    {
        if (T < Bot.NextZombieTargetCheck && LiveBotObject(Bot.ZombieTarget, Bot.ZombieTargetIndex) && Bot.ZombieTarget->Health > 0.f)
            return Bot.ZombieTarget;
        Bot.NextZombieTargetCheck = T + 1.0;
        Bot.ZombieTarget = nullptr; Bot.ZombieTargetIndex = -1;
        if (!LiveBotObject(Bot.Pawn, Bot.PawnIndex)) return nullptr;
        const FVector Here = Bot.Pawn->K2_GetActorLocation();
        float BestDistanceSquared = 3.402823466e+38f;
        for (const auto& Candidate : GZombieHumans) {
            if (Candidate.IsBot || Candidate.Char == Bot.Pawn || !LiveBotObject(Candidate.Char, Candidate.Index) || Candidate.Char->Health <= 0.f) continue;
            const FVector Delta = Candidate.Loc - Here;
            const float D = Delta.X*Delta.X + Delta.Y*Delta.Y + Delta.Z*Delta.Z;
            if (std::isfinite(D) && D < BestDistanceSquared) {
                BestDistanceSquared = D; Bot.ZombieTarget = Candidate.Char; Bot.ZombieTargetIndex = Candidate.Index;
            }
        }
        return Bot.ZombieTarget;
    }

    // a connected client (not the server's own local pawn)
    bool HasRemotePlayer(UWorld* World)
    {
        return FindRealPlayer(World) != nullptr;
    }

    void CacheBones()
    {
        if (GBonesCached)
            return;
        GHeadBone = UKismetStringLibrary::Conv_StringToName(FString(L"head"));
        GSpineBone = UKismetStringLibrary::Conv_StringToName(FString(L"spine_02"));
        GBonesCached = true;
    }

    // =================================================================================
    //  Weapons
    // =================================================================================
    int32 ObjectSerial(int32 Index)
    {
        const FUObjectItem* Item = GetItem(Index);
        return Item ? *reinterpret_cast<const int32*>(&Item->Pad_8[8]) : 0;
    }

    bool LiveWeaponChoice(const FWeaponChoice& W)
    {
        // These class pointers can outlive a map-loaded asset. Never read a
        // cached pointer until its object-array entry and generation match.
        if (!LiveBotObject(W.ItemClass, W.ItemIndex) ||
            !LiveBotObject(W.WeaponClass, W.WeaponIndex)) return false;
        if (ObjectSerial(W.ItemIndex) != W.ItemSerial ||
            ObjectSerial(W.WeaponIndex) != W.WeaponSerial) return false;
        UClass* ClassClass = UClass::StaticClass();
        return ClassClass && W.ItemClass->IsA(ClassClass) && W.WeaponClass->IsA(ClassClass) &&
            W.ItemClass->GetName() == W.ItemName && W.WeaponClass->GetName() == W.WeaponName;
    }

    void ScanWeapons()
    {
        GWeapons.clear();

        UClass* WeaponItemCls = UWeaponItem::StaticClass();
        UClass* GunCls = ATslWeapon_Gun::StaticClass();

        if (!WeaponItemCls || !GunCls)
            return;

        const int32 Num = UObject::GObjects->Num();
        for (int32 i = 0; i < Num; ++i)
        {
            UObject* O = UObject::GObjects->GetByIndex(i);

            if (!O || !O->IsDefaultObject() || !O->IsA(WeaponItemCls)) continue;

            UWeaponItem* WI = static_cast<UWeaponItem*>(O);
            // Prefer concrete non-toy item templates, not the generic base item.
            if (WI->bIsToy || O->GetClass() == WeaponItemCls) continue;
            UClass* WC = WI->WeaponType;

            if (!WC || !WC->IsSubclassOf(GunCls))
                continue;

            UClass* IC = O->GetClass();
            if (BotWeaponPolicy::IsCarePackageWeapon(IC->GetName()) ||
                BotWeaponPolicy::IsCarePackageWeapon(WC->GetName())) continue;
            const int32 ItemIndex = IC->GetIndex(), WeaponIndex = WC->GetIndex();
            GWeapons.push_back({ IC, WC, IC->GetName(), WC->GetName(),
                ItemIndex, WeaponIndex, ObjectSerial(ItemIndex), ObjectSerial(WeaponIndex) });
        }

        BLOG("Found " + std::to_string(GWeapons.size()) + " gun item classes (type 'weapons' to list them)");
    }

    const FWeaponChoice* PickWeapon()
    {
        const auto Before = GWeapons.size();
        GWeapons.erase(std::remove_if(GWeapons.begin(), GWeapons.end(),
            [](const FWeaponChoice& W) { return !LiveWeaponChoice(W); }), GWeapons.end());
        if (GWeapons.size() != Before)
            BLOG("Discarded " + std::to_string(Before - GWeapons.size()) + " stale gun class entries");
        if (GWeapons.empty())
            ScanWeapons();
        if (GWeapons.empty())
            return nullptr;

        std::vector<const FWeaponChoice*> Pool;
        const std::vector<std::string> Wanted = SplitList(S.Weapon);
        if (!Wanted.empty())
        {
            for (const FWeaponChoice& W : GWeapons)
            {
                const std::string A = Lower(W.WeaponName), B = Lower(W.ItemName);
                for (const std::string& K : Wanted)
                    if (A.find(K) != std::string::npos || B.find(K) != std::string::npos) { Pool.push_back(&W); break; }
            }
        }
        if (Pool.empty())
        {
            static const char* Skip[] = { "toy", "p92", "p1911", "r1895", "pistol", "flare", "crossbow", "pan", "machete", "sickle", "crowbar" };
            for (const FWeaponChoice& W : GWeapons)
            {
                const std::string A = Lower(W.WeaponName) + "|" + Lower(W.ItemName);
                bool bSkip = false;
                for (const char* K : Skip)
                    if (A.find(K) != std::string::npos) { bSkip = true; break; }
                if (!bSkip) Pool.push_back(&W);
            }
        }
        if (Pool.empty())
            for (const FWeaponChoice& W : GWeapons) Pool.push_back(&W);
        return Pool[RandI(0, (int)Pool.size() - 1)];
    }

    // Repair only empty/placeholder bot weapon names using the corresponding
    // concrete item default object. The engine property setter owns FText copies.
    void RepairBotWeaponName(ATslWeapon_Gun* Gun)
    {
        if (!Gun) return;
        const FText Existing = Gun->GetItemName();
        const std::string Old = Existing.TextData ? UKismetTextLibrary::Conv_TextToString(Existing).ToString() : std::string();
        const std::string OldLower = Lower(Old);
        if (!Old.empty() && OldLower.find("input a short item name") == std::string::npos &&
            OldLower.find("input short item name") == std::string::npos) return;
        UClass* GunClass = Gun->GetClass();
        for (const FWeaponChoice& Choice : GWeapons) {
            if (!LiveWeaponChoice(Choice)) continue;
            if (Choice.WeaponClass != GunClass || !Choice.ItemClass || !Choice.ItemClass->DefaultObject) continue;
            auto* DefaultItem = static_cast<UItem*>(Choice.ItemClass->DefaultObject);
            if (!DefaultItem->IsA(UWeaponItem::StaticClass()) || !DefaultItem->ItemName.TextData) continue;
            const std::string NewName = UKismetTextLibrary::Conv_TextToString(DefaultItem->ItemName).ToString();
            const std::string NewLower = Lower(NewName);
            if (NewName.empty() || NewLower.find("input a short item name") != std::string::npos ||
                NewLower.find("input short item name") != std::string::npos) continue;
            UClass* WeaponClass = ATslWeapon::StaticClass();
            UClass* TextClass = UTextProperty::StaticClass();
            if (!WeaponClass || !TextClass) return;
            UProperty* Property = nullptr;
            UField* Field = WeaponClass->Children;
            for (int N = 0; Field && N < 512; ++N, Field = Field->Next) {
                if (Field->GetName() == "ItemName" && Field->IsA(TextClass)) {
                    Property = static_cast<UProperty*>(Field); break;
                }
            }
            if (!Property || Property->Offset != 0x610) return;
            UKismetSystemLibrary::SetTextPropertyByName(Gun, Property->Name, DefaultItem->ItemName);
            Gun->ForceNetUpdate();
            BLOG("Bot weapon name candidate: " + Old + " -> " + NewName);
            return;
        }
    }

    // Arming (v5)
    //  1. make sure a gun is in the character's weapon slots (GiveWeapon, then PickUpItem,
    //     then AdminSetUpWeapon - one at a time, only until a gun shows up)
    //  2. put it in the hands. Method that works is remembered for every later bot:
    //       0 ServerArmWeapon(slot, no anim)   1 ServerArmWeapon(slot, anim)
    //       2 SimulateArmWeapon + CurrentWeaponIndex   3 OnWeapon_1
    //     If none works (e.g. arming is blocked in the lobby) it retries every 5 s.
    int FindGunSlot(FBot& B, UWeaponProcessorComponent*& OutWP, bool bLog)
    {
        OutWP = B.Pawn->WeaponProcessor;
        if (!OutWP)
        {
            OutWP = nullptr;
            return -1;
        }
        const int32 Num = OutWP->EquippedWeapons.Num();
        if (Num < 0 || Num > 32 || (Num && !OutWP->EquippedWeapons.IsValid())) return -1;
        int32 GunIdx = -1;
        std::string List;
        for (int32 i = 0; i < Num; ++i)
        {
            ATslWeapon* W = OutWP->EquippedWeapons[i];
            if (!W || !LiveBotObject(W, W->GetIndex()))
                continue;
            if (bLog) List += " [" + std::to_string(i) + "]" + W->GetClass()->GetName();
            if (GunIdx < 0 && W->IsA(ATslWeapon_Gun::StaticClass()) &&
                !BotWeaponPolicy::IsCarePackageWeapon(W->GetClass()->GetName()))
                GunIdx = i;
        }
        if (bLog)
            BLOG(B.Name + " weapon slots:" + (List.empty() ? std::string(" none") : List) +
                " | in hands index " + std::to_string(OutWP->GetWeaponIndex()));
        return GunIdx;
    }

    void RefreshBotWeaponRelevance(FBot& B, ATslWeapon_Gun* Gun, double T)
    {
        if (!Gun || !B.Pawn || T < B.WeaponRelevanceCheckAt) return;
        B.WeaponRelevanceCheckAt = T + 2.0;
        // The weapon follows the character's network relevancy only when the
        // engine says that the character is its owner. Never substitute owner.
        AActor* Owner = Gun->GetOwner();
        const bool UsePawnRelevance = Owner == B.Pawn;
        const bool Changed = Gun->bOnlyRelevantToOwner ||
            bool(Gun->bNetUseOwnerRelevancy) != UsePawnRelevance || !Gun->bReplicates;
        Gun->bOnlyRelevantToOwner = false;
        Gun->bNetUseOwnerRelevancy = UsePawnRelevance;
        Gun->NetCullDistanceSquared = (std::max)(Gun->NetCullDistanceSquared, 1600000000.f);
        if (!Gun->bReplicates) Gun->SetReplicates(true);
        if (Changed) {
            Gun->FlushNetDormancy(); Gun->ForceNetUpdate(); B.Pawn->ForceNetUpdate();
            BLOG("BOT WEAPON RELEVANCE " + B.Name + " owner=" +
                (Owner ? Owner->GetClass()->GetName() : "none") +
                " pawn_owner=" + std::to_string(UsePawnRelevance));
        }
    }

    void TryArm(FBot& B, double T)
    {
        ATslCharacter* Me = B.Pawn;
        if (ATslWeapon_Gun* CurrentGun = Me->GetWeaponGun())
        {
            if (BotWeaponPolicy::IsCarePackageWeapon(CurrentGun->GetClass()->GetName())) {
                B.Armed=false; B.NextArmTry=T+5.0; return;
            }
            RepairBotWeaponName(CurrentGun);
            RefreshBotWeaponRelevance(B, CurrentGun, T);
            B.Armed = true;
            if (GWorkingArmStep < 0 && B.LastArmMethod >= 0)
            {
                GWorkingArmStep = B.LastArmMethod;
                static const char* Names[] = { "ServerArmWeapon", "ServerArmWeapon+anim", "OnWeapon_1", "OnWeapon_1" };
                BLOG(std::string("Arming works via ") + Names[GWorkingArmStep] + " - used for every bot from now on");
            }
            if (S.VerboseLog) BLOG(B.Name + " is armed");
            return;
        }

        UWeaponProcessorComponent* WP = nullptr;
        const int Slot = FindGunSlot(B, WP, S.VerboseLog);

        if (Slot < 0)
        {
            // no gun in the slots yet - give one (one method per try)
            const FWeaponChoice* W = PickWeapon();
            switch (B.GiveTries)
            {
            case 0:
            {
                UObject* Transient = UObject::FindObjectFast("Transient");
                if (W && Transient)
                {
                    UItem* Item = static_cast<UItem*>(UGameplayStatics::SpawnObject(W->ItemClass, Transient));
                    if (Item)
                    {
                        if (Item->IsA(UWeaponItem::StaticClass()))
                            static_cast<UWeaponItem*>(Item)->AmmoCount = static_cast<UWeaponItem*>(Item)->AmmoPerClip;
                        Me->PickUpItem(Item, FName{}, FString(L""));
                        if (S.VerboseLog) BLOG(B.Name + " PickUpItem(" + W->ItemName + ")");
                    }
                }
                break;
            }
            case 1:
                if (W) { Me->GiveWeapon(TSubclassOf<ATslWeapon>(W->WeaponClass)); if (S.VerboseLog) BLOG(B.Name + " fallback GiveWeapon(" + W->WeaponName + ")"); }
                break;
            case 2:
                // The opaque admin routine can choose an unrestricted weapon.
                // Retry only an explicitly filtered normal weapon instead.
                if (W) { Me->GiveWeapon(TSubclassOf<ATslWeapon>(W->WeaponClass)); }
                break;
            default:
                if (!B.ArmFailLogged)
                {
                    BLOG(B.Name + ": no gun ends up in its weapon slots - it will roam unarmed");
                    B.ArmFailLogged = true;
                }
                B.NextArmTry = T + 10.0;
                return;
            }
            B.GiveTries++;
            B.NextArmTry = T + 0.7;
            return;
        }

        // gun is in a slot - put it in the hands
        const int M = (GWorkingArmStep >= 0) ? GWorkingArmStep : (B.ArmStep % 4);
        switch (M)
        {
        case 0: WP->ServerArmWeapon(WP->WeaponArmInfo.ServerUpdateSeqNum, Slot, false, WP->GetOwner()->K2_GetActorLocation(), 0, 0); break;
        case 1: WP->ServerArmWeapon(WP->WeaponArmInfo.ServerUpdateSeqNum, Slot, true, WP->GetOwner()->K2_GetActorLocation(), 0, 0); break;
            //case 2: WP->ServerArmWeapon(Slot, WP->GetWeaponIndex(), false); WP->CurrentWeaponIndex = Slot; break;
        default: Me->OnWeapon_1(); break;
        }
        B.LastArmMethod = M;
        B.ArmStep++;
        if (S.VerboseLog) BLOG(B.Name + " arm method " + std::to_string(M) + " on slot " + std::to_string(Slot));

        if (GWorkingArmStep < 0 && B.ArmStep % 4 == 0)
        {
            if (!B.ArmFailLogged)
            {
                BLOG(B.Name + " has a gun in slot " + std::to_string(Slot) + " but none of the 4 arm methods put it in its hands"
                    " - retrying every 5 s (may be blocked until the match starts)");
                B.ArmFailLogged = true;
            }
            B.NextArmTry = T + 5.0;
        }
        else
        {
            B.NextArmTry = T + 0.7;
        }
    }

    // =================================================================================
    //  Spawning
    // =================================================================================
    bool GPawnFromPlayer = false;
    bool GPawnClassesLogged = false;
    UClass* GFemaleCls = nullptr;
    UClass* GMaleCls = nullptr;
    bool GGenderClsResolved = false;

    UClass* GetPawnClass(UWorld* World)
    {
        std::string Want = S.PawnClass;
        for (char& c : Want) c = (char)std::tolower((unsigned char)c);
        if (Want == "random")
        {
            // Real players are PlayerFemale_A_C / the game mode's male class: those carry the Mutable appearance
            // object (F_BaseBody_01 ...) clients build the body, head and clothes from. PlayerPawn_v2_C has none.
            if (!GGenderClsResolved || !GFemaleCls || !GMaleCls)
            {
                GGenderClsResolved = true;
                GFemaleCls = UObject::FindClassFast("PlayerFemale_A_C");
                AGameModeBase* GMB = World ? UGameplayStatics::GetGameMode(World) : nullptr;
                if (GMB && GMB->IsA(ATslGameMode::StaticClass())) GMaleCls = static_cast<ATslGameMode*>(GMB)->MalePawnClass;
                if (!GMaleCls) GMaleCls = UObject::FindClassFast("PlayerMale_A_C");
                if (!GFemaleCls && GMB && GMB->DefaultPawnClass && GMB->DefaultPawnClass != GMaleCls) GFemaleCls = GMB->DefaultPawnClass;
                BLOG("Bot pawn classes: female " + (GFemaleCls ? GFemaleCls->GetName() : std::string("<not found>")) +
                    ", male " + (GMaleCls ? GMaleCls->GetName() : std::string("<not found>")));
            }
            UClass* C = nullptr;
            const std::string Gw = Lower(S.BotGender);
            bool WantF = true, WantM = true;
            if (Gw == "female") WantM = false;
            else if (Gw == "male") WantF = false;
            else if (Gw == "match")
            {
                // a bot's face / hair / skin is copied from a connected player of the same gender,
                // so only spawn genders that a player has
                bool F = false, M = false;
                for (const FCharInfo& CI : GChars)
                {
                    if (CI.IsBot || !CI.Char || !CI.Char->CustomizableObjectInstance) continue;
                    struct { void* D; int32 N; int32 Mx; } R;
                    std::memcpy(&R, &CI.Char->InstanceDescriptor, sizeof(R));
                    if (!R.D || R.N <= 0) continue;
                    ((uint8)CI.Char->Gender ? F : M) = true;
                }
                if (F || M) { WantF = F; WantM = M; }
                static int LastMix = -1;
                const int Mix = (F ? 1 : 0) | (M ? 2 : 0);
                if (Mix != LastMix)
                {
                    LastMix = Mix;
                    BLOG(std::string("Bot genders: ") + (WantF && WantM ? "female + male" : WantF ? "female only" : "male only") +
                        " (matching connected players" + (Mix == 0 ? " - none with a look yet, random" : "") + ")");
                }
            }
            if (!GFemaleCls) WantF = false;
            if (!GMaleCls) WantM = false;
            if (WantF && WantM) C = RandI(0, 1) ? GFemaleCls : GMaleCls;
            else if (WantF) C = GFemaleCls;
            else if (WantM) C = GMaleCls;
            else C = GFemaleCls ? GFemaleCls : GMaleCls;
            if (!C) C = UObject::FindClassFast("PlayerPawn_v2_C");
            GPawnClass = C;
            return C;
        }
        if (Want == "auto" || Want.empty())
        {
            // Clients build each character's body (clothes, head, hair) by its class + gender. Bots used
            // PlayerPawn_v2_C (female) while players are a different class -> headless / naked / red ghosts on
            // some PCs. Use exactly the class a real player's character has.
            if (GPawnFromPlayer && GPawnClass)
                return GPawnClass;

            if (World)
            {
                TUObjectArray* Arr = UObject::GObjects.GetTypedPtr();

                const int32 Num = Arr->Num();
                for (int32 i = 0; i < Num; ++i)
                {
                    UObject* O = Arr->GetByIndex(i);
                    if (O->IsDefaultObject())
                        continue;

                    if (!O || !O->IsA(ATslCharacter::StaticClass())) continue;
                    ATslCharacter* C = static_cast<ATslCharacter*>(O);
                    if (!C || IsBotChar(C)) continue;
                    AController* Ct = C->GetController();
                    if (!Ct || !Ct->IsA(APlayerController::StaticClass())) continue;
                    if (static_cast<APlayerController*>(Ct)->bIsLocalPlayerController) continue; // listen-server ghost
                    GPawnClass = C->GetClass();
                    GPawnFromPlayer = true;
                    AGameModeBase* GMB = UGameplayStatics::GetGameMode(World);
                    std::string Extra;
                    if (GMB && GMB->IsA(ATslGameMode::StaticClass()))
                    {
                        ATslGameMode* TGM = static_cast<ATslGameMode*>(GMB);
                        UClass* Male = TGM->MalePawnClass;
                        UClass* Def = GMB->DefaultPawnClass;
                        Extra = " (game mode: male " + (Male ? Male->GetName() : std::string("-")) + ", default " +
                            (Def ? Def->GetName() : std::string("-")) + ")";
                    }
                    BLOG("Bot pawn class: " + GPawnClass->GetName() + " - same as the players" + Extra);
                    return GPawnClass;
                }
                // no player yet: the game mode's male class (players are male by default), else the old class
                AGameModeBase* GMB = World ? UGameplayStatics::GetGameMode(World) : nullptr;
                UClass* C = nullptr;
                if (GMB && GMB->IsA(ATslGameMode::StaticClass())) C = static_cast<ATslGameMode*>(GMB)->MalePawnClass;
                if (!C) C = UObject::FindClassFast("PlayerPawn_v2_C");
                if (!C) C = Get_PlayerPawn_Class();
                if (C != GPawnClass && !GPawnClassesLogged)
                {
                    GPawnClassesLogged = true;
                    BLOG("Bot pawn class (no player in yet): " + (C ? C->GetName() : std::string("<NOT FOUND>")));
                }
                GPawnClass = C;
                return GPawnClass;
            }
            if (GPawnClass)
                return GPawnClass;
            GPawnClass = UObject::FindClassFast(S.PawnClass);
            if (!GPawnClass)
                GPawnClass = Get_PlayerPawn_Class();
            if (!GPawnClass)
            {
                AGameModeBase* GM = UGameplayStatics::GetGameMode(World);
                if (GM && GM->DefaultPawnClass)
                    GPawnClass = GM->DefaultPawnClass;
            }
            BLOG("Bot pawn class: " + (GPawnClass ? GPawnClass->GetName() : std::string("<NOT FOUND>")));
            return GPawnClass;
        }
    }

    FTransform MakeXf(const FVector& Loc, float Yaw)
    {
        FTransform Xf{};
        const float H = Yaw * kPi / 360.f; // half angle in radians
        Xf.Rotation.X = 0.f;
        Xf.Rotation.Y = 0.f;
        Xf.Rotation.Z = std::sin(H);
        Xf.Rotation.W = std::cos(H);
        Xf.Translation = Loc;
        Xf.Scale3D = V(1.f, 1.f, 1.f);
        return Xf;
    }

    // ---------------------------------------------------------------------------------
    //  Game-allocated memory. Arrays/strings the game owns must be grown with the GAME's
    //  allocator (it frees/reallocs them later). We borrow it: KismetStringLibrary::Concat_StrStr
    //  returns a brand new FString allocated by the game, big enough for what we ask.
    // ---------------------------------------------------------------------------------
    struct FRawArray { void* Data; int32 Num; int32 Max; };

    void* GameAlloc(int32 Bytes)
    {
        const int32 Chars = (std::max)(1, (Bytes + 1) / 2);
        std::wstring Fill((size_t)Chars, L'x');
        FString A(Fill.c_str());
        FString Empty(L"");
        FString R = UKismetStringLibrary::Concat_StrStr(A, Empty);
        FRawArray Raw;
        std::memcpy(&Raw, &R, sizeof(Raw));

        if (!Raw.Data || Raw.Num < Chars)
            return nullptr;

        std::memset(Raw.Data, 0, (size_t)Chars * 2);
        return Raw.Data;
    }

    // FString whose buffer belongs to the game (safe to put into game objects)
    bool SetGameString(void* FStringField, const std::string& Text)
    {
        std::wstring W(Text.begin(), Text.end());
        FString A(W.c_str());
        FString Empty(L"");
        FString R = UKismetStringLibrary::Concat_StrStr(A, Empty);
        FRawArray Raw;
        std::memcpy(&Raw, &R, sizeof(Raw));
        if (!Raw.Data)
            return false;
        std::memcpy(FStringField, &Raw, sizeof(Raw));
        return true;
    }

    void DumpTeams(UWorld* World, const std::string& Why)
    {
        ATslGameMode* GM = (ATslGameMode*)World->AuthorityGameMode;
        if (!GM) { BLOG("teams (" + Why + "): no TslGameMode"); return; }
        FRawArray* Arr = reinterpret_cast<FRawArray*>(&GM->Teams);
        BLOG("teams (" + Why + "): GameMode.Teams has " + std::to_string(Arr->Num) + " entries");
        FTeams* E = reinterpret_cast<FTeams*>(Arr->Data);
        for (int32 i = 0; E && i < Arr->Num && i < 64; ++i)
        {
            FRawArray* M = reinterpret_cast<FRawArray*>(&E[i].Members);
            std::string Line = "  team " + std::to_string(E[i].TeamNum) + ": " + std::to_string(M->Num) + " member(s)";
            ATeam** Mem = reinterpret_cast<ATeam**>(M->Data);
            for (int32 k = 0; Mem && k < M->Num && k < 4; ++k)
            {
                ATeam* T = Mem[k];
                if (!T || !T) { Line += " [invalid]"; continue; }
                Line += " [" + T->PlayerName.ToString() + " hp " + std::to_string((int)T->Health) +
                    (T->bIsDying ? " DYING" : "") + (T->bQuitter ? " QUIT" : "") +
                    (T->TslCharacter ? " char" : " nochar") + "]";
                auto* PS=T->AuthorizedPlayerState ? T->AuthorizedPlayerState : T->CachedTslPlayerState;
                auto* Own=PS ? PS->GetOwner() : nullptr;
                auto* PC=Own && Own->IsA(APlayerController::StaticClass()) ? static_cast<APlayerController*>(Own) : nullptr;
                Line += "{auth="+std::to_string(T->AuthorizedPlayerState!=nullptr)+" cached="+std::to_string(T->CachedTslPlayerState!=nullptr)+" pc="+std::to_string(PC!=nullptr)+" local="+std::to_string(PC && PC->bIsLocalPlayerController)+" net="+std::to_string(PC && PC->NetConnection!=nullptr)+" teamid="+std::to_string((int)T->TeamIdRep)+"}";
            }
            BLOG(Line);
        }
        AGameStateBase* GSB = UGameplayStatics::GetGameState(World);
        if (GSB && GSB->IsA(ATslGameState::StaticClass()))
        {
            ATslGameState* GS = static_cast<ATslGameState*>(GSB);
            BLOG("  game counters: joined " + std::to_string(GS->NumJoinPlayers) + " alive " + std::to_string(GS->NumAlivePlayers) +
                " alive teams " + std::to_string(GS->NumAliveTeams) + " start players " + std::to_string(GS->NumStartPlayers) +
                " start teams " + std::to_string(GS->NumStartTeams) + " NumTeams " + std::to_string(GS->NumTeams));
        }
    }

    // Make the game see an AI bot as a player: its own ATeam actor (what real players get),
    // Character->Team pointing at it, and an entry in ATslGameMode::Teams.
    void RegisterBotTeam(UWorld* World, FBot& B)
    {
        if (!S.RegisterTeams || B.IsPC || B.TeamActor)
            return;

        ATslGameMode* GM = (ATslGameMode*)World->AuthorityGameMode;
        if (!GM || !B.Ctrl || !B.Ctrl->PlayerState || !B.Ctrl->PlayerState->IsA(ATslPlayerState::StaticClass()))
            return;

        const int32 TeamNum = static_cast<ATslPlayerState*>(B.Ctrl->PlayerState)->TeamNumber;
        const FTransform Xf = MakeXf(B.Pawn->K2_GetActorLocation(), 0.f);

        AActor* A = nullptr;
        if (S.TeamSize <= 1 || !S.BotsInTeam)
        {
            A = UGameplayStatics::BeginDeferredActorSpawnFromClass(World, ATeam::StaticClass(), Xf, ESpawnActorCollisionHandlingMethod::AlwaysSpawn, B.Ctrl);
        }
        else
        {
            AGameStateBase* GS = World->AuthorityGameMode->GameState;
            for (APlayerState* PS1 : GS->PlayerArray)
            {
                ATslPlayerState* PS = (ATslPlayerState*)PS1;
                ATslCharacter* Player = (ATslCharacter*)PS->GetOwningTslPlayerController()->GetTslCharacter();

                if (Player)
                {
                    if (Player->GetTeam()->GetMemberNumber() >= S.TeamSize)
                    {
                        A = UGameplayStatics::BeginDeferredActorSpawnFromClass(World, ATeam::StaticClass(), Xf, ESpawnActorCollisionHandlingMethod::AlwaysSpawn, B.Ctrl);
                    }
                    else
                    {
                        while (!DecodeTeam(Player->Team)) Sleep(100);
                        A = (AActor*)DecodeTeam(Player->Team);
                        BLOG("Added bot to player's team");
                    }
                }
                else
                {
                    BLOG("Player character pointer is invalid");
                }
            }
        }
         
        if (!A)
        {
            BLOG(B.Name + ": could not spawn its ATeam actor");
            return;
        }
        ATeam* Tm = static_cast<ATeam*>(A);
        Tm->TslCharacter = B.Pawn;
        Tm->AuthorizedPlayerState = static_cast<ATslPlayerState*>(B.Ctrl->PlayerState);
        Tm->CachedTslPlayerState = Tm->AuthorizedPlayerState;
        Tm->Health = 100; Tm->HealthMax = 100;
        Tm->GroggyHealth = 100; Tm->GroggyHealthMax = 100;
        Tm->MemberIndex = 1;
        SetGameString(&Tm->PlayerName, B.Name);
        SetGameString(&Tm->UniqueId, "BOT-" + std::to_string(B.Id));
        UGameplayStatics::FinishSpawningActor(A, Xf);
		uint64 EncryptedTeam = EncodeTeam((uint64)Tm);
        B.Pawn->Team = EncryptedTeam;
        ATeam* DecodedTeam = (ATeam*)DecodeTeam(EncryptedTeam);
        BLOG("Decrypted team: " + DecodedTeam->GetName());
        B.TeamActor = Tm;
        B.TeamActorIndex = Tm->GetIndex();
        // Each bot gets its own replicated member actor; members with the same
        // team number share one GameMode entry.
        FRawArray* ExistingTeams = reinterpret_cast<FRawArray*>(&GM->Teams);
        if (S.TeamSize > 1 && ExistingTeams->Data && ExistingTeams->Num >= 0 && ExistingTeams->Num <= 256) {
            for (int32 I = 0; I < ExistingTeams->Num; ++I) {
                auto* Existing = reinterpret_cast<FTeams*>(static_cast<uint8*>(ExistingTeams->Data) + (size_t)I * sizeof(FTeams));
                if (Existing->TeamNum != TeamNum) continue;
                auto* List = reinterpret_cast<FRawArray*>(&Existing->Members);
                if (List->Num < 0 || List->Num >= S.TeamSize || (List->Num && !List->Data)) break;
                auto** NewMembers = static_cast<ATeam**>(GameAlloc(sizeof(ATeam*) * (List->Num + 1)));
                if (!NewMembers) { BLOG("Team allocation failed"); return; }
                if (List->Num) std::memcpy(NewMembers, List->Data, sizeof(ATeam*) * List->Num);
                Tm->MemberIndex = List->Num + 1;
                NewMembers[List->Num] = Tm; List->Data = NewMembers; ++List->Num; List->Max = List->Num;
                BLOG(B.Name + " registered with the game as team " + std::to_string(TeamNum) + " members=" + std::to_string(List->Num));
                return;
            }
        }
        ATeam** MemData = static_cast<ATeam**>(GameAlloc(sizeof(ATeam*)));
        if (!MemData)
        {
            BLOG(B.Name + ": game allocation failed - team actor made but not listed");
            return;
        }
        MemData[0] = Tm;
        FTeams Entry;
        std::memset(&Entry, 0, sizeof(Entry));
        Entry.TeamNum = TeamNum;
        FRawArray Members{ MemData, 1, 1 };
        std::memcpy(&Entry.Members, &Members, sizeof(Members));

        FRawArray* Arr = reinterpret_cast<FRawArray*>(&GM->Teams);
        if (Arr->Num < Arr->Max && Arr->Data)
        {
            std::memcpy(static_cast<uint8*>(Arr->Data) + (size_t)Arr->Num * sizeof(FTeams), &Entry, sizeof(Entry));
            Arr->Num += 1;
        }
        else
        {
            const int32 NewMax = Arr->Num + 8; // a little slack so the next bots don't need new blocks
            void* NewData = GameAlloc(NewMax * (int32)sizeof(FTeams));
            if (!NewData)
            {
                BLOG(B.Name + ": game allocation failed - not listed in GameMode.Teams");
                return;
            }
            if (Arr->Data && Arr->Num > 0)
                std::memcpy(NewData, Arr->Data, (size_t)Arr->Num * sizeof(FTeams));
            std::memcpy(static_cast<uint8*>(NewData) + (size_t)Arr->Num * sizeof(FTeams), &Entry, sizeof(Entry));
            Arr->Data = NewData;   // old block is simply abandoned (tiny leak, never touched again)
            Arr->Num += 1;
            Arr->Max = NewMax;
        }
        BLOG(B.Name + " registered with the game as team " + std::to_string(TeamNum) + " (GameMode.Teams now " + std::to_string(Arr->Num) + ")");
    }

    void UnregisterBotTeam(UWorld* World, FBot& B)
    {
        if (!B.TeamActor)
            return;
        ATeam* Tm = B.TeamActor;
        B.TeamActor = nullptr;
        if (ATslGameMode* GM = (ATslGameMode*)World->AuthorityGameMode)
        {
            FRawArray* Arr = reinterpret_cast<FRawArray*>(&GM->Teams);
            FTeams* E = reinterpret_cast<FTeams*>(Arr->Data);
            for (int32 i = 0; E && i < Arr->Num; ++i)
            {
                FRawArray* M = reinterpret_cast<FRawArray*>(&E[i].Members);
                if (!M->Data || M->Num < 1 || M->Num > 100 || M->Num > M->Max) continue;
                auto** Members=static_cast<ATeam**>(M->Data);
                for (int32 j=0; j<M->Num; ++j) if (Members[j]==Tm) {
                    if (j<M->Num-1) std::memmove(&Members[j],&Members[j+1],(size_t)(M->Num-1-j)*sizeof(ATeam*));
                    Members[--M->Num]=nullptr;
                    if (M->Num==0) {
                        if (i<Arr->Num-1) std::memmove(&E[i],&E[i+1],(size_t)(Arr->Num-1-i)*sizeof(FTeams));
                        --Arr->Num;
                    }
                    goto removed_team_member;
                }
            }
        }
removed_team_member:
        if (B.Pawn) B.Pawn->Team = EncodeTeam(0);
        if (Tm) Tm->K2_DestroyActor();
    }

    // Raise UGameViewportClient::MaxSplitscreenPlayers (default 4) - not exposed in the SDK.
    // UE 4.16 layout: DebugProperties 0x40, TitleSafeZone 0x50, SplitscreenInfo 0x60 (TArray), MaxSplitscreenPlayers 0x70.
    // Only written if the values there look right.
    void EnsureLocalPlayerCap()
    {
        static bool bDone = false;
        if (bDone) return;
        bDone = true;
        UEngine* E = UEngine::GetEngine();
        if (!E || !E->GameViewport)
        {
            BLOG("player bots: no GameViewport - local player limit unchanged");
            return;
        }
        uint8* GV = reinterpret_cast<uint8*>(E->GameViewport);
        int32* MaxPlayers = reinterpret_cast<int32*>(GV + 0x70);
        const int32 InfoNum = *reinterpret_cast<int32*>(GV + 0x68);
        if (*MaxPlayers >= 1 && *MaxPlayers <= 16 && InfoNum >= 3 && InfoNum <= 16)
        {
            const int32 Old = *MaxPlayers;
            *MaxPlayers = 128;
            BLOG("player bots: local player limit raised " + std::to_string(Old) + " -> 128");
        }
        else
        {
            BLOG("player bots: could not find the local player limit (read " + std::to_string(*MaxPlayers) + "/" +
                std::to_string(InfoNum) + ") - creating more than ~3 bots may fail");
        }
    }

    // Only repair this AI controller's empty PlayerState name. ChangeName and
    // the generic property setter were observed to leave this field empty.
    void RepairBotPlayerName(AController* Ctrl, const std::string& Name)
    {
        if (!Ctrl || !Ctrl->IsA(AAIController::StaticClass()) || !Ctrl->PlayerState) return;
        APlayerState* PS = Ctrl->PlayerState;
        if (!PS->PlayerName.ToString().empty()) return;
        if (SetGameString(&PS->PlayerName, Name)) {
            PS->ForceNetUpdate();
            BLOG("Bot player name assigned: " + Name);
        }
    }

    // Each request belongs to the authenticated controller that sent it.
    void CaptureHumanNameRequest(UObject* Obj, UFunction* Func, void* Parms)
    {
        static UFunction* Request = nullptr;
        if (!Request && APlayerController::StaticClass())
            Request = APlayerController::StaticClass()->GetFunction("PlayerController", "ServerChangeName");
        if (!Request || Func != Request || !Obj || !Parms || !Obj->IsA(APlayerController::StaticClass())) return;
        auto* PC = static_cast<APlayerController*>(Obj);
        if (!PC->NetConnection || PC->bIsLocalPlayerController || !PC->PlayerState) return;
        auto* PS = PC->PlayerState;
        if (PS->GetOwner() != PC) return;
        const FString& Value = static_cast<Params::PlayerController_ServerChangeName*>(Parms)->S;
        if (Value.Num() < 2 || Value.Num() > 25) return;
        const std::string Name = Value.ToString();
        if (Name.empty() || Name.size() > 24) return;
        for (const unsigned char C : Name)
            if (!((C >= 'A' && C <= 'Z') || (C >= 'a' && C <= 'z') || (C >= '0' && C <= '9') || C == '_' || C == '-')) return;
        if (GHumanNameRequests.size() >= 128 && !GHumanNameRequests.count(PS->GetIndex())) return;
        GHumanNameRequests[PS->GetIndex()] = {PC, PC->GetIndex(), PS, PS->GetIndex(), Name};
        BLOG("Player name request received: " + Name);
    }

    // Name updates must follow the player's team actor even while the aircraft
    // transition temporarily leaves the controller without a character pawn.
    bool IsLiveHumanNameTeam(ATeam* Team)
    {
        if (!Team) return false;
        UObject Header{};
        SIZE_T Read = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), Team, &Header, sizeof(Header), &Read) || Read != sizeof(Header)) return false;
        if (!LiveBotObject(Team, Header.GetIndex())) return false;
        return Team->IsA(ATeam::StaticClass());
    }

    ATeam* ResolveHumanNameTeam(const FHumanNameRequest& R)
    {
        if (!R.State->IsA(ATslPlayerState::StaticClass())) return nullptr;
        auto* State = static_cast<ATslPlayerState*>(R.State);
        ATeam* Direct = State->Team;
        if (IsLiveHumanNameTeam(Direct) && HumanNameTeamPolicy::DirectMatch(
            Direct->AuthorizedPlayerState != nullptr, Direct->AuthorizedPlayerState == State,
            Direct->CachedTslPlayerState != nullptr, Direct->CachedTslPlayerState == State)) return Direct;

        auto* Pawn = R.Controller->K2_GetPawn();
        if (Pawn && Pawn->IsA(ATslCharacter::StaticClass())) {
            auto* Character = static_cast<ATslCharacter*>(Pawn);
            auto* Team = Character->GetTeam();
            if (IsLiveHumanNameTeam(Team) && Team->TslCharacter == Character) return Team;
        }

        auto* World = UWorld::GetWorld();
        if (!World || !World->AuthorityGameMode || !World->AuthorityGameMode->IsA(ATslGameMode::StaticClass()) || State->TeamNumber <= 0) return nullptr;
        auto* Mode = static_cast<ATslGameMode*>(World->AuthorityGameMode);
        FRawArray Teams{};
        std::memcpy(&Teams, &Mode->Teams, sizeof(Teams));
        if (!Teams.Data || Teams.Num <= 0 || Teams.Num > 256 || Teams.Max < Teams.Num || Teams.Max > 1024) return nullptr;

        struct FRawTeamEntry { int32 TeamNumber; int32 Padding; FRawArray Members; };
        static_assert(sizeof(FRawTeamEntry) == sizeof(FTeams), "Team entry layout mismatch");
        std::vector<FRawTeamEntry> Entries(static_cast<size_t>(Teams.Num));
        SIZE_T Read = 0;
        const SIZE_T Bytes = Entries.size() * sizeof(FRawTeamEntry);
        if (!ReadProcessMemory(GetCurrentProcess(), Teams.Data, Entries.data(), Bytes, &Read) || Read != Bytes) return nullptr;
        for (const FRawTeamEntry& Entry : Entries) {
            const FRawArray& Members = Entry.Members;
            if (Entry.TeamNumber != State->TeamNumber || !Members.Data || Members.Num <= 0 || Members.Num > 16 || Members.Max < Members.Num || Members.Max > 128) continue;
            ATeam* Candidates[16]{};
            const SIZE_T MemberBytes = static_cast<SIZE_T>(Members.Num) * sizeof(ATeam*);
            Read = 0;
            if (!ReadProcessMemory(GetCurrentProcess(), Members.Data, Candidates, MemberBytes, &Read) || Read != MemberBytes) continue;
            for (int32 I = 0; I < Members.Num; ++I) {
                ATeam* Team = Candidates[I];
                if (!IsLiveHumanNameTeam(Team)) continue;
                if (HumanNameTeamPolicy::ExplicitMatch(Team->AuthorizedPlayerState == State, Team->CachedTslPlayerState == State)) return Team;
                // For a one-member team only, a detached actor with no other
                // identity can be associated by the connected state's team ID.
                if (HumanNameTeamPolicy::DetachedSoloMatch(Members.Num, Team->AuthorizedPlayerState != nullptr,
                    Team->CachedTslPlayerState != nullptr, Team->TslCharacter != nullptr)) return Team;
            }
        }
        return nullptr;
    }

    // The listen server has its own local controller even when no human hosts
    // the match. Rename that internal actor only; authenticated clients have a
    // NetConnection and are deliberately excluded.
    void ApplyServerHostName(UWorld* World, double T)
    {
        if (T<GNextServerHostNameCheck) return;
        GNextServerHostNameCheck=T+1.0;
        if (!World || !World->AuthorityGameMode || !World->AuthorityGameMode->GameState) return;
        auto* GameState=World->AuthorityGameMode->GameState;
        for (APlayerState* PS : GameState->PlayerArray) {
            if (!PS) continue;
            auto* Owner=PS->GetOwner();
            if (!Owner || !Owner->IsA(APlayerController::StaticClass())) continue;
            auto* PC=static_cast<APlayerController*>(Owner);
            if (!PC->bIsLocalPlayerController || PC->NetConnection || PC->PlayerState!=PS) continue;
            // The server host follows the configured bot names and uses the
            // next number after the configured number of match bots.
            const std::string Name=S.NamePrefix+std::to_string((std::max)(0,S.Count)+1);
            if (PS->PlayerName.ToString()!=Name && SetGameString(&PS->PlayerName,Name)) {
                PS->ForceNetUpdate(); BLOG("SERVER HOST NAME applied: " + Name);
            }
            FHumanNameRequest R{PC,PC->GetIndex(),PS,PS->GetIndex(),Name};
            auto* Team=ResolveHumanNameTeam(R);
            if (Team && PS->IsA(ATslPlayerState::StaticClass())) {
                auto* State=static_cast<ATslPlayerState*>(PS);
                if ((Team->AuthorizedPlayerState==State || Team->CachedTslPlayerState==State) &&
                    Team->PlayerName.ToString()!=Name && SetGameString(&Team->PlayerName,Name)) {
                    Team->ForceNetUpdate(); BLOG("SERVER HOST TEAM NAME applied: " + Name);
                }
            }
        }
    }

    void ApplyHumanNameRequests(double T)
    {
        if (T < GNextHumanNameCheck) return;
        GNextHumanNameCheck = T + 1.0;
        for (auto It = GHumanNameRequests.begin(); It != GHumanNameRequests.end();) {
            const FHumanNameRequest& R = It->second;
            if (!LiveBotObject(R.Controller, R.ControllerIndex) || !LiveBotObject(R.State, R.StateIndex) ||
                !R.Controller->NetConnection || R.Controller->PlayerState != R.State || R.State->GetOwner() != R.Controller) {
                It = GHumanNameRequests.erase(It); continue;
            }
            if (R.State->PlayerName.ToString() != R.Name && SetGameString(&R.State->PlayerName, R.Name)) {
                R.State->ForceNetUpdate(); BLOG("Player name applied: " + R.Name);
            }
            auto* Team = ResolveHumanNameTeam(R);
            if (Team && Team->PlayerName.ToString() != R.Name && SetGameString(&Team->PlayerName, R.Name)) {
                Team->ForceNetUpdate();
                BLOG("Player team name applied: " + R.Name);
            }
            ++It;
        }
    }

    bool SpawnBotAt(UWorld* World, FVector Loc)
    {
        if ((int)GBots.size() >= S.MaxBots)
        {
            BLOG("MaxBots reached (" + std::to_string(S.MaxBots) + ")");
            return false;
        }

        UClass* PawnCls = GetPawnClass(World);
        UClass* CtrlCls = AAIController::StaticClass();
        if (!PawnCls || !CtrlCls || !PawnCls->IsSubclassOf(ATslCharacter::StaticClass()))
            return false;
         
        Loc.Z += 110.f; // capsule half height + a bit
        const FTransform Xf = MakeXf(Loc, RandF(0.f, 360.f));

        static int SpawnLogLeft = 1; // log every step of the FIRST spawn only (all spawns with VerboseLog=1)
        const bool bStepLog = SpawnLogLeft > 0 || S.VerboseLog;
        if (SpawnLogLeft > 0) --SpawnLogLeft;
        if (bStepLog) BLOG("spawn: controller " + CtrlCls->GetName() + ", pawn " + PawnCls->GetName());

        // 1) Controller first, deferred, so bWantsPlayerState is set before PostInitializeComponents
        AActor* CA = UGameplayStatics::BeginDeferredActorSpawnFromClass(World, CtrlCls, Xf,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn, nullptr);
        if (!CA)
        {
            BLOG("Controller spawn failed");
            return false;
        }
        AAIController* Ctrl = static_cast<AAIController*>(CA);
        Ctrl->bWantsPlayerState = 1;
        UGameplayStatics::FinishSpawningActor(CA, Xf);
        if (bStepLog) BLOG(std::string("spawn: controller OK, PlayerState ") + (Ctrl->PlayerState ? "yes" : "NO"));

        // 2) Pawn, deferred, with auto-possess off (we possess manually)
        AActor* PA = UGameplayStatics::BeginDeferredActorSpawnFromClass(World, PawnCls, Xf, ESpawnActorCollisionHandlingMethod::AlwaysSpawn, nullptr);
        if (!PA)
        {
            BLOG("Pawn spawn failed");
            Ctrl->K2_DestroyActor();
            return false;
        }
        APawn* P = static_cast<APawn*>(PA);
        P->AutoPossessAI = EAutoPossessAI::Disabled;
        P->AIControllerClass = CtrlCls;
        UGameplayStatics::FinishSpawningActor(PA, Xf);
        if (bStepLog) BLOG("spawn: pawn OK");

        /*
        if (!ObjValid(P))
        {
            BLOG("Spawned pawn is invalid or not an ATslCharacter - check [Bots] PawnClass");
            BLOG("Pawn class is " + P->GetFullName());
            if (ObjValid(P)) P->K2_DestroyActor();
            Ctrl->K2_DestroyActor();
            return false;
        }
        */
    
        Ctrl->Possess(P);
        if (bStepLog) BLOG("spawn: possess OK");

        FBot B;
        B.Id = GNextBotId++;
        B.Name = S.NamePrefix + std::to_string(B.Id);
        B.Pawn = static_cast<ATslCharacter*>(P);
        if (World && World->AuthorityGameMode && World->AuthorityGameMode->IsA(ATslZombieGameMode::StaticClass())) InstallZombieDamageGuard(B.Pawn);
        B.PawnIndex = P->GetIndex();
        B.Ctrl = Ctrl;
        B.CtrlIndex = Ctrl->GetIndex();
        B.LastHealth = B.Pawn->Health;
        const double T = Now();
        B.NextArmTry = T + 0.3;
        B.NextRoamPick = T + RandF(0.5f, 2.0f);
        B.ProgressCheckAt = T + 2.0;
        B.ProgressPos = Loc;
        B.SpawnedAt = T;
        B.ZombieGroundSpawn=GMatchSpawnDone && World && World->AuthorityGameMode && World->AuthorityGameMode->IsA(ATslZombieGameMode::StaticClass()) && static_cast<ATslZombieGameMode*>(World->AuthorityGameMode)->ZombieRuleType==EZombieRuleType::BattleRoyale;

        // 3) Player state: name, bot flag, team
        if (Ctrl->PlayerState /* && ObjValid(Ctrl->PlayerState)*/)
        {
            APlayerState* PS = Ctrl->PlayerState;
            PS->bIsABot = S.MarkAsBot ? 1 : 0; // flagged bots are drawn by clients as test bots (red ghosts / headless)
            if (PS->IsA(ATslPlayerState::StaticClass()))
            {
                // PUBG may only count normal (small) team numbers, so stay just above the highest one in use
                int32 MaxTeam = 0;
                AGameStateBase* GSB = UGameplayStatics::GetGameState(World);
                if (GSB)
                    for (APlayerState* Other : GSB->PlayerArray)
                        if (Other && Other != PS && Other->IsA(ATslPlayerState::StaticClass()))
                            MaxTeam = (std::max)(MaxTeam, static_cast<ATslPlayerState*>(Other)->TeamNumber);
                int32 Team;
                if (S.FightEachOther) {
                    if (S.TeamSize <= 1) Team = MaxTeam + 1;
                    else {
                        // Keep bot teams in the high end of the 100-player range;
                        // native human logins allocate low team numbers.
                        if (GGroupTeam < 0) { GGroupTeam = 101; GGroupMembers = S.TeamSize; }
                        if (GGroupMembers >= S.TeamSize) { --GGroupTeam; GGroupMembers = 0; }
                        Team = GGroupTeam; ++GGroupMembers;
                    }
                }
                else
                {
                    if (GBotTeam < 0 || GBotTeam <= 0) GBotTeam = MaxTeam + 1;
                    Team = GBotTeam;
                }
                static_cast<ATslPlayerState*>(PS)->TeamNumber = Team;
                if (bStepLog) BLOG("spawn: team " + std::to_string(Team));
            }
            if (AGameModeBase* GM = UGameplayStatics::GetGameMode(World))
            {
                std::wstring WName(B.Name.begin(), B.Name.end());
                GM->ChangeName(Ctrl, FString(WName.c_str()), false);
                RepairBotPlayerName(Ctrl, B.Name);
                if (bStepLog) BLOG("spawn: name set");
            }
        }
        else
        {
            BLOG(B.Name + " has no PlayerState - damage / kill feed may not work for it");
        }

        if (S.CountAsJoined && !S.BotsInPlayerCount)
        {
            AGameStateBase* GSB = UGameplayStatics::GetGameState(World);
            if (GSB && GSB->IsA(ATslGameState::StaticClass()))
            {
                static_cast<ATslGameState*>(GSB)->NumJoinPlayers += 1;
                static_cast<ATslGameState*>(GSB)->NumAlivePlayers += 1;
                static_cast<ATslGameState*>(GSB)->NumStartPlayers += 1;

                static_cast<ATslGameState*>(GSB)->OnRep_NumJoinPlayers();
                static_cast<ATslGameState*>(GSB)->OnRep_NumAlivePlayers();

                B.CountedJoin = true;
            }
        }

        if (World->AuthorityGameMode && World->AuthorityGameMode->IsA(ATslZombieGameMode::StaticClass())) {
            if (Ctrl->PlayerState && Ctrl->PlayerState->IsA(ATslZombiePlayerState::StaticClass())) {
                auto* Zombie = static_cast<ATslZombiePlayerState*>(Ctrl->PlayerState);
                const auto Rule=static_cast<ATslZombieGameMode*>(World->AuthorityGameMode)->ZombieRuleType;
                Zombie->SetZombieModePlayerState(EZombieModePlayerState::Zombie);
                BLOG("[ZOMBIE TEST] bot " + B.Name + " initial state=" + std::to_string((int)Zombie->GetZombieModePlayerState()));
            } else BLOG("[ZOMBIE TEST] bot has no zombie-compatible player state");
        }
        RegisterBotTeam(World, B);

        GBots.push_back(B);
        if (World->AuthorityGameMode && World->AuthorityGameMode->IsA(ATslZombieGameMode::StaticClass())) {
            auto* Mode=static_cast<ATslZombieGameMode*>(World->AuthorityGameMode);
            if (Mode->GameState && Mode->GameState->IsA(ATslZombieGameState::StaticClass())) {
                auto* ZS=static_cast<ATslZombieGameState*>(Mode->GameState);
                // The first zombie must be visible before StartMatch. Later counts
                // are maintained by CountZombieSides without resetting to one.
                if (Mode->ZombieRuleType!=EZombieRuleType::BattleRoyale && Mode->MatchState.ToString()=="WaitingToStart" && ZS->NumAliveZombiePlayers<1) {
                    ZS->NumAliveZombiePlayers=1; ZS->OnRep_NumAliveZombiePlayers(); ZS->ForceNetUpdate();
                }
            }
        }
        if (S.VerboseLog)
            BLOG("Spawned " + B.Name + " at " + std::to_string((int)Loc.X) + "," + std::to_string((int)Loc.Y) + "," + std::to_string((int)Loc.Z));
        return true;
    }

    bool SpawnRandomBot(UWorld* World)
    {
        FVector Loc{};
        if (!FindRandomSpawn(World, Loc)) return false; // Never spawn at an uninitialized or unsafe location.
        return SpawnBotAt(World, Loc);
    }

    bool SpawnBotNearPlayer(UWorld* World)
    {
        ATslCharacter* P = FindRealPlayer(World);
        if (!P)
        {
            if (!GMatchSpawnDone) return false; // wait for the actual lobby pawn
            BLOG("No real player pawn found to spawn near - spawning at a random point instead");
            return SpawnRandomBot(World);
        }
        const FVector Base = P->K2_GetActorLocation();
        for (int i = 0; i < 10; ++i)
        {
            const float A = RandF(0.f, 2.f * kPi);
            const float R = RandF(1500.f, 3000.f);
            FVector G;
            if (TraceDown(World, Base.X + std::cos(A) * R, Base.Y + std::sin(A) * R, Base.Z - 200.f + 3000.f, -30000.f, G) && !BelowSea(G.Z))
                return SpawnBotAt(World, G);
        }
        return GMatchSpawnDone ? SpawnRandomBot(World) : false;
    }

    // =================================================================================
    //  Bot behaviour
    // =================================================================================
    bool IsBotPS(const APlayerState* PS)
    {
        if (!PS) return false;
        for (const FBot& B : GBots)
            if (B.Ctrl && B.Ctrl->PlayerState == PS) return true;
        return false;
    }

    std::string CharName(ATslCharacter* C)
    {
        if (!C) return "?";
        for (const FBot& B : GBots) if (B.Pawn == C) return B.Name;
        if (C->GetTslPlayerState()) return C->GetTslPlayerState()->PlayerName.ToString();
        return "a player";
    }

    FVector RotToVec(float Pitch, float Yaw)
    {
        const float P = Pitch * kPi / 180.f, Y = Yaw * kPi / 180.f;
        return V(std::cos(P) * std::cos(Y), std::cos(P) * std::sin(Y), std::sin(P));
    }
    float NormAngle(float A) { while (A > 180.f) A -= 360.f; while (A < -180.f) A += 360.f; return A; }

    // confirm the format from real players (it only changes what bots write if players clearly use another one)
    // The native field and RPC carry FRotator degrees, not normalized vectors.
    void SetBotAim(ATslCharacter* Me, float Pitch, float Yaw)
    {
        if (!Me || !std::isfinite(Pitch) || !std::isfinite(Yaw)) return;
        auto* Fn=Me->GetClass()->GetFunction("TslCharacter","ServerSetAimOffsets");
        if (!Fn) return;
        Params::TslCharacter_ServerSetAimOffsets Args{};
        Args.NewAimOffsets=FRotator{std::clamp(NormAngle(Pitch),-89.f,89.f),NormAngle(Yaw),0.f};
        const auto Flags=Fn->FunctionFlags;
        Fn->FunctionFlags=(Flags & ~0x40u)|0x400u;
        Me->ProcessEvent(Fn,&Args); Fn->FunctionFlags=Flags;
    }

    // Apply the normal targeting transition locally on the server-owned bot.
    // ServerSetTargeting normally goes through the player/network manager.
    bool SetNativeBotTargeting(ATslCharacter* Me, ETargetingType Type)
    {
        if (!Me || Me->Role!=ENetRole::ROLE_Authority || !IsBotChar(Me)) return false;
        static int Checked=0;
        const auto Base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        if (!Checked) {
            const unsigned char TargetSig[]={0x40,0x55,0x56,0x57,0x48,0x81,0xEC,0x80,0,0,0};
            const unsigned char ScopeSig[]={0x48,0x81,0xC1,0x48,0x11,0,0,0x4C,0x8B,0x01};
            const unsigned char AimSig[]={0x48,0x81,0xC1,0x48,0x11,0,0,0x4C,0x8B,0x01};
            Checked=std::memcmp(reinterpret_cast<void*>(Base+0x3A4E010),TargetSig,sizeof(TargetSig))==0 &&
                std::memcmp(reinterpret_cast<void*>(Base+0x3A496A0),ScopeSig,sizeof(ScopeSig))==0 &&
                std::memcmp(reinterpret_cast<void*>(Base+0x3A49660),AimSig,sizeof(AimSig))==0 ? 1 : -1;
            BLOG(Checked==1 ? "BOT AIM native targeting signatures verified" : "BOT AIM native targeting signature mismatch; bridge disabled");
        }
        if (Checked!=1) return false;
        reinterpret_cast<void(*)(ATslCharacter*,uint8,bool)>(Base+0x3A4E010)(Me,static_cast<uint8>(Type),true);
        reinterpret_cast<void(*)(ATslCharacter*,bool)>(Base+0x3A496A0)(Me,Type==ETargetingType::Targeting_Scoping);
        reinterpret_cast<void(*)(ATslCharacter*,bool)>(Base+0x3A49660)(Me,Type==ETargetingType::Targeting_Aiming);
        return true;
    }

    void StopFire(FBot& B)
    {
        if (B.Firing && LiveBotObject(B.FiringGun, B.FiringGunIndex))
        {
            B.FiringGun->ServerStopFire();
            if (S.ShowBotShots) B.FiringGun->ClientStopSimulatingWeaponFire();
        }
        B.Firing = false;
        B.FiringGun = nullptr;
        B.FiringGunIndex = -1;
    }

    void SetObjective(FBot& B, const char* Goal)
    {
        if (B.Objective==Goal) return;
        B.Objective=Goal;
        if (S.HumanlikeGoals) BLOG("BOT GOAL " + B.Name + " " + Goal);
    }

    void SetTargeting(FBot& B, ETargetingType Type, double T)
    {
        if (!S.UseADS) Type=ETargetingType::Targeting_None;
        if (!LiveBotObject(B.Pawn,B.PawnIndex) || B.Pawn->Health<=0.f) {B.AimType=ETargetingType::Targeting_None; return;}
        if (B.AimType==Type && (Type==ETargetingType::Targeting_None || T<B.NextTargetingAt || B.Pawn->IsScoping())) return;
        B.NextTargetingAt=T+2.0;
        auto* Fn=B.Pawn->GetClass()->GetFunction("TslCharacter","ServerSetTargeting");
        if (!Fn) return;
        Params::TslCharacter_ServerSetTargeting Args{}; Args.NewTargetingType=Type;
        const auto Flags=Fn->FunctionFlags;
        Fn->FunctionFlags=(Flags & ~0x40u)|0x400u;
        B.Pawn->ProcessEvent(Fn,&Args); Fn->FunctionFlags=Flags;
        // The remote stance flags are updated via their native RPC handlers,
        // not by overwriting packed fields in the character layout.
        for (const auto& Name : {"ServerBroadCastScopingRemote","ServerBroadCastAimingRemote"}) {
            auto* Remote=B.Pawn->GetClass()->GetFunction("TslCharacter",Name);
            if (!Remote) continue;
            const auto Saved=Remote->FunctionFlags;
            Remote->FunctionFlags=(Saved & ~0x40u)|0x400u;
            bool Enabled=Name==std::string("ServerBroadCastScopingRemote") ? Type==ETargetingType::Targeting_Scoping : Type==ETargetingType::Targeting_Aiming;
            B.Pawn->ProcessEvent(Remote,&Enabled); Remote->FunctionFlags=Saved;
        }
        SetNativeBotTargeting(B.Pawn,Type);
        B.AimType=Type; B.Pawn->ForceNetUpdate();
        if (Type!=ETargetingType::Targeting_None && T>=B.NextAimCheck) {
            B.NextAimCheck=T+10.0;
            BLOG("BOT AIM " + B.Name + " requested="+std::to_string(int(Type))+" scoped="+
                std::to_string(B.Pawn->IsScoping())+" targeting="+std::to_string(B.Pawn->IsTargeting()));
        }
    }

    void SetSprint(FBot& B, bool bOn)
    {
        if (!S.Sprint) bOn = false;
        if (bOn) SetTargeting(B, ETargetingType::Targeting_None, Now());
        if (B.Sprinting == bOn) return;
        B.Pawn->ServerSetSprinting(bOn);
        B.Sprinting = bOn;
    }

    void MoveTo(UWorld* World, FBot& B, FVector Dest, double T, bool bStrafe, float Acceptance=-1.f, bool KeepFloor=false)
    {
        if (!KeepFloor) Dest=GroundAt(World,Dest);
        B.DestAcceptance=Acceptance>0.f ? Acceptance : S.AcceptRadius;
        B.Dest = Dest;
        B.HasDest = true;
        B.DestSetAt = T;
        if (B.UseSteer) return;
        auto* AI=AIC(B);
        if (!AI || !LiveBotObject(AI,B.CtrlIndex)) {B.UseSteer=true; return;}

        const EPathFollowingRequestResult R = AI->MoveToLocation(Dest, B.DestAcceptance, false, false, false, bStrafe, TSubclassOf<UNavigationQueryFilter>(nullptr), true);
        if (R == EPathFollowingRequestResult::Failed)
        {
            if (++B.MoveFails >= 3)
            {
                B.UseSteer = true;
                if (S.VerboseLog) BLOG(B.Name + " MoveToLocation keeps failing - switching to input steering");
            }
        }
        else
        {
            B.MoveFails = 0;
        }
    }

    // smallest live zone circle (the white "safe" circle sits inside the blue one)
    bool GetSafeCircle(UWorld* World, FVector& C, float& R)
    {
        AGameStateBase* GSB = UGameplayStatics::GetGameState(World);
        if (!GSB || !GSB->IsA(ATslGameState::StaticClass()))
            return false;
        ATslGameState* GS = static_cast<ATslGameState*>(GSB);
        const FVector Ps[3] = { GS->SafetyZonePosition, GS->PoisonGasWarningPosition, GS->LerpSafetyZonePosition };
        const float   Rs[3] = { GS->SafetyZoneRadius, GS->PoisonGasWarningRadius, GS->LerpSafetyZoneRadius };
        bool bAny = false;
        for (int i = 0; i < 3; ++i)
        {
            if (!(Rs[i] > 1000.f && Rs[i] < 2000000.f)) continue;
            if (!bAny || Rs[i] < R) { C = Ps[i]; R = Rs[i]; bAny = true; }
        }
        return bAny;
    }

    // the gas wall right now (blue): the moving circle; falls back to the current safety zone
    bool GetBlueCircle(UWorld* World, FVector& C, float& R)
    {
        AGameStateBase* GSB = UGameplayStatics::GetGameState(World);
        if (!GSB || !GSB->IsA(ATslGameState::StaticClass()))
            return false;
        ATslGameState* GS = static_cast<ATslGameState*>(GSB);
        if (GS->LerpSafetyZoneRadius > 1000.f && GS->LerpSafetyZoneRadius < 2000000.f) { C = GS->LerpSafetyZonePosition; R = GS->LerpSafetyZoneRadius; return true; }
        if (GS->SafetyZoneRadius > 1000.f && GS->SafetyZoneRadius < 2000000.f) { C = GS->SafetyZonePosition; R = GS->SafetyZoneRadius; return true; }
        return false;
    }

    // Blue-zone damage uses authoritative SafetyZone fields; the server's
    // interpolated fields can be stale. The warning circle is the next safe area.
    bool ZoneEmergencyTarget(UWorld* World, const FVector& Here, FVector& SafeCenter, float& SafeRadius)
    {
        if (!S.FollowZone) return false;
        auto* Base = UGameplayStatics::GetGameState(World);
        if (!Base || !Base->IsA(ATslGameState::StaticClass())) return false;
        auto* GS = static_cast<ATslGameState*>(Base);
        const float BlueRadius = GS->SafetyZoneRadius;
        const FVector BlueCenter = GS->SafetyZonePosition;
        SafeCenter = GS->PoisonGasWarningPosition;
        SafeRadius = GS->PoisonGasWarningRadius;
        if (!GS->bIsWorkingBlueZone || !(BlueRadius > 1000.f && BlueRadius <= 600000.f) ||
            !(SafeRadius > 1000.f && SafeRadius <= 600000.f) ||
            !std::isfinite(BlueCenter.X) || !std::isfinite(BlueCenter.Y) ||
            !std::isfinite(SafeCenter.X) || !std::isfinite(SafeCenter.Y)) return false;
        const float SafeDistance = Len(Flat(SafeCenter - Here));
        const float BlueDistance = Len(Flat(BlueCenter - Here));
        return ZoneBoostPolicy::FarAndInBlue(SafeDistance, SafeRadius, BlueDistance, BlueRadius);
    }

    // Start travelling before the blue reaches a bot, not only after it has
    // already spent health in the gas. Positions are authoritative game-state
    // values and are checked before use, as in ZoneEmergencyTarget.
    bool ZoneTravelTarget(UWorld* World, const FVector& Here, bool Travelling,
                          FVector& Center, float& Radius, float& Gap, bool& InBlue)
    {
        if (!S.FollowZone) return false;
        auto* Base = UGameplayStatics::GetGameState(World);
        if (!Base || !Base->IsA(ATslGameState::StaticClass())) return false;
        auto* GS = static_cast<ATslGameState*>(Base);
        Center = GS->PoisonGasWarningPosition;
        Radius = GS->PoisonGasWarningRadius;
        const FVector Blue = GS->SafetyZonePosition;
        const float BlueRadius = GS->SafetyZoneRadius;
        if (!(Radius > 1000.f && Radius <= 600000.f) ||
            !(BlueRadius > 1000.f && BlueRadius <= 600000.f) ||
            (!GS->bIsWorkingBlueZone && !(Radius < BlueRadius * 0.98f)) ||
            !std::isfinite(Center.X) || !std::isfinite(Center.Y) ||
            !std::isfinite(Blue.X) || !std::isfinite(Blue.Y) ||
            !std::isfinite(Here.X) || !std::isfinite(Here.Y)) return false;
        const float Distance = Len(Flat(Center - Here));
        Gap = Distance - Radius;
        InBlue = GS->bIsWorkingBlueZone && Len(Flat(Blue - Here)) > BlueRadius;
        return ZoneBoostPolicy::ShouldStartOrContinueTravel(Distance, Radius, Travelling);
    }

    void ObserveZoneClosing(UWorld* World)
    {
        if (!GMatchSpawnDone) return;
        auto* Base = UGameplayStatics::GetGameState(World);
        if (!Base || !Base->IsA(ATslGameState::StaticClass())) return;
        auto* GS = static_cast<ATslGameState*>(Base);
        const bool Releasing = GS->bIsGasRelease;
        if (!GZoneReleaseObserved) {
            GZoneReleaseObserved = true; GZonePreviousRelease = false;
        }
        if (Releasing && !GZonePreviousRelease) {
            ++GZoneClosingsStarted;
            BLOG("BOT ZONE observed closing started " + std::to_string(GZoneClosingsStarted) +
                " game phase index=" + std::to_string(GS->NextPoisonGasIndex));
        } else if (!Releasing && GZonePreviousRelease) {
            BLOG("BOT ZONE observed closing completed " + std::to_string(GZoneClosingsStarted));
        }
        GZonePreviousRelease = Releasing;
    }

    bool ZoneSpeedLayoutSafe()
    {
        if (GZoneSpeedLayoutChecked) return GZoneSpeedLayoutSafe;
        GZoneSpeedLayoutChecked = true;
        UClass* Cls = ATslCharacter::StaticClass();
        UClass* FloatClass = UFloatProperty::StaticClass();
        if (!Cls || !FloatClass) return false;
        bool Run = false, Sprint = false;
        for (UField* Field = Cls->Children; Field; Field = Field->Next) {
            if (!Field->IsA(FloatClass)) continue;
            const std::string Name = Field->GetName();
            const int Offset = static_cast<UProperty*>(Field)->Offset;
            if (Name == "Stand_RunningSpeedModifier") Run = Offset == offsetof(ATslCharacter, Stand_RunningSpeedModifier);
            if (Name == "Stand_SprintingSpeedModifier") Sprint = Offset == offsetof(ATslCharacter, Stand_SprintingSpeedModifier);
        }
        GZoneSpeedLayoutSafe = Run && Sprint;
        BLOG(std::string("BOT ZONE speed layout ") + (GZoneSpeedLayoutSafe ? "verified" : "unavailable; speed unchanged"));
        return GZoneSpeedLayoutSafe;
    }

    void UpdateBotZoneBoost(UWorld* World, FBot& B)
    {
        if (!B.Pawn || B.State == EBotState::Dead || !LiveBotObject(B.Pawn, B.PawnIndex)) return;
        ATslCharacter* Me = B.Pawn;
        auto* Move = Me->GetTslCharacterMovement();
        FVector Center{}; float Radius = 0.f;
        const bool WithinThreeClosings = ZoneBoostPolicy::WithinFirstThreeClosings(
            GZoneReleaseObserved, GZoneClosingsStarted, GZonePreviousRelease);
        const double Stamp = Now();
        const bool Walking = Move && Move->MovementMode == EMovementMode::MOVE_Walking;
        if (Walking) B.ZoneLastWalking = Stamp;
        // Keep an already-enabled walking boost across a brief ground jump.
        // A fresh airborne bot can never start the boost and skydiving/chutes
        // remain explicitly excluded below.
        const bool Falling = Move && Move->MovementMode == EMovementMode::MOVE_Falling;
        const bool GroundTravel = ZoneBoostPolicy::GroundTravel(Walking, Falling, B.ZoneBoosted,
            B.ZoneLastWalking > 0 ? Stamp - B.ZoneLastWalking : 1e9,
            Falling && (Me->IsSkydiving() || Me->IsFallingWithParachute()));
        const bool Enable = WithinThreeClosings && GMatchSpawnDone && !B.Hidden && !B.NativeSkyActive && !B.DropPending &&
            GroundTravel && !Me->IsInVehicle() &&
            !Me->IsInAircraft() && !Me->IsSwimming() && !Me->IsGroggying() &&
            ZoneEmergencyTarget(World, Me->K2_GetActorLocation(), Center, Radius);
        if (!Enable && B.ZoneBoosted) {
            // Restore only if another game system has not changed that value.
            if (std::fabs(Me->Stand_RunningSpeedModifier - B.NormalRunModifier * 2.f) < 0.0001f)
                Me->Stand_RunningSpeedModifier = B.NormalRunModifier;
            if (std::fabs(Me->Stand_SprintingSpeedModifier - B.NormalSprintModifier * 2.f) < 0.0001f)
                Me->Stand_SprintingSpeedModifier = B.NormalSprintModifier;
            B.ZoneBoosted = false; B.ZoneBoostRejected = false;
            BLOG("BOT ZONE BOOST " + B.Name + " disabled mode=" +
                std::to_string(Move ? (int)Move->MovementMode : -1) +
                " phase=" + std::to_string(WithinThreeClosings ? 1 : 0) +
                " zone=" + std::to_string(ZoneEmergencyTarget(World, Me->K2_GetActorLocation(), Center, Radius) ? 1 : 0));
        }
        if (!Enable) { B.ZoneBoostRejected = false; return; }
        if (B.ZoneBoosted || B.ZoneBoostRejected || !ZoneSpeedLayoutSafe()) return;
        const float R = Me->Stand_RunningSpeedModifier, S = Me->Stand_SprintingSpeedModifier;
        if (!(R > 0.001f && R < 10.f && S > 0.001f && S < 10.f)) { B.ZoneBoostRejected = true; return; }
        const float BeforeRun = Me->GetRunningSpeedModifier_Stand();
        const float BeforeSprint = Me->GetSprintingSpeedModifier_Stand();
        Me->Stand_RunningSpeedModifier = R * 2.f;
        Me->Stand_SprintingSpeedModifier = S * 2.f;
        const float AfterRun = Me->GetRunningSpeedModifier_Stand();
        const float AfterSprint = Me->GetSprintingSpeedModifier_Stand();
        // Verify that the engine's own speed getters actually observe 2x.
        const bool Verified = BeforeRun > 0.001f && BeforeSprint > 0.001f &&
            std::fabs(AfterRun / BeforeRun - 2.f) < 0.03f &&
            std::fabs(AfterSprint / BeforeSprint - 2.f) < 0.03f;
        if (!Verified) {
            Me->Stand_RunningSpeedModifier = R; Me->Stand_SprintingSpeedModifier = S;
            B.ZoneBoostRejected = true;
            BLOG("BOT ZONE BOOST " + B.Name + " rejected: speed getters did not confirm 2x");
            return;
        }
        B.NormalRunModifier = R; B.NormalSprintModifier = S; B.ZoneBoosted = true;
        BLOG("BOT ZONE BOOST " + B.Name + " enabled 2x; closing=" + std::to_string(GZoneClosingsStarted));
    }

    // This candidate only handles ordinary dropped-item components. Check the
    // engine's reflected offset before reading any version-specific SDK field.
    bool LootLayoutSafe()
    {
        if (GLootLayoutChecked) return GLootLayoutSafe;
        UClass* Cls = UDroppedItemInteractionComponent::StaticClass();
        UClass* PropertyClass = UObjectProperty::StaticClass();
        if (!Cls || !PropertyClass) return false; // may be loaded later in the lobby
        GLootLayoutChecked = true;
        for (UField* Field = Cls->Children; Field; Field = Field->Next)
        {
            if (Field->GetName() != "Item" || !Field->IsA(PropertyClass)) continue;
            auto* Property = static_cast<UProperty*>(Field);
            GLootLayoutSafe = Property->Offset == offsetof(UDroppedItemInteractionComponent, Item);
            break;
        }
        BLOG(std::string("BOT LOOT item layout ") + (GLootLayoutSafe ? "verified" : "unavailable; existing weapon behavior retained"));
        return GLootLayoutSafe;
    }

    bool LiveLoot(const FLootRef& L)
    {
        return LiveBotObject(L.Component, L.Index) && ObjectSerial(L.Index) == L.Serial &&
            L.Component->IsA(UDroppedItemInteractionComponent::StaticClass());
    }

    bool Readable(const void* P, size_t N);
    bool InGameExe(const void* P);

    bool LiveDoor(const FDoorRef& D)
    {
        return LiveBotObject(D.Component, D.Index) && ObjectSerial(D.Index) == D.Serial &&
            D.Component->IsA(UDoorInteractionComponent::StaticClass());
    }

    bool DoorLayoutSafe()
    {
        if (GDoorLayoutChecked) return GDoorLayoutSafe;
        UClass* DoorClass = UDoorInteractionComponent::StaticClass();
        UClass* ByteClass = UByteProperty::StaticClass();
        UClass* EnumClass = UEnumProperty::StaticClass();
        UClass* PropertyClass = UProperty::StaticClass();
        if (!DoorClass || !ByteClass || !PropertyClass) return false;
        GDoorLayoutChecked = true;
        int Count = 0;
        for (UField* F = DoorClass->Children; F && Count++ < 512; F = F->Next) {
            if (F->GetName() != "DoorState" || !F->IsA(PropertyClass)) continue;
            auto* P = static_cast<UProperty*>(F);
            BLOG("BOT DOOR reflection DoorState type=" + F->GetClass()->GetName() +
                " offset=" + std::to_string(P->Offset) + " bytes=" + std::to_string(P->ElementSize));
            const bool Typed = F->IsA(ByteClass) || (EnumClass && F->IsA(EnumClass));
            if (Typed && P->ArrayDim == 1 && P->ElementSize == 1 &&
                P->Offset >= (int)sizeof(UObject) && P->Offset < DoorClass->Size && P->Offset < 8192)
                GDoorStateOffset = P->Offset;
        }
        GDoorLayoutSafe = GDoorStateOffset == 0x5D0; // also required by the verified native handler
        BLOG(std::string("BOT DOOR layout ") + (GDoorLayoutSafe ? "verified from runtime reflection" : "unavailable; interaction skipped"));
        return GDoorLayoutSafe;
    }

    int DoorStateValue(const UDoorInteractionComponent* Door)
    {
        if (!Door || !GDoorLayoutSafe || GDoorStateOffset < 0) return -1;
        const auto* Address = reinterpret_cast<const uint8*>(Door) + GDoorStateOffset;
        if (!Readable(Address, 1)) return -1;
        const int Value = *Address;
        return Value >= 0 && Value < int(EDoorState::EDoorState_MAX) ? Value : -1;
    }

    // The generated SDK has no declaration for the native interaction virtuals.
    // Resolved from this executable's controller path and the door constructor:
    // native IInteractionInterface is at +0x498; +0x88 is the door's physical
    // interaction handler. +0x78 only broadcasts a separate notification.
    // See DOOR_NATIVE_FINDINGS_EN.md. Fail closed on a different game build.
    bool DoorNativeLayoutSafe()
    {
        if (GDoorNativeChecked) return GDoorNativeSafe;
        GDoorNativeChecked = true;
        const uintptr_t Base = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
        static const uint8 CastBytes[] = {
            0x48,0x89,0x74,0x24,0x18,0x48,0x89,0x7c,0x24,0x20,0x41,0x56,0x48,0x83,0xec,0x20,
            0x33,0xf6,0x48,0x8b,0xfa,0x4c,0x8b,0xf1,0x48,0x85,0xd2,0x0f,0x84,0x1d,0x01,0x00};
        static const uint8 InteractBytes[] = {
            0x4c,0x8b,0xdc,0x57,0x48,0x83,0xec,0x70,0x49,0xc7,0x43,0xc8,0xfe,0xff,0xff,0xff,
            0x49,0x89,0x5b,0x10,0x4c,0x8b,0xd2,0x48,0x8b,0xd9,0x48,0x8b,0x89,0x10,0xfd,0xff,
            0xff,0x80,0xb9,0xe8,0x01,0x00,0x00,0x03,0x0f,0x84,0x23,0x01,0x00,0x00};
        static const uint8 OpenBytes[] = {
            0x0f,0xb6,0x93,0x38,0x01,0x00,0x00,0x8d,0x42,0xff,0xa8,0xfd,0x74,0x74,0x48,0x83,
            0xbb,0x48,0x01,0x00,0x00,0x00,0x74,0x6a,0x84,0xd2,0x75,0x3b,0xc6,0x83,0x38,0x01,
            0x00,0x00,0x03};
        const auto Matches = [Base](uintptr_t Rva, const uint8* Bytes, size_t N) {
            const void* P = reinterpret_cast<const void*>(Base + Rva);
            return InGameExe(P) && InGameExe(reinterpret_cast<const uint8*>(P) + N - 1) &&
                Readable(P, N) && std::memcmp(P, Bytes, N) == 0;
        };
        GDoorNativeSafe = Matches(0xC5E380, CastBytes, sizeof(CastBytes)) &&
            Matches(0xDD63B0, InteractBytes, sizeof(InteractBytes)) &&
            Matches(0xDD6501, OpenBytes, sizeof(OpenBytes));
        BLOG(std::string("BOT DOOR native handler ") + (GDoorNativeSafe ? "signature verified" : "signature mismatch; interaction skipped"));
        return GDoorNativeSafe;
    }

    bool DispatchDoorInteraction(UDoorInteractionComponent* Door, ATslCharacter* Character)
    {
        if (!Bots::IsGameThread() || !Door || !Character || !DoorLayoutSafe() || !DoorNativeLayoutSafe()) return false;
        AActor* Owner = Door->GetOwner();
        if (!Owner || !LiveBotObject(Owner, Owner->GetIndex()) || !Owner->HasAuthority()) return false;
        UClass* InterfaceClass = IInteractionInterface::StaticClass();
        if (!InterfaceClass || Door->IsDefaultObject() || DoorStateValue(Door) != int(EDoorState::Closed)) return false;
        const uintptr_t Base = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
        using FGetNativeInterface = void* (*)(UObject*, UClass*);
        const auto GetInterface = reinterpret_cast<FGetNativeInterface>(Base + 0xC5E380);
        void* Interface = GetInterface(Door, InterfaceClass);
        if (Interface != reinterpret_cast<uint8*>(Door) + 0x498 || !Readable(Interface, sizeof(void*))) return false;
        auto* Table = *reinterpret_cast<uintptr_t**>(Interface);
        if (reinterpret_cast<uintptr_t>(Table) != Base + 0x4F23530 || !Readable(Table, 0x90) ||
            Table[0x88 / sizeof(void*)] != Base + 0xDD63B0) return false;
        if (GDoorDispatchLogs++ < 8) BLOG("BOT DOOR native interface verified; dispatch=physical-interaction");
        // The native routine chooses direction, changes state, drives movement
        // and notifies replication. Never write DoorState in the add-on, or call
        // both notification and physical paths (which could toggle twice).
        using FInteract = void (*)(void*, ATslCharacter*);
        reinterpret_cast<FInteract>(Table[0x88 / sizeof(void*)])(Interface, Character);
        return true; // request sent; the caller separately records actual state
    }

    void RefreshLootSites()
    {
        GLootClusterCounts.clear(); GLootLandingSites.clear();
        // Choose live representatives anew; picking up the former third item
        // must not permanently delete the whole building from landing plans.
        for (const auto& L : GLoot) {
            if (!LiveLoot(L) || !L.Component->Item) continue;
            const int32 X=(int32)std::floor(L.Location.X/5000.f), Y=(int32)std::floor(L.Location.Y/5000.f);
            const uint64 Key=((uint64)(uint32)X<<32)|(uint32)Y;
            if (++GLootClusterCounts[Key]==3 && GLootLandingSites.size()<4000) GLootLandingSites.push_back(L);
        }
    }

    void ScanGroundLoot(double T, bool LootEnabled)
    {
        if (!LootLayoutSafe()) return;
        auto* Array=UObject::GObjects.GetTypedPtr();
        UClass* LootClass=UDroppedItemInteractionComponent::StaticClass();
        UClass* DoorClass=UDoorInteractionComponent::StaticClass();
        if (!Array || !LootClass) return;
        const int32 Count=Array->Num();
        if (GLootTailIndex<0 || GLootTailIndex>Count) GLootTailIndex=Count;
        auto Cache=[&](UObject* Object, int32 ObjectIndex) {

            if (!Object || !LiveBotObject(Object,ObjectIndex) || Object->IsDefaultObject()) return;
            if (LootEnabled && Object->IsA(ATslWheeledVehicle::StaticClass())) {
                const int32 Serial = ObjectSerial(ObjectIndex);
                auto It = GZoneVehicleIndices.find(ObjectIndex);
                if (It == GZoneVehicleIndices.end() || It->second != Serial) {
                    GZoneVehicleIndices[ObjectIndex] = Serial;
                    GZoneVehicles.push_back({static_cast<ATslWheeledVehicle*>(Object), ObjectIndex, Serial});
                }
                return;
            }
            if (LootEnabled && Object->IsA(ATslFloatingVehicle::StaticClass())) {
                const int32 Serial=ObjectSerial(ObjectIndex); auto It=GZoneBoatIndices.find(ObjectIndex);
                if (It==GZoneBoatIndices.end() || It->second!=Serial) {
                    GZoneBoatIndices[ObjectIndex]=Serial;
                    GZoneBoats.push_back({static_cast<ATslFloatingVehicle*>(Object),ObjectIndex,Serial});
                }
                return;
            }
            if (DoorClass && Object->IsA(DoorClass)) {
                const int32 Serial = ObjectSerial(ObjectIndex);
                const auto Found = GDoorIndices.find(ObjectIndex);
                if (Found == GDoorIndices.end() || Found->second != Serial) {
                    auto* Door = static_cast<UDoorInteractionComponent*>(Object);
                    const FVector P = Door->K2_GetComponentLocation();
                    if (std::isfinite(P.X) && std::isfinite(P.Y) && std::isfinite(P.Z)) {
                        GDoorIndices[ObjectIndex] = Serial;
                        GDoors.push_back({Door, ObjectIndex, Serial, P});
                        GDoorCells[LootSpatial::Key(LootSpatial::Cell(P.X), LootSpatial::Cell(P.Y))].push_back(GDoors.size() - 1);
                    }
                }
                return;
            }
            if (!LootEnabled || !Object->IsA(LootClass)) return;
            auto* Component = static_cast<UDroppedItemInteractionComponent*>(Object);
            const int32 Serial = ObjectSerial(ObjectIndex);
            const auto Found = GLootIndices.find(ObjectIndex);
            if (Found != GLootIndices.end() && Found->second == Serial) return;
            AActor* Owner = Component->GetOwner();
            if (!Owner || !Owner->IsA(ADroppedItemGroup::StaticClass()) || !Component->Item) return;
            const FVector P = Component->K2_GetComponentLocation();
            if (!std::isfinite(P.X) || !std::isfinite(P.Y) || !std::isfinite(P.Z) || P.Z < -5000.f) return;
            GLootIndices[ObjectIndex] = Serial;
            GLoot.push_back({Component, ObjectIndex, Serial, P});
            GLootCells[LootSpatial::Key(LootSpatial::Cell(P.X), LootSpatial::Cell(P.Y))].push_back(GLoot.size() - 1);
            // Three or more loot objects in a 50 m cell indicate a useful
            // landing area. This is a proxy, not proof of a building.
            const int32 CX = (int32)std::floor(P.X / 5000.f), CY = (int32)std::floor(P.Y / 5000.f);
            const uint64 Key = ((uint64)(uint32)CX << 32) | (uint32)CY;
            int& CountInCell = GLootClusterCounts[Key];
            if (++CountInCell == 3) {
                if (GBuildingTargets.size()<4000) GBuildingTargets.push_back(P);
                if (GLootLandingSites.size()<4000) GLootLandingSites.push_back({Component,ObjectIndex,Serial,P});
            }
        };
        const double TailStart=Now();
        for (int N=0; GLootTailIndex<Count && N<4096; ++N) {
            const int32 I=GLootTailIndex++; Cache(Array->GetByIndex(I),I);
            if ((N&31)==31 && Now()-TailStart>0.001) break;
        }
        // Item actors are registered late during map setup, behind a large
        // backlog of older objects. While the landing index is sparse, scan
        // newer objects backwards in an extra time-bounded pass. The ordinary
        // forward scans remain responsible for complete coverage and cleanup.
        if (LootEnabled && GLootLandingSites.size()<300 && Count>=500000) {
            if (GLootPriorityIndex<0 || Count-GLootPrioritySnapshot>=16384) {
                GLootPriorityIndex=Count-1; GLootPrioritySnapshot=Count;
            }
            const double PriorityStart=Now();
            for (int N=0; GLootPriorityIndex>=0 && N<4096; ++N) {
                const int32 I=GLootPriorityIndex--; Cache(Array->GetByIndex(I),I);
                if ((N&31)==31 && Now()-PriorityStart>0.001) break;
            }
        }
        if (T>=GLootRescanAt) {
            const double SweepStart=Now();
            for (int N=0; GLootScanIndex<Count && N<8192; ++N) {
                const int32 I=GLootScanIndex++; Cache(Array->GetByIndex(I),I);
                if ((N&31)==31 && Now()-SweepStart>0.001) break;
            }
            if (GLootScanIndex>=Count) {
                GLootScanIndex=0; GLootRescanAt=T+2.0;
                GLoot.erase(std::remove_if(GLoot.begin(),GLoot.end(),[](const FLootRef& L) {
                    return !LiveLoot(L) || !L.Component->Item;
                }),GLoot.end());
                GLootIndices.clear(); GLootCells.clear();
                for (size_t I=0; I<GLoot.size(); ++I) {
                    const auto& L=GLoot[I]; GLootIndices[L.Index]=L.Serial;
                    GLootCells[LootSpatial::Key(LootSpatial::Cell(L.Location.X),LootSpatial::Cell(L.Location.Y))].push_back(I);
                }
                GZoneVehicles.erase(std::remove_if(GZoneVehicles.begin(),GZoneVehicles.end(),[](const FZoneVehicle& V) {
                    return !LiveBotObject(V.Vehicle,V.Index) || ObjectSerial(V.Index)!=V.Serial;
                }),GZoneVehicles.end());
                GZoneVehicleIndices.clear();
                for (const auto& V:GZoneVehicles) GZoneVehicleIndices[V.Index]=V.Serial;
                GZoneBoats.erase(std::remove_if(GZoneBoats.begin(),GZoneBoats.end(),[](const FZoneBoat& V) {
                    return !LiveBotObject(V.Vehicle,V.Index) || ObjectSerial(V.Index)!=V.Serial;
                }),GZoneBoats.end());
                GZoneBoatIndices.clear(); for (const auto& V:GZoneBoats) GZoneBoatIndices[V.Index]=V.Serial;
                GDoors.erase(std::remove_if(GDoors.begin(),GDoors.end(),[](const FDoorRef& D) {return !LiveDoor(D);}),GDoors.end());
                GDoorIndices.clear(); GDoorCells.clear();
                for (size_t I=0; I<GDoors.size(); ++I) {
                    const auto& D=GDoors[I]; GDoorIndices[D.Index]=D.Serial;
                    GDoorCells[LootSpatial::Key(LootSpatial::Cell(D.Location.X),LootSpatial::Cell(D.Location.Y))].push_back(I);
                }
                GNextLootSiteRefresh=0;
                if (LootEnabled) BLOG("BOT LOOT scan complete items="+std::to_string(GLoot.size())+" sites="+
                    std::to_string(GLootLandingSites.size())+" objects="+std::to_string(Count));
            }
        }
        if (LootEnabled && T>=GNextLootSiteRefresh) {GNextLootSiteRefresh=T+10.0; RefreshLootSites();}
        if (LootEnabled && T>=GNextLootCacheStatus) {
            GNextLootCacheStatus=T+10.0;
            BLOG("BOT LOOT readiness items="+std::to_string(GLoot.size())+" sites="+std::to_string(GLootLandingSites.size())+
                " recent_backlog="+std::to_string(Count-GLootTailIndex)+" sweep="+std::to_string(GLootScanIndex)+"/"+std::to_string(Count));
        }
    }

    double GEntranceTraceAt = 0;
    bool EntranceLineClear(UWorld* World, ATslCharacter* Pawn, const FVector& A, const FVector& Z)
    {
        AActor* Actors[] = { Pawn }; TArray<AActor*> Ignore(Actors, 1, 1);
        FHitResult Hit{}; FLinearColor Color{};
        return !UKismetSystemLibrary::LineTraceSingle(World, A, Z, ETraceTypeQuery::TraceTypeQuery1,
            false, Ignore, EDrawDebugTrace::None, &Hit, true, Color, Color, 0.f) || !Hit.bBlockingHit;
    }

    FVector LootEntranceGoal(UWorld* World, FBot& B, const FVector& Here, const FVector& Loot, double T)
    {
        if (B.EntranceLoot != B.LootTargetIndex || B.EntranceLootSerial != B.LootTargetSerial) {
            B.EntranceStage = 0; B.EntranceSearchAt = 0; B.EntranceAvoidDoor = -1;
            B.EntranceLoot = B.LootTargetIndex; B.EntranceLootSerial = B.LootTargetSerial;
        }
        if (B.EntranceStage) {
            const auto It = GDoorIndices.find(B.EntranceDoor);
            if (T > B.EntranceUntil || It == GDoorIndices.end() || It->second != B.EntranceDoorSerial) {
                B.EntranceAvoidDoor = B.EntranceDoor; B.EntranceAvoidSerial = B.EntranceDoorSerial;
                B.EntranceAvoidUntil = T + 30.0;
                B.EntranceStage = 0; B.EntranceSearchAt = T + 5.0;
            } else {
                if (B.EntranceStage == 1 && Len(Flat(Here - B.EntranceNear)) < 150.f) {
                    B.EntranceStage = 2; B.NextLootMove = 0; B.LootTargetUntil = T + 20.0;
                }
                if (B.EntranceStage == 2 && Len(Flat(Here - B.EntranceFar)) < 140.f) {
                    B.EntranceAvoidDoor = B.EntranceDoor; B.EntranceAvoidSerial = B.EntranceDoorSerial;
                    B.EntranceAvoidUntil = T + 30.0;
                    B.EntranceStage = 0; B.EntranceSearchAt = T + 5.0;
                    B.NextLootMove = 0; B.LootBestDistance = 1e9f;
                    BLOG("BOT ENTRANCE passed " + B.Name);
                } else return B.EntranceStage == 1 ? B.EntranceNear : B.EntranceFar;
            }
        }
        if (T < B.EntranceSearchAt || T < GEntranceTraceAt || Len(Flat(Loot - Here)) < 350.f) return Loot;
        B.EntranceSearchAt = T + 4.0; GEntranceTraceAt = T + 0.15;
        const FVector SightLoot = Loot + V(0,0,70.f);
        if (EntranceLineClear(World, B.Pawn, Here, SightLoot)) return Loot;
        // Only examine nearby cached doors. Visibility on BOTH sides is required;
        // never assume two nearby objects belong to the same building.
        std::vector<std::pair<float,size_t>> Candidates;
        const int32 X = LootSpatial::Cell(Loot.X), Y = LootSpatial::Cell(Loot.Y);
        for (int DX=-1; DX<=1; ++DX) for (int DY=-1; DY<=1; ++DY) {
            auto Bucket=GDoorCells.find(LootSpatial::Key(X+DX,Y+DY));
            if (Bucket==GDoorCells.end()) continue;
            for (size_t I: Bucket->second) {
                if (I>=GDoors.size()) continue;
                const auto& D=GDoors[I];
                if (D.Index==B.EntranceAvoidDoor && D.Serial==B.EntranceAvoidSerial && T<B.EntranceAvoidUntil) continue;
                float A=Len(Flat(D.Location-Here)), Z=Len(Flat(D.Location-Loot));
                if (A<5000.f && Z<2500.f && std::fabs(D.Location.Z-Loot.Z)<220.f)
                    Candidates.push_back({A+Z,I});
            }
        }
        std::sort(Candidates.begin(),Candidates.end());
        int Checked=0;
        for (auto C: Candidates) {
            if (++Checked>6) break;
            const auto D=GDoors[C.second]; if (!LiveDoor(D)) continue;
            const FVector Direction=SafeNormal(Flat(D.Location-Here));
            FVector Near=D.Location-Direction*200.f, Far=D.Location+Direction*200.f;
            FVector FloorNear{}, FloorFar{};
            if (!TraceDown(World,Near.X,Near.Y,Here.Z+150.f,Here.Z-300.f,FloorNear) ||
                !TraceDown(World,Far.X,Far.Y,Here.Z+150.f,Here.Z-300.f,FloorFar) ||
                std::fabs(FloorNear.Z-FloorFar.Z)>80.f) continue;
            Near.Z=FloorNear.Z+90.f; Far.Z=FloorFar.Z+90.f;
            if (!EntranceLineClear(World,B.Pawn,Here,Near) ||
                !EntranceLineClear(World,B.Pawn,Far,SightLoot)) continue;
            B.EntranceNear=Near; B.EntranceFar=Far; B.EntranceStage=1;
            B.EntranceDoor=D.Index; B.EntranceDoorSerial=D.Serial;
            B.EntranceUntil=T+30.0; B.LootTargetUntil=T+30.0; B.NextLootMove=0;
            BLOG("BOT ENTRANCE selected " + B.Name + " door=" + std::to_string(D.Index));
            return Near;
        }
        return Loot;
    }

    // Unwanted weapons and clothing are intentionally ignored in this first
    // candidate. A missing rifle never causes the bot to take a pistol as AR.
    int LootKind(UItem* Item)
    {
        if (!Item || !Item->IsA(UEquipableItem::StaticClass()) || Item->StackCount != 1) return 0;
        if (GhilliePolicy::IsGhillie(Item->GetClass()->GetName())) return 0;
        auto* Eq = static_cast<UEquipableItem*>(Item);
        if (Item->IsA(UWeaponItem::StaticClass()))
        {
            const std::string N = Lower(Item->GetClass()->GetName());
            auto* WeaponItem=static_cast<UWeaponItem*>(Item);
            UClass* WeaponClass=WeaponItem->WeaponType;
            if (BotWeaponPolicy::IsCarePackageWeapon(N) ||
                (WeaponClass && Readable(WeaponClass, sizeof(UObject)) &&
                 LiveBotObject(WeaponClass, WeaponClass->GetIndex()) &&
                 BotWeaponPolicy::IsCarePackageWeapon(WeaponClass->GetName()))) return 0;
            static const char* Rifle[] = { "ak47", "akm", "m416", "m16a4", "scar", "groza", "aug", "beryl", "m762", "qbz", "g36", "mutant" };
            for (const char* Name : Rifle) if (N.find(Name) != std::string::npos) return 1;
            return 0;
        }
        if (Eq->EquipSlotID == EEquipSlotID::Backpack) return 2;
        if (Eq->EquipSlotID == EEquipSlotID::TorsoArmor && Eq->DamageReductionRate > 0.f) return 3;
        if (Eq->EquipSlotID == EEquipSlotID::Head && Eq->DamageReductionRate > 0.f) return 4;
        return 0;
    }

    bool NeedLoot(FBot& B, UItem* Item, int Kind)
    {
        if (Kind == 1) {
            UWeaponProcessorComponent* WP = nullptr;
            return !B.Armed && !B.Pawn->GetWeaponGun() && FindGunSlot(B, WP, false) < 0;
        }
        auto* Inventory = B.Pawn->GetInventoryFacade();
        auto* Equipment = Inventory ? Inventory->GetEquipment() : nullptr;
        if (!Equipment || !Item || !Item->IsA(UEquipableItem::StaticClass())) return false;
        FEquipPosition P{}; P.SlotID = static_cast<UEquipableItem*>(Item)->EquipSlotID; P.Index = 0;
        auto* Equipped = Equipment->GetItem(P);
        // A cosmetic hat should not prevent looking for a protective helmet.
        return !Equipped || (Kind == 4 && Equipped->DamageReductionRate <= 0.f);
    }

    // Runs on the game thread, only while landed and out of combat. The
    // interaction component is removed only after a successful native pickup.
    bool TryLootNearby(UWorld* World, FBot& B, const FVector& Here, double T)
    {
        if (!LootLayoutSafe() || B.State != EBotState::Roam || !GMatchSpawnDone || B.NativeSkyActive || B.DropPending || B.Hidden ||
            B.Pawn->IsInAircraft() || B.Pawn->IsSwimming()) return false;
        auto* Move = B.Pawn->GetTslCharacterMovement();
        if (!Move || Move->MovementMode != EMovementMode::MOVE_Walking) return false;
        if (!B.LootStarted) B.LootStarted = T;
        FVector Zone{}; float Radius = 0.f;
        if (GetSafeCircle(World, Zone, Radius) && Len(Flat(Zone - Here)) > Radius * 1.02f &&
            T >= B.LootStarted + 25.0) {
            B.LootTargetIndex = -1; return false; // do not ignore an unsafe zone for loot
        }
        if (T < B.LootPauseUntil) return true;
        if (B.LootTargetIndex >= 0 && (T > B.LootTargetUntil || T > B.LootRouteStarted + 120.0)) {
            BLOG("BOT LOOT route abandoned " + B.Name + " target=" + std::to_string(B.LootTargetIndex));
            B.LootAvoidIndex = B.LootTargetIndex; B.LootAvoidSerial = B.LootTargetSerial;
            B.LootAvoidUntil = T + 45.0; B.LootTargetIndex = -1;
        }
        if (B.LootTargetIndex < 0 && T >= B.NextLootSearch)
        {
            B.NextLootSearch = T + RandF(3.f, 5.f);
            float Best = 10000.f;
            bool BestIsRifle = false;
            const int32 CellX = LootSpatial::Cell(Here.X), CellY = LootSpatial::Cell(Here.Y);
            for (int DX = -1; DX <= 1; ++DX) for (int DY = -1; DY <= 1; ++DY)
            {
                const auto Bucket = GLootCells.find(LootSpatial::Key(CellX + DX, CellY + DY));
                if (Bucket == GLootCells.end()) continue;
                for (size_t Index : Bucket->second) {
                if (Index >= GLoot.size()) continue;
                const FLootRef& L = GLoot[Index];
                const float Dist = Len(Flat(L.Location - Here));
                if (Dist > 10000.f || std::abs(L.Location.Z - Here.Z) > 250.f || !LiveLoot(L)) continue;
                if (L.Index == B.LootAvoidIndex && L.Serial == B.LootAvoidSerial && T < B.LootAvoidUntil) continue;
                UItem* Item = L.Component->Item;
                if (!Item || !LiveBotObject(Item, Item->GetIndex())) continue;
                const int Kind = LootKind(Item);
                if (!Kind || !NeedLoot(B, Item, Kind)) continue;
                // Any eligible rifle in the search range takes precedence over
                // armor; distance breaks ties within the same priority class.
                const bool IsRifle = Kind == 1;
                if ((IsRifle && !BestIsRifle) || (IsRifle == BestIsRifle && Dist < Best)) {
                    Best = Dist; BestIsRifle = IsRifle;
                    B.LootTargetIndex = L.Index; B.LootTargetSerial = L.Serial;
                }
                }
            }
            B.LootTargetUntil = T + 20.0;
            B.LootRouteStarted = T; B.LootBestDistance = 1e9f;
            if (B.LootTargetIndex >= 0) {
                B.NextLootMove = 0; B.DetourUntil = 0; B.AvoidSideUntil = 0;
                BLOG("BOT LOOT route selected " + B.Name + " target=" + std::to_string(B.LootTargetIndex) + " priority=" + (BestIsRifle ? "rifle" : "equipment"));
            }
        }
        if (B.LootTargetIndex < 0 || T > B.LootTargetUntil) return false;
        auto* Array = UObject::GObjects.GetTypedPtr();
        if (!Array || B.LootTargetIndex >= Array->Num()) { B.LootTargetIndex = -1; return false; }
        UObject* O = Array->GetByIndex(B.LootTargetIndex);
        if (!O || !LiveBotObject(O, B.LootTargetIndex) || ObjectSerial(B.LootTargetIndex) != B.LootTargetSerial ||
            !O->IsA(UDroppedItemInteractionComponent::StaticClass())) { B.LootTargetIndex = -1; return false; }
        auto* Component = static_cast<UDroppedItemInteractionComponent*>(O);
        UItem* Item = Component->Item;
        if (!Item || !LiveBotObject(Item, Item->GetIndex())) { B.LootTargetIndex = -1; return false; }
        const int Kind = LootKind(Item);
        if (!Kind || !NeedLoot(B, Item, Kind)) { B.LootTargetIndex = -1; return false; }
        const FVector Destination = Component->K2_GetComponentLocation();
        const float Dist = Len(Flat(Destination - Here));
        // Keep a productive route; only measurable progress renews its deadline.
        if (Dist + 300.f < B.LootBestDistance) {
            B.LootBestDistance = Dist;
            B.LootTargetUntil = T + 20.0;
        }
        if (Dist <= 200.f && std::abs(Destination.Z - Here.Z) < 220.f)
        {
            AActor* Owner = Component->GetOwner();
            if (!Owner || !Owner->IsA(ADroppedItemGroup::StaticClass()) || !Component->AllowInteractBy(B.Pawn)) {
                B.LootAvoidIndex = B.LootTargetIndex; B.LootAvoidSerial = B.LootTargetSerial;
                B.LootAvoidUntil = T + 45.0; B.LootTargetIndex = -1; B.NextLootSearch = T + 3.0; return false;
            }
            const std::string ItemName = Item->GetClass()->GetName();
            const bool Accepted = B.Pawn->PickUpItem(Item, FName{}, FString(L""));
            if (Accepted && LiveBotObject(Component, B.LootTargetIndex) && ObjectSerial(B.LootTargetIndex) == B.LootTargetSerial)
                Owner->K2_DestroyComponent(Component);
            BLOG("BOT LOOT " + B.Name + " " + ItemName + (Accepted ? " accepted" : " rejected"));
            if (!Accepted) { B.LootAvoidIndex = B.LootTargetIndex; B.LootAvoidSerial = B.LootTargetSerial; B.LootAvoidUntil = T + 45.0; }
            B.LootTargetIndex = -1; B.LootPauseUntil = T + RandF(0.8f, 1.4f); B.NextLootSearch = B.LootPauseUntil;
            B.HasDest = false; B.Ctrl->StopMovement();
            B.NextArmTry = T + 0.3;
            B.DetourUntil = 0; SetSprint(B, false);
            if (Move) Move->StopMovementImmediately();
            return true;
        }
        const FVector RouteGoal = LootEntranceGoal(World, B, Here, Destination, T);
        if (!B.HasDest || T >= B.NextLootMove) {
            B.NextLootMove = T + 2.0;
            MoveTo(World, B, RouteGoal, T, false);
            SetSprint(B, Dist > 2500.f);
        }
        return true;
    }

    FVector LandLeg(UWorld* World, const FVector& From, const FVector& To, float MaxTurn);
    bool TryReachLootArea(UWorld* World, FBot& B, const FVector& Here, double T)
    {
        if (T>=B.NextKitCheck) {
            B.NextKitCheck=T+5.0;
            auto* Facade=B.Pawn->GetInventoryFacade();
            auto* Equipment=Facade ? Facade->GetEquipment() : nullptr;
            auto Has=[&](EEquipSlotID Slot, bool Armor) {
                if (!Equipment) return false;
                FEquipPosition Position{}; Position.SlotID=Slot; Position.Index=0;
                auto* Item=Equipment->GetItem(Position);
                return Item && (!Armor || Item->DamageReductionRate>0.f);
            };
            B.NeedsEquipment=!B.Armed || !Has(EEquipSlotID::Backpack,false) ||
                !Has(EEquipSlotID::TorsoArmor,true) || !Has(EEquipSlotID::Head,true);
        }
        if (!B.NeedsEquipment) {B.HasLootArea=false; return false;}
        if (B.HasLootArea) {
            if (!B.LootAreaUntil) {B.LootAreaUntil=T+180; B.LootAreaProgressAt=T+20;}
            const float Distance=Len(Flat(B.LootArea-Here));
            if (Distance+300.f<B.LootAreaBestDistance) {B.LootAreaBestDistance=Distance; B.LootAreaProgressAt=T+20;}
            if (T>B.LootAreaUntil || T>B.LootAreaProgressAt || Distance<7000.f) {
                B.AvoidLootArea=B.LootArea; B.LootAreaAvoidUntil=T+90;
                B.HasLootArea=false; B.NextAreaSearch=T; B.HasDest=false;
            }
        }
        if (!B.HasLootArea && T>=B.NextAreaSearch) {
            B.NextAreaSearch=T+5.0;
            float Best=60000.f; const FLootRef* Chosen=nullptr;
            FVector Circle{}; float Radius=0;
            const bool HasZone=GetSafeCircle(World,Circle,Radius);
            for (const auto& Site:GLootLandingSites) {
                const float Distance=Len(Flat(Site.Location-Here));
                if (Distance<6500.f || Distance>=Best || (T<B.LootAreaAvoidUntil && Len(Flat(Site.Location-B.AvoidLootArea))<6000.f) ||
                    (HasZone && Len(Flat(Site.Location-Circle))>Radius*0.95f) || !LiveLoot(Site) || !Site.Component->Item) continue;
                Best=Distance; Chosen=&Site;
            }
            if (Chosen) {
                B.HasLootArea=true; B.LootArea=Chosen->Location; B.LootAreaUntil=T+180;
                B.LootAreaProgressAt=T+20; B.LootAreaBestDistance=Best; B.NextAreaMove=0; B.HasDest=false;
                BLOG("BOT AREA " + B.Name + " target="+Pos(B.LootArea));
            }
        }
        if (!B.HasLootArea) return false;
        SetObjective(B,"reach-loot-area");
        if (!B.HasDest || T>=B.NextAreaMove) {
            B.NextAreaMove=T+3.0;
            FVector Goal=B.LootArea; const auto Delta=Flat(Goal-Here); const float Distance=Len(Delta);
            if (Distance>15000.f) Goal=Here+SafeNormal(Delta)*15000.f;
            Goal=LandLeg(World,Here,Goal,60.f);
            MoveTo(World,B,Goal,T,false); SetSprint(B,Distance>2500.f);
        }
        return true;
    }

    // straight line A->B stays over land (samples every 50 m; bridges count as land)
    bool PathOverLand(UWorld* World, const FVector& A, const FVector& Bp)
    {
        const float DX = Bp.X - A.X, DY = Bp.Y - A.Y;
        const float L = std::sqrt(DX * DX + DY * DY);
        const int N = (std::min)(16, (int)(L / 5000.f));
        for (int i = 1; i <= N; ++i)
        {
            const float t = (float)i / (float)(N + 1);
            FVector G;
            if (!TraceDown(World, A.X + DX * t, A.Y + DY * t, 60000.f, -30000.f, G)) continue;
            if (IsWaterGround(G.Z)) return false;
        }
        FVector G;
        if (TraceDown(World, Bp.X, Bp.Y, 60000.f, -30000.f, G) && IsWaterGround(G.Z)) return false;
        return true;
    }

    // keep a roam leg out of the water: try turning it left / right a bit
    FVector LandLeg(UWorld* World, const FVector& From, const FVector& To, float MaxTurn)
    {
        if (PathOverLand(World, From, To)) return To;
        const float DX = To.X - From.X, DY = To.Y - From.Y;
        static const float Turns[] = { 30.f, -30.f, 60.f, -60.f, 90.f, -90.f, 120.f, -120.f };
        for (float Deg : Turns)
        {
            if (std::fabs(Deg) > MaxTurn) continue;
            const float A = Deg * kPi / 180.f, C = std::cos(A), Sn = std::sin(A);
            const FVector P = V(From.X + DX * C - DY * Sn, From.Y + DX * Sn + DY * C, To.Z);
            if (PathOverLand(World, From, P)) return P;
        }
        // shorter leg straight ahead (up to the shore)
        const FVector Half = V(From.X + DX * 0.4f, From.Y + DY * 0.4f, To.Z);
        if (PathOverLand(World, From, Half)) return Half;
        return To;
    }

    bool IsGhostChar(ATslCharacter* C)
    {
        AController* Ct = C->Controller;
        return Ct && Ct->IsA(APlayerController::StaticClass()) && static_cast<APlayerController*>(Ct)->bIsLocalPlayerController;
    }

    ATslCharacter* NearestHuman(const FVector& Loc, float MaxDist, float& OutDist, FVector& OutLoc)
    {
        ATslCharacter* Best = nullptr;
        OutDist = MaxDist;

        for (const FCharInfo& C : GChars)
        {
            if (C.IsBot || IsGhostChar(C.Char)) continue;
            const float D = Len(C.Loc - Loc);

            if (D < OutDist)
            { 
                OutDist = D; OutLoc = C.Loc; Best = C.Char;
            }
        }

        return Best;
    }

    FVector PickRoamDestEx(UWorld* World, FBot& B, const FVector& MyLoc, bool& bRun)
    {
        bRun = false;
        FVector ZC{}; float ZR = 0.f;
        const bool bZone = S.FollowZone && GetSafeCircle(World, ZC, ZR);
        auto Jitter = [](float R) { const float A = RandF(0.f, 2.f * kPi), D = RandF(0.f, R); return V(std::cos(A) * D, std::sin(A) * D, 0.f); };

        if (B.ZoneStyle < 0) B.ZoneStyle = RandF(0.f, 1.f) < S.LateZoneShare ? 1 : 0;
        FVector BC{}; float BR = 0.f;
        const bool bBlue = bZone && GetBlueCircle(World, BC, BR);
        // late bots: happy out in the blue until the gas wall is close (or they're already in it)
        const bool bLateOk = B.ZoneStyle == 1 && bBlue && BR > ZR * 1.05f && Len(Flat(BC - MyLoc)) < BR - S.LateZoneMargin;

        // 1) outside (or near the edge of) the safe zone, or told to get off the coast: run inward
        if (bZone)
        {
            const FVector ToZ = Flat(ZC - MyLoc);
            const float D = Len(ToZ);
            if ((D > ZR * 0.8f && !bLateOk) || B.InlandNext)
            {
                B.InlandNext = false;
                bRun = true;
                const float Leg = (std::clamp)(D - ZR * 0.5f, 3000.f, S.ZoneLeg);
                const FVector P = LandLeg(World, MyLoc, MyLoc + SafeNormal(ToZ) * Leg + Jitter(1500.f), 90.f);
                B.Home = P; B.HasHome = GMatchSpawnDone;   // the zone moved this bot: its area moves with it
                return P;
            }
        }
        else if (B.InlandNext)
        {
            B.InlandNext = false;
            bRun = true;
            return MyLoc + SafeNormal(Flat(V(408000.f, 408000.f, MyLoc.Z) - MyLoc)) * 15000.f + Jitter(1500.f);
        }

        if (GMatchSpawnDone && !B.HasHome) { B.HasHome = true; B.Home = MyLoc; }

        // 2) drift toward the nearest player (once landed) - hunters; normal bots only rarely (DriftChance)
        if (!S.HumanlikeGoals && S.HuntPlayers && GMatchSpawnDone && RandF(0.f, 1.f) < (B.Hunter ? S.HuntChance : S.DriftChance))
        {
            float HD; FVector HL;
            if (NearestHuman(MyLoc, S.HuntDistance, HD, HL) && HD > 12000.f)
            {
                const float Leg = (std::min)(HD - 8000.f, 25000.f);
                FVector P = LandLeg(World, MyLoc, MyLoc + SafeNormal(Flat(HL - MyLoc)) * Leg + Jitter(3000.f), 60.f);
                if (!bZone || bLateOk || Len(Flat(P - ZC)) < ZR * 0.9f)
                {
                    bRun = HD > 40000.f;
                    return P;
                }
            }
        }

        // 3) wander, kept inside the zone; normal bots stay around their own area so they don't pile up
        const float A = RandF(0.f, 2.f * kPi), R = RandF(S.RoamRadius * 0.3f, S.RoamRadius);
        FVector P = MyLoc + V(std::cos(A) * R, std::sin(A) * R, 0.f);
        if (B.HasHome && !B.Hunter && Len(Flat(P - B.Home)) > S.RoamRadius * 1.5f)
        {
            const float A2 = RandF(0.f, 2.f * kPi), R2 = RandF(0.f, S.RoamRadius);
            P = V(B.Home.X + std::cos(A2) * R2, B.Home.Y + std::sin(A2) * R2, MyLoc.Z);
        }
        if (bZone && !bLateOk && Len(Flat(P - ZC)) > ZR * 0.8f)
            P = MyLoc + SafeNormal(Flat(ZC - MyLoc)) * R;
        if (bLateOk && Len(Flat(P - BC)) > BR - S.LateZoneMargin)   // don't wander into the gas
            P = MyLoc + SafeNormal(Flat(BC - MyLoc)) * R;
        return LandLeg(World, MyLoc, P, 120.f);
    }

    FVector PickRoamDest(UWorld* World, const FVector& MyLoc)
    {
        FVector Center = MyLoc;
        float Radius = S.RoamRadius;

        if (S.FollowZone)
        {
            AGameStateBase* GSB = UGameplayStatics::GetGameState(World);
            ATslGameState* GS = (GSB && GSB->IsA(ATslGameState::StaticClass())) ? static_cast<ATslGameState*>(GSB) : nullptr;
            if (GS && GS->SafetyZoneRadius > 1000.f)
            {
                FVector Zone = GS->SafetyZonePosition;
                Zone.Z = MyLoc.Z;
                const float DistToZone = Len(Zone - MyLoc);
                if (DistToZone > GS->SafetyZoneRadius * 0.75f)
                {
                    const FVector Dir = SafeNormal(Zone - MyLoc);
                    Center = MyLoc + Dir * (std::min)(DistToZone, S.RoamRadius * 1.5f);
                    Radius = S.RoamRadius * 0.4f;
                }
            }
        }

        const float A = RandF(0.f, 2.f * kPi), R = RandF(Radius * 0.3f, Radius);
        return Center + V(std::cos(A) * R, std::sin(A) * R, 0.f);
    }

    void ResolveGunDamageType()
    {
        GGunDamageResolved = true;
        if (!S.DamageTypeClass.empty())
            return; // set by hand in settings.ini
        TUObjectArray* Arr = UObject::GObjects.GetTypedPtr();
        UClass* TslDT = UTslDamageType::StaticClass();
        if (!Arr || !TslDT) return;
        UClass* Best = nullptr;
        std::string BestName;
        for (int32 i = 0; i < Arr->Num(); ++i)
        {
            UObject* O = Arr->GetByIndex(i);
            if (!O || !O->IsDefaultObject() || !O->IsA(TslDT)) continue;
            if (static_cast<UTslDamageType*>(O)->DamageTypeCategory != EDamageTypeCategory::Damage_Gun) continue;
            const std::string N = O->GetClass()->GetName();
            // prefer a plain bullet / gun type over special ones
            const bool bNice = N.find("Gun") != std::string::npos || N.find("Bullet") != std::string::npos;
            if (!Best || (bNice && BestName.find("Gun") == std::string::npos && BestName.find("Bullet") == std::string::npos))
            {
                Best = O->GetClass();
                BestName = N;
            }
        }
        if (Best)
        {
            GDamageType = TSubclassOf<UDamageType>(Best);
            BLOG("Bot gun damage type: " + BestName + " (blood / hit effects / kill icon)");
        }
        else
            BLOG("No gun damage type found - bot hits use the engine default (no blood effects)");
    }

    // fill the replicated "you got hit" info so the victim's client plays blood + the damage direction marker
    void ReplicateHitInfo(FBot& B, ATslCharacter* Target, float Damage, const FVector& From, const FVector& To, FName Bone, bool bKilled)
    {
        FTakeHitInfo& H = Target->LastTakeHitInfo;
        H.ActualDamage = Damage;
        H.DamageType = GDamageType;
        auto* Gun=B.Pawn ? B.Pawn->GetWeaponGun() : nullptr;
        float Ignored=0.f; TSubclassOf<UDamageType> Type(nullptr);
        if (SharedWeaponDamage::Resolve(Gun,Len(To-From),Ignored,Type)) H.DamageType=Type;
        APlayerState* PS = (B.Ctrl) ? B.Ctrl->PlayerState : nullptr;

        if (PS && PS->IsA(ATslPlayerState::StaticClass())) {
            SetWeak(H.PlayerInstigator, PS);
        }

        const FVector TL = Target->K2_GetActorLocation();
        static_cast<FVector&>(H.DamageOrigin) = From;
        static_cast<FVector&>(H.RelHitLocation) = To - TL;
        H.BoneName = Bone;
        H.DamageMaxRadius = 0.f;
        const FVector D = SafeNormal(To - From);
        const float Yaw = std::atan2(D.Y, D.X) * 180.f / kPi;
        const float Pitch = std::asin((std::clamp)(D.Z, -1.f, 1.f)) * 180.f / kPi;
        H.ShotDirPitch = (uint8)((int)std::lround(Pitch * 256.f / 360.f) & 0xFF);
        H.ShotDirYaw = (uint8)((int)std::lround(Yaw * 256.f / 360.f) & 0xFF);
        H.bPointDamage = 1;
        H.bRadialDamage = 0;
        H.bKilled = bKilled ? 1 : 0;
        H.EnsureReplicationByte = (uint8)(H.EnsureReplicationByte + 1);
        Target->ForceNetUpdate();
    }

    // true = this shot hit (damage applied)
    // can a bullet from From reach To? (ignores the shooter, the target and their guns)
    bool ClearShot(ATslCharacter* Me, ATslCharacter* Target, const FVector& From, const FVector& To)
    {
        AActor* Ign[4] = { Me, Target, nullptr, nullptr };
        int N = 2;
        if (ATslWeapon_Gun* G = Me->GetWeaponGun()) Ign[N++] = G;
        if (ATslWeapon_Gun* G = Target->GetWeaponGun()) Ign[N++] = G;
        TArray<AActor*> Ignore;
        FRawArray R{ Ign, N, N };
        std::memcpy(&Ignore, &R, sizeof(R));
        FHitResult Hit{};
        FLinearColor C{};
        const bool bHit = UKismetSystemLibrary::LineTraceSingle(Me, From, To, ETraceTypeQuery::TraceTypeQuery1, false, Ignore,
            EDrawDebugTrace::None, &Hit, true, C, C, 0.f);
        std::memset(&Ignore, 0, sizeof(Ignore));   // our stack buffer - nobody frees it
        return !(bHit && Hit.bBlockingHit);
    }

    int GCoverMisses = 0;
    std::unordered_map<int32, double> GBotHurtByBotAt;   // char index -> when one of our bots last damaged it
    std::unordered_map<int32, double> GHumanFiredAt;     // player char index -> last time they fired

    bool ResolveBotShot(FBot& B, ATslWeapon_Gun* Gun, ATslCharacter* Target, const FVector& MyLoc, const FVector& TLoc, float Dist)
    {
        if (IsSquadmate(B,Target)) return false;
        const double Tn = Now();
        // A shot is resolved geometrically from this weapon's deviation.
        // No warning shots, random hit percentage, scheduled windows or hit cooldown.
        CacheBones();
        const FVector From = MyLoc + V(0.f, 0.f, 60.f);
        // only body parts the bot can actually see can be hit - fully behind cover = a guaranteed miss
        FVector HeadP = TLoc + V(0.f, 0.f, 70.f), BodyP = TLoc + V(0.f, 0.f, 20.f);
        if (Target->Mesh)
        {
            HeadP = Target->Mesh->GetSocketLocation(GHeadBone);
            BodyP = Target->Mesh->GetSocketLocation(GSpineBone);
        }
        const bool bSeeHead = ClearShot(B.Pawn, Target, From, HeadP);
        const bool bSeeBody = ClearShot(B.Pawn, Target, From, BodyP);
        if (!bSeeHead && !bSeeBody)
        {
            if (S.CombatLog&& GCoverMisses < 20)
            {
                ++GCoverMisses;
                BLOG(B.Name + " would have hit " + CharName(Target) + " but they're behind cover - miss");
            }
            return false;
        }
        FVector FromMuzzle=From;
        if (auto* Mesh=Gun->GetWeaponMesh()) {
            if (!Gun->MuzzleAttachPoint.IsNone()) {
                const FVector Muzzle=Mesh->GetSocketLocation(Gun->MuzzleAttachPoint);
                if (std::isfinite(Muzzle.X) && std::isfinite(Muzzle.Y) && std::isfinite(Muzzle.Z) && Len(Muzzle-MyLoc)<500.f)
                    FromMuzzle=Muzzle;
            }
        }
        float Deviation=Gun->GetCurrentDeviation();
        const auto Config=Gun->GetWeaponDeviationConfigReplacement();
        const bool Scoped=B.Pawn->IsScoping();
        const float BaseDeviation=Scoped ? Config.DeviationBaseADS : Config.DeviationBase;
        if (!std::isfinite(Deviation) || Deviation<0.f || Deviation>90.f) Deviation=BaseDeviation;
        if (!std::isfinite(Deviation) || Deviation<0.f || Deviation>90.f) return false;
        // Intrinsic weapon MOA remains even if the current deviation is zero.
        if (std::isfinite(Config.MOA) && Config.MOA>0.f && Config.MOA<5400.f)
            Deviation=std::max(Deviation,Config.MOA/60.f);
        const FVector AimPoint=bSeeBody ? BodyP : HeadP;
        const FVector Aim=SafeNormal(AimPoint-FromMuzzle);
        const auto Spread=BotShotSpread::Sample({Aim.X,Aim.Y,Aim.Z},Deviation,RandF(0.f,1.f),RandF(0.f,1.f));
        const FVector Dir=V(Spread.x,Spread.y,Spread.z);
        if (Len(Dir)<0.9f) return false;
        const FVector End=FromMuzzle+Dir*std::max(Dist+1000.f,2000.f);
        AActor* Ign[]={B.Pawn,Gun};
        TArray<AActor*> Ignore; const FRawArray Raw{Ign,2,2};
        std::memcpy(&Ignore,&Raw,sizeof(Raw));
        FHitResult Hit{}; FLinearColor Color{};
        const bool HitSomething=UKismetSystemLibrary::LineTraceSingle(B.Pawn,FromMuzzle,End,ETraceTypeQuery::TraceTypeQuery1,
            true,Ignore,EDrawDebugTrace::None,&Hit,true,Color,Color,0.f);
        std::memset(&Ignore,0,sizeof(Ignore));
        UObject* HitActor=HitSomething ? SafeWeakGet(Hit.Actor) : nullptr;
        if (Tn>=B.NextShotGeometryLog) {
            B.NextShotGeometryLog=Tn+10.0;
            BLOG("BOT SHOT TRACE " + B.Name + " deviation="+std::to_string(Deviation)+" ads="+std::to_string(Scoped)+
                " hit="+(HitActor ? HitActor->GetName() : std::string("none"))+" target="+CharName(Target));
        }
        if (!HitSomething || !Hit.bBlockingHit || HitActor!=Target) return false;
        const FVector To=static_cast<FVector>(Hit.ImpactPoint);
        const float Travel=Len(To-FromMuzzle);
        const bool bHead=Hit.BoneName==GHeadBone;
        float Dmg=0.f; TSubclassOf<UDamageType> WeaponDamageType(nullptr);
        if (!SharedWeaponDamage::Resolve(Gun,Travel,Dmg,WeaponDamageType)) {
            if (Tn>=B.NextWhyLog) {B.NextWhyLog=Tn+10.0; BLOG("BOT DAMAGE missing weapon trajectory data " + B.Name);}
            return false;
        }
        const float Before = Target->Health;
        const uint8 RepBefore = Target->LastTakeHitInfo.EnsureReplicationByte;
        APlayerState* VPS = Target->GetTslPlayerState();
        const int32 VPSIndex=VPS ? VPS->GetIndex() : -1;
        const int32 VPSSerial=ObjectSerial(VPSIndex);
        ATslPlayerState* VTPS = (VPS && VPS->IsA(ATslPlayerState::StaticClass())) ? static_cast<ATslPlayerState*>(VPS) : nullptr;
        const float LastHitBefore = VTPS ? VTPS->LastHitTime : 0.f;
        // The native death broadcast can occur inside ApplyPointDamage.
        // Preserve exact victim/attacker identity only for this damage call.
        auto* PreviousVictim = GDamageVictimState;
        auto* PreviousKiller = GDamageBotKillerState;
        GDamageVictimState = VPS;
        auto* AttackerPS = LiveBotObject(B.Ctrl, B.CtrlIndex) ? B.Ctrl->PlayerState : nullptr;
        GDamageBotKillerState = AttackerPS && AttackerPS->IsA(ATslPlayerState::StaticClass())
            ? static_cast<ATslPlayerState*>(AttackerPS) : nullptr;
        UGameplayStatics::ApplyPointDamage(Target, Dmg, Dir, Hit, B.Ctrl, Gun, WeaponDamageType);
        GDamageVictimState = PreviousVictim;
        GDamageBotKillerState = PreviousKiller;
        if (!LiveBotObject(Target,B.TargetIndex)) return true;
        const float After = Target->Health;
        if (After < Before && !IsBotChar(Target)) B.LastHitPlayerAt = Tn;
        if (After < Before && IsBotChar(Target)) GBotHurtByBotAt[Target->GetIndex()] = Tn;
        // "attacked from here": the player state's attack marker (whatever UI this build drives from it)
        if (VTPS && LiveBotObject(VTPS,VPSIndex) && ObjectSerial(VPSIndex)==VPSSerial && After < Before)
        {
            static int MarkLogs = 0;
            const bool bGameDidIt = VTPS->LastHitTime != LastHitBefore;
            if (!bGameDidIt)
            {
                //static_cast<FVector&>(VTPS->CurrentAttackedPlayerLocation) = From;
                VTPS->LastHitTime = UGameplayStatics::GetTimeSeconds(Target);
                VTPS->ForceNetUpdate();
            }
            if (MarkLogs < 2) { ++MarkLogs; BLOG(std::string("Attack marker: ") + (bGameDidIt ? "the game sets it itself" : "set by us") + " for this hit"); }
        }
        // the game didn't fill its replicated hit info for this hit -> do it, so the victim sees blood / the hit marker
        if (Target && After < Before && Target->LastTakeHitInfo.EnsureReplicationByte == RepBefore)
            ReplicateHitInfo(B, Target, Before - After, From, To, Hit.BoneName, After <= 0.f);

        if (After < Before) ++GHitsApplied;
        else ++GHitsNoEffect;
        if (S.CombatLog && !IsBotChar(Target))
            BLOG(B.Name + " HIT " + CharName(Target) + (bHead ? " in the head" : "") + " for " + std::to_string((int)(Before - After)) +
                " (" + std::to_string((int)(Dist / 100.f)) + " m, hp " + std::to_string((int)After) + " left)");
        if (!GWarnedNoDamage && GHitsApplied == 0 && GHitsNoEffect >= 15)
        {
            GWarnedNoDamage = true;
            BLOG("WARNING: 15 bot hits did no damage. The game may be rejecting it via CanDealDamage/teams, or wants a TSL damage type. Try [Bots] DamageTypeClass=.");
        }
        return true;
    }

    // tell every client "this gun fired toward To" - muzzle flash, sound, tracer (cosmetic only)
    void SimShot(ATslWeapon_Gun* Gun)
    {
        if (S.ShowBotShots && Gun) Gun->ClientSimulateWeaponFire();
    }

    // one shot: roll it, then show it to the clients aimed where it actually went
    void TakeShot(FBot& B, ATslWeapon_Gun* Gun, ATslCharacter* Target, const FVector& MyLoc, const FVector& TLoc, float Dist)
    {
        // Dispatch the native muzzle/sound multicast before applying the hit.
        // This does not guarantee delivery or audible playback on every client.
        SimShot(Gun);
        if (S.OwnHitRoll && Target && Target->Health > 0.f)
            ResolveBotShot(B, Gun, Target, MyLoc, TLoc, Dist);
        ++B.ShotsThisEngage; ++B.BurstRoundsFired;
        B.LastEngageShot = Now();
    }

    std::unordered_map<int32, std::pair<int, double>> GShooterSlot; // target char index -> (bot id, last shot time)

    void FireLogic(FBot& B, ATslWeapon_Gun* Gun, ATslCharacter* Target, const FVector& MyLoc, const FVector& TLoc, float Dist, double T)
    {

        if (Target && !IsBotChar(Target) && T >= B.WeaponProbeAt) {
            B.WeaponProbeAt = T + 10.0;
            auto* WP = B.Pawn->WeaponProcessor;
            auto* Mesh = Gun->GetWeaponMesh();
            BLOG("BOT WEAPON PROBE " + B.Name + " gun=" + Gun->GetClass()->GetName() +
                " slots=" + std::to_string(WP ? WP->EquippedWeapons.Num() : -1) +
                " index=" + std::to_string(WP ? WP->GetWeaponIndex() : -1) +
                " arm_right=" + std::to_string(WP ? int(WP->WeaponArmInfo.RightWeaponIndex) : -9) +
                " arm_state=" + std::to_string(WP ? int(WP->WeaponArmInfo.State) : -9) +
                " replicates=" + std::to_string(int(Gun->bReplicates)) +
                " owner_only=" + std::to_string(int(Gun->bOnlyRelevantToOwner)) +
                " owner_relevancy=" + std::to_string(int(Gun->bNetUseOwnerRelevancy)) +
                " pawn_owner=" + std::to_string(Gun->GetOwner() == B.Pawn) +
                " mesh=" + std::to_string(Mesh != nullptr) +
                " visible=" + std::to_string(Mesh ? int(Mesh->bVisible) : -1) +
                " hidden=" + std::to_string(Mesh ? int(Mesh->bHiddenInGame) : -1));
        }
        if (S.UseADS && !B.Pawn->IsScoping()) { StopFire(B); return; }
        if (!B.Firing)
        {
            if (T < B.NextBurst)
                return;
            // new fight (new target, or nothing fired at it for a while): accuracy starts low again
            if (Target != B.EngageTarget || T - B.LastEngageShot > 10.0)
            {
                B.EngageTarget = Target;
                B.EngageStart = T;
                B.ShotsThisEngage = 0;
                B.BurstNo = 0;
                B.BlockedBursts = 0;
            }
            // only open fire when a bullet could actually reach them - no endless shooting into cover
            {
                CacheBones();
                FVector HeadP = TLoc + V(0.f, 0.f, 70.f), BodyP = TLoc + V(0.f, 0.f, 20.f);
                if (Target->Mesh)
                {
                    HeadP = Target->Mesh->GetSocketLocation(GHeadBone);
                    BodyP = Target->Mesh->GetSocketLocation(GSpineBone);
                }
                const FVector From = MyLoc + V(0.f, 0.f, 60.f);
                if (!ClearShot(B.Pawn, Target, From, BodyP) && !ClearShot(B.Pawn, Target, From, HeadP))
                {
                    ++B.BlockedBursts;
                    B.NextBurst = T + 0.7;
                    B.NextStrafe = (std::min)(B.NextStrafe, T + 0.3);   // move to get a shot instead
                    return;
                }
                B.BlockedBursts = 0;
            }
            ++B.BurstNo;
            const auto* Data=Gun->WeaponGunData;
            auto Mode=Gun->GetFiringMode();
            // Only select modes that the concrete weapon advertises.
            if (Data) {
                bool Auto=false, Burst=false;
                const auto& Modes=Data->WeaponGunConfig.FiringModes;
                if (Modes.Num()>0 && Modes.Num()<=8 && Modes.IsValid()) for (auto M:Modes) {
                    Auto|=M==EFiringMode::FullAuto; Burst|=M==EFiringMode::Burst;
                }
                const auto Desired=Auto ? EFiringMode::FullAuto : Burst ? EFiringMode::Burst : EFiringMode::Normal;
                if (Mode!=Desired && Gun->GetClass()->GetFunction("TslWeapon_Gun","SetFiringMode")) {
                    Gun->SetFiringMode(Desired); Mode=Gun->GetFiringMode();
                }
            }
            B.BurstRounds=Mode==EFiringMode::FullAuto ? RandI(S.AutoBurstShotsMin,S.AutoBurstShotsMax) :
                Mode==EFiringMode::Burst && Data ? (std::clamp)(Data->WeaponGunConfig.BurstShots,1,10) : 1;
            B.BurstRoundsFired=0;
            if (B.BurstNo<=2 && S.CombatLog) BLOG("BOT FIRE "+B.Name+" weapon="+Gun->GetClass()->GetName()+
                " mode="+std::to_string(int(Mode))+" rounds="+std::to_string(B.BurstRounds)+" ads="+std::to_string(B.Pawn->IsScoping()));
            Gun->ServerStartFire();
            B.Firing = true;
            B.FiringGun = Gun;
            B.FiringGunIndex = Gun->GetIndex();
            float Interval=Data ? Data->WeaponGunConfig.TimeBetweenShots : 0.1f;
            if (!std::isfinite(Interval) || Interval<0.03f || Interval>3.f) Interval=0.1f;
            B.BurstEnd=T+Interval*B.BurstRounds+0.5; // deadline only; count controls the trigger
            B.LastShot = T;
            TakeShot(B, Gun, Target, MyLoc, TLoc, Dist); // first round of the burst
            return;
        }

        float TBS = Gun->WeaponGunData ? Gun->WeaponGunData->WeaponGunConfig.TimeBetweenShots : 0.1f;
        if (!(TBS > 0.03f) || TBS > 3.f)
            TBS = 0.1f;
        int Shots = 0;
        while (T - B.LastShot >= TBS && Shots < 6 && B.BurstRoundsFired < B.BurstRounds)
        {
            B.LastShot += TBS;
            ++Shots;
            TakeShot(B, Gun, Target, MyLoc, TLoc, Dist);
        }

        if (B.BurstRoundsFired>=B.BurstRounds || T>=B.BurstEnd)
        {
            StopFire(B);
            B.NextBurst = T + RandF(S.BurstPauseMin, S.BurstPauseMax);
        }
    }

    void EnterDead(FBot& B, double T)
    {
        StopFire(B);

        if (LiveBotObject(B.Ctrl, B.CtrlIndex))
        {
            if (AIC(B)) AIC(B)->K2_ClearFocus();
            B.Ctrl->StopMovement();
        }

        B.State = EBotState::Dead;
        B.DiedAt = T;

        if (LiveBotObject(B.TeamActor, B.TeamActorIndex))
        {
            B.TeamActor->Health = 0;
            B.TeamActor->bIsDying = 1;
        }

        B.Target = nullptr;
        if (S.VerboseLog) BLOG(B.Name + " died");
    }

    // Follow one living, grounded teammate; the smallest bot ID is leader.
    // Combat, blue-zone survival, vehicles and door traversal retain priority.
    bool TryFollowSquad(UWorld* World, FBot& B, const FVector& MyLoc, double T)
    {
        const int32 Team=BotTeamNumber(B);
        if (!Team || !S.FightEachOther) { B.SquadFollowing=false; return false; }
        FBot* Leader=nullptr;
        for (auto& Other:GBots) {
            if (Other.Id==B.Id || Other.Remove || Other.State==EBotState::Dead || Other.Id>B.Id || BotTeamNumber(Other)!=Team ||
                !LiveBotObject(Other.Pawn,Other.PawnIndex) || Other.Pawn->Health<=0.f ||
                Other.NativeSkyActive || Other.Pawn->IsGroggying() || Other.Pawn->IsInAircraft() || Other.Pawn->IsFallingWithParachute() ||
                Other.Pawn->IsSwimming() || Other.Pawn->IsInVehicle()) continue;
            if (!Leader || Other.Id<Leader->Id) Leader=&Other;
        }
        if (!Leader) { B.SquadFollowing=false; return false; }
        const FVector ToLeader=Flat(Leader->Pawn->K2_GetActorLocation()-MyLoc);
        const float Distance=Len(ToLeader);
        if (!std::isfinite(Distance)) { B.SquadFollowing=false; return false; }
        if (Distance<2000.f) { B.SquadFollowing=false; return false; }
        if (!B.SquadFollowing && Distance<6000.f) return false;
        B.SquadFollowing=true; SetObjective(B,"follow-squad");
        if (T>=B.NextSquadFollowAt || !B.HasDest) {
            B.NextSquadFollowAt=T+4.0;
            const float Leg=(std::min)(Distance-1500.f,S.ZoneLeg);
            const FVector Goal=LandLeg(World,MyLoc,MyLoc+SafeNormal(ToLeader)*Leg,90.f);
            MoveTo(World,B,Goal,T,false); SetSprint(B,Distance>6000.f);
            if (S.VerboseLog) BLOG("SQUAD FOLLOW " + B.Name + " team=" + std::to_string(Team) + " leader=" + Leader->Name + " distance=" + std::to_string((int)(Distance/100.f)) + "m");
        }
        return true;
    }

    // think: remember which players are in the air (plane, skydive, parachute, high above the ground)
    void UpdateHumanAirborne(UWorld* World, double T)
    {
        for (const FCharInfo& C : GChars)
        {
            if (C.IsBot) continue;
            bool bAir = C.Char->IsInAircraft() || C.Char->IsFallingWithParachute();
            if (!bAir && C.Char->GetTslCharacterMovement() && C.Char->GetTslCharacterMovement()->IsFalling())
            {
                FVector G;
                bAir = !TraceDown(World, C.Loc.X, C.Loc.Y, C.Loc.Z - 100.f, C.Loc.Z - 800.f, G); // nothing within 8 m below
            }
            if (bAir) GHumanAirborneAt[C.Index] = T;
        }
    }

    std::unordered_map<int32, int> GEngaged;   // human char index -> bots currently fighting them

    // a player "taken" by one bot: others leave them alone until it's dead or hasn't seen them for ClaimHold s
    struct FClaim { int BotId; double Last; };
    std::unordered_map<int32, FClaim> GClaims;

    bool ClaimedByOther(int BotId, int32 HumanIdx)
    {
        auto It = GClaims.find(HumanIdx);
        if (It == GClaims.end()) return false;
        if (It->second.BotId == BotId) return false;
        if (Now() - It->second.Last > S.ClaimHold) { GClaims.erase(It); return false; }
        for (const FBot& O : GBots)
            if (O.Id == It->second.BotId)
            {
                if (O.State != EBotState::Dead && !O.Remove) return true;
                break;
            }
        GClaims.erase(It);
        return false;
    }

    std::unordered_set<int32> GEngagingHuman;   // bot pawns currently fighting a real player

    void CountEngaged()
    {
        GEngaged.clear();
        GEngagingHuman.clear();
        for (const FBot& B : GBots)
            if (B.State == EBotState::Combat && B.Target && B.TargetIndex >= 0)
            {
                ++GEngaged[B.TargetIndex];
                if (B.Target && !IsBotChar(B.Target) && B.PawnIndex >= 0)
                    GEngagingHuman.insert(B.PawnIndex);
            }
    }

    ATslCharacter* FindTarget(FBot& B, const FVector& MyLoc, const FVector& Fwd, bool bAlert, int32& OutIndex, FVector& OutLoc, float& OutDist)
    {
        const float CosHalfFov = std::cos((std::clamp)(S.FovDegrees, 10.f, 360.f) * 0.5f * kPi / 180.f);
        ATslCharacter* Best = nullptr;
        float BestScore = 1e30f;
        bool BestIsBot = false;

        for (const FCharInfo& C : GChars)
        {
            if (!LiveBotObject(C.Char,C.Index) || C.Char->Health<=0.f) continue;
            if (C.Char == B.Pawn)
                continue;
            if (IsSquadmate(B,C.Char)) { static int Logged=0; if (S.VerboseLog && Logged<24) { ++Logged; BLOG("SQUAD PROTECTED " + B.Name + " teammate=" + CharName(C.Char)); } continue; }
            if (C.IsBot && !S.FightEachOther)
                continue;
            if (C.IsBot && GMatchStartedAt > 0.0 && Now() - GMatchStartedAt < S.BotPeaceTime &&
                !GEngagingHuman.count(C.Index) && Len(C.Loc - MyLoc) > S.CloseFightDist)
                continue; // bot-vs-bot ceasefire early on - except bots busy with a player, or right next to each other
            if (!C.IsBot)
            {
                auto It = GHumanAirborneAt.find(C.Index);
                if (It != GHumanAirborneAt.end() && Now() - It->second < S.PlayerLandingGrace)
                    continue; // in the air, or landed only a moment ago
                // enough bots on this player already - leave them be unless they shot us
                if (S.MaxBotsPerPlayer > 0 && C.Char != B.Target && Now() - B.LastHurtAt >= 8.0)
                {
                    if (S.MaxBotsPerPlayer == 1 && ClaimedByOther(B.Id, C.Index)) continue;
                    auto E = GEngaged.find(C.Index);
                    if (E != GEngaged.end() && E->second >= S.MaxBotsPerPlayer) continue;
                }
            }
            const FVector Delta = C.Loc - MyLoc;
            const float D = Len(Delta);
            if (D > (bAlert ? (std::max)(S.ViewDistance, S.AlertDistance) : S.ViewDistance))
                continue;
            if (!bAlert && D > 400.f && S.FovDegrees < 359.f)
            {
                const FVector Dir = SafeNormal(Flat(Delta));
                if (Dir.Dot(SafeNormal(Flat(Fwd))) < CosHalfFov)
                    continue;
            }
            // prefer the current target (hysteresis) so bots don't flick between people
            float Score = (C.Char == B.Target) ? D * 0.6f : D;
            if (B.Hunter && !C.IsBot) Score *= 0.5f; // hunters go for players first
            if (C.Char == B.ProvokedBy && Now() - B.ProvokedAt < 10.0) Score *= 0.3f; // whoever just shot us
            if (Score >= BestScore)
                continue;
            if (!B.Ctrl->LineOfSightTo(C.Char, V(0, 0, 0), false))
                continue;
            Best = C.Char;
            BestScore = Score;
            OutIndex = C.Index;
            OutLoc = C.Loc;
            OutDist = D;
            BestIsBot = C.IsBot;
        }
        if (Best && !BestIsBot)
        {
            if (Best != B.Target) ++GEngaged[OutIndex];   // count it now so others this frame see it
            auto It = GClaims.find(OutIndex);
            if (It == GClaims.end() || It->second.BotId == B.Id || !ClaimedByOther(B.Id, OutIndex))
                GClaims[OutIndex] = { B.Id, Now() };
        }
        return Best;
    }

    void UncountJoin(FBot& B)
    {
        if (!B.CountedJoin || !GWorld)
            return;
        B.CountedJoin = false;
        AGameStateBase* GSB = UGameplayStatics::GetGameState(GWorld);
        if (GSB && GSB->IsA(ATslGameState::StaticClass()))
        {
            ATslGameState* GS = static_cast<ATslGameState*>(GSB);
            if (GS->NumJoinPlayers > 0) GS->NumJoinPlayers -= 1;
        }
    }

    // horizontal ray from the bot: true if a wall-like surface is within Dist
    bool HorizBlocked(ATslCharacter* Me, const FVector& From, const FVector& Dir, float Dist)
    {
        FHitResult Hit{};
        TArray<AActor*> Ignore;
        FLinearColor C{};
        const bool bHit = UKismetSystemLibrary::LineTraceSingle(Me, From, From + Dir * Dist,
            ETraceTypeQuery::TraceTypeQuery1, false, Ignore, EDrawDebugTrace::None, &Hit, true, C, C, 0.f);
        return bHit && Hit.bBlockingHit && Hit.ImpactNormal.Z < 0.7f;
    }

    // is the way ahead blocked? bLow = only knee-high (jump it)
    bool PathBlocked(UWorld* World, ATslCharacter* Me, const FVector& Loc, const FVector& Dir, bool& bLow)
    {
        bLow = false;
        if (HorizBlocked(Me, Loc + V(0.f, 0.f, 40.f), Dir, 230.f))
            return true;
        if (HorizBlocked(Me, Loc + V(0.f, 0.f, -50.f), Dir, 200.f)) { bLow = true; return true; }
        // water or a drop-off 5 m ahead
        const FVector P = Loc + Dir * 500.f;
        FVector G;
        if (!TraceDown(World, P.X, P.Y, Loc.Z + 300.f, Loc.Z - 1500.f, G))
            return true;
        return IsWaterGround(G.Z);
    }

    USceneComponent* DoorFrame(USceneComponent* Component)
    {
        for (int Depth=0; Component && Depth<8; ++Depth, Component=Component->GetAttachParent())
            if (Component->IsA(UDoorFrameComponent::StaticClass())) return Component;
        return nullptr;
    }

    bool HitMatchesDoor(const FDoorRef& Door, const FHitResult& Hit)
    {
        auto* Object=SafeWeakGet(Hit.Component);
        if (!Object || !Object->IsA(USceneComponent::StaticClass())) return false;
        auto* Component=static_cast<USceneComponent*>(Object);
        auto* Expected=DoorFrame(Door.Component);
        auto* Actual=DoorFrame(Component);
        if (Expected && Actual) return Expected==Actual;
        bool Physical=false;
        auto* P=Component;
        for (int Depth=0; P && Depth<8; ++Depth, P=P->GetAttachParent()) {
            if (P->IsA(UTslReactionDoorComponent::StaticClass()) || P->IsA(UDoorInteractionComponent::StaticClass()) ||
                P->IsA(UDoorFrameComponent::StaticClass())) {Physical=true; break;}
        }
        return Physical && Component->GetOwner()==Door.Component->GetOwner() &&
            Len(Flat(Hit.ImpactPoint-Door.Location))<180.f;
    }

    bool DoorFloor(UWorld* World, ATslCharacter* Pawn, const FVector& Point, const FVector& Here, FVector& Out)
    {
        AActor* Actors[]={Pawn}; TArray<AActor*> Ignore(Actors,1,1);
        FHitResult Hit{}; FLinearColor Color{};
        const bool Found=UKismetSystemLibrary::LineTraceSingle(World,V(Point.X,Point.Y,Here.Z+100.f),
            V(Point.X,Point.Y,Here.Z-220.f),ETraceTypeQuery::TraceTypeQuery1,false,Ignore,EDrawDebugTrace::None,
            &Hit,true,Color,Color,0.f);
        if (!Found || !Hit.bBlockingHit || Hit.bStartPenetrating || Hit.ImpactNormal.Z<0.65f) return false;
        float HalfHeight=88.f;
        auto* Root=Pawn->K2_GetRootComponent();
        if (Root && Root->IsA(UCapsuleComponent::StaticClass())) HalfHeight=static_cast<UCapsuleComponent*>(Root)->GetScaledCapsuleHalfHeight();
        Out=V(Point.X,Point.Y,Hit.ImpactPoint.Z+HalfHeight+2.f);
        return std::fabs(Out.Z-Here.Z)<130.f;
    }

    void EndDoorRoute(UWorld* World, FBot& B, double T, bool Complete, const char* Reason)
    {
        const int32 Index=B.DoorRouteIndex, Serial=B.DoorRouteSerial;
        BLOG("BOT DOOR TASK "+B.Name+" door="+std::to_string(Index)+(Complete ? " crossed " : " stopped ")+Reason);
        if (!Complete) {
            GDoorRetryAfter[Index]=T+8.0;
            B.EntranceAvoidDoor=Index; B.EntranceAvoidSerial=Serial; B.EntranceAvoidUntil=T+15;
        }
        if (B.EntranceDoor==Index && B.EntranceDoorSerial==Serial) {
            B.EntranceStage=0; B.EntranceSearchAt=T+5;
        }
        const FVector Resume=B.DoorResumeDest;
        B.DoorRouteIndex=-1; B.DoorRouteObject=nullptr; B.DoorRouteCrossing=false; B.DoorSawOpen=false;
        B.DoorWaitUntil=0; B.NextDoorTry=T+(Complete ? 0.05 : 0.4); B.DoorProbeDeferUntil=0;
        B.NextLootMove=0; B.NextAreaMove=0; B.NextZoneEmergency=0;
        B.DetourUntil=0; B.ProgressCheckAt=T+2;
        B.HasDest=false; B.DestAcceptance=-1.f; B.Ctrl->StopMovement();
        if (Complete && Len(Flat(Resume-B.Pawn->K2_GetActorLocation()))>S.AcceptRadius) MoveTo(World,B,Resume,T,false);
    }

    bool UpdateDoorRoute(UWorld* World, FBot& B, const FVector& Here, double T)
    {
        if (B.DoorRouteIndex<0) return false;
        const FDoorRef Door{B.DoorRouteObject,B.DoorRouteIndex,B.DoorRouteSerial,B.DoorRoutePortal};
        if (!LiveDoor(Door)) {EndDoorRoute(World,B,T,false,"door removed"); return false;}
        const int State=DoorStateValue(Door.Component);
        const FVector Point=Door.Component->K2_GetComponentLocation();
        const float Range=DoorTraversalPolicy::RequestRange(Door.Component->MaxInteractableDistance);
        const float Distance=Len(Point-Here), FarDistance=Len(Flat(B.DoorRouteFar-Here));
        const bool Crossed=B.DoorSawOpen && FarDistance<75.f && Flat(Here-B.DoorRoutePortal).Dot(B.DoorRouteDirection)>70.f;
        const auto Action=DoorTraversalPolicy::Decide(State,Distance,Range,Crossed,T>B.DoorRouteUntil);
        if (Action==DoorTraversalPolicy::Action::Abort) {EndDoorRoute(World,B,T,false,"invalid state or timeout"); return false;}
        if (Action==DoorTraversalPolicy::Action::Complete) {EndDoorRoute(World,B,T,true,"native open confirmed"); return false;}
        if (Action==DoorTraversalPolicy::Action::Cross) {
            B.DoorSawOpen=true; B.DoorWaitUntil=0;
            if (!B.DoorRouteCrossing) {B.DoorRouteCrossing=true; B.DoorRouteBest=FarDistance; B.DoorRouteProgressAt=T+4; B.DoorRouteMoveAt=0;
                BLOG("BOT DOOR TASK "+B.Name+" opened=1; crossing");}
            if (FarDistance+20.f<B.DoorRouteBest) {B.DoorRouteBest=FarDistance; B.DoorRouteProgressAt=T+4;}
            if (T>B.DoorRouteProgressAt) {EndDoorRoute(World,B,T,false,"crossing obstructed"); return false;}
            if (!B.HasDest || T>=B.DoorRouteMoveAt) {B.DoorRouteMoveAt=T+0.5; MoveTo(World,B,B.DoorRouteFar,T,false,35.f,true);}
            return true;
        }
        if (Action==DoorTraversalPolicy::Action::Approach) {
            B.DoorWaitUntil=0; B.DoorRouteCrossing=false;
            if (!B.HasDest || T>=B.DoorRouteMoveAt) {B.DoorRouteMoveAt=T+0.5;
                MoveTo(World,B,B.DoorRouteNear,T,false,DoorTraversalPolicy::ApproachAcceptance,true);}
            return true;
        }
        // Do not push into a moving door, strafe away from it, or toggle it again.
        StopFire(B); SetTargeting(B,ETargetingType::Targeting_None,T); SetSprint(B,false);
        B.Ctrl->StopMovement(); B.HasDest=false; B.DetourUntil=0; B.DoorWaitUntil=T+0.2;
        if (auto* Movement=B.Pawn->GetTslCharacterMovement()) Movement->StopMovementImmediately();
        if (B.JumpHeld) {B.Pawn->StopJumping(); B.JumpHeld=false;}
        const FRotator Look=UKismetMathLibrary::FindLookAtRotation(Here,Point+V(0,0,70.f));
        B.Ctrl->SetControlRotationBP(Look); B.Pawn->K2_SetActorRotation(FRotator{0,Look.Yaw,0},false);
        if (Action==DoorTraversalPolicy::Action::Wait || T<B.DoorRouteRequestAt) return true;
        B.DoorRouteRequestAt=T+0.4;
        if (++B.DoorRequestAttempts>3) {EndDoorRoute(World,B,T,false,"native interaction rejected"); return false;}
        const bool Allowed=Door.Component->AllowInteract(B.Pawn);
        if (!LiveDoor(Door)) {EndDoorRoute(World,B,T,false,"door removed during interaction"); return false;}
        const bool Sent=Allowed && DoorStateValue(Door.Component)==int(EDoorState::Closed) && DispatchDoorInteraction(Door.Component,B.Pawn);
        const int After=LiveDoor(Door) ? DoorStateValue(Door.Component) : -1;
        BLOG("BOT DOOR TASK "+B.Name+" request="+std::to_string(B.DoorRequestAttempts)+" allowed="+std::to_string(Allowed)+
            " sent="+std::to_string(Sent)+" state="+std::to_string(After)+" distance="+std::to_string((int)Distance));
        return true;
    }

    bool TryDoorRoute(UWorld* World, FBot& B, const FVector& Here, double T)
    {
        if (!GMatchSpawnDone || B.State!=EBotState::Roam || !B.HasDest || T<B.NextDoorTry || GDoors.empty() || !DoorLayoutSafe()) return false;
        auto* Movement=B.Pawn->GetTslCharacterMovement();
        if (!Movement || Movement->MovementMode!=EMovementMode::MOVE_Walking || B.Pawn->IsInVehicle() || B.Pawn->IsSwimming()) return false;
        const FVector Delta=Flat(B.Dest-Here); const float GoalDistance=Len(Delta); if (GoalDistance<50.f) return false;
        const FVector Want=SafeNormal(Delta);
        std::vector<std::pair<float,size_t>> Candidates;
        const int32 CX=LootSpatial::Cell(Here.X), CY=LootSpatial::Cell(Here.Y);
        for (int DX=-1;DX<=1;++DX) for (int DY=-1;DY<=1;++DY) {
            auto Bucket=GDoorCells.find(LootSpatial::Key(CX+DX,CY+DY)); if (Bucket==GDoorCells.end()) continue;
            for (size_t Index:Bucket->second) {
                if (Index>=GDoors.size()) continue;
                const auto& D=GDoors[Index]; const FVector To=Flat(D.Location-Here);
                const float Along=To.Dot(Want), Side=Len(To-Want*Along);
                const bool Planned=B.EntranceStage && B.EntranceDoor==D.Index && B.EntranceDoorSerial==D.Serial;
                if (Along<20.f || Along>DoorTraversalPolicy::ProbeDistance || Side>140.f || std::fabs(D.Location.Z-Here.Z)>200.f ||
                    (!Planned && GoalDistance<Along+60.f) || !LiveDoor(D)) continue;
                const int State=DoorStateValue(D.Component); if (State<0 || State==int(EDoorState::Opened)) continue;
                auto Retry=GDoorRetryAfter.find(D.Index); if (Retry!=GDoorRetryAfter.end() && T<Retry->second) continue;
                Candidates.push_back({Along+Side,Index});
            }
        }
        if (Candidates.empty()) {B.DoorProbeDeferUntil=0; B.NextDoorTry=T+0.25; return false;}
        if (GDoorProbeBudget<=0) {
            if (!B.DoorProbeDeferUntil) B.DoorProbeDeferUntil=T+0.5;
            if (T>B.DoorProbeDeferUntil) return false;
            // Let the next AI think inspect a likely doorway before avoidance
            // walks away. Requests are staggered and the pause is bounded.
            B.DoorWaitUntil=T+0.12; B.Ctrl->StopMovement();
            if (Movement) Movement->StopMovementImmediately();
            return true;
        }
        --GDoorProbeBudget; B.DoorProbeDeferUntil=0; B.NextDoorTry=T+RandF(0.35f,0.65f);
        std::sort(Candidates.begin(),Candidates.end());
        AActor* Actors[]={B.Pawn}; TArray<AActor*> Ignore(Actors,1,1); FLinearColor Color{};
        for (float Height : {0.f,40.f,-35.f}) {
            FHitResult Hit{};
            const bool Blocked=UKismetSystemLibrary::LineTraceSingle(World,Here+V(0,0,Height),Here+V(0,0,Height)+Want*DoorTraversalPolicy::ProbeDistance,
                ETraceTypeQuery::TraceTypeQuery1,false,Ignore,EDrawDebugTrace::None,&Hit,true,Color,Color,0.f);
            if (!Blocked || !Hit.bBlockingHit) continue;
            for (auto Candidate:Candidates) {
                const auto Door=GDoors[Candidate.second]; if (!LiveDoor(Door) || !HitMatchesDoor(Door,Hit)) continue;
                const FVector Point=Door.Component->K2_GetComponentLocation();
                const float Along=Flat(Point-Here).Dot(Want);
                const FVector Portal=Here+Want*Along;
                const float Side=Len(Flat(Point-Portal));
                const float Range=DoorTraversalPolicy::RequestRange(Door.Component->MaxInteractableDistance);
                const float StandOff=DoorTraversalPolicy::StandOff(Range,Point.Z-Here.Z,Side);
                if (StandOff<0.f) continue;
                FVector Near{}, Far{};
                if (!DoorFloor(World,B.Pawn,Portal-Want*StandOff,Here,Near) ||
                    !DoorFloor(World,B.Pawn,Portal+Want*220.f,Here,Far) || std::fabs(Near.Z-Far.Z)>90.f) continue;
                B.DoorRouteObject=Door.Component; B.DoorRouteIndex=Door.Index; B.DoorRouteSerial=Door.Serial;
                B.DoorRouteNear=Near; B.DoorRouteFar=Far; B.DoorRoutePortal=Portal; B.DoorRouteDirection=Want;
                B.DoorResumeDest=B.Dest; B.DoorRouteUntil=T+8.0; B.DoorRouteRequestAt=0; B.DoorRouteMoveAt=0;
                B.DoorRouteCrossing=false; B.DoorSawOpen=false; B.DoorRequestAttempts=0;
                B.DetourUntil=0; B.AvoidSideUntil=0; B.HasDest=false; SetSprint(B,false);
                B.ProgressCheckAt=T+10; B.NextLootMove=T+9; B.LootTargetUntil=std::max(B.LootTargetUntil,T+10);
                BLOG("BOT DOOR TASK "+B.Name+" start door="+std::to_string(Door.Index)+" purpose="+B.Objective+
                    " range="+std::to_string((int)Range));
                return UpdateDoorRoute(World,B,Here,T);
            }
        }
        return false;
    }

    FVector RotYaw(const FVector& D, float Deg)
    {
        const float A = Deg * kPi / 180.f, Cs = std::cos(A), Sn = std::sin(A);
        return V(D.X * Cs - D.Y * Sn, D.X * Sn + D.Y * Cs, 0.f);
    }

    // lobby: something to watch while the timer runs down (no emotes in this build)
    int GDropLogs = 0;

    void LobbyAntics(FBot& B, ATslCharacter* Me, const FVector& MyLoc, double T)
    {
        if (!B.HasLobbyHome)
        {
            B.HasLobbyHome = true;
            B.LobbyHome = MyLoc;
            B.CircleAng = RandF(0.f, 2.f * kPi);
            B.CircleR = RandF(250.f, 600.f);
            B.CircleDir = RandI(0, 1) ? 1.f : -1.f;
            B.NextMood = T + RandF(0.f, 3.f);
            B.UseSteer = true; // no navmesh - input steering
        }
        // never wander off the start island
        if (Me->IsSwimming() || Len(Flat(MyLoc - B.LobbyHome)) > 1500.f)
        {
            FHitResult Hit{};
            Me->K2_SetActorLocation(B.LobbyHome + V(0.f, 0.f, 60.f), false, &Hit, true);
            if (Me->GetTslCharacterMovement()) Me->GetTslCharacterMovement()->StopMovementImmediately();
            B.HasDest = false;
            return;
        }
        if (T >= B.NextMood)
        {
            B.LobbyMood = RandI(0, S.LobbyPunch && !Me->GetWeaponGun() ? 5 : 4);
            B.NextMood = T + RandF(4.f, 9.f);
            if (B.Crouched) { Me->UnCrouch(false); B.Crouched = false; }
            B.HasDest = false;
            if (RandI(0, 2) == 0) B.CircleDir = -B.CircleDir;
            SetSprint(B, B.LobbyMood == 4);
        }
        switch (B.LobbyMood)
        {
        case 0: // walk in a circle
        case 1: // circle and bunny-hop
        {
            B.CircleAng += B.CircleDir * (250.f / B.CircleR) * (1.f / (float)S.ThinkHz);
            const float Lead = B.CircleAng + B.CircleDir * 0.6f;
            B.Dest = B.LobbyHome + V(std::cos(Lead) * B.CircleR, std::sin(Lead) * B.CircleR, 0.f);
            B.HasDest = true;
            B.DestSetAt = T;
            if (B.LobbyMood == 1 && T >= B.NextLobbyAct)
            {
                Me->Jump();
                B.JumpHeld = true;
                B.NextLobbyAct = T + RandF(0.7f, 1.4f);
            }
            break;
        }
        case 2: // crouch spam
            B.HasDest = false;
            if (T >= B.NextLobbyAct)
            {
                if (B.Crouched) Me->UnCrouch(false); else Me->Crouch(false);
                B.Crouched = !B.Crouched;
                B.NextLobbyAct = T + RandF(0.25f, 0.5f);
            }
            break;
        case 3: // spin / look around
        {
            B.HasDest = false;
            const FRotator R = Me->K2_GetActorRotation();
            Me->K2_SetActorRotation(FRotator{ 0.f, R.Yaw + B.CircleDir * 25.f, 0.f }, false);
            break;
        }
        case 5: // walk up to someone close and throw punches (or shadow-box)
        {
            ATslCharacter* Buddy = nullptr;
            float BD = 900.f;
            FVector BL{};
            for (const FCharInfo& C : GChars)
            {
                if (C.Char == Me) continue;
                const float D = Len(Flat(C.Loc - MyLoc));
                if (D < BD) { BD = D; Buddy = C.Char; BL = C.Loc; }
            }
            if (Buddy && BD > 160.f)
            {
                B.Dest = BL; B.HasDest = true; B.DestSetAt = T;
                SetSprint(B, false);
                break;
            }
            B.HasDest = false;
            if (Buddy)
            {
                const float Yaw = std::atan2(BL.Y - MyLoc.Y, BL.X - MyLoc.X) * 180.f / kPi;
                Me->K2_SetActorRotation(FRotator{ 0.f, Yaw, 0.f }, false);
            }
            if (T >= B.NextLobbyAct)
            {
                Me->Client_PlayUnarmedAttack(RandI(0, 2));   // multicast: everyone sees the swing
                B.NextLobbyAct = T + RandF(0.55f, 1.1f);
            }
            break;
        }
        default: // sprint to a random spot near home, then another
            if (!B.HasDest)
            {
                const float A = RandF(0.f, 2.f * kPi), D = RandF(300.f, 900.f);
                B.Dest = B.LobbyHome + V(std::cos(A) * D, std::sin(A) * D, 0.f);
                B.HasDest = true;
                B.DestSetAt = T;
            }
            break;
        }
    }

    // Check only the actual landscape collision, never a roof or floor. A
    // pair of hits prevents a one-off trace from moving a bot unnecessarily.
    struct FHumanRockState {
        int32 Serial = 0; FVector Safe{}, FirstOverlap{};
        double SafeAt = 0, OverlapAt = 0, NextAllowed = 0, SeenAt = 0;
        float HealthAtOverlap = 0; bool HasSafe = false, Logged = false;
    };
    std::unordered_map<int32,FHumanRockState> GHumanRockStates;
    UWorld* GHumanRockWorld = nullptr;
    double GNextHumanRockPass = 0;

    bool HumanRockOverlap(UWorld* World, ATslCharacter* C, const FVector& P, float Radius, float Half, FHitResult& Hit)
    {
        AActor* Actors[]={C}; TArray<AActor*> Ignore(Actors,1,1); FLinearColor Color{};
        static FName PawnProfile=UKismetStringLibrary::Conv_StringToName(FString(L"Pawn"));
        Hit=FHitResult{};
        return UKismetSystemLibrary::CapsuleTraceSingleByProfile(World,P,P+V(0.f,0.f,0.1f),Radius,Half,
            PawnProfile,false,Ignore,EDrawDebugTrace::None,&Hit,true,Color,Color,0.f) && Hit.bStartPenetrating;
    }

    bool HumanRockObstacle(const FHitResult& Hit)
    {
        auto* C=SafeWeakGet(Hit.Component);
        if (!C) return false;
        if (C->IsA(ULandscapeHeightfieldCollisionComponent::StaticClass())) return true;
        if (!C->IsA(UStaticMeshComponent::StaticClass())) return false;
        auto* Mesh=static_cast<UStaticMeshComponent*>(C);
        if (!Mesh->StaticMesh || !LiveBotObject(Mesh->StaticMesh,Mesh->StaticMesh->GetIndex())) return false;
        std::string Name=Lower(Mesh->StaticMesh->GetName());
        return Name.find("rock")!=std::string::npos || Name.find("cliff")!=std::string::npos || Name.find("boulder")!=std::string::npos;
    }

    void HumanRockRecoveryPass(UWorld* World, double T)
    {
        if (World!=GHumanRockWorld) {GHumanRockStates.clear(); GHumanRockWorld=World; GNextHumanRockPass=0;}
        if (!GMatchSpawnDone || !World || T<GNextHumanRockPass) return;
        GNextHumanRockPass=T+0.5;
        for (const FCharInfo& Info : GChars) {
            auto* C=Info.Char;
            if (Info.IsBot || !LiveBotObject(C,Info.Index) || C->Health<=0.f || C->IsInAircraft() || C->IsInVehicle() || C->IsSwimming()) continue;
            auto* Controller=C->Controller;
            if (!Controller || !LiveBotObject(Controller,Controller->GetIndex()) || !Controller->IsA(APlayerController::StaticClass())) continue;
            auto* PC=static_cast<APlayerController*>(Controller);
            if (!PC->NetConnection || PC->bIsLocalPlayerController) continue;
            auto* Move=C->GetTslCharacterMovement(); auto* Root=C->K2_GetRootComponent();
            if (!Move || !Root || !Root->IsA(UCapsuleComponent::StaticClass())) continue;
            if (Move->MovementMode!=EMovementMode::MOVE_Walking && Move->MovementMode!=EMovementMode::MOVE_Falling) continue;
            auto* Capsule=static_cast<UCapsuleComponent*>(Root);
            float Radius=Capsule->GetScaledCapsuleRadius(), Half=Capsule->GetScaledCapsuleHalfHeight();
            if (!std::isfinite(Radius) || !std::isfinite(Half) || Radius<15.f || Radius>100.f || Half<Radius || Half>180.f) continue;
            const int32 Serial=ObjectSerial(Info.Index);
            if (Serial<=0) continue;
            auto& State=GHumanRockStates[Info.Index];
            if (State.Serial!=Serial) {State=FHumanRockState{};State.Serial=Serial;}
            State.SeenAt=T;
            FVector P=C->K2_GetActorLocation();
            if (!std::isfinite(P.X)||!std::isfinite(P.Y)||!std::isfinite(P.Z)) continue;
            FHitResult Core{};
            const bool Penetrating=HumanRockOverlap(World,C,P,Radius*0.70f,(std::max)(Radius*0.70f,Half*0.65f),Core);
            if (!Penetrating) {
                State.OverlapAt=0;State.Logged=false;
                if (Move->MovementMode==EMovementMode::MOVE_Walking) {
                    FFindFloorResult Floor{}; Move->K2_FindFloor(P,&Floor);
                    FHitResult Full{};
                    if (Floor.bBlockingHit && Floor.bWalkableFloor && !Floor.HitResult.bStartPenetrating &&
                        Floor.HitResult.ImpactNormal.Z>=0.65f && Floor.FloorDist<=10.f &&
                        !HumanRockOverlap(World,C,P,Radius*0.95f,Half*0.95f,Full)) {
                        State.Safe=P;State.SafeAt=T;State.HasSafe=true;
                    }
                }
                continue;
            }
            if (!HumanRockObstacle(Core)) {State.OverlapAt=0;State.Logged=false;continue;}
            const FVector Accel=Move->GetCurrentAcceleration();
            const bool Trying=std::isfinite(Accel.X)&&std::isfinite(Accel.Y)&&Len(Flat(Accel))>10.f;
            if (!Trying) {State.OverlapAt=0;State.Logged=false;continue;}
            if (State.OverlapAt==0) {State.OverlapAt=T;State.FirstOverlap=P;State.HealthAtOverlap=C->Health;State.Logged=false;}
            const float Motion=Len(Flat(P-State.FirstOverlap));
            if (Motion>35.f) {State.OverlapAt=T;State.FirstOverlap=P;State.HealthAtOverlap=C->Health;State.Logged=false;continue;}
            const double Stuck=T-State.OverlapAt;
            if (Stuck>=4.0 && !State.Logged) {
                BLOG("HUMAN ROCK overlap confirmed pawn="+std::to_string(Info.Index)+" health_before="+
                    std::to_string(State.HealthAtOverlap)+" health_now="+std::to_string(C->Health)+
                    " safe="+std::to_string(State.HasSafe)+" movement_mode="+std::to_string((int)Move->MovementMode));
                State.Logged=true;
            }
            FHitResult Destination{}; bool Clear=false;
            if (State.HasSafe && Stuck>=4.0 && T-State.SafeAt<=12.0) {
                FFindFloorResult SafeFloor{}; Move->K2_FindFloor(State.Safe,&SafeFloor);
                Clear=SafeFloor.bBlockingHit && SafeFloor.bWalkableFloor && !SafeFloor.HitResult.bStartPenetrating &&
                    SafeFloor.HitResult.ImpactNormal.Z>=0.65f && SafeFloor.FloorDist<=10.f &&
                    !HumanRockOverlap(World,C,State.Safe,Radius*0.95f,Half*0.95f,Destination);
            }
            const float Distance=State.HasSafe?Len(P-State.Safe):1e9f;
            HumanRockRecovery::Evidence E{true,Trying,true,Clear,Stuck,T-State.SafeAt,State.NextAllowed-T,Motion,Distance};
            if (!State.HasSafe || !HumanRockRecovery::ShouldRecover(E)) continue;
            FHitResult Ignored{};
            if (C->K2_SetActorLocation(State.Safe,false,&Ignored,true)) {
                Move->StopMovementImmediately(); C->ForceNetUpdate();
                PC->ClientSetLocation(State.Safe,C->K2_GetActorRotation());
                BLOG("HUMAN ROCK recovered pawn="+std::to_string(Info.Index)+" distance_cm="+std::to_string((int)Distance));
                State.NextAllowed=T+20.0;State.HasSafe=false;State.OverlapAt=0;State.Logged=false;
            }
        }
        for(auto I=GHumanRockStates.begin();I!=GHumanRockStates.end();) {
            if(T-I->second.SeenAt>30.0) I=GHumanRockStates.erase(I); else ++I;
        }
    }

    bool GuardBotBelowTerrain(UWorld* World, FBot& B, double T)
    {
        ATslCharacter* Me = B.Pawn;
        if (!GMatchSpawnDone || !Me || B.Hidden || B.NativeSkyActive || B.DropPending ||
            Me->IsInAircraft() || Me->IsFallingWithParachute() || Me->IsSwimming()) {
            B.UndergroundChecks = 0; return false;
        }
        auto* Move = Me->GetTslCharacterMovement();
        if (!Move || (Move->MovementMode != EMovementMode::MOVE_Walking &&
            Move->MovementMode != EMovementMode::MOVE_Falling) || Me->IsSkydiving()) {
            B.UndergroundChecks = 0; return false;
        }
        if (T < B.NextTerrainCheck || T < GNextBotTerrainTrace) return B.UndergroundChecks > 0;
        GNextBotTerrainTrace = T + 0.05;
        B.NextTerrainCheck = T + (B.UndergroundChecks ? 0.3 : 8.0 + RandF(0.f, 4.f));
        const FVector Here = Me->K2_GetActorLocation();
        std::vector<AActor*> Ignored{ Me };
        FHitResult Hit{}; FLinearColor Color{};
        bool Found = false; UObject* Component = nullptr;
        // A roof, tree or another bot may be hit before the landscape. Skip a
        // small bounded number of such actors, but never skip the landscape.
        for (int Attempt = 0; Attempt < 5; ++Attempt) {
            TArray<AActor*> Ignore(Ignored.data(), (int32)Ignored.size(), (int32)Ignored.size());
            Hit = FHitResult{};
            Found = UKismetSystemLibrary::LineTraceSingle(World,
                Here + V(0.f, 0.f, 12000.f), Here - V(0.f, 0.f, 1000.f),
                ETraceTypeQuery::TraceTypeQuery1, false, Ignore, EDrawDebugTrace::None,
                &Hit, true, Color, Color, 0.f);
            Component = Found ? SafeWeakGet(Hit.Component) : nullptr;
            if (!Found || !Hit.bBlockingHit || !Component) break;
            if (Component->IsA(ULandscapeHeightfieldCollisionComponent::StaticClass())) break;
            auto* Actor = static_cast<AActor*>(SafeWeakGet(Hit.Actor));
            if (!Actor || !Actor->IsA(AActor::StaticClass()) ||
                std::find(Ignored.begin(), Ignored.end(), Actor) != Ignored.end()) break;
            Ignored.push_back(Actor);
        }
        const float Depth = Found ? Hit.ImpactPoint.Z - Here.Z : 0.f;
        if (!Found || !Hit.bBlockingHit || Hit.bStartPenetrating || !Component ||
            !Component->IsA(ULandscapeHeightfieldCollisionComponent::StaticClass()) || Depth < 110.f || Depth > 10000.f) {
            B.UndergroundChecks = 0; return false;
        }
        StopFire(B);
        if (++B.UndergroundChecks < 2) return true;
        FHitResult MoveHit{}; const FVector Surface = V(Here.X, Here.Y, Hit.ImpactPoint.Z + 110.f);
        if (Me->K2_SetActorLocation(Surface, false, &MoveHit, true)) {
            Move->StopMovementImmediately(); Me->ForceNetUpdate();
            B.HasDest = false; B.NextRoamPick = T + 1.0;
            BLOG("TERRAIN " + B.Name + " recovered from " + std::to_string((int)Depth) + " cm below landscape");
        }
        B.UndergroundChecks = 0; B.NextTerrainCheck = T + 2.0;
        return true;
    }

    // Native query return buffers are retained rather than discarded by the SDK wrapper.
    const TArray<ATslCharacter*>& QueryRiders(UTslVehicleSeatComponent* Seat)
    {
        static Params::TslVehicleSeatComponent_GetRiders Query{};
        static TArray<ATslCharacter*> Empty;
        if (!Seat) return Empty;
        auto* Fn=Seat->GetClass()->GetFunction("TslVehicleSeatComponent","GetRiders");
        if (!Fn) return Empty;
        Query.ReturnValue.Clear();
        Seat->ProcessEvent(Fn,&Query);
        return Query.ReturnValue;
    }

    bool VehicleTraceBlocked(UWorld* World, const FVector& Start, const FVector& End,
                             const TArray<AActor*>& Ignore, FHitResult& Hit)
    {
        FLinearColor Color{};
        auto Trace=[&](const FVector& A,const FVector& B,FHitResult& Out) {
            return UKismetSystemLibrary::LineTraceSingle(World,A,B,ETraceTypeQuery::TraceTypeQuery1,false,
                Ignore,EDrawDebugTrace::None,&Out,true,Color,Color,0.f) && Out.bBlockingHit;
        };
        auto Ground=[&](const FHitResult& H) {
            UObject* Component=SafeWeakGet(H.Component);
            const bool Landscape=Component && Component->IsA(ULandscapeHeightfieldCollisionComponent::StaticClass());
            return VehicleObstaclePolicy::TraversableGround(Landscape,H.bStartPenetrating,H.ImpactNormal.Z);
        };
        if (!Trace(Start,End,Hit)) return false;
        if (!Ground(Hit)) return true;
        // A low probe can intersect drivable ground before an actual wall.
        // Check again higher up before treating the path as clear.
        FHitResult Raised{}; const FVector Lift{0.f,0.f,150.f};
        if (!Trace(Start+Lift,End+Lift,Raised)) return false;
        Hit=Raised;
        return !Ground(Hit);
    }

    // Initial vehicle behavior: seek empty working wheeled vehicles in a
    // blue-zone emergency. This remains direct steering, not road navigation.
    bool UpdateZoneVehicle(UWorld* World, FBot& B, const FVector& Here, const FVector& Center, float Radius, bool NeedTransport, bool InBlue, double T)
    {
        if (!B.Pawn || !GMatchSpawnDone || B.NativeSkyActive || B.DropPending || B.Pawn->IsSwimming()) return false;
        if (B.VehicleDriving) {
            APawn* Pawn = B.Pawn->GetVehiclePawn();
            if (!Pawn || !Pawn->IsA(ATslWheeledVehicle::StaticClass())) {
                B.VehicleDriving=false; B.VehicleTarget=-1; B.VehicleScanAt=T+30; return false;
            }
            auto* Car=static_cast<ATslWheeledVehicle*>(Pawn);
            auto* Seat=Car->VehicleSeatComponent;
            auto* Movement=Car->GetTslVehicleMovement();
            if (!Seat || !Movement || Seat->GetDriver()!=B.Pawn) { B.VehicleDriving=false; B.VehicleTarget=-1; B.VehicleScanAt=T+30; return false; }
            const FVector P=Car->K2_GetActorLocation();
            const FVector V=Car->GetVelocity();
            const float Speed=(std::max)(Len(Flat(V)),std::fabs(Movement->GetForwardSpeed()));
            const float Step=Len(Flat(P-B.VehicleLastPos));
            if (std::isfinite(Step) && Step<10000.f) B.VehicleTravelled+=Step;
            B.VehicleLastPos=P;
            const bool Arrived=VehicleTravelPolicy::AtCircleCentre(Len(Flat(P-Center)),B.VehicleTravelled,B.VehicleInitialGoalDistance);
            if (T >= B.VehicleStuckAt) {
                if (Len(Flat(P-B.VehicleProgress)) >= 250.f) B.VehicleDeadline=T+90.0;
                else if (T > B.VehicleDeadline-80.0) B.VehicleStalledUntil=T+8.0;
                B.VehicleProgress=P; B.VehicleStuckAt=T+8.0;
            }
            const FVector Ahead=SafeNormal(Flat(Car->GetActorForwardVector()));
            const FVector To=SafeNormal(Flat(Center-P));
            const float Dot=Ahead.X*To.X+Ahead.Y*To.Y;
            const float Cross=Ahead.X*To.Y-Ahead.Y*To.X;
            float Steer=(std::clamp)(std::atan2(Cross,Dot)/(3.14159265f/4.f),-1.f,1.f);
            AActor* IgnoredActors[] = { B.Pawn, Car };
            TArray<AActor*> Ignore(IgnoredActors,2,2);
            FHitResult Hit{};
            const FVector Start=P+Ahead*300.f+V*0.25f+FVector{0.f,0.f,150.f};
            const bool Obstacle=VehicleTraceBlocked(World,Start,Start+Ahead*1500.f,Ignore,Hit);
            if (Speed>250.f) B.VehicleStalledUntil=0;
            const bool BlockedAhead=Obstacle || T<B.VehicleStalledUntil;
            bool LeftBlocked=false,RightBlocked=false,RearBlocked=true;
            if (BlockedAhead || T<B.VehicleAvoidUntil || T<B.VehicleReverseUntil) {
                const FVector Side{-Ahead.Y,Ahead.X,0.f}; FHitResult Left{},Right{},Rear{};
                LeftBlocked=VehicleTraceBlocked(World,Start,Start+Ahead*1200.f-Side*1000.f,Ignore,Left);
                RightBlocked=VehicleTraceBlocked(World,Start,Start+Ahead*1200.f+Side*1000.f,Ignore,Right);
                if (Obstacle && T>=B.VehicleAvoidUntil && (!LeftBlocked || !RightBlocked)) {
                    B.VehicleAvoidTurn=LeftBlocked ? 1.f : RightBlocked ? -1.f : ((B.Id&1)?1.f:-1.f);
                    B.VehicleAvoidUntil=T+8; BLOG("BOT VEHICLE avoid "+B.Name);
                }
                if (T<B.VehicleAvoidUntil && ((B.VehicleAvoidTurn>0 && RightBlocked) || (B.VehicleAvoidTurn<0 && LeftBlocked))) B.VehicleAvoidUntil=0;
                const FVector RearStart=P-Ahead*400.f+FVector{0.f,0.f,150.f};
                RearBlocked=VehicleTraceBlocked(World,RearStart,RearStart-Ahead*900.f,Ignore,Rear);
            }
            const bool Avoiding=T<B.VehicleAvoidUntil;
            if (Avoiding) Steer=B.VehicleAvoidTurn;
            if (BlockedAhead && Speed<150.f) { if (B.VehicleBlockedSince<=0) B.VehicleBlockedSince=T; } else B.VehicleBlockedSince=0;
            const bool UrgentExit=VehicleTravelPolicy::LeaveBlockedInBlue(InBlue,B.VehicleBlockedSince,T);
            if (!UrgentExit && !InBlue && BlockedAhead && !RearBlocked && B.VehicleReverseAttempts<2 && B.VehicleBlockedSince>0 && T-B.VehicleBlockedSince>=2 && T>=B.VehicleReverseCooldown) {
                ++B.VehicleReverseAttempts; B.VehicleReverseUntil=T+3; B.VehicleReverseCooldown=T+8;
                B.VehicleReverseTurn=B.VehicleAvoidTurn!=0 ? -B.VehicleAvoidTurn : ((B.Id&1)?1.f:-1.f);
                B.VehicleBlockedSince=0; B.VehicleAvoidUntil=0; BLOG("BOT VEHICLE reverse "+B.Name);
            }
            const bool Reversing=T<B.VehicleReverseUntil && !UrgentExit && !Arrived && !RearBlocked;
            const bool WaitingReverse=BlockedAhead && !Avoiding && !RearBlocked && !InBlue && B.VehicleReverseAttempts<2 && !Reversing;
            const bool Stop=Arrived || UrgentExit || T>=B.VehicleDeadline || (BlockedAhead && !Avoiding && !Reversing && !WaitingReverse);
            const float SignedSpeed=Movement->GetForwardSpeed();
            const bool Brake=Stop || WaitingReverse || (!Reversing && Avoiding && Speed>250.f) || (Reversing ? SignedSpeed>150.f : SignedSpeed< -150.f);
            if (Reversing) Steer=B.VehicleReverseTurn;
            const float Throttle=Brake?0.f:(Reversing?0.4f:(Avoiding?0.35f:(Dot<0.f?0.35f:0.7f)));
            Movement->SetUseAutoGears(!Reversing); Movement->SetTargetGear(Reversing?-1:1,true);
            Movement->SetSteeringInput(Steer); Movement->SetThrottleInput(Throttle); Movement->SetBrakeInput(Brake?1.f:0.f);
            auto* Fn=Movement->GetClass()->GetFunction("TslWheeledVehicleMovement", "Server_SetMovementParameters");
            if (!Fn) {
                BLOG("BOT VEHICLE movement function missing " + B.Name); return true;
            }
            Params::TslWheeledVehicleMovement_Server_SetMovementParameters Args{};
            Args.InThrottle=Throttle; Args.InSteer=Steer; Args.InBreak=Brake?1.f:0.f; Args.Gear=Reversing?-1:1;
            const auto Flags=Fn->FunctionFlags; Fn->FunctionFlags=(Flags & ~0x40u)|0x400u;
            Movement->ProcessEvent(Fn,&Args); Fn->FunctionFlags=Flags;
            if (auto* StateFn=Movement->GetClass()->GetFunction("WheeledVehicleMovementComponent","ServerUpdateState")) {
                Params::WheeledVehicleMovementComponent_ServerUpdateState State{};
                State.InSteeringInput=Steer;State.InThrottleInput=Throttle;State.InBrakeInput=Brake?1.f:0.f;State.CurrentGear=Reversing?-1:1;
                const auto StateFlags=StateFn->FunctionFlags;StateFn->FunctionFlags=(StateFlags & ~0x40u)|0x400u;
                Movement->ProcessEvent(StateFn,&State);StateFn->FunctionFlags=StateFlags;
            }
            if (T>=B.VehicleLogAt) { B.VehicleLogAt=T+5; BLOG("BOT VEHICLE progress "+B.Name+" driven="+std::to_string((int)(B.VehicleTravelled/100.f))+"m speed="+std::to_string((int)(Speed/100.f))+"m/s blocked="+std::to_string(Obstacle)+" reverse="+std::to_string(Reversing)
#ifdef PUBG2019_TEST_CAR_FIXTURE
                +" sim="+std::to_string(Movement->IsSimulatingVehicle())+" active="+std::to_string(Movement->IsActive())+" requested="+std::to_string(Throttle)+" request_brake="+std::to_string(Brake)+" input="+std::to_string(Movement->GetThrottleInput())+" brake="+std::to_string(Movement->GetBrakeInput())+" gear="+std::to_string(Movement->GetCurrentGear())+" class="+Car->GetClass()->GetName()
#endif
                ); }
            if (Stop && Speed<150.f && T>=B.VehicleAttemptAt) {
                B.VehicleAttemptAt=T+3.0;
                if (!Arrived) {
                    B.VehicleRejectedIndex=B.VehicleTarget; B.VehicleRejectedSerial=B.VehicleSerial;
                    B.VehicleRejectedUntil=T+120.0;
                }
                Seat->TryToLeave(B.Pawn,false);
                BLOG("BOT VEHICLE exit requested " + B.Name + (Arrived ? " arrived" : " blocked or timeout"));
                // Keep braking and retain this task until the native seat
                // confirms departure. A request alone is not a successful exit.
            }
            return true;
        }
        if ((!NeedTransport && B.VehicleTarget<0) || (B.Pawn->IsInVehicle() && B.VehicleTarget<0)) { B.VehicleTarget=-1; return false; }
        if (B.VehicleTarget < 0 && T >= B.VehicleScanAt) {
            B.VehicleScanAt=T+12;
            static double NextVehicleScanLog = 0;
            if (T >= NextVehicleScanLog) {
                NextVehicleScanLog = T + 30.0;
                BLOG("BOT VEHICLE scan " + B.Name + " cached=" + std::to_string(GZoneVehicles.size()));
            }
            float Best=Len(Flat(Here-Center))-Radius>50000.f ? 30000.f : 15000.f;
            for (const auto& V:GZoneVehicles) {
                if (!LiveBotObject(V.Vehicle,V.Index) || ObjectSerial(V.Index)!=V.Serial) continue;
                if (VehicleObstaclePolicy::IsRejectedVehicle(B.VehicleRejectedIndex,B.VehicleRejectedSerial,B.VehicleRejectedUntil,T,V.Index,V.Serial)) continue;
                auto* C=V.Vehicle; const float D=Len(Flat(C->K2_GetActorLocation()-Here));
                if (D>=Best || std::fabs(C->K2_GetActorLocation().Z-Here.Z)>400.f || !C->VehicleSeatComponent || !C->VehicleCommonComponent ||
                    QueryRiders(C->VehicleSeatComponent).Num() > 0 || C->VehicleCommonComponent->GetVehicleFuelPercent()<0.05f ||
                    C->VehicleCommonComponent->GetVehicleHealthPercent()<0.15f || C->IsSunken()) continue;
                Best=D; B.VehicleTarget=V.Index; B.VehicleSerial=V.Serial;
            }
            if (B.VehicleTarget>=0) { B.VehicleDeadline=T+35; B.VehicleAttemptAt=0; B.HasDest=false;
                BLOG("BOT VEHICLE seek " + B.Name + " distance=" + std::to_string((int)(Best/100.f)) + "m"); }
        }
        if (B.VehicleTarget<0) return false;
        auto* Objects=UObject::GObjects.GetTypedPtr();
        if (!Objects || B.VehicleTarget>=Objects->Num()) { B.VehicleTarget=-1; return false; }
        auto* Object=Objects->GetByIndex(B.VehicleTarget);
        if (!LiveBotObject(Object,B.VehicleTarget) || ObjectSerial(B.VehicleTarget)!=B.VehicleSerial ||
            !Object->IsA(ATslWheeledVehicle::StaticClass()) || T>B.VehicleDeadline) { B.VehicleTarget=-1; B.VehicleScanAt=T+30; return false; }
        auto* Car=static_cast<ATslWheeledVehicle*>(Object); auto* Seat=Car->VehicleSeatComponent;
        if (!Seat) { B.VehicleTarget=-1; B.VehicleScanAt=T+15; return false; }
        if (Seat->GetDriver()==B.Pawn) {
            B.VehicleDriving=true; B.VehicleDeadline=T+90; B.VehicleStuckAt=T+10; B.VehicleProgress=Car->K2_GetActorLocation();
            B.VehicleLastPos=B.VehicleProgress; B.VehicleTravelled=0; B.VehicleInitialGoalDistance=Len(Flat(Center-B.VehicleProgress));
            B.VehicleAvoidUntil=B.VehicleBlockedSince=B.VehicleReverseUntil=B.VehicleReverseCooldown=B.VehicleStalledUntil=0; B.VehicleReverseAttempts=0; B.VehicleAvoidTurn=0;
#ifdef PUBG2019_TEST_CAR_FIXTURE
            if (auto* Move=Car->GetTslVehicleMovement()) {BLOG("TEST VEHICLE simulation before="+std::to_string(Move->IsSimulatingVehicle()));if(!Move->IsSimulatingVehicle())Move->SetSimulateVehicle(true);BLOG("TEST VEHICLE simulation after="+std::to_string(Move->IsSimulatingVehicle()));}
#endif
            B.HasDest=false; StopFire(B); BLOG("BOT VEHICLE driving " + B.Name); return true;
        }
        if (QueryRiders(Seat).Num()>0) { B.VehicleTarget=-1; B.VehicleScanAt=T+15; return false; }
        FVector P=Car->K2_GetActorLocation();
        if (Len(Flat(P-Here))>350.f) {
            if (T>=B.NextZoneEmergency || !B.HasDest) { B.NextZoneEmergency=T+3; MoveTo(World,B,P,T,false); SetSprint(B,true); }
            return true;
        }
        if (T>=B.VehicleAttemptAt) {
            B.VehicleAttemptAt=T+3;
            auto* DriverSeat=Seat->GetSeat(0);
            if (!DriverSeat) { B.VehicleTarget=-1; B.VehicleScanAt=T+30; return false; }
            Seat->TryToRide(B.Pawn,DriverSeat);
            BLOG("BOT VEHICLE enter requested " + B.Name);
            if (Seat->GetDriver()==B.Pawn) {
                B.VehicleDriving=true; B.VehicleDeadline=T+90; B.VehicleStuckAt=T+10; B.VehicleProgress=P;
                B.VehicleLastPos=P; B.VehicleTravelled=0; B.VehicleInitialGoalDistance=Len(Flat(Center-P));
                B.VehicleAvoidUntil=B.VehicleBlockedSince=B.VehicleReverseUntil=B.VehicleReverseCooldown=B.VehicleStalledUntil=0; B.VehicleReverseAttempts=0; B.VehicleAvoidTurn=0;
#ifdef PUBG2019_TEST_CAR_FIXTURE
                if (auto* Move=Car->GetTslVehicleMovement()) {BLOG("TEST VEHICLE simulation before="+std::to_string(Move->IsSimulatingVehicle()));if(!Move->IsSimulatingVehicle())Move->SetSimulateVehicle(true);BLOG("TEST VEHICLE simulation after="+std::to_string(Move->IsSimulatingVehicle()));}
#endif
                B.HasDest=false; StopFire(B); BLOG("BOT VEHICLE driving " + B.Name);
            }
        }
        return true;
    }

    // Use native boats only for a water crossing or when already swimming.
    bool UpdateZoneBoat(UWorld* World, FBot& B, const FVector& Here, double T)
    {
        if (!B.Pawn || !GMatchSpawnDone || B.NativeSkyActive || B.DropPending) return false;
        FVector Center{}; float Radius=0.f;
        if (!GetSafeCircle(World,Center,Radius) || Radius<1000.f) return false;
        if (B.BoatDriving) {
            APawn* Pawn=B.Pawn->GetVehiclePawn();
            if (!Pawn || !Pawn->IsA(ATslFloatingVehicle::StaticClass())) {
                B.BoatDriving=false; B.BoatTarget=-1; B.BoatScanAt=T+30; BLOG("BOT BOAT exit confirmed "+B.Name); return false;
            }
            auto* Boat=static_cast<ATslFloatingVehicle*>(Pawn); auto* Seat=Boat->VehicleSeatComponent; auto* Move=Boat->MovementComponent;
            if (!Seat || !Move || Seat->GetDriver()!=B.Pawn) {B.BoatDriving=false;B.BoatTarget=-1;B.BoatScanAt=T+30;return false;}
            const FVector P=Boat->K2_GetActorLocation(); const float Speed=Len(Flat(Boat->GetVelocity()));
            if (T>=B.BoatStuckAt) {
                if (!B.BoatHolding && Len(Flat(P-B.BoatProgress))<250.f && T>B.BoatDeadline-165) B.BoatDeadline=T;
                B.BoatProgress=P; B.BoatStuckAt=T+12;
            }
            const FVector Ahead=SafeNormal(Flat(Boat->GetActorForwardVector())), To=SafeNormal(Flat(Center-P));
            const float Dot=Ahead.X*To.X+Ahead.Y*To.Y, Cross=Ahead.X*To.Y-Ahead.Y*To.X;
            const float Turn=(std::clamp)(std::atan2(Cross,Dot)/(kPi/4.f),-1.f,1.f);
            FVector Ground{}; const float Sea=SeaZ(); const float ShoreHeight=Sea> -1e8f ? (std::max)(Sea+150.f,P.Z+100.f) : P.Z+150.f;
            const bool Shore=TraceDown(World,P.X+Ahead.X*1000.f,P.Y+Ahead.Y*1000.f,P.Z+1500.f,P.Z-1000.f,Ground) && Ground.Z>ShoreHeight;
            AActor* IgnoredActors[]={B.Pawn,Boat}; TArray<AActor*> Ignore(IgnoredActors,2,2); FHitResult Hit{}; FLinearColor Color{};
            const FVector Start=P+Ahead*500.f+FVector{0.f,0.f,150.f};
            const bool Blocked=UKismetSystemLibrary::LineTraceSingle(World,Start,Start+Ahead*700.f,ETraceTypeQuery::TraceTypeQuery1,false,Ignore,EDrawDebugTrace::None,&Hit,true,Color,Color,0.f) && Hit.bBlockingHit;
            const float GoalDistance=Len(Flat(P-Center)); if(GoalDistance<10000.f) B.BoatHolding=true; else if(GoalDistance>20000.f) B.BoatHolding=false;
            const bool Stop=Shore || Blocked || T>=B.BoatDeadline;
            auto* Fn=Move->GetClass()->GetFunction("TslFloatingVehicleMovement","ServerUpdateInput");
            if (!Fn) {BLOG("BOT BOAT movement function missing "+B.Name);return true;}
            Params::TslFloatingVehicleMovement_ServerUpdateInput Args{}; Args.InForwardInput=(Stop || B.BoatHolding)?0.f:0.8f; Args.InRotationInput=Turn;
            const auto Flags=Fn->FunctionFlags; Fn->FunctionFlags=(Flags & ~0x40u)|0x400u; Move->ProcessEvent(Fn,&Args); Fn->FunctionFlags=Flags;
            if(T>=B.BoatLogAt) {B.BoatLogAt=T+5;BLOG("BOT BOAT progress "+B.Name+" speed="+std::to_string((int)(Speed/100.f))+"m/s shore="+std::to_string(Shore)+" blocked="+std::to_string(Blocked));}
            if(Stop && Speed<150.f && T>=B.BoatAttemptAt) {B.BoatAttemptAt=T+3;Seat->TryToLeave(B.Pawn,false);BLOG("BOT BOAT exit requested "+B.Name);}
            return true;
        }
        if (B.Pawn->IsInVehicle() && B.BoatTarget<0) return false;
        const float Gap=Len(Flat(Here-Center))-Radius;
        if(B.BoatTarget<0 && (T<B.BoatScanAt || GZoneBoats.empty())) return false;
        if(B.BoatTarget<0 && !B.Pawn->IsSwimming() && (Gap<30000.f || PathOverLand(World,Here,Center))) {B.BoatScanAt=T+12;return false;}
        if(B.BoatTarget<0) {
            B.BoatScanAt=T+12;float Best=15000.f;
            for(const auto& Site:GZoneBoats) {
                if(!LiveBotObject(Site.Vehicle,Site.Index) || ObjectSerial(Site.Index)!=Site.Serial)continue;
                auto* Boat=Site.Vehicle;auto* Seat=Boat->VehicleSeatComponent;
                if(!Seat || !Boat->MovementComponent || QueryRiders(Seat).Num()>0 || Boat->FuelMax<=0.f || Boat->Fuel<Boat->FuelMax*0.05f || Boat->HealthMax<=0.f || Boat->Health<Boat->HealthMax*0.15f)continue;
                const FVector P=Boat->K2_GetActorLocation();const float D=Len(Flat(P-Here));
                if(D<Best && std::fabs(P.Z-Here.Z)<1200.f){Best=D;B.BoatTarget=Site.Index;B.BoatSerial=Site.Serial;}
            }
            if(B.BoatTarget>=0){B.BoatDeadline=T+50;B.BoatAttemptAt=0;B.HasDest=false;BLOG("BOT BOAT seek "+B.Name+" distance="+std::to_string((int)(Best/100.f))+"m");}
        }
        if(B.BoatTarget<0)return false;
        auto* Objects=UObject::GObjects.GetTypedPtr();
        if(!Objects || B.BoatTarget>=Objects->Num()){B.BoatTarget=-1;return false;}
        auto* Object=Objects->GetByIndex(B.BoatTarget);
        if(!LiveBotObject(Object,B.BoatTarget) || ObjectSerial(B.BoatTarget)!=B.BoatSerial || !Object->IsA(ATslFloatingVehicle::StaticClass()) || T>B.BoatDeadline){B.BoatTarget=-1;B.BoatScanAt=T+30;return false;}
        auto* Boat=static_cast<ATslFloatingVehicle*>(Object);auto* Seat=Boat->VehicleSeatComponent;
        if(!Seat){B.BoatTarget=-1;return false;}
        if(Seat->GetDriver()==B.Pawn){B.BoatDriving=true;B.BoatHolding=false;B.BoatDeadline=T+180;B.BoatStuckAt=T+18;B.BoatProgress=Boat->K2_GetActorLocation();B.HasDest=false;StopFire(B);BLOG("BOT BOAT driving "+B.Name);return true;}
        if(QueryRiders(Seat).Num()>0){B.BoatTarget=-1;B.BoatScanAt=T+15;return false;}
        const FVector P=Boat->K2_GetActorLocation();
        if(Len(Flat(P-Here))>350.f){
            if(B.Pawn->IsSwimming()){B.HasShoreTarget=true;B.ShoreTarget=P;}
            else if(!B.HasDest || T>=B.BoatAttemptAt){B.BoatAttemptAt=T+3;MoveTo(World,B,P,T,false);SetSprint(B,true);}
            return true;
        }
        if(T>=B.BoatAttemptAt){B.BoatAttemptAt=T+3;auto* Driver=Seat->GetSeat(0);if(!Driver){B.BoatTarget=-1;return false;}Seat->TryToRide(B.Pawn,Driver);BLOG("BOT BOAT enter requested "+B.Name);}
        return true;
    }

    void StartNativeBotSky(FBot& B, ATslCharacter* Me, double T, const char* Source)
    {
        if (B.JumpedAt > 0.0 && !B.SkyStartAttempted && !Me->IsInAircraft()) {
            B.SkyStartAttempted = true;
            BLOG("SKY START " + B.Name + " path=" + Source + " delay_ms=" + std::to_string((int)((T-B.JumpedAt)*1000.0)));
            auto* Sky = Me->SkydivingComponent;
            if (!B.SkyDirectionSet) {
                const FVector Forward = SafeNormal(Flat(Me->GetActorForwardVector()));
                B.SkyHeading = std::atan2(Forward.Y, Forward.X) * 180.f / kPi + ((GJumpSide++ & 1u) ? -90.f : 90.f);
                B.SkyDirectionSet = true;
            }
            if (Sky) {
                BLOG("PRIVATE JUMP " + B.Name + " before state=" + std::to_string((int)Sky->SkydiveState));
                Sky->Server_SetSkydiveState(ESkydiveState::Skydive_Freefall);
                // PRIVATE TEST: local ProcessEvent with RPC routing disabled.
                // Restore shared UFunction flags immediately after the call.
                if (Sky->SkydiveState == ESkydiveState::Skydive_None) {
                    auto* Fn = Sky->GetClass()->GetFunction("TslSkydiveComponent", "Server_SetSkydiveState");
                    if (Fn) {
                        const auto Flags = Fn->FunctionFlags;
                        BLOG("PRIVATE JUMP " + B.Name + " rpc flags=" + std::to_string(Flags) + " refs=" + std::to_string(Sky->CharacterRef != nullptr) + "," + std::to_string(Sky->CharacterMovementRef != nullptr));
                        Params::TslSkydiveComponent_Server_SetSkydiveState Args{};
                        Args.InSkydiveState = ESkydiveState::Skydive_Freefall;
                        Fn->FunctionFlags = (Flags & ~0x40u) | 0x400u;
                        Sky->ProcessEvent(Fn, &Args);
                        Fn->FunctionFlags = Flags;
                        BLOG("PRIVATE JUMP " + B.Name + " local call state=" + std::to_string((int)Sky->SkydiveState));
                    }
                }
                // PRIVATE TEST ONLY: check replicated state-driven animation.
                // This bypasses the normal state machine and is not a release fix.
                if (Sky->SkydiveState == ESkydiveState::Skydive_None && Sky->CharacterMovementRef) {
                    Sky->SkydiveState = ESkydiveState::Skydive_Freefall;
                    Sky->CharacterMovementRef->SetMovementMode(EMovementMode::MOVE_Custom, 1);
                    BLOG("PRIVATE JUMP " + B.Name + " forced state and movement mode for observation; mode=" +
                        std::to_string((int)Sky->CharacterMovementRef->MovementMode) + "," +
                        std::to_string((int)Sky->CharacterMovementRef->CustomMovementMode) +
                        " isSkydiving=" + std::to_string(Me->IsSkydiving()));
                }
                SetBotAim(Me,0.f,B.SkyHeading);
                Me->ForceNetUpdate();
                B.NativeSkyActive = Sky->SkydiveState != ESkydiveState::Skydive_None;
                // A native skydive owns the whole descent. The old simulated
                // drop must not also teleport this bot when the component ends.
                if (B.NativeSkyActive) B.DropPending = false;
                BLOG("PRIVATE JUMP " + B.Name + " after state=" + std::to_string((int)Sky->SkydiveState));
            }
        }
    }

    void UpdateBot(UWorld* World, FBot& B, double T)
    {
        if (!LiveBotObject(B.Ctrl, B.CtrlIndex))
        {
            CUSTOMLOG("UpdateBot: Pawn's controller is invalid");
            if (B.State!=EBotState::Dead) EnterDead(B,T);
            UnregisterBotTeam(World,B); UncountJoin(B);
            B.Remove = true;
            return;
        }

        if (B.IsPC && B.State != EBotState::Dead)
        {
            // player bots: the game spawns the pawn (now or a bit later) - pick it up / follow changes
            APawn* Cur = B.Ctrl->K2_GetPawn();
            if (Cur && Cur != B.Pawn && Cur->IsA(ATslCharacter::StaticClass()))
            {
                B.Pawn = static_cast<ATslCharacter*>(Cur);
                B.PawnIndex = Cur->GetIndex();
                B.LastHealth = B.Pawn->Health;
                B.SpawnedAt = T;
                B.Armed = false; B.ArmStep = 0; B.GiveTries = 0; B.ArmFailLogged = false; B.NextArmTry = T + 1.0;
                B.HasDest = false; B.HasDry = false;
                FVector Here = Cur->K2_GetActorLocation();
                if (B.HasTeleport)
                {
                    B.HasTeleport = false;
                    Here = B.TeleportTo + V(0.f, 0.f, 110.f);
                    FHitResult Hit{};
                    Cur->K2_SetActorLocation(Here, false, &Hit, true);
                }
                B.ProgressPos = Here;
                B.ProgressCheckAt = T + 2.0;
                BLOG(B.Name + " got its character at " + std::to_string((int)Here.X) + "," + std::to_string((int)Here.Y) + "," + std::to_string((int)Here.Z));
            }
            if (!B.Pawn)
            {
                if (!B.PawnWarned && T > B.PawnWaitUntil)
                {
                    B.PawnWarned = true;
                    BLOG(B.Name + " still has no character 15 s after joining");
                }
                return;
            }
        }

        if (B.State == EBotState::Dead)
        {
            if (T - B.DiedAt > 10.0)
            {
                DestroyBotController(B);
                B.Remove = true;
            }
            return;
        }

        ATslCharacter* Me = B.Pawn;
        if (!LiveBotObject(Me, B.PawnIndex)) { EnterDead(B, T); return; }
        // Zombie-mode bots must never enter normal looting or armed combat
        // while their zombie PlayerState is still initializing.
        const bool ZombieBotMode=World && World->AuthorityGameMode && World->AuthorityGameMode->IsA(ATslZombieGameMode::StaticClass());
        if (ZombieBotMode && (!B.Ctrl || !B.Ctrl->PlayerState ||
            !B.Ctrl->PlayerState->IsA(ATslZombiePlayerState::StaticClass()) ||
            static_cast<ATslZombiePlayerState*>(B.Ctrl->PlayerState)->GetZombieModePlayerState()!=EZombieModePlayerState::Zombie)) return;
#ifdef PUBG2019_ENABLE_DIAGNOSTICS
        if (Me==GServerOnlyZombieTarget) return; // stationary surrogate, only in the isolated server-only test
        char DelayedSpawn[4]{};
        if (B.Id==1 && GetEnvironmentVariableA("PUBG2019_ZOMBIE_DELAYED_SPAWN_PROBE",DelayedSpawn,sizeof(DelayedSpawn)) && DelayedSpawn[0]=='1')
            B.NoFireUntil=T+10.0; // the existing airborne bot only keeps this isolated test match active
#endif
        // The first landed bot seeds the infection only if the native mode has
        // not already assigned a zombie. This applies to real matches too.
        if (!GZombieBrSeeded && World && World->AuthorityGameMode && World->AuthorityGameMode->IsA(ATslZombieGameMode::StaticClass()) &&
            static_cast<ATslZombieGameMode*>(World->AuthorityGameMode)->ZombieRuleType==EZombieRuleType::BattleRoyale &&
            static_cast<ATslZombieGameMode*>(World->AuthorityGameMode)->MatchState.ToString()=="InProgress" &&
            GMatchSpawnDone && B.JumpedAt>0 && !B.NativeSkyActive && !B.DropPending &&
            !Me->IsInAircraft() && !Me->IsFallingWithParachute() && Me->GetTslCharacterMovement() &&
            !Me->GetTslCharacterMovement()->IsFalling() && B.Ctrl && B.Ctrl->PlayerState && B.Ctrl->PlayerState->IsA(ATslZombiePlayerState::StaticClass())) {
            bool AlreadyInfected=false;
            auto* State=World->AuthorityGameMode->GameState;
            if (State) for (auto* PS:State->PlayerArray) {
                if (PS && PS->IsA(ATslZombiePlayerState::StaticClass()) && static_cast<ATslZombiePlayerState*>(PS)->GetZombieModePlayerState()==EZombieModePlayerState::Zombie) { AlreadyInfected=true; break; }
            }
            if (!AlreadyInfected) for (const auto& Other:GBots) {
                if (Other.Ctrl && LiveBotObject(Other.Ctrl,Other.CtrlIndex) && Other.Ctrl->PlayerState && Other.Ctrl->PlayerState->IsA(ATslZombiePlayerState::StaticClass()) && static_cast<ATslZombiePlayerState*>(Other.Ctrl->PlayerState)->GetZombieModePlayerState()==EZombieModePlayerState::Zombie) { AlreadyInfected=true; break; }
            }
            auto* Seed=static_cast<ATslZombiePlayerState*>(B.Ctrl->PlayerState);
            if (!AlreadyInfected && Seed->GetZombieModePlayerState()==EZombieModePlayerState::Human) {
                Seed->SetZombieModePlayerState(EZombieModePlayerState::Zombie); Seed->ForceNetUpdate();
                AlreadyInfected=true; BLOG("[ZOMBIE] Initial infection assigned to landed bot " + B.Name);
            }
            if (AlreadyInfected) GZombieBrSeeded=true;
        }
        if (World && World->AuthorityGameMode && World->AuthorityGameMode->IsA(ATslZombieGameMode::StaticClass()) &&
            B.Ctrl->PlayerState && B.Ctrl->PlayerState->IsA(ATslZombiePlayerState::StaticClass()) &&
            static_cast<ATslZombiePlayerState*>(B.Ctrl->PlayerState)->GetZombieModePlayerState()==EZombieModePlayerState::Zombie &&
            (static_cast<ATslZombieGameMode*>(World->AuthorityGameMode)->ZombieRuleType != EZombieRuleType::BattleRoyale ||
                (GMatchSpawnDone && (B.ZombieGroundSpawn || B.JumpedAt > 0) && !B.NativeSkyActive && !B.DropPending && !Me->IsInAircraft() && !Me->IsFallingWithParachute()))) {
            // The zombie branch returns before the normal movement cleanup.
            // Release jump here so a previous obstacle does not leave it held.
            if (B.JumpHeld) { Me->StopJumping(); B.JumpHeld=false; }
            if (T>=B.NextZombieZoneCheck) {
                B.NextZombieZoneCheck=T+1.0;
                FVector Center{}; float Radius=0.f; bool HasWhite=false;
                auto* BaseState=UGameplayStatics::GetGameState(World);
                if (BaseState && BaseState->IsA(ATslGameState::StaticClass())) {
                    auto* State=static_cast<ATslGameState*>(BaseState);
                    if (State->SafetyZoneRadius>1000.f && State->SafetyZoneRadius<2000000.f &&
                        std::isfinite(State->SafetyZonePosition.X) && std::isfinite(State->SafetyZonePosition.Y)) {
                        Center=State->SafetyZonePosition;Radius=State->SafetyZoneRadius;HasWhite=true;
                    }
                }
                if (HasWhite) {
                    FVector Here=Me->K2_GetActorLocation(); const FVector Delta=Flat(Here-Center); const float DistanceToCenter=Len(Delta);
                    // SafetyZoneRadius is the live blue boundary, measured in centimetres.
                    if (DistanceToCenter>Radius && DistanceToCenter>1.f) {
                        const float BaseAngle=std::atan2(Delta.Y,Delta.X);
                        const float Angles[]={0.f,0.04f,-0.04f,0.08f,-0.08f,0.2f,-0.2f,0.4f,-0.4f,0.8f,-0.8f,1.4f,-1.4f,3.14f};
                        for(float Offset:Angles) {
                            const float A=BaseAngle+Offset, R=(std::max)(100.f,Radius-2000.f);
                            FVector Ground{};
                            if (!TraceLand(World,Center.X+std::cos(A)*R,Center.Y+std::sin(A)*R,Ground) ||
                                IsWaterGround(Ground.Z) || !DrySpot(World,Ground)) continue;
                            FHitResult Hit{};
                            if (Me->K2_SetActorLocation(Ground+V(0,0,110),false,&Hit,true)) {
                                Me->ForceNetUpdate(); Here=Me->K2_GetActorLocation(); B.HasDest=false;
                                BLOG("[ZOMBIE TEST] AI zombie moved inside blue circle edge: "+B.Name+
                                    " outside_m="+std::to_string((int)((DistanceToCenter-Radius)/100.f)));
                                break;
                            }
                        }
                    }
                }
            }
            ATslCharacter* Human = FindZombieHumanTarget(B, T);
            if (!Human || !B.Ctrl || !LiveBotObject(Human, B.ZombieTargetIndex)) return;
            if (static_cast<ATslZombieGameMode*>(World->AuthorityGameMode)->ZombieRuleType!=EZombieRuleType::BattleRoyale &&
                (!GMatchSpawnDone || GMatchStartedAt<=0)) return;
            static bool DumpedZombieTeams=false;
            if (!DumpedZombieTeams) { DumpedZombieTeams=true; DumpTeams(World,"zombie participant diagnostics"); }
            static bool LoggedBotAppearance=false;
            if (!LoggedBotAppearance) {
                LoggedBotAppearance=true; FRawArray Raw{}; std::memcpy(&Raw,&Me->InstanceDescriptor,sizeof(Raw));
                auto* Builder=Me->AppearanceCustomizationBuilder;
                BLOG("[ZOMBIE TEST] initial bot appearance gender="+std::to_string((int)Me->Gender)+" descriptor_bytes="+std::to_string(Raw.Num)+" builder="+(Builder ? Builder->GetFullName() : std::string("none")));
            }
            if (B.TeamActor && LiveBotObject(B.TeamActor,B.TeamActorIndex) && !B.TeamActor->bZombie) {
                B.TeamActor->bZombie=true; B.TeamActor->ForceNetUpdate();
                BLOG("[ZOMBIE TEST] bot team marked zombie for clients");
            }
            auto* TargetState=Human->GetTslPlayerState();
            if (TargetState && TargetState->IsA(ATslZombiePlayerState::StaticClass()) &&
                static_cast<ATslZombiePlayerState*>(TargetState)->GetZombieModePlayerState()!=EZombieModePlayerState::Human) {
                static bool ReportedZombieHuman=false;
                if (!ReportedZombieHuman && static_cast<ATslZombiePlayerState*>(TargetState)->GetZombieModePlayerState()==EZombieModePlayerState::Zombie) {
                    ReportedZombieHuman=true;
                    BLOG("[ZOMBIE TEST] Converted human alive as zombie; health="+std::to_string(Human->Health));
                    auto DescribeAppearance=[](const char* Label, ATslCharacter* Character) {
                        if (!Character) return;
                        FRawArray Raw{}; std::memcpy(&Raw,&Character->InstanceDescriptor,sizeof(Raw));
                        auto* Builder=Character->AppearanceCustomizationBuilder;
                        BLOG(std::string("[ZOMBIE TEST] appearance ")+Label+" gender="+std::to_string((int)Character->Gender)+" descriptor_bytes="+std::to_string(Raw.Num)+" builder="+(Builder ? Builder->GetFullName() : std::string("none")));
                        char Enabled[4]{};
                        if (GetEnvironmentVariableA("PUBG2019_ZOMBIE_APPEARANCE_PROBE",Enabled,sizeof(Enabled)) && Enabled[0]=='1' && Raw.Data && Raw.Num>0 && Raw.Num<65536 && Readable(Raw.Data,Raw.Num)) {
                            std::ofstream Out(std::string("zombie-appearance-")+Label+".bin",std::ios::binary|std::ios::trunc);
                            if (Out) Out.write(static_cast<const char*>(Raw.Data),Raw.Num);
                            auto* Instance=Character->CustomizableObjectInstance;
                            if (Instance) {
                                BLOG(std::string("[ZOMBIE TEST] customizable ")+Label+" int_count="+std::to_string(Instance->IntParameters.Num()));
                                for (int i=0;i<Instance->IntParameters.Num() && i<300;++i) {
                                    const auto& Parameter=Instance->IntParameters[i];
                                    BLOG(std::string("[ZOMBIE TEST] parameter ")+Label+" ["+std::to_string(i)+"] "+Parameter.ParameterName.ToString()+"="+Parameter.ParameterValueName.ToString());
                                }
                            }
                        }
                    };
                    DescribeAppearance("human",Human); DescribeAppearance("bot",Me);
                    auto* State=World->AuthorityGameMode->GameState;
                    if (State && State->IsA(ATslZombieGameState::StaticClass())) {
                        auto* ZS=static_cast<ATslZombieGameState*>(State);
                        BLOG("[ZOMBIE TEST] Match after conversion state="+ZS->MatchState.ToString()+" zombies="+std::to_string(ZS->NumAliveZombiePlayers)+" limit="+std::to_string(ZS->TimeLimitSeconds)+" timeup="+std::to_string(ZS->IsTimeUp())+" zombie_won="+std::to_string(ZS->bIsWinnerZombieTeam));
                    }
                }
                return;
            }
            auto* PawnState=reinterpret_cast<EZombieModePlayerState*>(reinterpret_cast<uintptr_t>(Me)+0x1194);
            // DressPass rebuilds the descriptor. Apply the zombie face/body only
            // after that pass, otherwise the rebuild erases those options again.
            if (*PawnState != EZombieModePlayerState::Zombie && S.DressBots && !GDressed.count(B.PawnIndex)) return;
            if (*PawnState != EZombieModePlayerState::Zombie) {
                if (GZombieAppearanceBudget <= 0) return;
                FRawArray BotLook{}, HumanLook{};
                std::memcpy(&BotLook,&Me->InstanceDescriptor,sizeof(BotLook));
                std::memcpy(&HumanLook,&Human->InstanceDescriptor,sizeof(HumanLook));
                char AppearanceProbe[4]{};
                const bool CaptureAppearance=GetEnvironmentVariableA("PUBG2019_ZOMBIE_APPEARANCE_PROBE",AppearanceProbe,sizeof(AppearanceProbe)) && AppearanceProbe[0]=='1';
                if (CaptureAppearance && HumanLook.Data && HumanLook.Num>0 && HumanLook.Num<65536 && Readable(HumanLook.Data,HumanLook.Num)) {
                    std::ofstream Out("zombie-appearance-human-before.bin",std::ios::binary|std::ios::trunc);
                    if (Out) Out.write(static_cast<const char*>(HumanLook.Data),HumanLook.Num);
                }
                if ((!BotLook.Data || BotLook.Num<=0) && HumanLook.Data && HumanLook.Num>0 && HumanLook.Num<65536 && Readable(HumanLook.Data,HumanLook.Num)) {
                    void* Clone=GameAlloc(HumanLook.Num);
                    if (Clone) {
                        std::memcpy(Clone,HumanLook.Data,HumanLook.Num);
                        FRawArray NewLook{Clone,HumanLook.Num,HumanLook.Num};
                        std::memcpy(&Me->InstanceDescriptor,&NewLook,sizeof(NewLook));
                        Me->Gender=Human->Gender;
                        BLOG("[ZOMBIE TEST] Prepared zombie base appearance from same-gender human, bytes="+std::to_string(HumanLook.Num));
                    }
                }
                std::memcpy(&BotLook,&Me->InstanceDescriptor,sizeof(BotLook));
                if (!BotLook.Data || BotLook.Num<=0) return;
                --GZombieAppearanceBudget;
                *PawnState=EZombieModePlayerState::Zombie;
                auto* ZombieBuilder=UZombieAppearanceCustomizationBuilder::GetDefaultObj();
                if (ZombieBuilder) {
                    Me->AppearanceCustomizationBuilder=ZombieBuilder;
                    Me->UpdateAppearence();
                    FRawArray Raw{}; std::memcpy(&Raw,&Me->InstanceDescriptor,sizeof(Raw));
                    auto* Instance=Me->CustomizableObjectInstance;
                    auto* Model=Instance ? Instance->CustomizableObject : nullptr;
                    int PatchedParameters=0;
                    char ApplyAppearance[4]{};
                    // Use the game's zombie face and body by default. Keep an explicit
                    // diagnostic override, but do not require a test-only environment variable.
                    if (!GetEnvironmentVariableA("PUBG2019_ZOMBIE_APPEARANCE_PATCH",ApplyAppearance,sizeof(ApplyAppearance))) ApplyAppearance[0]='z';
                    if ((ApplyAppearance[0]=='1' || ApplyAppearance[0]=='f' || ApplyAppearance[0]=='h' || ApplyAppearance[0]=='b' || ApplyAppearance[0]=='c' || ApplyAppearance[0]=='z') &&
                        Model && Raw.Data && Raw.Num>=8 && Instance->IntParameters.Num()>0 && Instance->IntParameters.Num()<=512 &&
                        Raw.Num==8+Instance->IntParameters.Num()*static_cast<int>(sizeof(int32)) && Readable(Raw.Data,Raw.Num)) {
                        auto* Values=static_cast<uint8*>(Raw.Data);
                        uint32 Version=0, Reserved=0; std::memcpy(&Version,Values,4); std::memcpy(&Reserved,Values+4,4);
                        if (Version==1 && Reserved==0) {
                            for (const wchar_t* Key:{L"Face",L"Hair",L"NudeBody"}) {
                                if (ApplyAppearance[0]=='f' && Key[0]!=L'F') continue;
                                if (ApplyAppearance[0]=='h' && Key[0]!=L'H') continue;
                                if (ApplyAppearance[0]=='b' && Key[0]!=L'N') continue;
                                if (ApplyAppearance[0]=='z' && Key[0]==L'H') continue;
                                const FString Name(Key);
                                const int IntIndex=Instance->FindIntParameterNameIndex(Name);
                                const int ModelIndex=Model->FindParameter(Name);
                                if (IntIndex<0 || IntIndex>=Instance->IntParameters.Num() || ModelIndex<0 || Instance->IntParameters[IntIndex].ParameterName.ToString()!=Name.ToString()) continue;
                                if (ApplyAppearance[0]=='c') {
                                    if (Key[0]==L'N' || !HumanLook.Data || HumanLook.Num!=Raw.Num || !Readable(HumanLook.Data,HumanLook.Num)) continue;
                                    std::memcpy(Values+8+IntIndex*sizeof(int32),static_cast<const uint8*>(HumanLook.Data)+8+IntIndex*sizeof(int32),sizeof(int32));
                                    ++PatchedParameters;
                                    BLOG("[ZOMBIE TEST] copied original human option for "+Name.ToString());
                                    continue;
                                }
                                const int Count=Model->GetIntParameterNumOptions(ModelIndex);
                                if (Count<=0 || Count>500) continue;
                                int Selected=-1; std::string SelectedName;
                                for (int j=0;j<Count;++j) {
                                    const std::string Option=Model->GetIntParameterAvailableOption(ModelIndex,j).ToString();
                                    // Both genders share this model's option list. Applying
                                    // a female part to a male pawn (or the reverse) makes
                                    // the generated mesh lose its head or body on clients.
                                    const char* GenderPrefix=(uint8)Me->Gender ? "F_" : "M_";
                                    if (Option.rfind(GenderPrefix,0)!=0 || Option.find("_Zombie")==std::string::npos) continue;
                                    const char* Preferred=Key[0]==L'F' ? "_06_Zombie" : (ApplyAppearance[0]=='z' ? "_05_Zombie" : "_01_Zombie");
                                    if (Selected<0 || Option.find(Preferred)!=std::string::npos) {Selected=j;SelectedName=Option;}
                                    if (Option.find(Preferred)!=std::string::npos) break;
                                }
                                if (Selected<0) continue;
                                // Descriptor values differ from the available-option ordinal.
                                // This offset produced a visible zombie in the graphical client audit.
                                const int32 EncodedOption=Selected+1;
                                std::memcpy(Values+8+IntIndex*sizeof(int32),&EncodedOption,sizeof(EncodedOption));
                                ++PatchedParameters;
                                BLOG("[ZOMBIE TEST] zombie appearance "+Name.ToString()+"="+SelectedName);
                            }
                        }
                    }
                    BLOG("[ZOMBIE TEST] native zombie appearance builder installed; descriptor_bytes="+std::to_string(Raw.Num));
                    if (CaptureAppearance && Raw.Data && Raw.Num>0 && Raw.Num<65536 && Readable(Raw.Data,Raw.Num)) {
                        std::ofstream Out("zombie-appearance-bot-after-builder.bin",std::ios::binary|std::ios::trunc);
                        if (Out) Out.write(static_cast<const char*>(Raw.Data),Raw.Num);
                        auto* Instance=Me->CustomizableObjectInstance;
                        auto* Model=Instance ? Instance->CustomizableObject : nullptr;
                        if (Model) {
                            for (const wchar_t* Key:{L"Face",L"Hair",L"NudeBody"}) {
                                const FString Name(Key); const int Index=Model->FindParameter(Name);
                                if (Index<0) continue;
                                const int Count=Model->GetIntParameterNumOptions(Index);
                                BLOG("[ZOMBIE TEST] available parameter="+Name.ToString()+" model_index="+std::to_string(Index)+" int_index="+std::to_string(Instance->FindIntParameterNameIndex(Name))+" count="+std::to_string(Count));
                                for (int j=0;j<Count && j<500;++j) {
                                    const std::string Option=Model->GetIntParameterAvailableOption(Index,j).ToString();
                                    if (Option.find("Zombie")!=std::string::npos || Option=="M_Normal" || Option=="F_Normal" || Option=="None") BLOG("[ZOMBIE TEST] available "+Name.ToString()+" ["+std::to_string(j)+"]="+Option);
                                }
                            }
                        }
                    }
                }
                Me->OnRep_ZombieModePlayerState();
                Me->ForceNetUpdate();
                BLOG("[ZOMBIE TEST] pawn set to zombie and update requested");
            }
            if (!GMatchSpawnDone) return; // Keep zombies passive while room players and bots are loading.
            if (static_cast<ATslZombieGameMode*>(World->AuthorityGameMode)->ZombieRuleType!=EZombieRuleType::BattleRoyale && T-GMatchStartedAt<20.0) return;
            FVector Here=Me->K2_GetActorLocation(), Target=Human->K2_GetActorLocation();
            FVector PursuitTarget=Target;
            auto* ZoneBase=UGameplayStatics::GetGameState(World);
            if (ZoneBase && ZoneBase->IsA(ATslGameState::StaticClass())) {
                auto* Zone=static_cast<ATslGameState*>(ZoneBase);
                const float Radius=Zone->SafetyZoneRadius;
                FVector Delta=Flat(Target-Zone->SafetyZonePosition);
                const float Distance=Len(Delta);
                if (Radius>1000.f && Radius<2000000.f && std::isfinite(Distance) && Distance>Radius-2000.f && Distance>1.f) {
                    const float R=(std::max)(100.f,Radius-2000.f);
                    PursuitTarget.X=Zone->SafetyZonePosition.X+Delta.X*R/Distance;
                    PursuitTarget.Y=Zone->SafetyZonePosition.Y+Delta.Y*R/Distance;
                }
            }
            // Normal swimmers use a shore target in SteerStep. Zombies return
            // earlier than that navigation path, so provide a pursuit target.
            if (Me->IsSwimming()) {
                StopFire(B); B.HasShoreTarget=true; B.ShoreTarget=PursuitTarget; B.HasDest=false;
                return;
            }
            if (B.HasShoreTarget) { B.HasShoreTarget=false; B.NextShoreSearch=0; }
            const float Distance=Len(Flat(Target-Here));
            if (T >= B.NextWhyLog) {
                B.NextWhyLog=T+5.0;
                BLOG("[ZOMBIE TEST] zombie="+B.Name+" distance="+std::to_string((int)Distance)+" pawn_state="+std::to_string((int)Me->GetZombieModePlayerState())+" human_health="+std::to_string(Human->Health));
            }
            StopFire(B);
            // Close horizontal distance alone is insufficient when a survivor
            // is on another floor or elevated above the zombie.
            if (Distance > 170.f || std::fabs(Target.Z-Here.Z) > 160.f) {
                // Zombie pursuit returns before the ordinary bot door pass.
                // Complete an existing open-and-cross task before replanning
                // pursuit, otherwise the zombie can stop at the closed door.
                if (B.DoorRouteIndex>=0 && UpdateDoorRoute(World,B,Here,T)) return;
                if (Len(Flat(PursuitTarget-Here))<200.f && Len(Flat(Target-PursuitTarget))>200.f) {
                    if (B.HasDest) { B.HasDest=false; B.Ctrl->StopMovement(); }
                    return;
                }
                if (T >= B.NextRoamPick) { B.NextRoamPick=T+1.0; MoveTo(World,B,PursuitTarget,T,false,120.f); }
                if (T<B.DoorWaitUntil || TryDoorRoute(World,B,Here,T)) return;
                if (B.UseSteer && B.HasDest && T>=B.NextProbe && T>=B.DetourUntil && !Me->IsSwimming()) {
                    B.NextProbe=T+0.3; const FVector Want=SafeNormal(Flat(B.Dest-Here)); bool Low=false;
                    if (PathBlocked(World,Me,Here,Want,Low)) {
                        if (Low) {Me->Jump();B.JumpHeld=true;}
                        else {
                            const float Angles[]={40.f,-40.f,75.f,-75.f,110.f,-110.f,150.f,-150.f};
                            for(float Angle:Angles) {const FVector Direction=RotYaw(Want,Angle);bool LowSide=false;
                                if(!PathBlocked(World,Me,Here,Direction,LowSide)) {B.DetourDir=Direction;B.DetourUntil=T+1.5;break;}}
                        }
                    }
                }
                if (T>=B.ProgressCheckAt) {
                    const float Moved=Len(Flat(Here-B.ProgressPos));
                    if (B.HasDest && Moved<60.f) {
                        Me->Jump(); B.JumpHeld=true;
                        if (++B.StuckCount>=3) {
                            B.StuckCount=0; B.UseSteer=true; B.HasDest=false; B.NextRoamPick=T;
                            BLOG("[ZOMBIE TEST] zombie="+B.Name+" stuck; retrying with steering");
                        }
                    } else B.StuckCount=0;
                    B.ProgressPos=Here;B.ProgressCheckAt=T+1.5;
                }
                return;
            }
            B.HasDest=false;
            if (T >= B.NextLobbyAct && B.Ctrl->LineOfSightTo(Human,V(0,0,0),false)) {
                B.NextLobbyAct=T+1.2;
                Me->Client_PlayUnarmedAttack(RandI(0,2));
                const float Before=Human->Health;
                auto* Mode=static_cast<ATslZombieGameMode*>(World->AuthorityGameMode);
                auto DamageType=Mode->ZombieDamageType;
                const FVector From=Here+V(0,0,60), To=Target+V(0,0,40), Direction=SafeNormal(To-From);
                FHitResult Hit{}; FLinearColor Color{}; TArray<AActor*> Ignore;
                bool HasHit=UKismetSystemLibrary::LineTraceSingle(Me,From,To,ETraceTypeQuery::TraceTypeQuery1,true,Ignore,EDrawDebugTrace::None,&Hit,true,Color,Color,0.f);
                auto* HitObject=HasHit ? SafeWeakGet(Hit.Actor) : nullptr;
                float Result=-1.f;
                BLOG("[ZOMBIE TEST] human bCanBeDamaged="+std::to_string((int)Human->bCanBeDamaged));
                if (DamageType && HasHit && HitObject==Human)
                    Result=UGameplayStatics::ApplyPointDamage(Human,15.f,Direction,Hit,B.Ctrl,Me,DamageType);
                auto* Attacker = B.Ctrl->PlayerState && B.Ctrl->PlayerState->IsA(ATslPlayerState::StaticClass()) ? static_cast<ATslPlayerState*>(B.Ctrl->PlayerState) : nullptr;
                auto* Victim = Human->GetTslPlayerState();
                bool Allowed = Attacker && Victim && Mode->CanDealDamage(Attacker,Victim);
                BLOG("[ZOMBIE TEST] melee type="+(DamageType ? DamageType->GetName() : std::string("none"))+" trace="+std::to_string(HasHit)+" hit="+(HitObject ? HitObject->GetName() : std::string("none"))+" target="+Human->GetName()+" result="+std::to_string(Result)+" allowed="+std::to_string(Allowed)+" human_health="+std::to_string(Before)+" -> "+std::to_string(Human->Health)+" groggy="+std::to_string(Human->IsGroggying())+" groggy_hp="+std::to_string(Human->GroggyHealth));
                if (Result == 0.f && HitObject==Human) {
                    TArray<int32> Values; int32 Checksum=0;
                    Me->Server_UnarmedHitNotify(Checksum,Hit,EPunchDamageType::PunchDamage_Light,0,Here,Values,Target,false);
                    BLOG("[ZOMBIE TEST] native punch notify human_health="+std::to_string(Human->Health));
                }
            }
            return;
        }
        if (B.State!=EBotState::Combat || Me->IsInVehicle() || Me->IsSwimming() || Me->IsInAircraft())
            SetTargeting(B,ETargetingType::Targeting_None,T);
        if (T >= B.NextBotNameCheck && LiveBotObject(B.Ctrl, B.CtrlIndex)) {
            B.NextBotNameCheck = T + 15.0;
            RepairBotPlayerName(B.Ctrl, B.Name);
        }
        if (B.GroundProbeAt > 0.0 && T >= B.GroundProbeAt) {
            const FVector P=Me->K2_GetActorLocation();
            const float DX=P.X-B.GroundProbePos.X, DY=P.Y-B.GroundProbePos.Y;
            auto* Move=Me->GetTslCharacterMovement();
            auto* WP=Me->WeaponProcessor;
            BLOG("PRIVATE GROUND " + B.Name + " distance=" + std::to_string((int)std::sqrt(DX*DX+DY*DY)) +
                " mode=" + std::to_string(Move ? (int)Move->MovementMode : -1) +
                " sky=" + std::to_string(Me->IsSkydiving()) + " weapon=" + std::to_string(Me->GetWeaponGun()!=nullptr) +
                " weaponIndex=" + std::to_string(WP ? WP->WeaponArmInfo.RightWeaponIndex : -99));
            B.GroundProbePos=P; B.GroundProbeAt=T+10.0;
        }

        if (B.Hidden)
        {
            StopFire(B);
            if (T < B.JumpedAt + B.HoldFor)
                return;
            B.Hidden = false;
            Me->SetActorHiddenInGame(false);
            Me->SetActorEnableCollision(true);
            if (Me->GetTslCharacterMovement()) Me->GetTslCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking, 0);
            Me->ForceNetUpdate();
            B.SpawnedAt = T;
            B.HasDest = false;
            B.HasDry = false;
            if (B.NoFireUntil < T + S.LandingGrace) B.NoFireUntil = T + S.LandingGrace;
            if (S.VerboseLog || GDropLogs < 10) { ++GDropLogs; BLOG(B.Name + " is on the ground now"); }
        }

        // ---------------------------------------------------------------- plane / parachute
        if (Me->IsInAircraft())
        {
            StopFire(B); SetSprint(B,false);
            if (B.HasDest) {B.HasDest=false; B.Ctrl->StopMovement();}
            // Private experiment: initialize only bot components before their ejection.
            if (!B.SkyPrepareAttempted) {
                B.SkyPrepareAttempted = true;
                if (!Me->SkydivingComponent && !Me->SkydivingComponentClass.AssetLongPathname.ToString().empty()) {
                    Me->CreateSkydiveComponent();
                    if (Me->SkydivingComponent) {
                        Me->InitilizeServerSkydiveComp();
                        auto* Sky = Me->SkydivingComponent;
                        auto* Move = Me->GetTslCharacterMovement();
                        BLOG("PRIVATE JUMP " + B.Name + " movement before=" + std::to_string(Sky->CharacterMovementRef != nullptr) + " character movement=" + std::to_string(Move != nullptr) +
                            " default auto altitude=" + std::to_string(Sky->ParachuteDeploySettings.AutoDeployAltitude) +
                            " start=" + std::to_string(Sky->ParachuteDeploySettings.StartDeployAltitude));
                        if (!Sky->CharacterMovementRef && Move) Sky->CharacterMovementRef = Move;
                        // Request opening ourselves at the measured low altitude.
                        Sky->ParachuteDeploySettings.bUseAutoDeploy = false;
                        Me->ForceNetUpdate();
                    }
                }
                BLOG("PRIVATE JUMP " + B.Name + " component=" + std::to_string(Me->SkydivingComponent != nullptr));
            }
            if (B.EjectAt <= 0.0)
            {
                const int32 Team=BotTeamNumber(B);
                if (Team) { auto& Plan=GSquadDropPlans[Team]; if (Plan.EjectAt<=0.0) Plan.EjectAt=T+RandF(S.EjectMin,S.EjectMax); B.EjectAt=Plan.EjectAt; }
                else B.EjectAt = T + RandF(S.EjectMin, S.EjectMax);
                if (S.VerboseLog) BLOG(B.Name + " is in the plane, jumping in " + std::to_string((int)(B.EjectAt - T)) + " s");
            }
            if (!B.HasSkyTarget && (!S.SuperheroDrop || B.LandTimerSet) && B.EjectAt-T<=8.0 && T>=B.NextSkyTargetSearch && T>=GNextSkyTargetSearch) {
                B.NextSkyTargetSearch=T+1.0; GNextSkyTargetSearch=T+0.1;
                auto* GM=static_cast<ATslGameMode*>(World->AuthorityGameMode);
                auto* Prep=GM ? GM->MatchPreparer : nullptr;
                if (Prep && Prep->IsA(UAirborneMatchPreparer::StaticClass())) {
                    auto Planes=static_cast<UAirborneMatchPreparer*>(Prep)->Aircrafts;
                    for (auto* Plane:Planes) {
                        if (!Plane || !Plane->VehicleSeatComponent || !Plane->bIsEnteredAtEjectionArea) continue;
                        bool Mine=Planes.Num()==1;
                        if (!Mine) for (auto* Rider:QueryRiders(Plane->VehicleSeatComponent)) if (Rider==Me) Mine=true;
                        if (Mine) { SetAircraftLootPlan(World,B,Plane,T); break; }
                    }
                }
            }
            if (T >= B.EjectAt && T >= B.NextEjectTry)
            {
                B.NextEjectTry = T + 2.0;
                bool bFound = false;

                // The native preparer owns the current aircraft. Scanning the
                // complete global object array independently for every bot can
                // stall the server just as the jump timers start expiring.
                auto* GM = static_cast<ATslGameMode*>(World->AuthorityGameMode);
                auto* Prep = GM ? GM->MatchPreparer : nullptr;
                if (!Prep || !Prep->IsA(UAirborneMatchPreparer::StaticClass())) return;
                for (auto* Plane : static_cast<UAirborneMatchPreparer*>(Prep)->Aircrafts)
                {
                    if (!Plane) continue;
                    UTslVehicleSeatComponent* Seat = Plane->VehicleSeatComponent;
                    if (!Seat)
                        continue;

                    const auto& Riders = QueryRiders(Seat);
                    bool bMine = false;
                    for (ATslCharacter* R : Riders) if (R == Me) { bMine = true; break; }
                    if (!bMine && B.EjectTries < 3)
                        continue;

                    if (!Plane->bIsEnteredAtEjectionArea)
                    {
                        // leaving before the jump zone kills you (same as the Eject prompt not showing yet)
                        bFound = true;
                        B.EjectTries = 0;
                        --B.EjectTries; // cancelled by the ++ below
                        break;
                    }
                    if (S.SuperheroDrop)
                    {
                        // only jump when there is land under the plane (it starts over the sea)
                        const FVector PP = Plane->K2_GetActorLocation();
                        FVector Below;
                        const bool bLand = TraceDown(World, PP.X, PP.Y, PP.Z - 3000.f, -30000.f, Below) && !IsWaterGround(Below.Z);
                        if (!bLand)
                        {
                            bFound = true;
                            B.EjectTries = -1; // the ++ below makes it 0 again: keep waiting
                            break;
                        }
                        if (!B.LandTimerSet)
                        {
                            // everyone whose timer ran out over the sea would jump the moment the coast
                            // appears and land in one pile - spread them over the next JumpSpread seconds
                            B.LandTimerSet = true;
                            const int32 Team=BotTeamNumber(B);
                            if (Team) { auto& Plan=GSquadDropPlans[Team]; if (Plan.LandEjectAt<=0.0) Plan.LandEjectAt=T+RandF(0.f,(std::max)(1.f,S.JumpSpread)); B.EjectAt=(std::max)(T,Plan.LandEjectAt); }
                            else B.EjectAt = T + RandF(0.f, (std::max)(1.f, S.JumpSpread));
                            B.NextEjectTry = B.EjectAt;
                            bFound = true;
                            B.EjectTries = -1;
                            break;
                        }
                    }
                    SetAircraftLootPlan(World,B,Plane,T); // final bounded attempt before leaving
                    if (!B.SkyDirectionSet) {
                        const FVector Forward=SafeNormal(Flat(Plane->GetActorForwardVector()));
                        if (B.SkyPlanSide==0.f) { const int32 Team=BotTeamNumber(B); B.SkyPlanSide=Team ? ((Team & 1) ? -1.f : 1.f) : ((GJumpSide++ & 1u) ? -1.f : 1.f); }
                        B.SkyHeading=std::atan2(Forward.Y,Forward.X)*180.f/kPi+B.SkyPlanSide*90.f;
                        B.SkyDirectionSet=true;
                        BLOG("SKY PLAN " + B.Name + " no verified loot-site; using side heading");
                    }
                    if (B.HasSkyTarget) {
                        const FVector Delta=Flat(B.SkyTarget-Plane->K2_GetActorLocation());
                        B.SkyHeading=std::atan2(Delta.Y,Delta.X)*180.f/kPi;
                    }
                    Seat->TryToLeave(Me, B.EjectTries >= 2);
                    B.DropPending = S.SuperheroDrop;
                    B.DropTries = 0;
                    {
                        // fall like a player would (skydive + chute) - the superhero landing only finishes
                        // the drop if it's still in the air after its drop time, so bots aren't waiting on the ground
                        const float Lo = (std::max)(0.f, S.DropTimeMin), Hi = (std::max)(Lo, S.DropTimeMax);
                        const double Fall = S.SuperheroDrop ? RandF(Lo, Hi) : 0.0;
                        B.NextDropTry = T + (std::max)(0.3, (double)S.VisibleFall);   // seen falling for a moment
                        B.JumpedAt = T;
                        B.HoldFor = Fall;
                        B.NoFireUntil = T + Fall + S.LandingGrace;
                    }
                    // If the native seat has already released the pawn, start
                    // its skydive in the same think instead of waiting for the next.
                    if (!Me->IsInAircraft()) StartNativeBotSky(B, Me, T, "same-think");
                    bFound = true;
                    if (S.VerboseLog || B.EjectTries == 0)
                        BLOG(B.Name + " jumps out of the plane" + (B.EjectTries ? " (try " + std::to_string(B.EjectTries + 1) + ")" : std::string()));
                    if (bMine) break;
                }
                // The 2017 implementation advances the retry once per scan,
                // including when no aircraft lists the rider.
                if (!bFound && B.EjectTries == 0)
                    BLOG(B.Name + " is in the plane but no plane lists it as a rider - retrying");
                ++B.EjectTries;
            }
            return;
        }
        B.EjectAt = 0.0;
        B.EjectTries = 0;
        // Also initialize bots ejected automatically at the end of the flight.
        if (B.SkyPrepareAttempted && B.JumpedAt <= 0.0 && !B.SkyStartAttempted) {
            B.JumpedAt = T;
            B.DropPending = S.SuperheroDrop;
            B.DropTries = 0;
            B.NoFireUntil = T + 120.0;
            BLOG("PRIVATE JUMP " + B.Name + " detected automatic aircraft exit");
        }
        StartNativeBotSky(B, Me, T, "following-think");
        if (B.NativeSkyActive) {
            auto* Sky = Me->SkydivingComponent;
            auto* Move = Me->GetTslCharacterMovement();
            const bool GroundMode = Move && (Move->MovementMode == EMovementMode::MOVE_Walking || Move->MovementMode == EMovementMode::MOVE_Swimming);
            const bool FinalState = !Sky || Sky->SkydiveState == ESkydiveState::Skydive_None || Sky->SkydiveState == ESkydiveState::Skydive_ParachuteCollapsing;
            if (GroundMode && FinalState) {
                // Native landing has already changed movement and replicated
                // the parachute collapse. Do not overwrite its state or pose.
                if (Sky && LiveBotObject(Sky, Sky->GetIndex())) {
                    B.ChuteDetachIndex = Sky->GetIndex(); B.ChuteDetachSerial = ObjectSerial(B.ChuteDetachIndex);
                    B.ChuteDetachAt = T + 0.5; // short pause after native landing
                }
                B.NativeSkyActive = false; B.DropPending = false; B.DropTries = 1;
                B.NoFireUntil = T + S.LandingGrace; B.SpawnedAt = T;
                B.GroundProbePos = Me->K2_GetActorLocation(); B.GroundProbeAt = T + 10.0;
                B.HasDest = false; B.HasDry = false;
                B.LootAreaUntil=T+180.0; B.LootAreaProgressAt=T+20.0; B.LootAreaBestDistance=1e9f;
                BLOG("LANDING NATIVE COMPLETE " + B.Name + " mode=" + std::to_string(int(Move->MovementMode)) +
                    " sky=" + std::to_string(Sky ? int(Sky->SkydiveState) : -1) + " position=" + Pos(B.GroundProbePos) +
                    " target_distance=" + std::to_string(B.HasSkyTarget ? (int)Len(Flat(B.SkyTarget-B.GroundProbePos)) : -1));
            } else if (!Sky) {
                if (T < B.JumpedAt + 120.0) return;
                B.NativeSkyActive = false; B.DropPending = true; B.NextDropTry = T; B.HoldFor = 0.0;
                BLOG("LANDING RECOVERY " + B.Name + " component missing after timeout");
            }
        }
        if (B.ChuteDetachAt > 0 && T >= B.ChuteDetachAt) {
            B.ChuteDetachAt = 0;
            auto* Sky = Me->SkydivingComponent;
            auto* Move = Me->GetTslCharacterMovement();
            const bool Ground = Move && (Move->MovementMode == EMovementMode::MOVE_Walking || Move->MovementMode == EMovementMode::MOVE_Swimming);
            if (Sky && Ground && LiveBotObject(Sky, B.ChuteDetachIndex) && ObjectSerial(B.ChuteDetachIndex) == B.ChuteDetachSerial &&
                (Sky->SkydiveState == ESkydiveState::Skydive_None || Sky->SkydiveState == ESkydiveState::Skydive_ParachuteCollapsing)) {
                UFunction* Fn = Sky->GetClass()->GetFunction("TslSkydiveComponent", "Multi_DetachParachute");
                if (Fn) { Sky->Multi_DetachParachute(); BLOG("BOT CHUTE detach multicast " + B.Name); }
                else BLOG("BOT CHUTE detach function unavailable " + B.Name);
            } else BLOG("BOT CHUTE detach skipped " + B.Name);
        }
        if (B.NativeSkyActive && Me->SkydivingComponent) {
            auto* Sky = Me->SkydivingComponent;
            // Native skydive forward input, equivalent to holding W. Feed the
            // desired heading to the server simulation, not teleport movement.
            const FVector Here = Me->K2_GetActorLocation();
            if (T >= B.NextSkySteer) {
                const float Dt=B.LastSkySteerAt>0 ? (float)(T-B.LastSkySteerAt) : 0.5f;
                B.LastSkySteerAt=T; B.NextSkySteer=T+0.5;
                float InputForward=1.f;
                const int SkyState=(int)Sky->SkydiveState;
                const bool Parachute=Sky->SkydiveState==ESkydiveState::Skydive_ParachuteDeploying ||
                    Sky->SkydiveState==ESkydiveState::Skydive_ParachuteDeployed;
                const FVector Velocity=Me->GetVelocity();
                float Distance=-1.f;
                if (B.HasSkyTarget) {
                    const FVector Delta=Flat(B.SkyTarget-Here);
                    Distance=Len(Delta);
                    const auto Approach=LandingApproach::Approach(Distance,Len(Flat(Velocity)),B.SkyApproachHold,Parachute);
                    B.SkyApproachHold=Approach.hold; InputForward=Approach.forward;
                    if (Distance>300.f) {
                        const float Desired=std::atan2(Delta.Y,Delta.X)*180.f/kPi;
                        B.SkyHeading=LandingApproach::Turn(B.SkyHeading,Desired,Dt,Parachute);
                    }
                }
                if (B.SkyLoggedState!=SkyState) {
                    B.SkyLoggedState=SkyState;
                    BLOG("SKY FLIGHT " + B.Name + " state=" + std::to_string(SkyState) +
                        " target=" + Pos(B.SkyTarget) + " loot=" + std::to_string(B.SkyTargetIsLoot) +
                        " heading=" + std::to_string(B.SkyHeading) + " velocity=" + Pos(Velocity) +
                        " distance=" + std::to_string((int)Distance));
                }
                const float Pitch=Parachute || !B.HasSkyTarget ? 0.f :
                    std::clamp(UKismetMathLibrary::FindLookAtRotation(Here,B.SkyTarget).Pitch,-85.f,-10.f);
                SetBotAim(Me,Pitch,B.SkyHeading);
                Sky->TargetHeading = B.SkyHeading;
                Sky->TargetInputForward = InputForward;
                Sky->TartgetInputRight = 0.f;
                if (B.Ctrl) B.Ctrl->SetControlRotationBP(FRotator{Pitch, B.SkyHeading, 0.f});
                // Server-owned bots do not produce the local player's normal
                // skydive input stream: the native simulation can keep moving
                // along world X even after accepting a different heading.
                // Correct only the horizontal direction of the existing
                // velocity; preserve its magnitude, vertical physics, native
                // parachute state, collision and landing. Never move the pawn.
                if (B.HasSkyTarget && Sky->CharacterMovementRef) {
                    auto* Movement=Sky->CharacterMovementRef;
                    const float Speed=Len(Flat(Movement->Velocity));
                    if (std::isfinite(Speed) && Speed>1.f && Speed<10000.f) {
                        const FVector Direction=RotToVec(0.f,B.SkyHeading);
                        Movement->Velocity.X=Direction.X*Speed;
                        Movement->Velocity.Y=Direction.Y*Speed;
                    }
                }
                const char* Names[] = {"Server_SetPlayerHeading", "Server_SetForwardInput"};
                const float Values[] = {B.SkyHeading, Sky->TargetInputForward};
                for (int i=0; i<2; ++i) {
                    UFunction* Fn = Sky->GetClass()->GetFunction("TslSkydiveComponent", Names[i]);
                    if (!Fn) continue;
                    const auto Flags = Fn->FunctionFlags;
                    Fn->FunctionFlags = (Flags & ~0x40u) | 0x400u;
                    float Arg = Values[i]; Sky->ProcessEvent(Fn, &Arg);
                    Fn->FunctionFlags = Flags;
                }
            }
            if (T >= B.NextSkyGroundCheck) {
                B.NextSkyGroundCheck = T + 0.2;
                FVector Ground;
                // Start below the pawn's capsule so the trace does not hit the bot.
                const bool HasGround = TraceSkyGround(World, Me, Here, Ground);
                if (HasGround) {
                    // The down trace sees the sea floor. Near the coast, use
                    // the known or configured sea surface instead of forcing
                    // a walking-mode landing at the bottom of the ocean.
                    const float WaterZ = SeaZ() > -1e8f ? SeaZ() : 0.f;
                    const bool OverWater = IsWaterGround(Ground.Z);
                    const float LandingZ = OverWater ? (std::max)(Ground.Z, WaterZ) : Ground.Z;
                    const float Altitude = Here.Z - LandingZ;
                    // Re-aim toward dry land before reaching the sea, with a
                    // global budget so 95 bots do not all trace at once.
                    if (OverWater && (!B.HasSkyTarget || (Altitude<12000.f && Len(Flat(B.SkyTarget-Here))>40000.f)) && Altitude<70000.f && T>=B.NextSkyShoreSearch && T>=GNextSkyShoreSearch) {
                        B.NextSkyShoreSearch = T + 15.0; GNextSkyShoreSearch = T + 0.75;
                        FVector Land{};
                        if (FindSkyShore(World, Here, Land)) {
                            B.SkyTarget = Land; B.HasSkyTarget = true; B.SkyTargetIsLoot = false; B.SkyApproachHold = false;
                            BLOG("SKY WATER " + B.Name + " steering toward land " + Pos(Land));
                        }
                    }
                    // User-requested 300 m above the terrain, measured in cm.
                    const float DeployAt = 30000.f;
                    if (T >= B.NextSkyLog) {
                        B.NextSkyLog = T + 5.0;
                        BLOG("PRIVATE ALT " + B.Name + " above_ground=" + std::to_string((int)Altitude) +
                            " deploy_at=" + std::to_string((int)DeployAt) + " state=" + std::to_string((int)Sky->SkydiveState));
                    }
                    if (Altitude <= DeployAt && Sky->SkydiveState == ESkydiveState::Skydive_Freefall && T >= B.NextSkyRequest) {
                        B.NextSkyRequest = T + 1.0;
                        Sky->Server_RequestParachuteOpen();
                        BLOG("PRIVATE ALT " + B.Name + " open requested at " + std::to_string((int)Altitude) +
                            " state=" + std::to_string((int)Sky->SkydiveState));
                    }
                    // Let the engine collide with the floor and perform its
                    // parachute-collapse transition. No 20 m landing teleport.
                    if (Altitude < 3000.f && T >= B.NextSkyLandingLog) {
                        B.NextSkyLandingLog = T + 3.0;
                        auto* Move = Me->GetTslCharacterMovement();
                        BLOG("LANDING APPROACH " + B.Name + " clearance=" + std::to_string(int(Altitude)) +
                            " mode=" + std::to_string(Move ? int(Move->MovementMode) : -1) +
                            " sky=" + std::to_string(int(Sky->SkydiveState)));
                    }
                }
            }
            if (T < B.JumpedAt + 120.0) return;
            B.NativeSkyActive = false;
            B.DropPending = true; B.NextDropTry = T; B.HoldFor = 0.0;
            BLOG("LANDING RECOVERY " + B.Name + " native descent timed out; using existing fallback");
        }

        // In 2019 ParachuteType is a UClass*, not a live actor. The 2017
        // GetParachute() API does not exist here; never cast the class to an actor.
        const bool bChute = Me->IsFallingWithParachute();

        if (B.DropPending && T >= B.NextDropTry)
        {
            // superhero landing: straight onto land below, speed zeroed -> no freefall, no fall damage
            B.NextDropTry = T + 0.7;
            const FVector Here = Me->K2_GetActorLocation();
            const float SA = RandF(0.f, 2.f * kPi), SD = RandF(0.f, (std::max)(0.f, S.DropSpread));
            FVector Ground;
            if (!FindLandNear(World, Here.X + std::cos(SA) * SD, Here.Y + std::sin(SA) * SD, Ground) &&
                !FindLandNear(World, Here.X, Here.Y, Ground) && !FindRandomSpawn(World, Ground))
                Ground = V(Here.X, Here.Y, 2000.f);
            const FVector Dest = Ground + V(0.f, 0.f, 110.f);
            FHitResult Hit{};
            if (bChute)
            {
                Me->Server_SetParachuteLandingAssist(true);
                Me->SetParachuteLandingAssist(true);
            }
            Me->K2_SetActorLocation(Dest, false, &Hit, true);
            if (Me->GetTslCharacterMovement())
                Me->GetTslCharacterMovement()->StopMovementImmediately();
            ++B.DropTries;
            // done once it is no longer on a parachute / falling from height
            const bool bStillChute = !Me->IsInAircraft() && Me->IsFallingWithParachute();
            if (!bStillChute || B.DropTries >= 4)
            {
                B.DropPending = false;
                B.NoFireUntil = (std::max)((double)(T + S.LandingGrace), B.NoFireUntil);
                const double Left = B.JumpedAt + B.HoldFor - T;
                if (Left > 1.0)
                {
                    // "still skydiving": invisible, no collision, frozen until its fall time is over
                    Me->SetActorHiddenInGame(true);
                    Me->SetActorEnableCollision(false);
                    if (Me->GetTslCharacterMovement()) Me->GetTslCharacterMovement()->DisableMovement();
                    B.Hidden = true;
                }
                if (S.VerboseLog || GDropLogs < 5)
                {
                    ++GDropLogs;
                    BLOG(B.Name + " landing spot " + Pos(Dest) + (B.Hidden ? " - shows up there in " + std::to_string((int)Left) + " s (simulated skydive)" : ""));
                }
            }
            B.SpawnedAt = T;
            B.HasDest = false;
            B.HasDry = false;
            return;
        }
        if (bChute || Me->IsFallingWithParachute() || (Me->GetTslCharacterMovement() && Me->GetTslCharacterMovement()->IsFalling() && Me->K2_GetActorLocation().Z > 20000.f))
        {
            // falling out of the sky without having jumped through us (e.g. the game ejected everyone at the
            // end of the flight path): give it the superhero landing too
            if (S.SuperheroDrop && !B.DropPending && B.DropTries == 0 && Me->K2_GetActorLocation().Z > 20000.f)
            {
                B.DropPending = true;
                B.NextDropTry = T;
                if (B.JumpedAt <= 0.0)
                {
                    const float Lo = (std::max)(0.f, S.DropTimeMin), Hi = (std::max)(Lo, S.DropTimeMax);
                    B.JumpedAt = T;
                    B.HoldFor = RandF(Lo, Hi);
                    B.NoFireUntil = T + B.HoldFor + S.LandingGrace;
                }
                return;
            }
            B.SpawnedAt = T;
            B.HasDest = false;
            return;
        }
        B.ReleaseAlt = 0.f;
        B.ChuteLogged = false;

        if ((B.BoatDriving || B.Pawn->IsSwimming()) && UpdateZoneBoat(World,B,Me->K2_GetActorLocation(),T)) return;

        // Swimming takes priority over combat and ordinary navigation.
        // Find a nearby point above the local water surface and steer there;
        // do not destroy or teleport a bot merely because it landed in water.
        {
            const bool bWet = Me->IsSwimming();
            const FVector Here = Me->K2_GetActorLocation();
            if (!bWet)
            {
                B.HasDry = true; B.LastDry = Here; B.SwimSince = 0;
                B.HasShoreTarget = false; B.NextShoreSearch = 0;
            }
            else
            {
                if (B.SwimSince <= 0) B.SwimSince = T;
                StopFire(B);
                const float Surf = Me->GetCurrentWaterSurfaceZ();
                if (std::isfinite(Surf) && Surf > -1e6f && Surf < 1e6f && Surf != 0.f && (GSeaZ < -1e8f || Surf < GSeaZ)) GSeaZ = Surf;
                if (T >= B.NextShoreSearch)
                {
                    B.NextShoreSearch = T + 8.0 + RandF(0.f, 2.f);
                    bool Found = false; FVector Shore{};
                    if (B.HasDry && Len(Flat(B.LastDry - Here)) > 100.f) { Shore = B.LastDry; Found = true; }
                    if (!Found && std::isfinite(Surf) && Surf > -1e6f && Surf < 1e6f)
                    {
                        const float Radii[] = {5000.f, 15000.f, 30000.f, 60000.f, 120000.f};
                        for (float Radius : Radii)
                        {
                            for (int i = 0; i < 8; ++i)
                            {
                                const float A = (float)i * (kPi / 4.f); FVector G;
                                if (TraceLand(World, Here.X + std::cos(A)*Radius, Here.Y + std::sin(A)*Radius, G) && G.Z > Surf + 50.f)
                                { Shore = G; Found = true; break; }
                            }
                            if (Found) break;
                        }
                    }
                    if (!Found) {
                        FVector Circle{}; float Radius = 0.f;
                        if (GetSafeCircle(World, Circle, Radius) && Len(Flat(Circle - Here)) > 100.f) { Shore = Circle; Found = true; }
                    }
                    B.HasShoreTarget = Found;
                    if (Found) B.ShoreTarget = Shore;
                    const auto* Breath = Me->BreathComponent;
                    BLOG("SWIM " + B.Name + " z=" + std::to_string((int)Here.Z) + " water=" + std::to_string((int)Surf) +
                        " breath=" + std::to_string(Breath ? Breath->Breath : -1.f) +
                        (Found ? " shore=" + Pos(Shore) : " no shore target"));
                }
                B.HasDest = false;
                return;
            }
        }

        if (B.JumpHeld) { Me->StopJumping(); B.JumpHeld = false; }

        if (GFrozen)
        {
            StopFire(B);
            return;
        }
        if (GuardBotBelowTerrain(World, B, T)) return;

        // Start the existing fallback timer on the ground even if zone travel
        // preempts the loot routine. Otherwise a travelling bot can remain
        // unarmed forever because TryLootNearby never gets to start the timer.
        if (!B.LootStarted && GMatchSpawnDone && LootLayoutSafe()) {
            auto* GroundMove=Me->GetTslCharacterMovement();
            if (GroundMove && GroundMove->MovementMode == EMovementMode::MOVE_Walking)
                B.LootStarted=T;
        }
        if (!B.Armed && T >= B.NextArmTry && (!S.UnarmedInLobby || (GMatchSpawnDone && !Me->IsInAircraft() && !B.DropPending && !B.Hidden))) {
            UWeaponProcessorComponent* ExistingWP = nullptr;
            const bool HasGun = FindGunSlot(B, ExistingWP, false) >= 0;
            // Allow real ground-loot first. Avoid leaving a bot unarmed forever
            // when this map has no reachable AR or the item layout is unknown.
            const bool LootWait = LootLayoutSafe() && (!B.LootStarted || T < B.LootStarted + 120.0);
            if (HasGun || !LootWait) TryArm(B, T);
        }
        ATslWeapon_Gun* Gun = B.Armed ? Me->GetWeaponGun() : nullptr;
        if (Gun && BotWeaponPolicy::IsCarePackageWeapon(Gun->GetClass()->GetName())) {
            StopFire(B); B.Armed=false; Gun=nullptr;
        }
        if (Gun) RefreshBotWeaponRelevance(B, Gun, T);
        if (B.Armed && !Gun)
        {
            // gun left its hands - try re-equipping slot 1 a few times before re-arming from scratch
            B.Armed = false;
            B.ArmStep = 0;
            B.NextArmTry = T + 1.0;
        }

        // took damage -> look everywhere (and further) for a few seconds, and turn toward the likely shooter
        const uint8 HitRep = Me->LastTakeHitInfo.EnsureReplicationByte;
        if (B.LastHealth >= 0.f && Me->Health < B.LastHealth - 0.5f)
        {
            B.LastHurtAt = T;
            B.AlertUntil = T + 6.0;
            // who did it? our own bot hits are recorded; otherwise the game's hit info / who just fired
            ATslCharacter* Attacker = nullptr;
            auto BH = GBotHurtByBotAt.find(Me->GetIndex());
            const bool bByBot = BH != GBotHurtByBotAt.end() && T - BH->second < 0.6;
            if (!bByBot)
            {
                if (HitRep != B.LastHitRep)
                {
                    UObject* PSO = SafeWeakGet(Me->LastTakeHitInfo.PlayerInstigator);
                    if (PSO)
                    {
                        for (const FCharInfo& C : GChars)
                        {
                            if (!C.IsBot && C.Char->GetTslPlayerState() == PSO)
                            {
                                Attacker = C.Char; break;
                            }
                        }
                    }
                }
                if (!Attacker)
                {
                    float Best = S.MaxFireDistance * 1.5f;
                    for (const FCharInfo& C : GChars)
                    {
                        if (C.IsBot) continue;
                        auto F = GHumanFiredAt.find(C.Index);
                        if (F == GHumanFiredAt.end() || T - F->second > 1.5) continue;
                        const float D = Len(C.Loc - Me->K2_GetActorLocation());

                        if (D < Best)
                        {
                            Best = D;
                            Attacker = C.Char;
                        }
                    }
                }
            }
            if (Attacker)
            {
                B.ProvokedBy = Attacker;
                B.ProvokedAt = T;
                B.EvadeUntil = T + S.EvadeTime;
                B.EvadeSide = RandI(0, 1) ? 1.f : -1.f;
                B.NextStrafe = (std::min)(B.NextStrafe, T + 0.15);
                if (S.CombatLog) BLOG(B.Name + " got shot by " + CharName(Attacker) + " - fighting back");
                if (B.State != EBotState::Combat)
                {
                    B.HeardLoc = Attacker->K2_GetActorLocation();
                    B.HeardPending = true;
                }
            }
            else
            {
                float HD; FVector HL;
                if (!bByBot && B.State != EBotState::Combat && NearestHuman(Me->K2_GetActorLocation(), S.AlertDistance, HD, HL))
                {
                    B.HeardLoc = HL;
                    B.HeardPending = true;
                }
            }
        }
        B.LastHitRep = HitRep;
        B.LastHealth = Me->Health;

        const FVector MyLoc = Me->K2_GetActorLocation();
        const bool bDowned = Me->IsGroggying();
        if (bDowned) { StopFire(B); SetTargeting(B,ETargetingType::Targeting_None,T); SetSprint(B,false);
            B.HasDest=false; B.Ctrl->StopMovement(); SetObjective(B,"downed"); return; }
#ifdef PUBG2019_TEST_CAR_FIXTURE
        // Isolated fixture only: place one landed bot next to an existing
        // working vehicle, then exercise the same native seat and drive code.
        static bool TestVehicleClaimed=false;
        if (!TestVehicleClaimed && GMatchSpawnDone && !B.NativeSkyActive && !Me->IsSwimming() && !Me->IsInVehicle() &&
            Me->GetTslCharacterMovement() && Me->GetTslCharacterMovement()->MovementMode==EMovementMode::MOVE_Walking) {
            for (const auto& Site:GZoneVehicles) {
                if (!LiveBotObject(Site.Vehicle,Site.Index) || ObjectSerial(Site.Index)!=Site.Serial) continue;
                auto* Car=Site.Vehicle; auto* Seat=Car->VehicleSeatComponent; auto* Common=Car->VehicleCommonComponent;
                if (!Seat || !Common || !Car->GetTslVehicleMovement() || Car->IsSunken() || QueryRiders(Seat).Num()>0 ||
                    Common->GetVehicleFuelPercent()<0.05f || Common->GetVehicleHealthPercent()<0.15f) continue;
                FVector P=Car->K2_GetActorLocation(); FVector Forward=SafeNormal(Flat(Car->GetActorForwardVector()));
#ifdef PUBG2019_TEST_CAR_OPEN
                // For this fixture only, find a dry, gently sloped 400 m
                // corridor and move the existing vehicle to its start.
                FVector Base{}; bool Found=false;
                if (TraceLand(World,MyLoc.X,MyLoc.Y,Base) && !IsWaterGround(Base.Z) && DrySpot(World,Base)) {
                    for(int I=0;I<16 && !Found;++I) {
                        const float A=(float)I*kPi/8.f;const FVector Dir{std::cos(A),std::sin(A),0.f};
                        float Previous=Base.Z; bool Clear=true;
                        for(int J=1;J<=4;++J){FVector G{};if(!TraceLand(World,Base.X+Dir.X*10000.f*J,Base.Y+Dir.Y*10000.f*J,G) || IsWaterGround(G.Z) || std::fabs(G.Z-Previous)>400.f){Clear=false;break;}Previous=G.Z;}
                        if(Clear){AActor* IgnoreRaw[]={Me,Car};TArray<AActor*> Ignore(IgnoreRaw,2,2);FHitResult Hit{};FLinearColor Color{};
                            const FVector Start=Base+FVector{0.f,0.f,200.f},End=Start+Dir*40000.f;
                            Clear=!(UKismetSystemLibrary::LineTraceSingle(World,Start,End,ETraceTypeQuery::TraceTypeQuery1,false,Ignore,EDrawDebugTrace::None,&Hit,true,Color,Color,0.f) && Hit.bBlockingHit);}
                        if(Clear){P=Base+FVector{0.f,0.f,150.f};Forward=Dir;Found=true;}
                    }
                }
                if(!Found)continue;
                FHitResult VehicleHit{};
                if(!Car->K2_SetActorLocation(P,false,&VehicleHit,true))continue;
                Car->K2_SetActorRotation(FRotator{0.f,std::atan2(Forward.Y,Forward.X)*180.f/kPi,0.f},true);
                BLOG("TEST VEHICLE open corridor selected");
#endif
                if (Len(Forward)<0.5f) continue;
                const FVector Side{-Forward.Y,Forward.X,0.f}; FHitResult Hit{};
                if (!Me->K2_SetActorLocation(P+Side*250.f+FVector{0.f,0.f,100.f},false,&Hit,true)) continue;
                TestVehicleClaimed=true; B.TestVehicleAssigned=true; B.TestVehicleGoal=P+Forward*100000.f;
                B.VehicleTarget=Site.Index; B.VehicleSerial=Site.Serial; B.VehicleDeadline=T+45; B.VehicleAttemptAt=0;
                B.HasDest=false; BLOG("TEST VEHICLE fixture assigned "+B.Name); break;
            }
        }
        if (B.TestVehicleAssigned) {
            if (UpdateZoneVehicle(World,B,Me->K2_GetActorLocation(),B.TestVehicleGoal,1000.f,true,false,T)) return;
            B.TestVehicleAssigned=false; BLOG("TEST VEHICLE fixture ended "+B.Name);
        }
#endif
        FVector EmergencyCenter{}; float EmergencyRadius = 0.f;
        const bool ZoneEmergency = !bDowned && GMatchSpawnDone && ZoneEmergencyTarget(World, MyLoc, EmergencyCenter, EmergencyRadius);
        FVector TravelCenter{}; float TravelRadius = 0.f, TravelGap = 0.f; bool InBlue = false;
        const bool Travel = GMatchSpawnDone && ZoneTravelTarget(World, MyLoc, B.ZoneTravel,
            TravelCenter, TravelRadius, TravelGap, InBlue);
        if (Travel != B.ZoneTravel) {
            B.ZoneTravel = Travel;
            if (Travel) {
                BLOG("BOT ZONE PLAN " + B.Name + " early rotation gap=" + std::to_string((int)(TravelGap / 100.f)) + "m");
                // Drop an old loot-door route; subsequent movement can find a
                // door on the way out using the normal traversal logic.
                B.DoorRouteIndex=-1; B.DoorRouteObject=nullptr; B.DoorWaitUntil=0;
                B.NextZoneEmergency=0; B.HasDest=false;
            } else BLOG("BOT ZONE PLAN " + B.Name + " early rotation ended");
        }
        // A recently hit bot may defend itself briefly while still in safety.
        // Once in the blue, escape takes precedence over further fighting.
        // Give a freshly landed, unarmed bot a short chance to pick up a gun
        // while it is still outside the damaging blue wall.
        const bool LootGrace = !B.Armed && !InBlue && !ZoneEmergency && B.LootStarted > 0 && T - B.LootStarted < 25.0;
        const bool ZonePriority = ZoneEmergency || (Travel && !LootGrace && (InBlue || T - B.LastHurtAt > 3.0));
        if (ZonePriority && Travel && T >= B.NextZoneProgressLog) {
            B.NextZoneProgressLog = T + 30.0;
            BLOG("BOT ZONE PROGRESS " + B.Name + " gap=" + std::to_string((int)(TravelGap / 100.f)) +
                "m in_blue=" + std::to_string(InBlue ? 1 : 0));
        }
        if (ZonePriority && !ZoneEmergency) { EmergencyCenter=TravelCenter; EmergencyRadius=TravelRadius; }
        if (ZonePriority && !B.VehicleDriving && !Me->IsInVehicle()) {
            const float CurrentGap=Len(Flat(EmergencyCenter-MyLoc))-EmergencyRadius;
            if (B.ZoneProgressCheck <= 0) {
                B.ZoneProgressPos=MyLoc; B.ZonePreviousGap=CurrentGap;
                B.ZoneProgressCheck=T+8.0; B.ZoneStuckCount=0; B.ZoneNoProgressSamples=0;
            } else if (T >= B.ZoneProgressCheck) {
                const float Moved=Len(Flat(MyLoc-B.ZoneProgressPos));
                const float GapImprovement=B.ZonePreviousGap-CurrentGap;
                const bool Stalled=ZoneBoostPolicy::NeedsNavigationRecovery(Moved,B.ZonePreviousGap,CurrentGap,B.ZoneNoProgressSamples);
                B.ZoneProgressPos=MyLoc; B.ZonePreviousGap=CurrentGap; B.ZoneProgressCheck=T+8.0;
                if (Stalled && B.DoorRouteIndex < 0 && T >= B.DoorWaitUntil) {
                    ++B.ZoneStuckCount;
                    if (!B.UseSteer) {
                        B.UseSteer=true; B.HasDest=false; B.NextZoneEmergency=0;
                        BLOG("BOT ZONE RECOVERY " + B.Name + " stalled: switched to input steering");
                    } else {
                        const FVector Dir=SafeNormal(Flat(EmergencyCenter-MyLoc));
                        const float Angles[] = {65.f, -65.f, 110.f, -110.f, 155.f, -155.f, 180.f};
                        const int Start=(B.ZoneStuckCount-1) % 7;
                        FVector Detour=RotYaw(Dir,Angles[Start]);
                        for (int I=0; I<7; ++I) {
                            const FVector Candidate=RotYaw(Dir,Angles[(Start+I)%7]);
                            bool Low=false;
                            if (!PathBlocked(World,Me,MyLoc,Candidate,Low)) { Detour=Candidate; break; }
                        }
                        B.DetourDir=Detour; B.DetourUntil=T+(std::min)(15.0,5.0+2.0*B.ZoneStuckCount);
                        B.HasDest=false; B.NextZoneEmergency=0;
                        BLOG("BOT ZONE RECOVERY " + B.Name + " stalled: trying another direction");
                    }
                } else if (GapImprovement > 200.f) B.ZoneStuckCount=0;
            }
        } else { B.ZoneProgressCheck=0; B.ZoneStuckCount=0; B.ZoneNoProgressSamples=0; }
        if (ZoneEmergency && !B.ZoneEmergency) {
            BLOG("BOT ZONE EMERGENCY " + B.Name + " is at least 500 m outside safety and in the blue; running inward");
            B.ZoneEmergency = true; B.NextZoneEmergency = 0;
        } else if (!ZoneEmergency && B.ZoneEmergency) {
            BLOG("BOT ZONE EMERGENCY " + B.Name + " ended");
            B.ZoneEmergency = false; B.NextZoneEmergency = 0;
        }
        if ((B.BoatTarget>=0 || (ZonePriority && TravelGap>30000.f)) && UpdateZoneBoat(World,B,MyLoc,T)) {SetObjective(B,"zone-boat");return;}
        if (ZoneEmergency || B.VehicleDriving || B.VehicleTarget>=0 || (ZonePriority && TravelGap>30000.f)) {
            StopFire(B); SetTargeting(B,ETargetingType::Targeting_None,T);
            B.Target=nullptr;
            if (AIC(B)) AIC(B)->K2_ClearFocus();
            B.State=EBotState::Roam;
            FVector VehicleCenter=EmergencyCenter; float VehicleRadius=EmergencyRadius;
            if (!ZoneEmergency && !GetSafeCircle(World,VehicleCenter,VehicleRadius)) { VehicleCenter=MyLoc; VehicleRadius=1.f; }
            if (UpdateZoneVehicle(World,B,MyLoc,VehicleCenter,VehicleRadius,ZoneEmergency || (ZonePriority && TravelGap>30000.f),InBlue,T)) {
                SetObjective(B,"zone-vehicle"); return;
            }
        }
        if (ZonePriority) {
            StopFire(B); B.Target = nullptr; B.HeardPending = false; B.LootTargetIndex = -1;
            if (B.State == EBotState::Combat && AIC(B)) AIC(B)->K2_ClearFocus();
            B.State = EBotState::Roam;
        }

        // ---------------------------------------------------------------- perception
        int32 TIndex = -1; FVector TLoc{}; float TDist = 0.f;
        // unarmed or downed bots don't pick fights - they keep roaming
        ATslCharacter* Target = (ZonePriority || bDowned || !Gun || !GMatchSpawnDone || T < B.NoFireUntil) ? nullptr : FindTarget(B, MyLoc, Me->GetActorForwardVector(), T < B.AlertUntil, TIndex, TLoc, TDist);

        if (!ZonePriority && !Target && S.CombatLog && GMatchSpawnDone && T >= B.NextWhyLog && B.State != EBotState::Combat)
        {
            float HD; FVector HL;
            ATslCharacter* H = NearestHuman(MyLoc, 6000.f, HD, HL);
            if (H)
            {
                B.NextWhyLog = T + 10.0;
                const char* Why = !Gun ? "no gun in its hands (re-arming)" : bDowned ? "it is downed" : T < B.NoFireUntil ? "landing grace" : 
                    !B.Ctrl->LineOfSightTo(H, V(0, 0, 0), false) ? "no line of sight" : "not facing you";

                BLOG(B.Name + " is " + std::to_string((int)(HD / 100.f)) + " m from " + CharName(H) + " but not fighting: " + Why);
            }
        }

        if (Target)
        {
            if (B.State != EBotState::Combat || Target != B.Target)
            {
                StopFire(B); B.TargetSeenAt=T; B.NextBurst=T;
                if (S.CombatLog && !IsBotChar(Target))
                    BLOG(B.Name + " spots " + CharName(Target) + " at " + std::to_string((int)(TDist / 100.f)) + " m" + (T < B.AlertUntil ? " (alerted)" : "") + (B.Hunter ? " [hunter]" : ""));
                if (B.State != EBotState::Combat)
                {
                    B.TargetSeenAt = T;
                    B.Ctrl->StopMovement();
                    B.HasDest = false;
                    B.NextStrafe = T + RandF(0.3f, 1.0f);
                    SetSprint(B, false);
                }
                B.State = EBotState::Combat;
                if (AIC(B)) AIC(B)->K2_SetFocus(Target);
            }
            B.Target = Target;
            B.TargetIndex = TIndex;
            B.LastSeenTarget = T;
            B.LastKnownTargetLoc = TLoc;

            // aim: controller rotation (pitch+yaw) and body yaw
            CacheBones();
            FVector AimPoint=TLoc+V(0.f,0.f,20.f);
            if (Target->Mesh) {
                const FVector Bone=Target->Mesh->GetSocketLocation(GSpineBone);
                if (std::isfinite(Bone.X) && std::isfinite(Bone.Y) && std::isfinite(Bone.Z) && Len(Bone-TLoc)<300.f) AimPoint=Bone;
            }
            const FRotator Look = UKismetMathLibrary::FindLookAtRotation(MyLoc + V(0, 0, 60.f), AimPoint);
            B.Ctrl->SetControlRotationBP(Look);
            Me->K2_SetActorRotation(FRotator{ 0.f, Look.Yaw, 0.f }, false);
            SetBotAim(Me, NormAngle(Look.Pitch), Look.Yaw);
            SetSprint(B,false);
            SetTargeting(B,ETargetingType::Targeting_Scoping,T);
            SetObjective(B,"combat");

            const bool bHuman = !IsBotChar(Target);
            const bool bTooFar = bHuman && TDist > S.MaxFireDistance;
            bool bMayFire = !bTooFar;
            if (Gun && T - B.TargetSeenAt >= S.ReactionTime && bMayFire)
            {
                if (bHuman) GShooterSlot[TIndex] = { B.Id, T };
                FireLogic(B, Gun, Target, MyLoc, TLoc, TDist, T);
            }
            else if (!bMayFire)
            {
                StopFire(B);
            }

            const bool bEvading = T < B.EvadeUntil;
            if (bEvading && S.EvadeCrouch && T >= B.NextCrouchFlip)
            {
                if (B.Crouched)
                {
                    Me->UnCrouch(false);
                }
                else
                {
                    Me->Crouch(false);
                }
                
                B.Crouched = !B.Crouched;
                B.NextCrouchFlip = T + RandF(0.25f, 0.6f);
            }
            else if (!bEvading && B.Crouched)
            {
                Me->UnCrouch(false);
                B.Crouched = false;
            }
            if (bEvading && T >= B.NextStrafe && B.BlockedBursts == 0)
            {
                // got shot: quick side to side steps instead of standing there
                const FVector ToT = SafeNormal(Flat(TLoc - MyLoc));
                B.EvadeSide = -B.EvadeSide;
                FVector Dest = MyLoc + V(-ToT.Y * B.EvadeSide, ToT.X * B.EvadeSide, 0.f) * RandF(300.f, 800.f);
                if (TDist < 800.f) Dest = Dest - ToT * 300.f;
                MoveTo(World, B, Dest, T, true);
                B.NextStrafe = T + RandF(0.5f, 1.0f);
            }
            else if (T >= B.NextStrafe)
            {
                const FVector ToT = SafeNormal(Flat(TLoc - MyLoc));
                const float Side = RandI(0, 1) ? 1.f : -1.f;
                FVector Dest = MyLoc + V(-ToT.Y * Side, ToT.X * Side, 0.f) * (B.BlockedBursts > 0 ? RandF(700.f, 1500.f) : RandF(250.f, 650.f));
                if (B.BlockedBursts > 2) Dest = Dest + ToT * 600.f;   // still no shot: flank in
                if (!IsBotChar(Target) && TDist > S.MaxFireDistance) Dest = Dest + ToT * 1500.f; // too far to shoot: close in
                else if (TDist > S.ViewDistance * 0.6f) Dest = Dest + ToT * 600.f;   // push in
                else if (B.Hunter && TDist > 3000.f) Dest = Dest + ToT * 500.f; // hunters keep closing
                else if (TDist < 800.f)            Dest = Dest - ToT * 300.f;   // back off
                MoveTo(World, B, Dest, T, true);
                B.NextStrafe = T + RandF(1.0f, 2.2f);
            }
        }
        else if (B.State == EBotState::Combat)
        {
            StopFire(B); SetTargeting(B,ETargetingType::Targeting_None,T);
            if (T - B.LastSeenTarget > 2.5)
            {
                if (AIC(B)) AIC(B)->K2_ClearFocus();
                B.State = EBotState::Roam;
                B.Target = nullptr;
                B.InvestigateUntil=T+8.0;
                MoveTo(World, B, B.LastKnownTargetLoc, T, false); // go check where they were
                SetSprint(B, true);
            }
        }

        if (B.DoorRouteIndex>=0) {
            if (B.State==EBotState::Combat) {
                // An immediately visible opponent takes precedence; an already
                // requested native opening may still finish in the background.
                B.DoorRouteIndex=-1; B.DoorRouteObject=nullptr; B.DoorWaitUntil=0;
            } else if (UpdateDoorRoute(World,B,MyLoc,T)) return;
        }

        // ---------------------------------------------------------------- heard gunfire / got shot
        if (B.HeardPending && B.State == EBotState::Roam && GMatchSpawnDone && T >= B.NoFireUntil)
        {
            B.HeardPending = false;
            const FVector ToS = Flat(B.HeardLoc - MyLoc);
            const float D = Len(ToS);
            const FRotator Look = UKismetMathLibrary::FindLookAtRotation(MyLoc, V(B.HeardLoc.X, B.HeardLoc.Y, MyLoc.Z));
            B.Ctrl->SetControlRotationBP(Look);
            Me->K2_SetActorRotation(FRotator{ 0.f, Look.Yaw, 0.f }, false);
            // close the distance (but not all the way), then the alerted sight check picks them up
            const float Leg = (std::clamp)(D - 3000.f, 0.f, 15000.f);
            if (Leg > 200.f)
            {
                MoveTo(World, B, MyLoc + SafeNormal(ToS) * Leg, T, false);
                SetSprint(B, true);
            }
            B.NextRoamPick = T + 8.0; B.InvestigateUntil=T+8.0;
        }
        else if (B.HeardPending && B.State == EBotState::Combat)
        {
            B.HeardPending = false;
        }

        if (B.State != EBotState::Combat)
            SetBotAim(Me, 0.f, Me->K2_GetActorRotation().Yaw); // gun forward, level

        // ---------------------------------------------------------------- roaming
        if (B.State == EBotState::Roam && !GMatchSpawnDone)
        {
            if (S.LobbyAntics)
            {
                LobbyAntics(B, Me, MyLoc, T);
            }
            else
            {
                // lobby: stand still - the start island is tiny and surrounded by water
                if (B.HasDest) { B.HasDest = false; B.Ctrl->StopMovement(); }
                SetSprint(B, false);
            }
        }
        else if (B.State == EBotState::Roam)
        {
            bool Rotate=ZonePriority;
            if (!ZonePriority && !LootGrace && S.HumanlikeGoals && S.FollowZone && GetSafeCircle(World,EmergencyCenter,EmergencyRadius) &&
                Len(Flat(EmergencyCenter-MyLoc))>EmergencyRadius*0.98f) Rotate=true;
            if (B.Crouched)
            {
                Me->UnCrouch(false);
                B.Crouched = false;
            } // lobby crouch-spam left over
            if (Rotate) {
                SetObjective(B,"rotate-zone"); B.LootTargetIndex=-1;
                if (T >= B.NextZoneEmergency || !B.HasDest) {
                    B.NextZoneEmergency = T + 4.0;
                    const FVector Dir = SafeNormal(Flat(EmergencyCenter - MyLoc));
                    const float Gap = Len(Flat(EmergencyCenter - MyLoc)) - EmergencyRadius;
                    const float Leg = (std::clamp)(Gap + 5000.f, 5000.f, S.ZoneLeg);
                    const FVector Goal = LandLeg(World, MyLoc, MyLoc + Dir * Leg, 90.f);
                    MoveTo(World, B, Goal, T, false);
                    SetSprint(B, true);
                }
            } else {
            bool LootMoving=TryFollowSquad(World,B,MyLoc,T);
            if (LootMoving) { /* Team regrouping owns the current movement. */ }
            else if (S.HumanlikeGoals && T<B.InvestigateUntil && B.HasDest) {
                SetObjective(B,"investigate"); LootMoving=true;
            } else {
                LootMoving=TryLootNearby(World,B,MyLoc,T);
                if (LootMoving) SetObjective(B,"loot-item");
                else if (S.HumanlikeGoals) LootMoving=TryReachLootArea(World,B,MyLoc,T);
            }
            if (!LootMoving) {
            if (!S.HumanlikeGoals && B.Hunter && T >= B.NextHuntReplan && T >= B.NoFireUntil)
            {
                B.NextHuntReplan = T + S.HunterReplan * RandF(0.8f, 1.2f);
                FVector ZC{}; float ZR = 0.f;
                const bool bOutsideZone = S.FollowZone && GetSafeCircle(World, ZC, ZR) && Len(Flat(ZC - MyLoc)) > ZR * 0.95f;
                float HD; FVector HL;
                // too far away, or keeps ending up in the water (no navmesh = can't path round lakes / the river):
                // put it on land a few hundred metres from its player
                if (S.HunterCatchUp && B.CanCatchUp && T >= B.NextCatchUp && NearestHuman(MyLoc, 1e9f, HD, HL) && (HD > S.HunterCatchUpDist || B.SwimResets >= 2))
                {
                    B.NextCatchUp = T + 45.0;
                    for (int Try = 0; Try < 8; ++Try)
                    {
                        const float A = RandF(0.f, 2.f * kPi), Dd = RandF(S.HunterCatchUpMin, S.HunterCatchUpMax);
                        FVector G;
                        if (!FindLandNear(World, HL.X + std::cos(A) * Dd, HL.Y + std::sin(A) * Dd, G)) continue;
                        const float GD2 = Len(Flat(G - HL));
                        if (GD2 < S.HunterCatchUpMin * 0.6f || GD2 > S.HunterCatchUpMax * 1.5f) continue;
                        if (bOutsideZone == false && GetSafeCircle(World, ZC, ZR) && Len(Flat(G - ZC)) > ZR * 0.95f) continue;
                        FHitResult Hit{};

                        Me->K2_SetActorLocation(G + V(0.f, 0.f, 110.f), false, &Hit, true);
                        if (Me->GetTslCharacterMovement()) Me->GetTslCharacterMovement()->StopMovementImmediately();
                        BLOG(B.Name + " caught up with its player (" + std::to_string((int)(HD / 100.f)) + " m away" + 
                            (B.SwimResets >= 2 ? ", kept hitting water" : "") + ") - now " + std::to_string((int)(GD2 / 100.f)) + " m away");

                        B.SwimResets = 0;
                        B.HasDest = false;
                        B.HasDry = false;
                        B.NextHuntReplan = T + 0.5;
                        return;
                    }
                }
                if (!bOutsideZone && NearestHuman(MyLoc, 1e9f, HD, HL))
                {
                    // come in from this bot's own side so a pack doesn't arrive in single file
                    const FVector FromH = SafeNormal(Flat(MyLoc - HL));
                    const FVector Goal = HL + RotYaw(FromH, B.FlankAngle) * (HD > 20000.f ? S.HunterStandoff * 2.f : S.HunterStandoff);
                    const FVector ToG = Flat(Goal - MyLoc);
                    const float GD = Len(ToG);

                    if (GD > 300.f)
                    {
                        MoveTo(World, B, GD > S.ZoneLeg ? MyLoc + SafeNormal(ToG) * S.ZoneLeg : Goal, T, false);
                        SetSprint(B, HD > 6000.f); // sprint in, walk the last 60 m
                    }

                    B.NextRoamPick = T + S.HunterReplan + 1.0; // keep normal roaming from overriding the hunt
                }
            }
            if (B.HasDest && !B.UseSteer && T - B.DestSetAt > 0.75 && AIC(B) && AIC(B)->GetMoveStatus() == EPathFollowingStatus::Idle)
            {
                B.HasDest = false; // arrived (or path following gave up)
                B.NextRoamPick = T + RandF(0.2f, 1.0f);
            }
            if (B.HasDest && T - B.DestSetAt > 90.0)
            {
                B.HasDest = false; // taking forever - pick something else
            }
            if (!B.HasDest && T >= B.NextRoamPick)
            {
                SetObjective(B,"search");
                bool bRun = false;
                MoveTo(World, B, PickRoamDestEx(World, B, MyLoc, bRun), T, false);
                SetSprint(B, bRun || RandF(0.f, 1.f) < 0.5f);
            }
            }
            }
        }

        if (T<B.DoorWaitUntil || TryDoorRoute(World,B,MyLoc,T)) return;

        // ---------------------------------------------------------------- obstacle avoidance (no navmesh)
        if (S.AvoidObstacles && B.UseSteer && B.HasDest && T >= B.NextProbe && T >= B.DetourUntil && !Me->IsSwimming())
        {
            B.NextProbe = T + 0.2;
            const FVector Want = SafeNormal(Flat(B.Dest - MyLoc));
            bool bLow = false;
            if (Len(Flat(B.Dest - MyLoc)) > 300.f && PathBlocked(World, Me, MyLoc, Want, bLow))
            {
                if (bLow)
                {
                    Me->Jump();
                    B.JumpHeld = true;
                }
                else
                {
                    const float Tries[] = { 40.f, 75.f, 110.f, 150.f, -40.f, -75.f, -110.f, -150.f };
                    if (T >= B.AvoidSideUntil || B.AvoidSide == 0) {
                        B.AvoidSide = (B.Id % 2) ? 1.f : -1.f;
                        B.AvoidSideUntil = T + 8.0;
                    }
                    const float First = B.AvoidSide; // keep one side around the same obstacle
                    for (float A : Tries)
                    {
                        const FVector D = RotYaw(Want, A * First);
                        bool bL2;
                        if (!PathBlocked(World, Me, MyLoc, D, bL2))
                        {
                            B.AvoidSide = A > 0 ? First : -First;
                            B.AvoidSideUntil = T + 8.0;
                            B.DetourDir = D;
                            B.DetourUntil = T + 1.2;
                            break;
                        }
                    }
                }
            }
        }

        // ---------------------------------------------------------------- stuck check
        if (T >= B.ProgressCheckAt)
        {
            const float Moved = Len(Flat(MyLoc - B.ProgressPos));
            if (B.HasDest && Moved < 60.f)
            {
                Me->Jump();
                B.JumpHeld = true;
                if (++B.StuckCount >= 3)
                {
                    B.StuckCount = 0;
                    B.HasDest = false;
                    B.NextRoamPick = T;
                    if (B.State == EBotState::Combat) B.NextStrafe = T;
                }
            }
            else
            {
                B.StuckCount = 0;
            }
            B.ProgressPos = MyLoc;
            B.ProgressCheckAt = T + 1.5;
        }
    }

    // Per-frame input steering for bots whose MoveToLocation doesn't work on this map.
    void SteerStep()
    {
        if (GFrozen)
            return;

        for (FBot& B : GBots)
        {
            if (B.State == EBotState::Dead || !LiveBotObject(B.Pawn, B.PawnIndex) ||
                !LiveBotObject(B.Ctrl, B.CtrlIndex)) continue;
            if (B.Pawn->IsGroggying() || B.NativeSkyActive || B.VehicleDriving || B.Pawn->IsInVehicle()) continue;
            if (Now() < B.DoorWaitUntil) continue;
            if (B.Pawn->IsSwimming())
            {
                const FVector Loc = B.Pawn->K2_GetActorLocation();
                const FVector Direction = B.HasShoreTarget ? SafeNormal(Flat(B.ShoreTarget - Loc)) : V(0.f, 0.f, 0.f);
                const float Surf = B.Pawn->GetCurrentWaterSurfaceZ();
                // First reach breathable surface water, even when no shore was found.
                const bool BelowSurface = std::isfinite(Surf) && Surf > -1e6f && Surf < 1e6f && Surf - Loc.Z > 80.f;
                const FVector Input = BelowSurface ? V(0.f, 0.f, 1.f) : Direction;
                B.Pawn->AddMovementInput(Input, 1.f, false);
                if (B.HasShoreTarget) {
                    const FRotator Face = UKismetMathLibrary::FindLookAtRotation(Loc, Loc + Direction * 100.f);
                    if (B.Ctrl) B.Ctrl->SetControlRotationBP(Face);
                }
                continue;
            }
            if (!B.UseSteer || !B.HasDest) continue;

            if (B.Pawn->IsInAircraft() || B.Pawn->IsFallingWithParachute())
                continue;

            const FVector Loc = B.Pawn->K2_GetActorLocation();
            const FVector Delta = Flat(B.Dest - Loc);
            if (Len(Delta) <= (B.DestAcceptance>0.f ? B.DestAcceptance : S.AcceptRadius))
            {
                B.HasDest = false;
                B.NextRoamPick = Now() + RandF(0.2f, 1.0f);
                continue;
            }
            const FVector Dir = (Now() < B.DetourUntil) ? B.DetourDir : SafeNormal(Delta);
            B.Pawn->AddMovementInput(Dir, 1.f, false);
            if (B.State == EBotState::Combat && LiveBotObject(B.Target, B.TargetIndex))
            {
                const FVector TL = B.Target->K2_GetActorLocation();
                const float Yaw = std::atan2(TL.Y - Loc.Y, TL.X - Loc.X) * 180.f / kPi;
                B.Pawn->K2_SetActorRotation(FRotator{ 0.f, Yaw, 0.f }, false);
            }
            else if (B.State != EBotState::Combat)
            {
                const FRotator Face = UKismetMathLibrary::FindLookAtRotation(Loc, Loc + Dir * 100.f);
                if (B.IsPC && B.Ctrl)
                    B.Ctrl->SetControlRotationBP(Face);
                else
                    B.Pawn->K2_SetActorRotation(Face, false);
            }
        }
    }

    void RemoveAllBots()
    {
        int N = 0;
        for (FBot& B : GBots)
        {
            UnregisterBotTeam(GWorld, B);
            UncountJoin(B);
            StopFire(B);
            if (LiveBotObject(B.Pawn, B.PawnIndex)) B.Pawn->K2_DestroyActor();
            DestroyBotController(B);
            ++N;
        }
        GBots.clear();
        GPendingSpawns = 0;
        GPendingNearSpawns = 0;
        GPendingLobbySpawns = 0;
        BLOG("Removed " + std::to_string(N) + " bots");
    }

    // Re-resolve everything that depends on settings (safe to call with live bots)
    void ApplySettings(UWorld* World)
    {
        GPawnClass = nullptr;
        GWeapons.clear();
        GDamageType = TSubclassOf<UDamageType>(nullptr);
        GGunDamageResolved = false;

        if (!World)
            return;

        if (!S.DamageTypeClass.empty())
        {
            UClass* DT = UObject::FindClassFast(S.DamageTypeClass);
            GDamageType = TSubclassOf<UDamageType>(DT);
            BLOG("DamageTypeClass " + S.DamageTypeClass + (DT ? " found" : " NOT found - using engine default"));
        }
    }

    bool   GZoneWasReleasing = false;
    double GZoneStartReal = 0, GZoneNextLog = 0, GZoneNextPush = 0;
    float  GZoneStartGame = 0.f;
    double GPerfWinStart = 0; float GPerfWinGame = 0.f; double GPerfLastThink = 0, GPerfMaxGap = 0;
    bool   GZoneNetTuned = false;

    std::unordered_map<int, std::string> GRoomTeamSignatures;
    double GNextRoomTeamCheck = 0;
    bool GRoomMatchAnnounced = false;

    void OnWorldChanged(UWorld* World)
    {
        // Old objects are gone with the old world - forget them without touching them.
        GRoomTeamSignatures.clear();
        GNextRoomTeamCheck = 0;
        GRoomMatchAnnounced = false;
        GBots.clear();
        GChars.clear();
        GZombieHumans.clear(); GNextZombieHumanSnapshot = 0;
        GZombieBrSeeded = false;
        GZombieAppearancePhase = false;
        GHumanNameRequests.clear();
        GNextHumanNameCheck = 0.0; GNextServerHostNameCheck = 0.0;
        GPendingSpawns = 0;
        GPendingNearSpawns = 0;
        GMatchSpawnDone = false;
        GBonesCached = false;
        GMatchStateKnown = false;
        GSeaZ = -1e9f;
        GPreStartCleared = false;
        GZombieStartWaitSince = 0; GZombieStartNextCheck = 0;
        GMatchStartedAt = 0.0;
        GNextKickTry = 0.0;
        GKickAnnounced = false;
        GPlanesSeen.clear();
        GDoors.clear(); GDoorIndices.clear(); GDoorCells.clear(); GDoorRetryAfter.clear(); GZoneVehicles.clear(); GZoneVehicleIndices.clear(); GZoneBoats.clear(); GZoneBoatIndices.clear();
        GDoorLayoutChecked = false; GDoorLayoutSafe = false;
        GDoorStateOffset = -1; GDoorDispatchLogs = 0;
        GDoorNativeChecked = false; GDoorNativeSafe = false; GEntranceTraceAt = 0;
        GLoot.clear(); GLootCells.clear(); GLootIndices.clear(); GLootClusterCounts.clear(); GLootLandingSites.clear(); GReservedLandingCells.clear(); GNextSkyTargetSearch = 0;
        GLootScanIndex = 0; GLootTailIndex = -1; GLootPriorityIndex = -1; GLootPrioritySnapshot = 0; GNextLootSiteRefresh = 0; GNextLootCacheStatus=0; GLootRescanAt = 0; GLootLayoutChecked = false; GLootLayoutSafe = false;
        GZoneClosingsStarted = 0; GZoneReleaseObserved = false; GZonePreviousRelease = false;
        GZoneSpeedLayoutChecked = false; GZoneSpeedLayoutSafe = false;
        GJumpSide = 0; GNextSkyShoreSearch = 0; GNextBotTerrainTrace = 0; GBuildingTargets.clear(); GBuildingIndices.clear(); GBuildingScanIndex = 0; GBuildingRescanAt = 0;
        GHaveCounts = false;
        GLastPublishedZombieCount = -1;
        GEndPending = false;
        GTraceDumped = false;
        GNatAlive = GNatTeams = -1;
        GNatAliveLogged = GNatTeamsLogged = -2;
        GLastWroteAlive = GLastWroteTeams = -1;
        GLastHumanAliveAt = 0;
        GHumanTeamsAlive = 0;
        GNoHumansSince = 0;
        GMatchOver = false;
        GDressed.clear(); GPlayerDressFirstSeen.clear();
        GAppearanced.clear();
        GZoneNetTuned = false; GZoneWasReleasing = false; GPerfWinStart = 0; GPerfLastThink = 0; GPerfMaxGap = 0;
        GClaims.clear();
        GPawnFromPlayer = false;
        GGenderClsResolved = false; GFemaleCls = GMaleCls = nullptr;
        GDonorChars.clear();
        GStartPlayers = GStartTeams = 0;
        GHumanAirborneAt.clear();
        GCountGS = nullptr;
        GPendingLobbySpawns = 0;
        GLobbySpawned = false;
        GCarryOver = 0;
        GSpawnNotBefore = 0;
        GBotTeam = -1; GGroupTeam = -1; GGroupMembers = 0; GSquadDropPlans.clear(); GModeApplied = false; GIsTeamSet = false;
        GWorld = World;
        if (World)
            BLOG("World changed - bot manager reset");
        ApplySettings(World);
    }

    void PrintStatus()
    {
        int Alive = 0, Armed = 0, Combat = 0, Steer = 0;
        for (const FBot& B : GBots)
        {
            if (B.State == EBotState::Dead) continue;
            ++Alive;
            if (B.Armed) ++Armed;
            if (B.State == EBotState::Combat) ++Combat;
            if (B.UseSteer) ++Steer;
        }
        BLOG("bots alive " + std::to_string(Alive) + "/" + std::to_string(GBots.size()) +
            " | armed " + std::to_string(Armed) + " | in combat " + std::to_string(Combat) +
            " | input-steering " + std::to_string(Steer) + " | pending " + std::to_string(GPendingSpawns + GPendingNearSpawns));
        BLOG(std::string("frozen ") + (GFrozen ? "yes" : "no") +
            " | hook calls/s " + std::to_string((int)GTickRate) + " | gun classes " + std::to_string(GWeapons.size()) +
            " | hits applied " + std::to_string(GHitsApplied) + " no-effect " + std::to_string(GHitsNoEffect));
    }

    std::string Pos(const FVector& P)
    {
        return std::to_string((int)P.X) + "," + std::to_string((int)P.Y) + "," + std::to_string((int)P.Z);
    }

    void PrintGameCounters(UWorld* World)
    {
        AGameStateBase* GSB = World ? UGameplayStatics::GetGameState(World) : nullptr;
        if (!GSB || !GSB->IsA(ATslGameState::StaticClass()))
            return;
        ATslGameState* GS = static_cast<ATslGameState*>(GSB);
        BLOG("live: humans alive " + std::to_string(GHumansAlive) + " | bots alive " + std::to_string(GBotsAlive) +
            " | teams alive " + std::to_string(GTeamsAlive) + " | match ends held back " + std::to_string(GVetoCount) + "x");
        BLOG("game: joined " + std::to_string(GS->NumJoinPlayers) + " | alive " + std::to_string(GS->NumAlivePlayers) +
            " | alive teams " + std::to_string(GS->NumAliveTeams) + " | start players " + std::to_string(GS->NumStartPlayers) +
            " | PlayerArray " + std::to_string(GS->PlayerArray.Num()));
    }

    // Who is where: every character on the server (real players, the server's own local pawn, bots)
    void PrintWhere(UWorld* World)
    {
        GatherCharacters(World);
        BLOG("characters on the server: " + std::to_string(GChars.size()));
        for (const FCharInfo& CI : GChars)
        {
            std::string What;
            if (CI.IsBot)
            {
                for (const FBot& B : GBots)
                    if (B.Pawn == CI.Char)
                    {
                        What = B.Name + (B.IsPC ? " [player]" : " [ai]") + (CI.Char->IsInAircraft() ? " IN PLANE" : "") +
                            (CI.Char->IsSwimming() ? " IN WATER" : "") + (B.Armed ? " armed" : " UNARMED") +
                            (B.State == EBotState::Combat ? " fighting" : " roaming") +
                            (B.UseSteer ? " (steering)" : "");
                        break;
                    }
            }
            else
            {
                AController* C = CI.Char->Controller;
                if (C && C->IsA(APlayerController::StaticClass()))
                    What = static_cast<APlayerController*>(C)->bIsLocalPlayerController ? "SERVER'S OWN LOCAL PAWN (the ghost)" : "real player";
                else
                    What = C ? "AI / other controller" : "no controller";
                if (C && C->PlayerState)
                    What += " '" + C->PlayerState->PlayerName.ToString() + "'";
            }
            std::string Team;
            AController* TC = CI.Char->GetController();

            if (TC && TC->PlayerState && TC->PlayerState->IsA(ATslPlayerState::StaticClass()))
                Team = " team " + std::to_string(static_cast<ATslPlayerState*>(TC->PlayerState)->TeamNumber);

            BLOG("  " + What + Team + (CI.Char->GetTeam() ? " (ATeam yes)" : " (ATeam NO)") + " at " + Pos(CI.Loc) + " hp " + std::to_string((int)CI.Char->Health));
        }
        PrintGameCounters(World);
        if (SeaZ() > -1e8f) BLOG("water level used for spawns: Z " + std::to_string((int)SeaZ()));
    }

    // Recount who is alive (humans = characters with a PlayerController) and push humans + bots into
    // the game's counters, so JOINED / "X left" / ranks include the bots.
    void SyncGameCounters(UWorld* World)
    {
        if (!World || !World->AuthorityGameMode) return;
        if (IsTrainingWorld(World)) { GHaveCounts = false; return; }
        AGameStateBase* GSB = World->AuthorityGameMode->GameState;
        if (!GSB)
        {
            BLOG("GameState is invalid");
            return;
        }

        ATslGameState* GS = static_cast<ATslGameState*>(GSB);

        std::vector<int32> Teams;
        auto AddTeam = [&Teams](int32 T) { if (std::find(Teams.begin(), Teams.end(), T) == Teams.end()) Teams.push_back(T); };

        int Humans = 0;
        int ConvertedZombies = 0;
        int JoinedHumans = 0;
        const bool ZombieMode=World->AuthorityGameMode->IsA(ATslZombieGameMode::StaticClass());
        std::unordered_set<APlayerController*> CountedControllers;
        std::unordered_set<APlayerController*> CountedHumanControllers;
        std::unordered_set<ATslPlayerState*> CountedHumanStates;
        std::unordered_set<ATslPlayerState*> JoinedStates;
        // PlayerState/NetConnection survive the native aircraft handoff, during
        // which the old lobby pawn may be dying or temporarily unpossessed.
        // Exclude our registered bot states and the listen-server local ghost.
        for (APlayerState* PS : GS->PlayerArray)
        {
            if (!PS || PS->bIsSpectator || PS->bOnlySpectator || IsBotPS(PS) || !PS->IsA(ATslPlayerState::StaticClass())) continue;
            AActor* Owner = PS->GetOwner();
            if (!Owner || !Owner->IsA(APlayerController::StaticClass())) continue;
            auto* PC = static_cast<APlayerController*>(Owner);
            if (!PC->NetConnection || PC->bIsLocalPlayerController || !CountedControllers.insert(PC).second) continue;
            auto* State = static_cast<ATslPlayerState*>(PS);
            if (State->bQuitter) continue;
            ++JoinedHumans;
            JoinedStates.insert(State);
            if (State->bKilled) continue;
            if (ZombieMode && State->IsA(ATslZombiePlayerState::StaticClass()) &&
                static_cast<ATslZombiePlayerState*>(State)->GetZombieModePlayerState()==EZombieModePlayerState::Zombie) { ++ConvertedZombies; continue; }
            ++Humans;
            CountedHumanControllers.insert(PC);
            CountedHumanStates.insert(State);
            AddTeam(State->TeamNumber);
        }
        // A native aircraft handoff can mark the lobby PlayerState killed even
        // though the connected player's character is still alive. The game's
        // fresh pawn snapshot is authoritative evidence during that handoff.
        TArray<AActor*> RemoteControllers;
        bool RemoteControllersScanned=false;
        if (ZombieMode) for (const FCharInfo& Info : GChars) {
            if (Info.IsBot || !LiveBotObject(Info.Char,Info.Index) || Info.Char->Health<=0.f) continue;
            auto* State=Info.Char->GetTslPlayerState();
            if (!State || CountedHumanStates.count(State) || !State->IsA(ATslZombiePlayerState::StaticClass())) continue;
            const auto Role=static_cast<ATslZombiePlayerState*>(State)->GetZombieModePlayerState();
            auto RemoteController=[](AActor* Actor)->APlayerController* {
                if (!Actor || !Actor->IsA(APlayerController::StaticClass())) return nullptr;
                auto* PC=static_cast<APlayerController*>(Actor);
                return PC->NetConnection && !PC->bIsLocalPlayerController ? PC : nullptr;
            };
            auto* RemotePC=RemoteController(State->GetOwner());
            if (!RemotePC) RemotePC=RemoteController(Info.Char->GetController());
            if (!RemotePC) {
                if (!RemoteControllersScanned) {
                    UGameplayStatics::GetAllActorsOfClass(World,APlayerController::StaticClass(),&RemoteControllers);
                    RemoteControllersScanned=true;
                }
                for (auto* Actor : RemoteControllers) {
                    auto* PC=RemoteController(Actor);
                    if (PC && PC->PlayerState==State) {RemotePC=PC;break;}
                }
            }
            if (RemotePC && CountedHumanControllers.count(RemotePC)) continue;
            if (!ZombieHumanCountPolicy::RecoverLiveHuman(Info.IsBot,Info.Char->Health>0.f,
                    Role==EZombieModePlayerState::Human || Role==EZombieModePlayerState::ReadyToRevive,
                    State->bQuitter,State->bOnlySpectator,RemotePC!=nullptr)) {
                static double NextSkippedLog=0;
                if (GMatchSpawnDone && !Humans && Now()>=NextSkippedLog) {
                    NextSkippedLog=Now()+10.;
                    BLOG("[ZOMBIE] Living pawn not counted: role="+std::to_string((int)Role)+
                        " killed="+std::to_string((int)State->bKilled)+" quitter="+std::to_string((int)State->bQuitter)+
                        " spectator="+std::to_string((int)State->bOnlySpectator)+" connected="+std::to_string(RemotePC!=nullptr)+
                        " plane="+std::to_string(Info.Char->IsInAircraft()));
                }
                continue;
            }
            ++Humans; CountedHumanStates.insert(State); AddTeam(State->TeamNumber);
            if (RemotePC) CountedHumanControllers.insert(RemotePC);
            if (JoinedStates.insert(State).second && (!RemotePC || CountedControllers.insert(RemotePC).second)) ++JoinedHumans;
            static double NextRecoveryLog=0;
            if (Now()>=NextRecoveryLog) {NextRecoveryLog=Now()+10.;BLOG("[ZOMBIE] Counting connected living player during character handoff; player-state killed="+std::to_string((int)State->bKilled));}
        }
        const int HumanTeams=ZombieMode && S.TeamSize==1 ? Humans : (int)Teams.size();
        int Bots = 0;
        for (const FBot& B : GBots)
        {
            if (B.State == EBotState::Dead || !B.Pawn || B.Pawn->Health <= 0.f) continue;
            ++Bots;

            if (B.Ctrl && B.Ctrl->PlayerState && B.Ctrl->PlayerState->IsA(ATslPlayerState::StaticClass())) {
                AddTeam(static_cast<ATslPlayerState*>(B.Ctrl->PlayerState)->TeamNumber);
            }
            else {
                AddTeam(-2000 - B.Id);
            }
        }
        GHumansAlive = Humans;
        GHumanTeamsAlive = HumanTeams;
        GHumanConvertedZombies = ConvertedZombies;
        if (Humans > 0) GLastHumanAliveAt = Now();
        GBotsAlive = Bots;
        GTeamsAlive = (int)Teams.size();
        // Direct writes made around ProcessEvent are not sufficient to promptly
        // refresh the replicated zombie scoreboard as AI spawns and dies.
        if (ZombieMode && GS->IsA(ATslZombieGameState::StaticClass())) {
            const bool GroundSpawnBR=static_cast<ATslZombieGameMode*>(World->AuthorityGameMode)->ZombieRuleType==EZombieRuleType::BattleRoyale && S.SpawnOnMatchStart;
            const int Planned=GroundSpawnBR && S.Enabled ? (std::min)((std::max)(0,S.Count),(std::max)(0,S.MaxBots)) : 0;
            const int Count=GroundSpawnBR ? ZombieHumanCountPolicy::DisplayZombieCount(Bots+ConvertedZombies,GPendingSpawns+GPendingNearSpawns,Planned,GMatchSpawnDone) : Bots+ConvertedZombies;
            GZombieDisplayCount=Count;
            auto* State=static_cast<ATslZombieGameState*>(GS);
            if (ZombieHumanCountPolicy::PublishZombieCount(GLastPublishedZombieCount,Count,State->NumAliveZombiePlayers)) {
                GLastPublishedZombieCount=Count;
                State->NumAliveZombiePlayers=Count;
                State->OnRep_NumAliveZombiePlayers();
                State->ForceNetUpdate();
                BLOG("[ZOMBIE] scoreboard humans="+std::to_string(Humans)+" zombies="+std::to_string(Count));
            }
        }

        if (!S.BotsInPlayerCount)
        {
            GHaveCounts = false;
            return;
        }

        GCountGS = GS;
        GCountGSIndex = GS->GetIndex();
        GCountsInMatch = GMatchSpawnDone;
        GWantJoined = JoinedHumans + (ZombieMode ? 0 : Bots);
        GWantAlive = ZombieMode ? ZombieHumanCountPolicy::TotalAliveForHud(Humans, GZombieDisplayCount) : Humans + Bots;
        // Humans retain their own solo/team identities. Zombies are the
        // additional opposing side for the native match/zone controller.
        GWantTeams = ZombieMode ? HumanTeams + (GZombieDisplayCount > 0 ? 1 : 0) : GTeamsAlive;
        // Seed all counters in the lobby too: the native start callback can
        // otherwise evaluate a solo win before our next periodic AI think.
        if (!GMatchSpawnDone || Now() - GMatchStartedAt < 90.0)
        {
            GStartPlayers = (std::max)(GStartPlayers, GWantAlive);
            GStartTeams = (std::max)(GStartTeams, GWantTeams);
        }
        GHaveCounts = true;
    }

    // Apply before and after each game-thread ProcessEvent, including nested
    // native aircraft setup calls. Plain field writes do not trigger events.
    void ApplyCounters()
    {
        if (GetCurrentThreadId() != GGameThreadId || !GHaveCounts || !GCountGS ||
            GCountGSIndex < 0 || GCountGSIndex >= UObject::GObjects->Num() ||
            UObject::GObjects->GetByIndex(GCountGSIndex) != GCountGS) return;

        if (GLastWroteAlive >= 0 &&
            (GCountGS->NumAlivePlayers != GLastWroteAlive || GCountGS->NumAliveTeams != GLastWroteTeams))
        {
            GNatAlive = GCountGS->NumAlivePlayers;
            GNatTeams = GCountGS->NumAliveTeams;
        }
        GCountGS->NumJoinPlayers = GCountsInMatch ? GStartPlayers : GWantJoined;
        GCountGS->NumAlivePlayers = GWantAlive;
        GCountGS->NumAliveTeams = GWantTeams;
        GCountGS->NumStartPlayers = GStartPlayers;
        GCountGS->NumStartTeams = GStartTeams;
        GLastWroteAlive = GWantAlive;
        GLastWroteTeams = GWantTeams;

        auto* GM = static_cast<ATslGameMode*>(GCountGS->AuthorityGameMode);
        if (GM && GM->IsA(ATslZombieGameMode::StaticClass()) && GCountGS->IsA(ATslZombieGameState::StaticClass())) static_cast<ATslZombieGameState*>(GCountGS)->NumAliveZombiePlayers=GZombieDisplayCount;
        if (GM) GM->NumBots = GBotsAlive;
        if (GM && !GIsTeamSet)
        {
            GM->TeamSize = S.TeamSize;
            GCountGS->TeamSize = S.TeamSize;
            if (S.TeamSize > 1) GM->SquadSize = GCountGS->SquadSize = S.TeamSize;
            GM->bEnableSquadInTeam = S.TeamSize > 1;
            if (S.TeamSize > 1) GCountGS->bUsingSquadInTeam = GCountGS->bIsTeamMatch = true;
            GIsTeamSet = true;
            BLOG("Team size set to " + std::to_string(S.TeamSize));
        }
    }

    // The listen-server's local control pawn also occupies an aircraft seat,
    // although it is not a participating human. Keep native seat occupancy
    // intact and publish counts that omit this server-only passenger in Zombie BR.
    void SyncZombieAircraftPassengerCount(UWorld* World, double T)
    {
        static UWorld* LastWorld=nullptr;
        static double NextCheck=0;
        static std::unordered_map<int32,int32> AdjustedPlanes;
        if (World!=LastWorld) { LastWorld=World; NextCheck=0; AdjustedPlanes.clear(); }
        if (!World || T<NextCheck || !World->AuthorityGameMode ||
            !World->AuthorityGameMode->IsA(ATslZombieGameMode::StaticClass())) return;
        auto* Mode=static_cast<ATslZombieGameMode*>(World->AuthorityGameMode);
        if (Mode->ZombieRuleType!=EZombieRuleType::BattleRoyale || Mode->MatchState.ToString()!="InProgress") return;
        NextCheck=T+0.5;
        auto* Preparer=Mode->MatchPreparer;
        if (!Preparer || !Preparer->IsA(UAirborneMatchPreparer::StaticClass())) return;
        auto* Airborne=static_cast<UAirborneMatchPreparer*>(Preparer);
        for (auto* Plane:Airborne->Aircrafts) {
            if (!Plane || !LiveBotObject(Plane,Plane->GetIndex()) || !Plane->VehicleSeatComponent) continue;
            const auto& Riders=QueryRiders(Plane->VehicleSeatComponent);
            int Ghosts=0, RealHumans=0;
            for (auto* Rider:Riders) {
                auto* Controller=Rider ? Rider->GetController() : nullptr;
                if (!Controller || !Controller->IsA(APlayerController::StaticClass())) continue;
                auto* PC=static_cast<APlayerController*>(Controller);
                if (PC->NetConnection && !PC->bIsLocalPlayerController) ++RealHumans;
                else if (!PC->NetConnection && PC->bIsLocalPlayerController) ++Ghosts;
            }
            if (!Ghosts || !RealHumans) continue;
            const int Actual=(std::max)(0,Riders.Num()-Ghosts);
            const int32 Index=Plane->GetIndex(), Serial=ObjectSerial(Index);
            const auto It=AdjustedPlanes.find(Index);
            bool Changed=false;
            if (It==AdjustedPlanes.end() || It->second!=Serial) {
                AdjustedPlanes[Index]=Serial;
                Plane->TotalRiderCount=(std::max)(0,Plane->TotalRiderCount-Ghosts);
                Changed=true;
            }
            if (Plane->RiderCount!=Actual) {Plane->RiderCount=Actual;Changed=true;}
            if (Changed) Plane->ForceNetUpdate();
        }
    }

    void KickFromPlanes(UWorld* World, double T)
    {
        if (S.PlaneKickTime <= 0.f || GMatchStartedAt <= 0.0 || !World)
            return;

        const double Since = Now() - GMatchStartedAt;
        if (Since < S.PlaneKickTime || T < GNextKickTry)
            return;

        GNextKickTry = T + 2.0;
        TUObjectArray* Arr = UObject::GObjects.GetTypedPtr();
        if (!Arr)
            return;

        const int32 Num = Arr->Num();
        for (int32 i = 0; i < Num; ++i) {
            UObject* O = Arr->GetByIndex(i);
            if (!O || O->IsDefaultObject())
                continue;

            if (!O->IsA(ATransportAircraftVehicle::StaticClass()))
                continue;

            ATransportAircraftVehicle* Plane = static_cast<ATransportAircraftVehicle*>(O);
            UTslVehicleSeatComponent* Seat = Plane->VehicleSeatComponent;

            if (!Seat)
                continue;

            const auto& Riders = QueryRiders(Seat);
            if (Riders.Num() <= 0)
                continue;
            const FVector PP = Plane->K2_GetActorLocation();
            FVector Below;
            const bool bLand = TraceDown(World, PP.X, PP.Y, PP.Z - 3000.f, -30000.f, Below) && !IsWaterGround(Below.Z);
            if (!bLand && Since < S.PlaneKickTime + 60.0)
                continue; // give it up to a minute to reach land
            if (!GKickAnnounced)
            {
                GKickAnnounced = true;
                BLOG("Plane time is up - throwing " + std::to_string(Riders.Num()) + " rider(s) out" + (bLand ? "" : " (still over water)"));
            }
            Plane->EjectAll();
            for (ATslCharacter* R : Riders)
                if (R && R->IsInAircraft())
                    Seat->TryToLeave(R, true);
        }
    }

    struct FTraceEnt { UFunction* Func; UClass* Cls; };
    FTraceEnt   GTrace[2048];
    uint32      GTraceHead = 0;

    FName       GNameWaitingPost{}, GNameInProgress{};
    bool        GHaveNames = false;
    bool        GGuardTried = false, GGuardOk = false;
    bool        GSetStateByRef = false;
    using FnEndMatch = void (*)(void*, uint64, uint64, uint64);       // extra args passed straight through
    using FnSetMatchState = void (*)(void*, uint64, uint64, uint64);
    FnEndMatch      GOrigEndMatch = nullptr;
    FnSetMatchState GOrigSetMatchState = nullptr;
    std::string GGuardWhat = "not installed";
    int         GHeldEnds = 0;
    double      GLastHeldLog = 0;
    uintptr_t   GModBase = 0, GModEnd = 0;

    bool SameName(const FName& A, const FName& B) { return A.GetComparisonIndex() == B.GetComparisonIndex() && A.GetNumber() == B.GetNumber(); }

    bool Readable(const void* P, size_t N)
    {
        MEMORY_BASIC_INFORMATION Mbi{};
        if (!P || !VirtualQuery(P, &Mbi, sizeof(Mbi)) || Mbi.State != MEM_COMMIT)
            return false;
        if ((Mbi.Protect & 0xFF) == PAGE_NOACCESS || (Mbi.Protect & PAGE_GUARD))
            return false;
        return reinterpret_cast<const uint8*>(P) + N <= reinterpret_cast<const uint8*>(Mbi.BaseAddress) + Mbi.RegionSize;
    }

    bool InGameExe(const void* P)
    {
        if (!GModBase)
        {
            GModBase = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
            auto* Dos = reinterpret_cast<IMAGE_DOS_HEADER*>(GModBase);
            auto* Nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(GModBase + Dos->e_lfanew);
            GModEnd = GModBase + Nt->OptionalHeader.SizeOfImage;
        }
        return reinterpret_cast<uintptr_t>(P) >= GModBase && reinterpret_cast<uintptr_t>(P) < GModEnd;
    }

    std::string Rva(const void* P)
    {
        if (!InGameExe(P)) return "<not in game exe>";
        char B[32];
        snprintf(B, sizeof(B), "exe+0x%llX", (unsigned long long)(reinterpret_cast<uintptr_t>(P) - GModBase));
        return B;
    }

    std::string Bytes(const void* P, int N)
    {
        if (!Readable(P, N)) return "<unreadable>";
        std::string R;
        char B[4];
        for (int i = 0; i < N; ++i) { snprintf(B, sizeof(B), "%02X ", static_cast<const uint8*>(P)[i]); R += B; }
        return R;
    }

    bool CountZombieSides(UWorld* World, int& Humans, int& Zombies)
    {
        Humans=0; Zombies=0;
        if (!World || !World->AuthorityGameMode || !World->AuthorityGameMode->GameState) return false;
        auto CountState=[&](ATslZombiePlayerState* State) {
            if (!State || State->bQuitter || State->bKilled || State->bOnlySpectator) return;
            const auto Role=State->GetZombieModePlayerState();
            if (Role==EZombieModePlayerState::Human || Role==EZombieModePlayerState::ReadyToRevive) ++Humans;
            else if (Role==EZombieModePlayerState::Zombie) ++Zombies;
        };
        for (auto* PS:World->AuthorityGameMode->GameState->PlayerArray) {
            if (!PS || IsBotPS(PS) || !PS->IsA(ATslZombiePlayerState::StaticClass())) continue;
            auto* Owner=PS->GetOwner();
            if (!Owner || !Owner->IsA(APlayerController::StaticClass())) continue;
            auto* PC=static_cast<APlayerController*>(Owner);
            if (!PC->NetConnection || PC->bIsLocalPlayerController) continue;
            CountState(static_cast<ATslZombiePlayerState*>(PS));
        }
        for (const FBot& Bot:GBots) {
            if (Bot.State==EBotState::Dead || !Bot.Ctrl || !Bot.Ctrl->PlayerState ||
                !Bot.Ctrl->PlayerState->IsA(ATslZombiePlayerState::StaticClass())) continue;
            CountState(static_cast<ATslZombiePlayerState*>(Bot.Ctrl->PlayerState));
        }
        return Humans+Zombies>0;
    }

    void RemoveZombieServerGhostFromClientTeams(UWorld* World, double T)
    {
        static double NextCheck=0;
        if (T<NextCheck || !World || !World->AuthorityGameMode || !World->AuthorityGameMode->IsA(ATslZombieGameMode::StaticClass()) || !World->AuthorityGameMode->GameState) return;
        NextCheck=T+1.0;
        for (auto* PS:World->AuthorityGameMode->GameState->PlayerArray) {
            if (!PS || IsBotPS(PS)) continue;
            auto* Owner=PS->GetOwner();
            if (!Owner || !Owner->IsA(ATslPlayerController::StaticClass())) continue;
            auto* Client=static_cast<ATslPlayerController*>(Owner);
            if (!Client->NetConnection || Client->bIsLocalPlayerController) continue;
            auto* Teams=reinterpret_cast<FRawArray*>(&Client->Teams);
            if (!Teams->Data || Teams->Num<1 || Teams->Num>64 || Teams->Max<Teams->Num || Teams->Max>128 || !Readable(Teams->Data,Teams->Num*sizeof(ATeam*))) continue;
            auto** Members=static_cast<ATeam**>(Teams->Data);
            bool Changed=false;
            for (int i=Teams->Num-1;i>=0;--i) {
                auto* Member=Members[i];
                if (!Member || !Readable(Member,sizeof(ATeam))) continue;
                auto* MemberState=Member->AuthorizedPlayerState ? Member->AuthorizedPlayerState : Member->CachedTslPlayerState;
                if (!MemberState || MemberState==PS || !Readable(MemberState,sizeof(ATslPlayerState))) continue;
                auto* MemberOwner=MemberState->GetOwner();
                if (!MemberOwner || !MemberOwner->IsA(APlayerController::StaticClass())) continue;
                auto* Ghost=static_cast<APlayerController*>(MemberOwner);
                if (!Ghost->bIsLocalPlayerController || Ghost->NetConnection) continue;
                if (i+1<Teams->Num) std::memmove(Members+i,Members+i+1,(Teams->Num-i-1)*sizeof(ATeam*));
                --Teams->Num; Changed=true;
            }
            if (Changed) { Client->ForceNetUpdate(); BLOG("[ZOMBIE TEST] Removed verified server-local ghost from remote player team list"); }
        }
    }

    bool HoldCondition()
    {
        auto* ZombieWorld=UWorld::GetWorld();
        if (ZombieWorld && ZombieWorld->AuthorityGameMode && ZombieWorld->AuthorityGameMode->IsA(ATslZombieGameMode::StaticClass())) {
            auto* Mode=static_cast<ATslZombieGameMode*>(ZombieWorld->AuthorityGameMode);
#ifdef PUBG2019_ENABLE_DIAGNOSTICS
            char Probe[4]{};
            if (Mode->ZombieRuleType==EZombieRuleType::BattleRoyale && GetEnvironmentVariableA("PUBG2019_ZOMBIE_RESPAWN_PROBE",Probe,sizeof(Probe)) && Probe[0]=='1' &&
                GHumansAlive==0 && GBotsAlive>0 && GLastHumanAliveAt>0 && Now()-GLastHumanAliveAt<60.0) return true;
#endif
            if (Mode->ZombieRuleType!=EZombieRuleType::BattleRoyale) {
                int Humans=0, Zombies=0;
                if (CountZombieSides(ZombieWorld,Humans,Zombies)) {
                    static int LastHumans=-1, LastZombies=-1;
                    if (LastHumans!=Humans || LastZombies!=Zombies) {
                        LastHumans=Humans; LastZombies=Zombies;
                        BLOG("[ZOMBIE TEST] alive sides human="+std::to_string(Humans)+" zombie="+std::to_string(Zombies));
                    }
                    if (Mode->GameState && Mode->GameState->IsA(ATslZombieGameState::StaticClass())) {
                        auto* State=static_cast<ATslZombieGameState*>(Mode->GameState);
                        if (State->NumAliveZombiePlayers!=Zombies) { State->NumAliveZombiePlayers=Zombies; State->OnRep_NumAliveZombiePlayers(); State->ForceNetUpdate(); }
                        if (Humans==0 && Zombies>0 && !State->bIsWinnerZombieTeam) { State->bIsWinnerZombieTeam=true; State->ForceNetUpdate(); }
                        if (State->TimeLimitSeconds>0.f && State->IsTimeUp() && Humans>0) { State->bIsWinnerZombieTeam=false; return false; }
                    }
                    if (Mode->ZombieRuleType==EZombieRuleType::Survival && Humans>0) return true;
                    return Humans>0 && Zombies>0;
                }
            }
            return ZombieHumanCountPolicy::HoldBattleRoyale(GHumansAlive,GHumanTeamsAlive,GBotsAlive+GHumanConvertedZombies,
                GPendingSpawns+GPendingNearSpawns,GMatchSpawnDone,Now()-GMatchStartedAt);
        }
#ifdef PUBG2019_TEST_BOTS_ONLY
        // Isolated headless test only: observe the rest of the bot round
        // after the test client exits. Production retains its human rule.
#ifdef PUBG2019_TEST_CAR_FIXTURE
        if (GHumansAlive==0 && GBotsAlive>1) return true;
#else
        if (GHumansAlive==0 && GBotsAlive>1 && GTeamsAlive>1) return true;
#endif
#endif
        const bool bHuman = GHumansAlive > 0 || (GLastHumanAliveAt > 0 && Now() - GLastHumanAliveAt < 5.0);
        const int Teams = GTeamsAlive + ((GHumansAlive == 0 && bHuman) ? 1 : 0);
        return bHuman && GBotsAlive > 0 && Teams > 1;
    }

    void DumpTrace(const std::string& Why)
    {
        static const char* const Spam[] = { "ReceiveTick", "ReadyToEndMatch", "BlueprintUpdateCamera", "UpdateWorldTimeSecondsDelta",
            "OnRep_ReplicatedWorldTimeSeconds", "ReceiveDrawHUD", "BlueprintUpdateAnimation", "EvaluateGraphExposedInputs",
            "ExecuteUbergraph", "ReceiveHit", "ServerUpdateCamera", "OnParameterUpdated", "BlueprintThreadSafeUpdateAnimation" };
        std::vector<std::string> Lines;
        std::string Last;
        int Rep = 0;
        auto Flush = [&]() { if (!Last.empty()) Lines.push_back(Last + (Rep > 1 ? "  x" + std::to_string(Rep) : std::string())); };
        const uint32 N = (std::min)(GTraceHead, (uint32)2048);
        for (uint32 i = GTraceHead - N; i != GTraceHead; ++i)
        {
            const FTraceEnt& E = GTrace[i % 2048];
            if (!E.Func) continue;
            const std::string F = E.Func->GetName();
            bool bSpam = false;
            for (const char* Sp : Spam) if (F.find(Sp) != std::string::npos) { bSpam = true; break; }
            if (bSpam) continue;
            const std::string L = (E.Cls ? E.Cls->GetName() : std::string("?")) + "." + F;
            if (L == Last) { ++Rep; continue; }
            Flush();
            Last = L; Rep = 1;
        }
        Flush();
        BLOG("---- last game calls " + Why + " (newest last) ----");
        const size_t From = Lines.size() > 70 ? Lines.size() - 70 : 0;
        for (size_t i = From; i < Lines.size(); ++i)
            BLOG("   " + Lines[i]);
        BLOG("---- end of call trace ----");
    }

    // true = don't let the match end now
    bool HoldMatchEnd(const char* Why)
    {
        if (!GReady || !S.KeepMatchWhileBotsAlive)
            return false;
        const bool bHold = HoldCondition();
        if (!GTraceDumped)
        {
            GTraceDumped = true;
            DumpTrace(std::string("before ") + Why);
        }
        const double T = Now();
        if (bHold)
        {
            ++GHeldEnds;
            ++GVetoCount;
            GEndPending = true;
            GEndDecidedAt = 0;
            if (T - GLastHeldLog > 10.0)
            {
                GLastHeldLog = T;
                BLOG(std::string("Game tried to end the match (") + Why + ") - HELD: " + std::to_string(GHumansAlive) + " player(s) + " +
                    std::to_string(GBotsAlive) + " bots alive, " + std::to_string(GTeamsAlive) + " teams (held " + std::to_string(GHeldEnds) + "x so far)");
            }
        }
        else
        {
            BLOG(std::string("Match ends (") + Why + ") - players alive " + std::to_string(GHumansAlive) + ", bots alive " +
                std::to_string(GBotsAlive) + ", teams " + std::to_string(GTeamsAlive));
            GEndPending = false;
        }
        return bHold;
    }

    // ---- SetMatchState is found by what it does: it is the game mode virtual that writes
    //      AGameMode::MatchState (offset 0x430) after comparing it. Nothing else is hooked.
    constexpr uint32 kMatchStateOff = 0x478;
    FName       GKnownStates[6] = {};
    const char* GKnownStateNames[6] = { "EnteringMap", "WaitingToStart", "InProgress", "WaitingPostMatch", "LeavingMap", "Aborted" };
    bool        GGuardBad = false;          // detour proved it is NOT SetMatchState -> pass everything through
    int         GSetCalls = 0, GSetCallsLogged = 0;
    double      GSetWindowStart = 0;
    int         GSetWindowCalls = 0;

    // which known match state is this argument (by value, or by pointer)? -1 = none
    int KnownState(uint64 Arg)
    {
        FName V; std::memcpy(&V, &Arg, sizeof(V));
        for (int i = 0; i < 6; ++i) if (SameName(V, GKnownStates[i])) return i;
        if (Arg > 0x10000 && Readable(reinterpret_cast<void*>(Arg), 8))
        {
            const FName& R = *reinterpret_cast<const FName*>(Arg);
            for (int i = 0; i < 6; ++i) if (SameName(R, GKnownStates[i])) return i;
        }
        return -1;
    }

    void DetSetMatchState(void* This, uint64 Arg, uint64 A3, uint64 A4)
    {
        if (!GGuardBad)
        {
            // self check: the real SetMatchState runs a handful of times per match, never in a tight loop
            const double T = Now();
            if (T - GSetWindowStart > 10.0) { GSetWindowStart = T; GSetWindowCalls = 0; }
            if (++GSetWindowCalls > 30)
            {
                GGuardBad = true;
                BLOG("End guard: the hooked function runs constantly - it is NOT SetMatchState. Guard switched OFF (everything passes through).");
            }
        }
        if (!GGuardBad)
        {
            const int K = KnownState(Arg);
            ++GSetCalls;
            if (GSetCallsLogged < 12)
            {
                ++GSetCallsLogged;
                BLOG(std::string("End guard: SetMatchState -> ") + (K >= 0 ? GKnownStateNames[K] : "<unknown arg>"));
            }
            if (K == 3 && S.KeepMatchWhileBotsAlive)
            {
                // only while the match is actually running
                const FName& Cur = *reinterpret_cast<const FName*>(static_cast<uint8*>(This) + kMatchStateOff);
                if (Readable(&Cur, 8) && SameName(Cur, GNameInProgress) && HoldMatchEnd("SetMatchState WaitingPostMatch"))
                    return;
            }
        }
        GOrigSetMatchState(This, Arg, A3, A4);
    }

    // does this function body compare/read and then write a field at [reg+Off]?
    void ScanFieldUse(uint8* Code, int MaxBytes, uint32 Off, bool& bStore, bool& bReadFirst)
    {
        bStore = bReadFirst = false;
        int O = 0, Cc = 0;
        while (O < MaxBytes && Readable(Code + O, 16))
        {
            hde64s H{};
            const unsigned L = hde64_disasm(Code + O, &H);
            if (!L || (H.flags & F_ERROR)) break;
            Cc = (H.opcode == 0xCC) ? Cc + 1 : 0;
            if (Cc >= 2) break;
            // [reg+disp32], no index register, never the stack pointer
            const bool bPlainBase = H.modrm_rm != 4 ||
                ((H.flags & F_SIB) && H.sib_index == 4 && !H.rex_x && (H.sib_base != 4 || H.rex_b));
            if ((H.flags & F_MODRM) && H.modrm_mod == 2 && bPlainBase && H.disp.disp32 == Off)
            {
                if (H.opcode == 0x89) bStore = true;
                else if (!bStore && (H.opcode == 0x8B || H.opcode == 0x39 || H.opcode == 0x3B)) bReadFirst = true;
            }
            if (bStore) break;
            O += L;
        }
    }

#include "NativeMatchEnd.inl"

    // every think: install the guard, finish a held match once it's decided, log game counters
    // ---- empty lobby: keep the countdown at full so an empty server never plays a match on its own
    int  GLobbyFullTime = 0;
    bool GLobbyHeld = false;
    bool GServerTerrainReady = false;
    void LobbyHoldThink(UWorld* World)
    {
        if (IsTrainingWorld(World)) return;
        auto* ZM=World && World->AuthorityGameMode && World->AuthorityGameMode->IsA(ATslZombieGameMode::StaticClass()) ? static_cast<ATslZombieGameMode*>(World->AuthorityGameMode) : nullptr;
        const bool ZombieWaiting=ZM && ZM->ZombieRuleType!=EZombieRuleType::BattleRoyale && ZM->MatchState.ToString()=="WaitingToStart";
        if ((!S.HoldLobbyWhenEmpty && !ZombieWaiting) || GMatchSpawnDone || GMatchStartedAt > 0) return;

        AGameStateBase* GSB = World->AuthorityGameMode->GameState;
        if (!GSB || !GSB->IsA(ATslGameState::StaticClass())) return;
        
        ATslGameState* GS = static_cast<ATslGameState*>(GSB);
        ATslGameMode* GM = (ATslGameMode*)World->AuthorityGameMode;

        if (GS->RemainingTime <= 0) return;                         // plane already going
        if (GS->RemainingTime > GLobbyFullTime) GLobbyFullTime = GS->RemainingTime;
        int Humans = 0;

        for (int i = 0; i < GS->PlayerArray.Num(); ++i)
        {
            APlayerState* PS = GS->PlayerArray[i];
            if (!PS || PS->bIsABot) continue;
            AActor* Own = PS->GetOwner();
            if (!Own || !Own->IsA(APlayerController::StaticClass())) { continue; }
            if (static_cast<APlayerController*>(Own)->bIsLocalPlayerController) { continue; }  // the server's ghost
            ++Humans;
        }
        if (Humans == 0 || !GServerTerrainReady || ZombieWaiting)
        {
            GS->bTimerPaused = true;
            if (GS->RemainingTime != GLobbyFullTime)
            {
                GS->RemainingTime = GLobbyFullTime;
                GM->MatchPreparer->WarmupTime = GLobbyFullTime;
            }
            
            if (!GLobbyHeld) { GLobbyHeld = true; BLOG(ZombieWaiting && Humans>0 ? "[ZOMBIE] Lobby held while players and bots get ready." : "Lobby empty - countdown held at " + std::to_string(GLobbyFullTime) + " s until a player joins"); }
        }
        else
        {
            GS->bTimerPaused = false;
            if (GLobbyHeld)
            {
                GLobbyHeld = false;
                BLOG("A player is in the lobby - countdown running (" + std::to_string(GS->RemainingTime) + " s)");
            }
        }
    }

    void ScheduleExit(const char* Why)
    {
        GMatchOver = true;
        if (S.ExitAfterMatch <= 0.f || GExitAt > 0) return;
        GExitAt = Now() + S.ExitAfterMatch;
        BLOG(std::string("Match over (") + Why + ") - server closes in " + std::to_string((int)S.ExitAfterMatch) + " s");
    }

    // close the server cleanly-ish before the engine's post-match teardown crashes (no crash dialog)
    void ExitThink()
    {
        if (GExitAt <= 0 || Now() < GExitAt) return;
        BLOG("Closing the server process now (ExitAfterMatch) - the watchdog restarts it");
        Sleep(400);                                   // let the log thread write that line
        TerminateProcess(GetCurrentProcess(), 0);
    }

    // The stock BR mode controller can retain a dead internal dependency after
    // a zombie round ends. Stop its actor tick only as the round transitions
    // to post-match; it remains active for the entire match and blue zone.
    void StopZombiePostmatchController(UWorld* World) {
        if (!World || !World->AuthorityGameMode || !World->AuthorityGameMode->IsA(ATslZombieGameMode::StaticClass())) return;
        static thread_local bool Inside=false;
        if (Inside) return;
        Inside=true;
        int Stopped=0, ComponentsStopped=0;
        // The engine actor iterator omits this controller in zombie worlds.
        // This vtable is verified against the exact game executable required by the host.
        auto* Objects=UObject::GObjects.GetTypedPtr();
        const auto VTable=InSDKUtils::GetImageBase()+0x561e958;
        const std::string WorldPrefix=" "+World->GetName()+".";
        if (Objects) for (int I=0;I<Objects->Num();++I) {
            auto* Object=Objects->GetByIndex(I);
            if (!Object || !Readable(Object,sizeof(void*)) || *reinterpret_cast<uintptr_t*>(Object)!=VTable) continue;
            const auto Name=Object->GetFullName();
            if (Name.find(WorldPrefix)==std::string::npos || Name.find(".PersistentLevel.")==std::string::npos) continue;
            auto* Controller=static_cast<AActor*>(Object);
            auto Components=Controller->GetComponentsByClass(UActorComponent::StaticClass());
            for (auto* Component:Components) {
                if (Component && Readable(Component,sizeof(void*)) && Component->IsComponentTickEnabled()) {
                    Component->SetComponentTickEnabled(false);++ComponentsStopped;
                }
            }
            Controller->SetActorTickEnabled(false);++Stopped;
        }
        BLOG("[ZOMBIE] Post-match zone controller stopped: "+std::to_string(Stopped)+"; components="+std::to_string(ComponentsStopped));
        Inside=false;
    }

    void EndGuardThink(UWorld* World, double T)
    {
        RemoveZombieServerGhostFromClientTeams(World,T);
        if (IsTrainingWorld(World)) return;
        // every real player is dead (or gone): end the match - bots alone don't keep it going
        if (S.EndWhenPlayersDead > 0.f && GMatchSpawnDone && !GMatchOver && GMatchStartedAt > 0)
        {
            const bool bAnyHumanEver = GLastHumanAliveAt > 0;
            if (GHumansAlive > 0) GNoHumansSince = 0;
            else if (GNoHumansSince <= 0) GNoHumansSince = T;
            const double Wait = bAnyHumanEver ? S.EndWhenPlayersDead : 60.0;   // nobody ever joined: give it a minute
            if (GHumansAlive == 0 && GNoHumansSince > 0 && T - GNoHumansSince >= Wait && T - GMatchStartedAt > 20.0)
            {
                AGameModeBase* GMB = World ? World->AuthorityGameMode : nullptr;
                if (GMB && GMB->IsA(AGameMode::StaticClass()))
                {
                    AGameMode* GM = static_cast<AGameMode*>(GMB);
                    BLOG(std::string("No real player alive for ") + std::to_string((int)(T - GNoHumansSince)) + " s (" +
                        std::to_string(GBotsAlive) + " bots left) - ending the match");
                    GEndPending = false;
                    if (GM->IsA(ATslGameMode::StaticClass()) && GFnFinishMatch)
                        static_cast<ATslGameMode*>(GM)->FinishMatch();
                    else
                        GM->EndMatch();
                }
                ScheduleExit(bAnyHumanEver ? "all players dead" : "no players joined");
            }
        }

        if (GMatchSpawnDone && (GNatAlive != GNatAliveLogged || GNatTeams != GNatTeamsLogged) && GNatAlive >= 0)
        {
            GNatAliveLogged = GNatAlive;
            GNatTeamsLogged = GNatTeams;
            if (T - GMatchStartedAt < 300.0)
                BLOG("game's own count: alive " + std::to_string(GNatAlive) + ", alive teams " + std::to_string(GNatTeams) +
                    " (bots are added on top of this)");
        }

        if (GMatchSpawnDone && GMatchStartedAt>0 && T-GMatchStartedAt>=25.0 && World && World->AuthorityGameMode && World->AuthorityGameMode->IsA(ATslZombieGameMode::StaticClass())) {
            auto* ZombieMode=static_cast<ATslZombieGameMode*>(World->AuthorityGameMode);
            auto* ZombieState=ZombieMode->GameState && ZombieMode->GameState->IsA(ATslZombieGameState::StaticClass()) ? static_cast<ATslZombieGameState*>(ZombieMode->GameState) : nullptr;
            if (ZombieState && ZombieMode->MatchState.ToString()=="InProgress" && ZombieMode->ZombieRuleType!=EZombieRuleType::BattleRoyale) {
                int Humans=0, Zombies=0;
                if (CountZombieSides(World,Humans,Zombies)) {
                    if (ZombieState->NumAliveZombiePlayers!=Zombies) { ZombieState->NumAliveZombiePlayers=Zombies; ZombieState->OnRep_NumAliveZombiePlayers(); ZombieState->ForceNetUpdate(); }
                    const bool ZombiesWin=(Humans==0 && Zombies>0);
                    const bool HumansWin=(Humans>0 && ((ZombieMode->ZombieRuleType==EZombieRuleType::Elimination && Zombies==0) || (ZombieState->TimeLimitSeconds>0.f && ZombieState->IsTimeUp())));
                    if (ZombiesWin || HumansWin) {
                        if (ZombieState->bIsWinnerZombieTeam!=ZombiesWin) { ZombieState->bIsWinnerZombieTeam=ZombiesWin; ZombieState->ForceNetUpdate(); }
                        if (!GEndPending) { GEndPending=true; BLOG("[ZOMBIE TEST] win condition reached human="+std::to_string(Humans)+" zombie="+std::to_string(Zombies)); }
                    }
                }
            }
        }
        if (!GEndPending)
            return;
        if (HoldCondition()) { GEndDecidedAt = 0; return; }
        if (GEndDecidedAt == 0) { GEndDecidedAt = T; return; }
        if (T - GEndDecidedAt < 4.0)
            return;
        GEndPending = false;
        AGameModeBase* GMB = World ? World->AuthorityGameMode : nullptr;
        if (!GMB || !GMB->IsA(AGameMode::StaticClass()))
            return;
        AGameMode* GM = static_cast<AGameMode*>(GMB);
        if (GM->MatchState.ToString() != "InProgress")
            return;
        BLOG("The match is decided (players alive " + std::to_string(GHumansAlive) + ", bots alive " + std::to_string(GBotsAlive) +
            ") - ending it now");
        ScheduleExit("match decided");
        auto* ZombieResult=GM->IsA(ATslZombieGameMode::StaticClass()) && GM->GameState && GM->GameState->IsA(ATslZombieGameState::StaticClass()) ? static_cast<ATslZombieGameState*>(GM->GameState) : nullptr;
        const bool ExpectedZombieWinner=ZombieResult && ZombieResult->bIsWinnerZombieTeam;
        if (GM->IsA(ATslGameMode::StaticClass()) && GFnFinishMatch)
            static_cast<ATslGameMode*>(GM)->FinishMatch();
        else
            GM->EndMatch();
        if (ZombieResult) {
            if (ZombieResult->bIsWinnerZombieTeam!=ExpectedZombieWinner) {
                ZombieResult->bIsWinnerZombieTeam=ExpectedZombieWinner; ZombieResult->ForceNetUpdate();
            }
            BLOG("[ZOMBIE TEST] finish requested state="+GM->MatchState.ToString()+" zombie_winner="+std::to_string(ZombieResult->bIsWinnerZombieTeam));
            if (GM->MatchState.ToString()=="WaitingPostMatch") {StopZombiePostmatchController(World);BLOG("Match state -> WaitingPostMatch");}
        }
    }

    std::unordered_set<UFunction*> GFireSeen, GFireFuncs;
    struct FShot { FVector Loc; };
    std::vector<FShot> GShots;

    bool IsFireFunc(UFunction* F)
    {
        if (GFireSeen.insert(F).second)
        {
            const std::string N = F->GetName();
            if (N == "ServerStartFire" || N == "ServerHandleFiring" || N == "Server_StartManualFireCycle")
                GFireFuncs.insert(F);
        }
        return GFireFuncs.count(F) != 0;
    }

    // think: every bot within earshot of a player's shot turns toward it
    struct FResponse { FVector Loc; double At; };
    std::vector<FResponse> GResponses;

    void ProcessShots(double T)
    {
        if (GShots.empty())
            return;
        GResponses.erase(std::remove_if(GResponses.begin(), GResponses.end(),
            [T](const FResponse& R) { return T - R.At > 20.0; }), GResponses.end());
        if (GMatchSpawnDone && S.HearingDistance > 0.f)
        {
            for (const FShot& Sh : GShots)
            {
                bool bDone = false;
                for (const FResponse& R : GResponses)
                    if (Len(R.Loc - Sh.Loc) < 5000.f) { bDone = true; break; }   // already sent someone here recently
                if (bDone) continue;
                // shooter already has enough bots on them?
                float HD; FVector HL;
                if (ATslCharacter* H = NearestHuman(Sh.Loc, 1000.f, HD, HL))
                {
                    auto E = GEngaged.find(H->GetIndex());
                    if (S.MaxBotsPerPlayer > 0 && E != GEngaged.end() && E->second >= S.MaxBotsPerPlayer) continue;
                    if (S.MaxBotsPerPlayer == 1 && ClaimedByOther(-1, H->GetIndex())) continue;   // someone's already on them
                }
                std::vector<std::pair<float, FBot*>> Near;
                for (FBot& B : GBots)
                {
                    if (B.State != EBotState::Roam || B.Pawn) continue;
                    const float D = Len(Sh.Loc - B.Pawn->K2_GetActorLocation());
                    if (D <= S.HearingDistance) Near.push_back({ D, &B });
                }
                if (Near.empty()) continue;
                std::sort(Near.begin(), Near.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
                const int N = (std::min)((int)Near.size(), (std::max)(S.MaxResponders, 0));
                for (int k = 0; k < N; ++k)
                {
                    FBot& B = *Near[k].second;
                    B.HeardLoc = Sh.Loc;
                    B.HeardPending = true;
                    B.AlertUntil = T + 6.0;
                }
                GResponses.push_back({ Sh.Loc, T });
            }
        }
        GShots.clear();
    }

    // ---------------------------------------------------------------- clothes
    struct FClothSlot { EEquipSlotID Slot; float Chance; std::vector<UClass*> Classes; };
    std::vector<FClothSlot> GClothes;
    bool GClothesScanned = false;
    int GDressLogs = 0;

    void ScanClothes()
    {
        GClothesScanned = true;
        GClothes = {
            { EEquipSlotID::Torso, 1.0f, {} }, { EEquipSlotID::Legs, 1.0f, {} }, { EEquipSlotID::Feet, 1.0f, {} },
            { EEquipSlotID::Outer, 0.5f, {} }, { EEquipSlotID::Hands, 0.4f, {} }, { EEquipSlotID::Head, 0.3f, {} },
            { EEquipSlotID::Eyes, 0.25f, {} }, { EEquipSlotID::Mask, 0.15f, {} } };
        
        UClass* EqCls = UEquipableItem::StaticClass();
        UClass* WeapCls = UWeaponItem::StaticClass();

        int Total = 0;
        for (int32 i = 0; i < UObject::GObjects->Num(); ++i)
        {
            UObject* O = UObject::GObjects->GetByIndex(i);
            if (!O || !O->IsDefaultObject() || !O->IsA(EqCls) || O->IsA(WeapCls))
                continue;

            UEquipableItem* E = static_cast<UEquipableItem*>(O);
            if (E->DamageReductionRate > 0.f) continue;            // helmets / vests
            const std::string N = O->GetClass()->GetName();
            if (N.rfind("Item_", 0) != 0 || N.find("_Lv") != std::string::npos || GhilliePolicy::IsGhillie(N)) continue;
            for (FClothSlot& Sl : GClothes)
                if (Sl.Slot == E->EquipSlotID) { Sl.Classes.push_back(O->GetClass()); ++Total; break; }
        }
        BLOG("Clothes: found " + std::to_string(Total) + " clothing items for bots / players");
    }

    namespace SkinSelection { bool HasAppearance(ATslCharacter* C); bool ReservesSlot(ATslCharacter* C, EEquipSlotID Slot); }

    void DressCharacter(ATslCharacter* C, const std::string& Who)
    {
        AInventoryFacade* Inv = C->GetInventoryFacade();

        if (!Inv || !Inv->GetEquipment())
            return;

        AEquipment* Eq = Inv->GetEquipment();
        const std::string Level=GWorld ? UGameplayStatics::GetCurrentLevelName(GWorld,true).ToString() : std::string();
        std::string Put;
        for (const FClothSlot& Sl : GClothes)
        {
            // Leave slots chosen in the saved outfit free for the selected items.
            if (!IsBotChar(C) && SkinSelection::ReservesSlot(C, Sl.Slot)) continue;
            if (Sl.Classes.empty() || RandF(0.f, 1.f) > Sl.Chance) continue;
            FEquipPosition P{}; P.SlotID = Sl.Slot; P.Index = 0;
            if (Eq->GetItem(P)) continue;                           // already wearing something there
            std::vector<UClass*> Allowed;
            for (UClass* Candidate : Sl.Classes)
                if (Candidate && !GhilliePolicy::IsGhillie(Candidate->GetName()) && !GhilliePolicy::Forbidden(Level,Candidate->GetName())) Allowed.push_back(Candidate);
            if (Allowed.empty()) continue;
            UClass* Cls = Allowed[RandI(0, (int)Allowed.size() - 1)];
            UItem* Item = static_cast<UItem*>(UGameplayStatics::SpawnObject(Cls, Eq));
            if (Item && Eq->TryEquipItem(Item, false, true))
                Put += (Put.empty() ? "" : ", ") + Cls->GetName();
        }
        if (GDressLogs < 6 && !Put.empty())
        {
            ++GDressLogs;
            BLOG("Dressed " + Who + ": " + Put);
        }
    }

    // Real players' characters carry a Mutable "InstanceDescriptor" (face, hair, body setup) that each client
    // builds the merged character mesh from. Bots have none -> some clients render them as red ghosts / headless.
    // Copy a real player's descriptor (same gender) into every bot.
    bool HasZombieBotRole(const FBot& Bot)
    {
        return GWorld && GWorld->AuthorityGameMode && GWorld->AuthorityGameMode->IsA(ATslZombieGameMode::StaticClass()) &&
            Bot.Ctrl && Bot.Ctrl->PlayerState && Bot.Ctrl->PlayerState->IsA(ATslZombiePlayerState::StaticClass()) &&
            static_cast<ATslZombiePlayerState*>(Bot.Ctrl->PlayerState)->GetZombieModePlayerState()==EZombieModePlayerState::Zombie;
    }

    void AppearancePass()
    {
        if (!S.CopyAppearance)
            return;
        for (const FCharInfo& CI : GChars)
        {
            if (CI.IsBot || GDonorChars.count(CI.Index)) continue;
            AMutableCharacter* M = CI.Char;
            FRawArray R; std::memcpy(&R, &M->InstanceDescriptor, sizeof(R));
            if (!R.Data || R.Num <= 0 || R.Num > 262144) continue;
            const int G = (int)(uint8)M->Gender & 1;
            if (GDonorLooks[G].size() >= 8) { GDonorChars.insert(CI.Index); continue; }
            GDonorLooks[G].emplace_back(static_cast<uint8*>(R.Data), static_cast<uint8*>(R.Data) + R.Num);
            GDonorChars.insert(CI.Index);
            BLOG("Appearance: copied a " + std::string(G ? "female" : "male") + " look from a player (" + std::to_string(R.Num) +
                " bytes) - bots will use it");
        }
        int Done = 0;
        bool bMissing = false;
        for (FBot& B : GBots)
        {
            if (Done >= 4) break;
            if (B.State == EBotState::Dead || !B.Pawn || GAppearanced.count(B.PawnIndex) || HasZombieBotRole(B)) continue;
            AMutableCharacter* M = B.Pawn;
            FRawArray R; std::memcpy(&R, &M->InstanceDescriptor, sizeof(R));
            if (R.Data && R.Num > 0) { GAppearanced.insert(B.PawnIndex); continue; } // already has one
            int G = (int)(uint8)M->Gender & 1;
            if (GDonorLooks[G].empty())
            {
                // bot pawns default to the other gender - switch the bot to the gender we have a look for
                if (GDonorLooks[G ^ 1].empty()) { bMissing = true; continue; }
                static bool bLogged = false;
                if (!bLogged)
                {
                    bLogged = true;
                    BLOG(std::string("Appearance: bots are ") + (G ? "female" : "male") + ", players' looks are " + (G ? "male" : "female") +
                        " - switching bots to match");
                }
                G ^= 1;
                M->Gender = (EGender)G;
            }
            const std::vector<uint8>& Look = GDonorLooks[G][RandI(0, (int)GDonorLooks[G].size() - 1)];
            void* Mem = GameAlloc((int32)Look.size());
            if (!Mem) continue;
            std::memcpy(Mem, Look.data(), Look.size());
            FRawArray NewArr{ Mem, (int32)Look.size(), (int32)Look.size() };
            std::memcpy(&M->InstanceDescriptor, &NewArr, sizeof(NewArr));
            M->ForceNetUpdate();
            GAppearanced.insert(B.PawnIndex);
            ++Done;
        }
        if (bMissing && GMatchSpawnDone == false && Now() >= GLookWarnAt)
        {
            GLookWarnAt = Now() + 60.0;
            int Humans = 0, WithLook = 0;
            for (const FCharInfo& CI : GChars)
            {
                if (CI.IsBot) continue;
                ++Humans;
                FRawArray R; std::memcpy(&R, &CI.Char->InstanceDescriptor, sizeof(R));
                if (R.Data && R.Num > 0) ++WithLook;
            }
            BLOG("Appearance: no player look of the bots' gender to copy yet (" + std::to_string(WithLook) + "/" + std::to_string(Humans) +
                " players have one)");
        }
    }

    // ---------------------------------------------------------------- body: face / hair / skin
    // The appearance record (InstanceDescriptor) is a list of named values ("Backpack" = -1, ...). Clothing
    // values come from the items a character wears; the rest (face, hair, skin ...) a real player gets from
    // their account - bots never get them, so clients built clothes with no body inside. Copy those
    // non-clothing values from a connected player of the same gender.
    struct FDescTok { std::string Key; std::string Name; int Off; int Len; };
    std::unordered_set<std::string> GClothNames;     // lowercase clothing group names (from item definitions)
    std::unordered_set<std::string> GClothLearned;   // lowercase names that changed when a bot was dressed
    bool   GClothNamesBuilt = false;
    int    GBodyLogs = 0;
    double GNextBodyPass = 0;
    double GBodyWarnAt[2] = { 0, 0 };

    std::vector<FDescTok> ParseDesc(const uint8* D, int N)
    {
        std::vector<FDescTok> Out;
        std::unordered_map<std::string, int> Seen;
        int i = 0;
        while (i + 4 <= N)
        {
            int32 L; std::memcpy(&L, D + i, 4);
            bool bTok = false;
            if (L >= 2 && L <= 64 && i + 4 + L <= N && D[i + 4 + L - 1] == 0)
            {
                const uint8 c0 = D[i + 4];
                bTok = (c0 >= 'A' && c0 <= 'Z') || (c0 >= 'a' && c0 <= 'z') || c0 == '_';
                for (int k = 0; bTok && k < L - 1; ++k) { const uint8 c = D[i + 4 + k]; if (c < 0x20 || c > 0x7E) bTok = false; }
            }
            if (!bTok) { ++i; continue; }
            if (!Out.empty()) Out.back().Len = i - Out.back().Off;
            std::string Name(reinterpret_cast<const char*>(D + i + 4), (size_t)(L - 1));
            const int Occ = Seen[Name]++;
            i += 4 + L;
            Out.push_back({ Name + "#" + std::to_string(Occ), Name, i, 0 });
        }
        if (!Out.empty()) Out.back().Len = N - Out.back().Off;
        return Out;
    }

    std::string DescVal(const uint8* P, int Len)
    {
        char Buf[64];
        if (Len == 4) { int32 V; std::memcpy(&V, P, 4); snprintf(Buf, sizeof(Buf), "%d", V); return Buf; }
        if (Len == 8) { int32 A, B; std::memcpy(&A, P, 4); std::memcpy(&B, P + 4, 4); snprintf(Buf, sizeof(Buf), "%d,%d", A, B); return Buf; }
        std::string H = "0x";
        for (int k = 0; k < Len && k < 20; ++k) { snprintf(Buf, sizeof(Buf), "%02X", P[k]); H += Buf; }
        if (Len > 20) H += "..";
        return H;
    }

    bool GetLook(ATslCharacter* C, uint8*& D, int& N)
    {
        FRawArray R; std::memcpy(&R, &C->InstanceDescriptor, sizeof(R));
        if (!R.Data || R.Num <= 0 || R.Num > 65536) return false;
        D = static_cast<uint8*>(R.Data); N = R.Num;
        return true;
    }

    void BuildClothNames()
    {
        GClothNamesBuilt = true;
        TUObjectArray* Arr = UObject::GObjects.GetTypedPtr();
        if (!Arr) return;
        UClass* EqCls = UEquipableItem::StaticClass();
        GClothNames.insert("mask");
        for (int32 i = 0; i < Arr->Num(); ++i)
        {
            UObject* O = Arr->GetByIndex(i);
            if (!O || !O->IsDefaultObject() || !O->IsA(EqCls)) continue;
            UEquipableItem* E = static_cast<UEquipableItem*>(O);
            const std::string A = E->CustomizableObjectIdentifierForMale.CustomizableObjectGroupName.ToString();
            const std::string B = E->CustomizableObjectIdentifierForFemale.CustomizableObjectGroupName.ToString();
            if (!A.empty()) GClothNames.insert(Lower(A));
            if (!B.empty()) GClothNames.insert(Lower(B));
        }
        std::string L;
        for (const std::string& N : GClothNames) L += (L.empty() ? "" : ", ") + N;
        BLOG("Body: clothing slots in the appearance record: " + (L.empty() ? std::string("<none found>") : L));
    }

    // which names count as clothing for this record: item group names if they match it, else what dressing changed
    const std::unordered_set<std::string>& ClothSetFor(const std::vector<FDescTok>& Toks)
    {
        for (const FDescTok& t : Toks)
            if (GClothNames.count(Lower(t.Name))) return GClothNames;
        return GClothLearned;
    }

    ATslCharacter* FindLookTemplate(int G)
    {
        ATslCharacter* Fallback = nullptr;

        for (const FCharInfo& CI : GChars)
        {
            if (CI.IsBot || !CI.Char) continue;
            ATslCharacter* C = CI.Char;
            if (((uint8)C->Gender ? 1 : 0) != G) continue;
            if (!C->CustomizableObjectInstance) continue;
            uint8* D; int N;
            if (!GetLook(C, D, N)) continue;
            if (!IsGhostChar(C)) return C;
            if (!Fallback) Fallback = C;
        }
        return Fallback;
    }

    int FixBody(ATslCharacter* Bot, ATslCharacter* Tpl, std::string* OutNames)
    {
        uint8* TD, * BD; int TN, BN;
        if (!GetLook(Tpl, TD, TN) || !GetLook(Bot, BD, BN)) return 0;
        const std::vector<FDescTok> TT = ParseDesc(TD, TN), BT = ParseDesc(BD, BN);
        const std::unordered_set<std::string>& Cloth = ClothSetFor(BT);
        std::unordered_map<std::string, const FDescTok*> TM;
        for (const FDescTok& t : TT) TM[t.Key] = &t;
        int Changed = 0;
        for (const FDescTok& b : BT)
        {
            if (Cloth.count(Lower(b.Name))) continue;
            auto It = TM.find(b.Key);
            if (It == TM.end()) continue;
            const FDescTok& t = *It->second;
            if (t.Len != b.Len || b.Len <= 0 || b.Len > 64) continue;
            if (std::memcmp(BD + b.Off, TD + t.Off, b.Len) == 0) continue;
            if (OutNames) *OutNames += (OutNames->empty() ? "" : ", ") + b.Name + " " + DescVal(BD + b.Off, b.Len) + "->" + DescVal(TD + t.Off, t.Len);
            std::memcpy(BD + b.Off, TD + t.Off, b.Len);
            ++Changed;
        }
        return Changed;
    }

    void BodyPass(double T)
    {
        if (!S.BotBodyFix || T < GNextBodyPass) return;
        GNextBodyPass = T + 2.0;
        if (!GClothNamesBuilt) BuildClothNames();
        ATslCharacter* Tpl[2] = { FindLookTemplate(0), FindLookTemplate(1) };
        for (FBot& B : GBots)
        {
            if (B.State == EBotState::Dead || !B.Pawn || HasZombieBotRole(B)) continue;
            ATslCharacter* P = B.Pawn;
            if (!P->CustomizableObjectInstance) continue;                     // not a player-body class
            if (S.DressBots && !GDressed.count(B.PawnIndex)) continue;         // wait until it's dressed
            const int G = (uint8)P->Gender ? 1 : 0;
            if (!Tpl[G])
            {
                if (T >= GBodyWarnAt[G])
                {
                    GBodyWarnAt[G] = T + 60.0;
                    BLOG(std::string("Body: no connected ") + (G ? "female" : "male") + " player to copy a face / hair from yet");
                }
                continue;
            }
            std::string Names;
            const int Ch = FixBody(P, Tpl[G], GBodyLogs < 4 ? &Names : nullptr);
            if (Ch <= 0) continue;
            P->ForceNetUpdate();
            if (S.BodyServerRebuild) P->OnRep_InstanceDescriptor();
            if (GBodyLogs < 4)
            {
                ++GBodyLogs;
                BLOG("Body: " + B.Name + " (" + (G ? "female" : "male") + ") took " + std::to_string(Ch) + " value(s) from " +
                    CharName(Tpl[G]) + ": " + Names);
            }
        }
    }

    void DressPass()
    {
        if (!S.DressBots && !S.DressPlayers) return;
        if (!GClothesScanned) ScanClothes();
        int Done = 0;
        for (const FCharInfo& CI : GChars)
        {
            if (Done >= 3) break;
            if (GDressed.count(CI.Index)) continue;
            // Random clothes are for bots only. A real player's appearance can
            // arrive after their pawn, including after the match has started;
            // filling empty slots here races the saved selection and can leave
            // unwanted glasses or prevent an item swap when inventory is full.
            if (!CI.IsBot) { GDressed.insert(CI.Index); continue; }
            if (CI.IsBot ? !S.DressBots : !S.DressPlayers) { GDressed.insert(CI.Index); continue; }
            if (CI.Char->IsInAircraft()) continue;
            if (!CI.Char->GetInventoryFacade()) continue;               // not set up yet - try next think
            if (!CI.IsBot && !GMatchSpawnDone && !SkinSelection::HasAppearance(CI.Char)) {
                // The room can deliver the outfit several seconds after the pawn.
                // Wait briefly before filling unselected slots with random clothes.
                const double T=Now(); auto It=GPlayerDressFirstSeen.find(CI.Index);
                if (It==GPlayerDressFirstSeen.end()) It=GPlayerDressFirstSeen.emplace(CI.Index,T).first;
                if (T-It->second<15.0) continue;
            }
            GPlayerDressFirstSeen.erase(CI.Index);
            if (CI.IsBot && GWorld && GWorld->AuthorityGameMode && GWorld->AuthorityGameMode->IsA(ATslZombieGameMode::StaticClass())) {
                auto* PS=CI.Char->GetTslPlayerState();
                if (PS && PS->IsA(ATslZombiePlayerState::StaticClass()) && static_cast<ATslZombiePlayerState*>(PS)->GetZombieModePlayerState()==EZombieModePlayerState::Zombie) {
                    if (GZombieDressBudget<=0) continue;
                    --GZombieDressBudget;
                }
            }
            GDressed.insert(CI.Index);
            std::string Who = CI.IsBot ? "a bot" : "a player";
            for (const FBot& B : GBots) if (B.Pawn == CI.Char) { Who = B.Name; break; }
            std::vector<uint8> LookBefore;
            if (CI.IsBot) { uint8* D; int N; if (GetLook(CI.Char, D, N)) LookBefore.assign(D, D + N); }
            DressCharacter(CI.Char, Who);
            if (CI.IsBot && S.BotUpdateAppearance)
            {
                // Players get their appearance record (InstanceDescriptor: gender, head, hair, clothes) rebuilt by the
                // game; bots never did, so clients built them headless / naked / red. Ask the game to do it for the bot.
                FRawArray Before; std::memcpy(&Before, &CI.Char->InstanceDescriptor, sizeof(Before));
                CI.Char->UpdateAppearence();
                FRawArray After; std::memcpy(&After, &CI.Char->InstanceDescriptor, sizeof(After));
                CI.Char->ForceNetUpdate();
                if (!LookBefore.empty() && After.Data && After.Num == (int32)LookBefore.size())
                {
                    const uint8* AD = static_cast<const uint8*>(After.Data);
                    for (const FDescTok& t : ParseDesc(AD, After.Num))
                        if (std::memcmp(AD + t.Off, LookBefore.data() + t.Off, t.Len) != 0)
                            GClothLearned.insert(Lower(t.Name));
                }
                static int Logs = 0;
                if (Logs < 4)
                {
                    ++Logs;
                    int PlayerBytes = -1;
                    for (const FCharInfo& H : GChars)
                    {
                        if (H.IsBot) continue;
                        FRawArray R; std::memcpy(&R, &H.Char->InstanceDescriptor, sizeof(R));
                        PlayerBytes = R.Num; break;
                    }
                    BLOG("Appearance: " + Who + " (" + std::string((uint8)CI.Char->Gender ? "female" : "male") + ") record " +
                        std::to_string(Before.Num) + " -> " + std::to_string(After.Num) + " bytes after UpdateAppearence (a player's is " +
                        std::to_string(PlayerBytes) + ")");
                }
            }
            ++Done;
        }
    }

    #include "SkinSelection.inl"

    // keep HuntersPerPlayer x living players hunting; replace hunters that die
    double GNextHunterAssign = 0;
    double GHunterRefillAt = 0;
    int    GHuntersLast = 0;
    bool   GFirstWaveDone = false;
    double GHuntMatch = -1;
    void AssignHunters(double T)
    {
        if (GHuntMatch != GMatchStartedAt) // new match: fresh hunter state
        {
            GHuntMatch = GMatchStartedAt;
            GHunterRefillAt = 0; GHuntersLast = 0; GFirstWaveDone = false;
        }
        if (S.HumanlikeGoals || !GMatchSpawnDone || S.HuntersPerPlayer <= 0 || T < GNextHunterAssign || T - GMatchStartedAt < 10.0)
            return;
        GNextHunterAssign = T + 2.0;
        const int Humans = (std::max)(GHumansAlive, 0);
        if (Humans == 0)
            return;
        const int Want = S.HuntersPerPlayer * Humans;
        int Have = 0;
        for (const FBot& B : GBots)
            if (B.Hunter && B.State != EBotState::Dead && !B.Remove && B.Pawn) ++Have;
        // a hunter died: wait before sending the next one
        if (GFirstWaveDone && Have < GHuntersLast && S.HunterRefillDelay > 0.f)
        {
            GHunterRefillAt = T + S.HunterRefillDelay;
            BLOG("A hunter died - next one in " + std::to_string((int)S.HunterRefillDelay) + " s");
        }
        GHuntersLast = Have;
        if (Have < Want && GFirstWaveDone && T < GHunterRefillAt)
            return;
        Have = 0;
        std::vector<FBot*> Cand;
        for (FBot& B : GBots)
        {
            if (B.State == EBotState::Dead || B.Remove || !B.Pawn) continue;
            if (B.Hunter) { ++Have; continue; }
            if (B.Pawn->IsInAircraft() || B.DropPending || B.Pawn->IsFallingWithParachute() || T < B.NoFireUntil) continue; // not landed yet
            Cand.push_back(&B);
        }
        for (int i = (int)Cand.size() - 1; i > 0; --i) std::swap(Cand[i], Cand[RandI(0, i)]);
        {
            // no teleporting: prefer bots already 300 m .. HunterPickRange from a player, then the nearest others
            std::vector<std::pair<float, FBot*>> Ranked;
            for (FBot* B : Cand)
            {
                float HD = 1e9f; FVector HL;
                NearestHuman(B->Pawn->K2_GetActorLocation(), 1e9f, HD, HL);
                const bool bGood = HD >= 30000.f && HD <= S.HunterPickRange;
                Ranked.push_back({ bGood ? RandF(0.f, 1.f) : 10.f + HD / 100000.f, B });
            }
            std::stable_sort(Ranked.begin(), Ranked.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
            for (size_t i = 0; i < Ranked.size(); ++i) Cand[i] = Ranked[i].second;
        }
        for (FBot* B : Cand)
        {
            if (Have >= Want) break;
            B->Hunter = true;
            B->CanCatchUp = !GFirstWaveDone || S.HunterCatchUpReplacements;
            B->FlankAngle = RandF(-75.f, 75.f);
            B->NextHuntReplan = T;
            ++Have;
            BLOG(B->Name + " is now hunting players (" + std::to_string(Have) + "/" + std::to_string(Want) + " hunters" +
                (GFirstWaveDone ? ", replacement - walks in" : "") + ")");
        }
        GHuntersLast = Have;
        // first wave = the hunters picked in the first 2 minutes after landing
        if (!GFirstWaveDone && (Have >= Want || T - GMatchStartedAt > 120.0))
            GFirstWaveDone = true;
    }

    void TryNativeSpawnBot(UWorld* World)
    {
        APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
        if (!PC)
        {
            BLOG("nativebot: no local PlayerController");
            return;
        }
        if (!PC->CheatManager)
        {
            UClass* CMClass = PC->CheatClass ? (UClass*)PC->CheatClass : UTslCheatManager::StaticClass();
            UObject* CM = UGameplayStatics::SpawnObject(CMClass, PC);
            PC->CheatManager = static_cast<UCheatManager*>(CM);
            BLOG(std::string("nativebot: created cheat manager ") + (CM ? CM->GetClass()->GetName() : "<failed>"));
        }
        if (PC->CheatManager && PC->CheatManager->IsA(UTslCheatManager::StaticClass()))
        {
            BLOG("nativebot: calling UTslCheatManager::SpawnBot() - watch for a new pawn (or a crash)...");
            static_cast<UTslCheatManager*>(PC->CheatManager)->SpawnBot();
            BLOG("nativebot: returned");
        }
        else
        {
            BLOG("nativebot: cheat manager is not a UTslCheatManager");
        }
    }

    void ListWeapons()
    {
        if (GWeapons.empty())
            ScanWeapons();
        for (const FWeaponChoice& W : GWeapons)
            BLOG("  " + W.WeaponName + "   (item " + W.ItemName + ")");
    }

    bool IsOurWindowFocused()
    {
        HWND F = GetForegroundWindow();
        if (!F)
            return false;
        if (F == GetConsoleWindow())
            return true;
        DWORD Pid = 0;
        GetWindowThreadProcessId(F, &Pid);
        return Pid == GetCurrentProcessId();
    }
} // namespace

// =====================================================================================
//  Public API
// =====================================================================================
namespace Bots
{
    SDK::UClass* SelectedPlayerPawn(SDK::AController* Controller){
        if(!Controller||!Controller->IsA(APlayerController::StaticClass())||!Controller->PlayerState)return nullptr;
        auto* PC=static_cast<APlayerController*>(Controller);if(!PC->NetConnection||PC->bIsLocalPlayerController)return nullptr;
        SkinSelection::ReadSelections();auto Name=Controller->PlayerState->PlayerName.ToString();BLOG("CHARACTER pawn request name="+Name);auto Gender=SkinSelection::Genders.find(Lower(Name));
        if(Gender==SkinSelection::Genders.end())return nullptr;
        auto* Class=UObject::FindClassFast(Gender->second=="female"?"PlayerFemale_A_C":"PlayerMale_A_C");
        if(Class)BLOG("CHARACTER pawn player="+Name+" gender="+Gender->second+" class="+Class->GetName());return Class;
    }
    void AfterCall() { ApplyCounters(); }
    bool IsGameThread() { return GGameThreadId != 0 && GetCurrentThreadId() == GGameThreadId; }
    void SetTerrainReady(bool Ready) { GServerTerrainReady = Ready; }
    void Init(HMODULE Module)
    {
        if (GReady)
            return;

        LoadIni(Module);
        LoadSettings();
        // Thread identity is also needed by the server's non-bot hooks.
        GGameThreadId = FindMainThreadId();
        if (!S.Enabled)
        {
            BLOG("Disabled in settings.ini ([Bots] Enabled=0)");
            return;
        }
        BLOG("Game thread id: " + std::to_string(GGameThreadId));

        GTickRateStart = Now();
        GReady = true;
    }


    bool IsEndMatchQuery(UFunction* Func)
    {
        for (int i = 0; i < GNumEndMatchFuncs; ++i)
            if (GEndMatchFuncs[i] == Func) return true;
        return false;
    }

    bool IsBotPawn(APawn* Pawn)
    {
        if (!Pawn)
            return false;
        for (const FBot& B : GBots)
            if (B.Pawn == Pawn) return true;
        return false;
    }

    // true = skip this call (it would end the match while players and bots are still alive)
// The match end is a Blueprint behaviour-tree task (Task_FinishMatch_C, run by
// BattleRoyaleModeController). Its ReceiveExecute event goes through ProcessEvent.
    std::unordered_set<UFunction*> GSeenFuncs;
    std::unordered_set<UFunction*> GRecvExecFuncs;     // every ReceiveExecute / ReceiveExecuteAI
    std::unordered_set<UClass*>    GSeenTaskCls;
    std::unordered_set<UClass*>    GFinishTaskCls;      // task classes named *FinishMatch*

    bool IsTaskFinishMatch(UObject* Obj, UFunction* Func)
    {
        if (GSeenFuncs.insert(Func).second)
        {
            const std::string N = Func->GetName();
            if (N == "ReceiveExecute" || N == "ReceiveExecuteAI")
                GRecvExecFuncs.insert(Func);
        }
        if (!Obj || !GRecvExecFuncs.count(Func))
            return false;
        UClass* C = Obj->GetClass();
        if (GSeenTaskCls.insert(C).second)
        {
            const std::string CN = C ? C->GetName() : std::string();
            if (CN.find("FinishMatch") != std::string::npos)
            {
                GFinishTaskCls.insert(C);
                BLOG("Found the game's match-end task: " + CN + "." + Func->GetName());
            }
        }
        return GFinishTaskCls.count(C) != 0;
    }

    void OnMatchStarted()
    {
        if (IsTrainingWorld(UWorld::GetWorld())) return;
        if (!GReady || GMatchSpawnDone)
            return;
        GMatchSpawnDone = true;
        const int Want = GCarryOver + ((S.SpawnOnMatchStart && !GLobbySpawned) ? (std::max)(0, S.Count) : 0);
        GCarryOver = 0;
        GSpawnNotBefore = Now() + 5.0; // let the plane take the players first
        GMatchStartedAt = Now();
        if (S.BotPeaceTime > 0.f)
            BLOG("Bots won't fight each other for the first " + std::to_string((int)(S.BotPeaceTime / 60.f)) + " min");
        if (Want > 0) GPendingSpawns += Want;
        SyncGameCounters(GWorld);
        ApplyCounters();
        DumpTeams(GWorld, "match start");
        if (Want > 0)
        {
            BLOG("Match started - " + std::to_string(Want) + " bots will spawn on land in 5 s");
        }
    }

    void SetAutoDetectMatchStart(bool bEnable)
    {
        GAutoMatchStart = bEnable;
    }

    // Runs on the injector thread BEFORE the ProcessEvent hook exists, so nothing else in this
    // DLL is touching bot state yet. Every class lookup-by-name that the game-thread code needs
    // is done here once and cached (StaticClass() caches in a static), plus the world setup.
    void Prewarm()
    {
        if (!GReady)
            return;

        BLOG("Prewarm: resolving classes on the loader thread...");
        struct FEntry { const char* Name; UClass* Cls; };
        const FEntry List[] = {
            { "TslCharacter",       ATslCharacter::StaticClass() },
            { "TslPlayerState",     ATslPlayerState::StaticClass() },
            { "TslGameState",       ATslGameState::StaticClass() },
            { "TslGameMode",        ATslGameMode::StaticClass() },
            { "AIController",       AAIController::StaticClass() },
            { "TslAIController",    ATslAIController::StaticClass() },
            { "TslBotAIController", ATslBotAIController::StaticClass() },
            { "RecastNavMesh",      ARecastNavMesh::StaticClass() },
            { "WeaponItem",         UWeaponItem::StaticClass() },
            { "TslWeapon_Gun",      ATslWeapon_Gun::StaticClass() },
            { "TslCheatManager",    UTslCheatManager::StaticClass() },
            { "GameplayStatics",    UGameplayStatics::StaticClass() },
            { "KismetSystemLibrary",UKismetSystemLibrary::StaticClass() },
            { "KismetMathLibrary",  UKismetMathLibrary::StaticClass() },
            { "KismetStringLibrary",UKismetStringLibrary::StaticClass() },
            { "NavigationSystem",   UNavigationSystem::StaticClass() },
            { "Actor",              AActor::StaticClass() },
            { "Pawn",               APawn::StaticClass() },
            { "Character",          ACharacter::StaticClass() },
            { "Controller",         AController::StaticClass() },
            { "PlayerController",   APlayerController::StaticClass() },
            { "GameModeBase",       AGameModeBase::StaticClass() },
            { "PlayerState",        APlayerState::StaticClass() },
            { "TransportAircraftVehicle", ATransportAircraftVehicle::StaticClass() },
            { "Team",               ATeam::StaticClass() },
            { "BTTask_BlueprintBase", UBTTask_BlueprintBase::StaticClass() },
        };
        const int Total = (int)(sizeof(List) / sizeof(List[0]));
        int Missing = 0;
        for (const FEntry& E : List)
            if (!E.Cls) { ++Missing; BLOG(std::string("  class NOT FOUND: ") + E.Name); }
        BLOG("  " + std::to_string(Total - Missing) + "/" + std::to_string(Total) + " classes resolved");

        GNumEndMatchFuncs = 0;
        UClass* FuncCls = UFunction::StaticClass();
        for (int32 i = 0; i < UObject::GObjects->Num() && GNumEndMatchFuncs < 8; ++i)
        {
            UObject* O = UObject::GObjects->GetByIndex(i);
            if (!O || !O->IsA(FuncCls)) continue;
            const std::string FN = O->GetName();

            if (FN == "ReadyToEndMatch")
                GEndMatchFuncs[GNumEndMatchFuncs++] = static_cast<UFunction*>(O);
            else if ((FN == "EndMatch" || FN == "FinishMatch") && GNumFinishFuncs < 8)
            {
                GFinishFuncs[GNumFinishFuncs++] = static_cast<UFunction*>(O);
                if (FN == "EndMatch") GFnEndMatch = static_cast<UFunction*>(O);
                else GFnFinishMatch = static_cast<UFunction*>(O);
            }
            else if ((FN == "ClientSendRoundEndEvent" || FN == "ClientSetTslPlayerMatchResultInfos" || FN == "ClientShowMatchResult" ||
                FN == "ClientGameEnded" || FN == "ClientEndOnlineGame") && GNumResultFuncs < 12)
            {
                GResultFuncs[GNumResultFuncs++] = static_cast<UFunction*>(O);
                if (FN == "ClientGameEnded") GFnGameEnded = static_cast<UFunction*>(O);
                else if (FN == "ClientSendRoundEndEvent") GFnRoundEnd = static_cast<UFunction*>(O);
            }
            else if (FN == "BroadcastDeath" && !GFnBroadcastDeath)
                GFnBroadcastDeath = static_cast<UFunction*>(O);
            else if (FN == "K2_OnSetMatchState" && GNumOnSetState < 4)
                GFnOnSetState[GNumOnSetState++] = static_cast<UFunction*>(O);
        }
        BLOG("  EndMatch/FinishMatch functions found: " + std::to_string(GNumFinishFuncs));
        BLOG("  ReadyToEndMatch functions found: " + std::to_string(GNumEndMatchFuncs) +
            (GNumEndMatchFuncs ? " (match end can be held while bots live)" : " - KeepMatchWhileBotsAlive will not work"));

        UWorld* World = UWorld::GetWorld();
        OnWorldChanged(World);   // navmesh check + damage type, sets GWorld so Tick won't redo it
        GetPawnClass(World);
        ScanWeapons();
        InstallNativeMatchEndGuard();
        BLOG("Prewarm done.");
    }

    // Called right after the game answered ReadyToEndMatch (game thread). Parms = { bool ReturnValue }.
    void FilterEndMatch(void* Parms)
    {
        if (!Parms || !GReady || !S.KeepMatchWhileBotsAlive || !GMatchSpawnDone)
            return;
        bool& Ret = *static_cast<bool*>(Parms);
        if (!Ret)
            return;
        if (!(GHumansAlive > 0 && GBotsAlive > 0 && GTeamsAlive > 1))
        {
            const double T0 = Now();
            if (T0 - GLastNoHoldLog > 5.0)
            {
                GLastNoHoldLog = T0;
                BLOG("Game wants to end the match - NOT held (humans " + std::to_string(GHumansAlive) + ", bots " +
                    std::to_string(GBotsAlive) + ", teams " + std::to_string(GTeamsAlive) + ")");
            }
        }
        if (GHumansAlive > 0 && GBotsAlive > 0 && GTeamsAlive > 1)
        {
            Ret = false;
            ++GVetoCount;
            const double T = Now();
            if (T - GLastVetoLog > 15.0)
            {
                GLastVetoLog = T;
                BLOG("Game wanted to end the match - held: " + std::to_string(GHumansAlive) + " players + " +
                    std::to_string(GBotsAlive) + " bots alive (" + std::to_string(GTeamsAlive) + " teams)");
            }
        }
    }

    bool ShouldBlockCall(UObject* Obj, UFunction* Func)
    {
        if (!Func || !GReady || GetCurrentThreadId() != GGameThreadId)
            return false;
        if (IsTaskFinishMatch(Obj, Func))
        {
            if (Obj && Obj->IsA(UBTTask_BlueprintBase::StaticClass()) && HoldMatchEnd("Task_FinishMatch"))
            {
                // tell the behaviour tree this task failed instead of running it - the tree carries on
                static_cast<UBTTask_BlueprintBase*>(Obj)->FinishExecute(false);
                return true;
            }
            return false;
        }
        /*
        if (Func->GetName() == "ServerMove" || Func->GetName() == "ServerMoveDual" || Func->GetName() == "ServerMoveDualHybridRootMotion" || Func->GetName() == "ServerMoveOld")
        {
            UCharacterMovementComponent* CMC = nullptr;

            if (Obj->IsA(UCharacterMovementComponent::StaticClass()))
            {
                CMC = (UCharacterMovementComponent*)Obj;
                CMC->bIgnoreClientMovementErrorChecksAndCorrection = true;
            }

            if (CMC && CMC->GetOwner()->IsA(AAIController::StaticClass())) return true; else return false;
        }
        */
        // Result notifications are never hidden. Correct the server counters
        // and prevent only a demonstrably premature native match-end decision.
        if (GNumFinishFuncs == 0)
            return false;
        bool bIs = false;
        for (int i = 0; i < GNumFinishFuncs; ++i)
            if (GFinishFuncs[i] == Func) { bIs = true; break; }
        if (!bIs)
            return false;
        const bool bBlock = GReady && S.KeepMatchWhileBotsAlive && GMatchSpawnDone && HoldCondition();
        BLOG(std::string("Game called ") + Func->GetName() + (bBlock ? " - BLOCKED" : " - allowed") +
            " (humans " + std::to_string(GHumansAlive) + ", bots " + std::to_string(GBotsAlive) +
            ", teams " + std::to_string(GTeamsAlive) + ")");
        return bBlock;
    }

    // top of the ProcessEvent hook: records the call trace and logs deaths / match end
    void OnCall(UObject* Obj, UFunction* Func, void* Parms)
    {
        if (!GReady || !Func || GetCurrentThreadId() != GGameThreadId)
            return;

        ApplyCounters();
        CaptureHumanNameRequest(Obj, Func, Parms);
        if (Func == GFnBroadcastDeath && Parms && Obj == GDamageVictimState && GDamageBotKillerState) {
            auto* Death = static_cast<Params::TslPlayerState_BroadcastDeath*>(Parms);
            if (!Death->KillerPlayerState) {
                Death->KillerPlayerState = GDamageBotKillerState;
                BLOG("Bot kill attribution restored for the matching damage event");
            }
        }
        if (Func == GFnBroadcastDeath && Obj && Parms) {
            auto* Death = static_cast<Params::TslPlayerState_BroadcastDeath*>(Parms);
            if (Death->KillerDamageType && Obj->IsA(ATslPlayerState::StaticClass()) && IsBotPS(static_cast<APlayerState*>(Obj))) {
                auto* Type = const_cast<UDamageType*>(Death->KillerDamageType);
                const bool Fall = Type->IsA(UTslDamageType::StaticClass()) && static_cast<UTslDamageType*>(Type)->bFallDamage;
                BLOG("Bot death cause: " + static_cast<APlayerState*>(Obj)->PlayerName.ToString() +
                    " type=" + Type->GetName() + " fall=" + std::to_string(Fall ? 1 : 0) +
                    " reason=" + std::to_string((int)Death->DamageReason));
            }
            // Measure the actual killer and victim rather than trusting the
            // engine's distance, which may use the local listen-server host.
            // The dying bot can already be absent from the live sight snapshot,
            // so check registered bot pawns before the snapshot fallback.
            static thread_local bool CheckingDistance = false;
            if (!CheckingDistance && Death->KillerPlayerState && Death->KillerPlayerState != Obj &&
                Obj->IsA(ATslPlayerState::StaticClass())) {
                CheckingDistance = true;
                const bool WasInTick = GInTick; GInTick = true;
                auto PositionForState = [&](APlayerState* State, FVector& Out) -> bool {
                    for (const FBot& B : GBots) {
                        if (!LiveBotObject(B.Ctrl,B.CtrlIndex) || B.Ctrl->PlayerState != State ||
                            !LiveBotObject(B.Pawn,B.PawnIndex)) continue;
                        Out=B.Pawn->K2_GetActorLocation(); return true;
                    }
                    for (const FCharInfo& C : GChars) {
                        if (!LiveBotObject(C.Char,C.Index)) continue;
                        auto* Controller=C.Char->Controller;
                        if (!Readable(Controller,sizeof(UObject)) || !LiveBotObject(Controller,Controller->GetIndex()) ||
                            Controller->PlayerState!=State) continue;
                        Out=C.Char->K2_GetActorLocation(); return true;
                    }
                    return false;
                };
                FVector KillerPosition{}, VictimPosition{}; float Metres=0.f;
                if (PositionForState(Death->KillerPlayerState,KillerPosition) &&
                    PositionForState(static_cast<APlayerState*>(Obj),VictimPosition) &&
                    KillDistancePolicy::Metres(KillerPosition.X,KillerPosition.Y,KillerPosition.Z,
                        VictimPosition.X,VictimPosition.Y,VictimPosition.Z,Metres)) {
                    const float Old=Death->InDistanceFromKillerToVictim;
                    Death->InDistanceFromKillerToVictim=Metres;
                    BLOG("KILL DISTANCE corrected old=" + std::to_string(Old) +
                        " measured_m=" + std::to_string(Metres));
                } else BLOG("KILL DISTANCE left unchanged: actual killer or victim position unavailable");
                GInTick = WasInTick;
                CheckingDistance = false;
            }
        }
        if (!GMatchSpawnDone && Parms && !IsTrainingWorld(UWorld::GetWorld()))
            for (int i = 0; i < GNumOnSetState; ++i)
                if (Func == GFnOnSetState[i] && static_cast<FName*>(Parms)->ToString() == "InProgress")
                {
                    OnMatchStarted();
                    SyncGameCounters(UWorld::GetWorld());
                    ApplyCounters();
                    break;
                }

        if (Func == GFnBroadcastDeath && Parms && S.FixKillFeedCount && S.BotsInPlayerCount && GBotsAlive > 0)
        {
            // the game counts only real logins: with one player every death reads "match finished".
            // Put players + bots in instead (the victim is still in our last count, so -1).
            int32& AliveNum = *reinterpret_cast<int32*>(static_cast<uint8*>(Parms) + 0x44);
            int32& AliveTeams = *reinterpret_cast<int32*>(static_cast<uint8*>(Parms) + 0x48);
            const int OurAlive = GHumansAlive + GBotsAlive - 1;
            const int OurTeams = GTeamsAlive - 1;
            if (OurTeams > 1 && OurTeams > AliveTeams)
            {
                AliveNum = (std::max)(OurAlive, OurTeams);
                AliveTeams = OurTeams;
            }
        }
        if (S.FixKillFeedCount && GBotsAlive > 0 && Parms && Obj)
        {
            // victim's rank = teams still alive after this death + 1
            if (Func == GFnBroadcastDeath && Obj->IsA(ATslPlayerState::StaticClass()) && !static_cast<APlayerState*>(Obj)->bIsABot &&
                !IsBotPS(static_cast<APlayerState*>(Obj)))
            {
                const int Left = (std::max)(1, GTeamsAlive - 1);
                static_cast<ATslPlayerState*>(Obj)->Ranking = Left + 1;
            }
            // end-of-round message to a dead player while bots are alive: not a win
            if ((Func == GFnGameEnded || Func == GFnRoundEnd) && Obj->IsA(AController::StaticClass()))
            {
                APawn* P = static_cast<AController*>(Obj)->K2_GetPawn();
                const bool bDead = !P || !P->IsA(ATslCharacter::StaticClass()) || static_cast<ATslCharacter*>(P)->Health <= 0.f;
                if (bDead) {
                    *(static_cast<uint8*>(Parms) + (Func == GFnGameEnded ? 0x08 : 0x00)) = 0; // bIsWinner = false
                }
            }
        }

        if (GInTick)
            return;

        if (Obj && IsFireFunc(Func) && GShots.size() < 32 && Obj->IsA(AActor::StaticClass()))
        {
            APawn* Shooter = static_cast<AActor*>(Obj)->GetInstigator();
            if (Shooter && !IsBotPawn(Shooter))
            {
                GShots.push_back({ Shooter->K2_GetActorLocation() });
                if (GHumanFiredAt.size() < 256) GHumanFiredAt[Shooter->GetIndex()] = Now();
            }
        }
        FTraceEnt& E = GTrace[GTraceHead % 2048];
        E.Func = Func;
        E.Cls = Obj ? Obj->GetClass() : nullptr;
        ++GTraceHead;
        if (Func == GFnBroadcastDeath && Parms && Obj)
        {
            const uint8* P = static_cast<const uint8*>(Parms);
            ATslPlayerState* KillerPS = *reinterpret_cast<ATslPlayerState* const*>(P + 0x00);
            const int32 AliveNum = *reinterpret_cast<const int32*>(P + 0x44);
            const int32 AliveTeams = *reinterpret_cast<const int32*>(P + 0x48);
            APlayerState* Victim = static_cast<APlayerState*>(Obj);
            std::string Killer = "?";
            if (KillerPS) Killer = KillerPS->PlayerName.ToString();
            BLOG("death: " + Victim->PlayerName.ToString() + " (killer " + Killer + ") - the game says " +
                std::to_string(AliveNum) + " alive, " + std::to_string(AliveTeams) + " teams alive");
            return;
        }
        for (int i = 0; i < GNumOnSetState; ++i)
        {
            if (Func != GFnOnSetState[i] || !Parms) continue;
            const FName NS = *static_cast<const FName*>(Parms); // GameMode.K2_OnSetMatchState parms: FName NewState @0x00
            if (GHaveNames ? SameName(NS, GNameWaitingPost) : NS.ToString() == "WaitingPostMatch")
            {
                if (!IsTrainingWorld(UWorld::GetWorld())) StopZombiePostmatchController(UWorld::GetWorld());
                BLOG(std::string("Match state -> WaitingPostMatch (end guard: ") + GGuardWhat + ", held " + std::to_string(GHeldEnds) + "x)");
                ScheduleExit("match state WaitingPostMatch");
                if (!GTraceDumped) { GTraceDumped = true; DumpTrace("before the match ended"); }
            }
            break;
        }
    }

    void OnMatchStarted();

    struct FRoomMember { ATslPlayerState* State; ATslPlayerController* PC; ATeam* Actor; };

    std::string RoomLower(std::string Name) {
        std::transform(Name.begin(), Name.end(), Name.begin(), [](unsigned char C){return (char)std::tolower(C);});
        return Name;
    }

    void ApplyRoomTeams(UWorld* World, double T) {
        if (S.TeamSize <= 1 || T < GNextRoomTeamCheck || !World || !World->AuthorityGameMode || !World->AuthorityGameMode->IsA(ATslGameMode::StaticClass())) return;
        GNextRoomTeamCheck = T + 1.0;
        auto* GM=static_cast<ATslGameMode*>(World->AuthorityGameMode); auto* Base=GM->GameState;
        if(!Base || !Base->IsA(ATslGameState::StaticClass()) || static_cast<ATslGameState*>(Base)->MatchState.ToString()!="WaitingToStart") return;
        wchar_t Executable[MAX_PATH]{}; if(!GetModuleFileNameW(nullptr,Executable,MAX_PATH))return;
        std::ifstream File(std::filesystem::path(Executable).parent_path()/L"LAN_Teams.ini"); if(!File)return;
        std::unordered_map<std::string,int> Selected; std::string Line; bool InTeams=false;
        while(std::getline(File,Line)) {
            if(!Line.empty() && Line.back()=='\r')Line.pop_back();
            if(Line=="[Teams]"){InTeams=true;continue;} if(!Line.empty() && Line[0]=='['){InTeams=false;continue;} if(!InTeams)continue;
            auto Sep=Line.find('=');if(Sep==std::string::npos || Sep==0 || Sep>24)continue;
            std::string Name=Line.substr(0,Sep); bool Valid=true;for(unsigned char C:Name)if(!std::isalnum(C) && C!='_' && C!='-')Valid=false;
            if(!Valid)continue;char* End=nullptr;long Group=std::strtol(Line.c_str()+Sep+1,&End,10);
            if(End && *End=='\0' && Group>=1 && Group<=100)Selected[RoomLower(Name)]=(int)Group;
        }
        std::unordered_map<int,std::vector<FRoomMember>> Groups;
        for(APlayerState* PS0:Base->PlayerArray) {
            if(!PS0 || !PS0->IsA(ATslPlayerState::StaticClass()))continue;
            auto It=Selected.find(RoomLower(PS0->PlayerName.ToString()));if(It==Selected.end())continue;
            auto* O=PS0->GetOwner();if(!O || !O->IsA(ATslPlayerController::StaticClass()))continue;
            auto* PC=static_cast<ATslPlayerController*>(O);auto* PS=static_cast<ATslPlayerState*>(PS0);
            if(!PC->NetConnection || !IsLiveHumanNameTeam(PS->Team) || PS->TeamNumber<=0)continue;
            Groups[It->second].push_back({PS,PC,PS->Team});
        }
        for(auto& Group:Groups) {
            auto& People=Group.second;if(People.size()<2 || People.size()>(size_t)S.TeamSize)continue;
            std::sort(People.begin(),People.end(),[](const FRoomMember& A,const FRoomMember& B){return A.State->GetIndex()<B.State->GetIndex();});
            std::string Signature;for(auto& M:People)Signature+=std::to_string(M.State->GetIndex())+":"+std::to_string(M.Actor->GetIndex())+",";
            if(GRoomTeamSignatures[Group.first]==Signature)continue;
            int Team=People.front().State->TeamNumber;if(Team<1 || Team>100)continue;
            auto* Teams=reinterpret_cast<FRawArray*>(&GM->Teams);
            if(!Teams->Data || Teams->Num<1 || Teams->Num>128)continue;
            struct FGroupCopy { int Number; std::vector<ATeam*> Members; };
            std::vector<FGroupCopy> NewGroups;std::vector<bool> Found(People.size(),false);bool Valid=true,TargetFound=false;
            auto* Entries=static_cast<FTeams*>(Teams->Data);
            for(int I=0;I<Teams->Num && Valid;++I) {
                auto* Raw=reinterpret_cast<FRawArray*>(&Entries[I].Members);
                if(Raw->Num<0 || Raw->Num>128 || (Raw->Num && !Raw->Data)){Valid=false;break;}
                FGroupCopy Copy{Entries[I].TeamNum,{}};if(Copy.Number==Team)TargetFound=true;
                for(int J=0;J<Raw->Num;++J) {
                    auto* Actor=static_cast<ATeam**>(Raw->Data)[J]; bool Participates=false;
                    for(size_t K=0;K<People.size();++K)if(Actor==People[K].Actor){Found[K]=true;Participates=true;break;}
                    if(Participates)continue;
                    if(Copy.Number==Team) {
                        if(!IsLiveHumanNameTeam(Actor)){Valid=false;break;}
                        auto* PS=Actor->AuthorizedPlayerState ? Actor->AuthorizedPlayerState : Actor->CachedTslPlayerState;
                        auto* Owner=PS ? PS->GetOwner() : nullptr;
                        // The listen server's own local, non-network controller is not a player slot.
                        if(!PS || IsBotPS(PS) || !Owner || !Owner->IsA(APlayerController::StaticClass()) || static_cast<APlayerController*>(Owner)->NetConnection) {Valid=false;break;}
                        continue;
                    }
                    Copy.Members.push_back(Actor);
                }
                if(Copy.Number==Team)for(auto& M:People)Copy.Members.push_back(M.Actor);
                if(!Copy.Members.empty())NewGroups.push_back(std::move(Copy));
            }
            if(!Valid || !TargetFound || std::find(Found.begin(),Found.end(),false)!=Found.end())continue;
            auto* NewEntries=static_cast<FTeams*>(GameAlloc((int32)(sizeof(FTeams)*NewGroups.size())));if(!NewEntries)continue;
            for(size_t I=0;I<NewGroups.size() && Valid;++I) {
                auto& G=NewGroups[I];auto** Data=static_cast<ATeam**>(GameAlloc((int32)(sizeof(ATeam*)*G.Members.size())));if(!Data){Valid=false;break;}
                std::memcpy(Data,G.Members.data(),sizeof(ATeam*)*G.Members.size());std::memset(&NewEntries[I],0,sizeof(FTeams));NewEntries[I].TeamNum=G.Number;
                FRawArray Members{Data,(int32)G.Members.size(),(int32)G.Members.size()};std::memcpy(&NewEntries[I].Members,&Members,sizeof(Members));
            }
            if(!Valid)continue;
            std::vector<ATeam**> PlayerLists;
            for(size_t I=0;I<People.size();++I){auto** Data=static_cast<ATeam**>(GameAlloc((int32)(sizeof(ATeam*)*People.size())));if(!Data){Valid=false;break;}for(size_t J=0;J<People.size();++J)Data[J]=People[J].Actor;PlayerLists.push_back(Data);}
            if(!Valid)continue;
            Teams->Data=NewEntries;Teams->Num=Teams->Max=(int32)NewGroups.size();
            for(size_t I=0;I<People.size();++I){auto& M=People[I];M.State->TeamNumber=Team;M.State->SquadMemberIndex=(int32)I;M.Actor->TeamIdRep=(int8)Team;M.Actor->MemberIndex=(int8)I;
                auto* List=reinterpret_cast<FRawArray*>(&M.PC->Teams);List->Data=PlayerLists[I];List->Num=List->Max=(int32)People.size();M.State->ForceNetUpdate();M.Actor->ForceNetUpdate();M.PC->ForceNetUpdate();}
            GRoomTeamSignatures[Group.first]=Signature;
            BLOG("LAN ROOM TEAM selected="+std::to_string(Group.first)+" native="+std::to_string(Team)+" members="+std::to_string(People.size()));
        }
    }

    // Wait for the launcher's room roster and team assignment before starting
    // ground-based zombie matches. Direct joins without a room file can start
    // as soon as a human and the configured zombie bots are present.
    void TryStartZombieGroundMatch(UWorld* World, double T) {
        if (!World || !World->AuthorityGameMode || !World->AuthorityGameMode->IsA(ATslZombieGameMode::StaticClass())) return;
        auto* Mode=static_cast<ATslZombieGameMode*>(World->AuthorityGameMode);
        if (Mode->ZombieRuleType==EZombieRuleType::BattleRoyale || Mode->MatchState.ToString()!="WaitingToStart") return;
        if (!HasRemotePlayer(World)) {GZombieStartWaitSince=0;return;}
        if (GZombieStartWaitSince<=0) {GZombieStartWaitSince=T;BLOG("[ZOMBIE] Waiting for room players, teams and bots.");}
        if (T<GZombieStartNextCheck) return; GZombieStartNextCheck=T+0.5;
        bool HasZombie=false;for (const auto& Bot:GBots) {
            if (Bot.State==EBotState::Dead || !Bot.Ctrl || !LiveBotObject(Bot.Ctrl,Bot.CtrlIndex) || !Bot.Ctrl->PlayerState || !Bot.Ctrl->PlayerState->IsA(ATslZombiePlayerState::StaticClass())) continue;
            if (static_cast<ATslZombiePlayerState*>(Bot.Ctrl->PlayerState)->GetZombieModePlayerState()==EZombieModePlayerState::Zombie) {HasZombie=true;break;}
        }
        if (!HasZombie) return;
        const bool TimedOut=T-GZombieStartWaitSince>=120.0;
        if (GPendingLobbySpawns>0 || GPendingNearSpawns>0 || GPendingSpawns>0) {
            if (!TimedOut) return;
            BLOG("[ZOMBIE] Bot loading timed out; starting with the bots already available.");
            GPendingLobbySpawns=0;GPendingNearSpawns=0;GPendingSpawns=0;
        }
        bool ParticipantsReady=true, TeamsReady=true;
        wchar_t Exe[MAX_PATH]{};
        if (GetModuleFileNameW(nullptr,Exe,MAX_PATH)) {
            std::ifstream File(std::filesystem::path(Exe).parent_path()/L"LAN_Teams.ini");
            std::unordered_map<std::string,int> Expected; std::unordered_map<int,int> GroupSizes;std::string Line;bool InTeams=false;
            while(std::getline(File,Line)) {
                if(!Line.empty() && Line.back()=='\r') Line.pop_back();
                if(Line=="[Teams]"){InTeams=true;continue;}if(!Line.empty()&&Line[0]=='['){InTeams=false;continue;}if(!InTeams)continue;
                auto Sep=Line.find('=');if(Sep==std::string::npos || Sep==0 || Sep>24)continue;
                std::string Name=Line.substr(0,Sep);bool Valid=true;for(unsigned char C:Name)if(!std::isalnum(C)&&C!='_'&&C!='-')Valid=false;
                char* End=nullptr;long Group=std::strtol(Line.c_str()+Sep+1,&End,10);
                if(Valid && End && *End=='\0' && Group>=1 && Group<=100)Expected[RoomLower(Name)]=(int)Group;
            }
            for (const auto& Entry:Expected) ++GroupSizes[Entry.second];
            auto* State=Mode->GameState;std::unordered_map<int,int> NativeTeams, ConnectedGroups;
            for (const auto& Entry:Expected) {
                bool Found=false;
                if(State)for(auto* PS:State->PlayerArray) {
                    if(!PS || PS->bIsABot || RoomLower(PS->PlayerName.ToString())!=Entry.first)continue;
                    auto* Owner=PS->GetOwner();if(!Owner || !Owner->IsA(APlayerController::StaticClass()))continue;
                    auto* PC=static_cast<APlayerController*>(Owner);if(PC->NetConnection && !PC->bIsLocalPlayerController && PC->K2_GetPawn() && PS->IsA(ATslPlayerState::StaticClass())) {
                        const int Team=static_cast<ATslPlayerState*>(PS)->TeamNumber;
                        auto Existing=NativeTeams.find(Entry.second);
                        if (Team<=0 || (Existing!=NativeTeams.end() && Existing->second!=Team)) TeamsReady=false;
                        NativeTeams[Entry.second]=Team;++ConnectedGroups[Entry.second];Found=true;break;
                    }
                }
                if(!Found) ParticipantsReady=false;
            }
            if(S.TeamSize>1)for(const auto& Group:GroupSizes) {
                if(Group.second>S.TeamSize) {TeamsReady=false;break;}
                auto Signature=GRoomTeamSignatures.find(Group.first);
                if(ConnectedGroups[Group.first]>1 && (Signature==GRoomTeamSignatures.end() || Signature->second.empty())) {TeamsReady=false;break;}
            }
        }
        // A missing player may time out; players who did connect must never be
        // started in the wrong team merely because the wait expired.
        if(!TeamsReady) {
            static double NextWarning=0;
            if(TimedOut && T>=NextWarning){NextWarning=T+30.0;BLOG("[ZOMBIE] Still waiting for connected players' teams.");}
            return;
        }
        if(!ParticipantsReady && !TimedOut) return;
        if(!ParticipantsReady) BLOG("[ZOMBIE] Room readiness timed out; starting with connected players.");
        if (Mode->GameState && Mode->GameState->IsA(ATslGameState::StaticClass())) {
            auto* GameState=static_cast<ATslGameState*>(Mode->GameState);GameState->bTimerPaused=false;GameState->ForceNetUpdate();
        }
        BLOG("[ZOMBIE] Room ready; starting match.");Mode->StartMatch();
    }

    void Tick()
    {
        if (!GReady || GInTick)
            return;

        if (GGameThreadId == 0)
            GGameThreadId = GetCurrentThreadId(); // fallback: first caller
        if (GetCurrentThreadId() != GGameThreadId)
            return;

        ApplyCounters();
        ExitThink();

        ++GTickCalls;
        const double T = Now();
        if (T - GTickRateStart >= 5.0)
        {
            GTickRate = GTickCalls / (T - GTickRateStart);
            GTickCalls = 0;
            GTickRateStart = T;
        }

        const bool bThink = (T - GLastThink) >= 1.0 / S.ThinkHz;
        const bool bSteer = (T - GLastSteer) >= 0.008;
        if (!bThink && !bSteer)
            return;

        GInTick = true;

        static bool bLoggedFirstTick = false;
        if (!bLoggedFirstTick)
        {
            bLoggedFirstTick = true;
            BLOG("First tick on the game thread");
        }

        UWorld* World = UWorld::GetWorld();
        if (World != GWorld)
            OnWorldChanged(World);

        if (World)
        {
            if (!GModeApplied && World->AuthorityGameMode && World->AuthorityGameMode->IsA(ATslGameMode::StaticClass())) {
                auto* Mode = static_cast<ATslGameMode*>(World->AuthorityGameMode);
                Mode->CameraViewBehaviour = S.CameraMode ? ECameraViewBehaviour::FpsOnly : ECameraViewBehaviour::FpsAndTps;
                Mode->TeamSize = S.TeamSize;
                GModeApplied = true;
                BLOG("HOST MODE team=" + std::to_string(S.TeamSize) + " camera=" + (S.CameraMode ? "FPP" : "TPP"));
            }
            if (bThink)
            {
                GLastThink = T; GDoorProbeBudget=6;
                // Alternate the two costly operations so one AI pass does not
                // both dress a zombie and rebuild another zombie's model.
                GZombieAppearancePhase=!GZombieAppearancePhase;
                GZombieAppearanceBudget=GZombieAppearancePhase ? 1 : 0;
                GZombieDressBudget=GZombieAppearancePhase ? 0 : 1;
                ApplyHumanNameRequests(T);
                ApplyServerHostName(World,T);
                ApplyRoomTeams(World,T);
                SkinSelection::EnsurePlayerModels(World);
                if (World->AuthorityGameMode && World->AuthorityGameMode->IsA(ATslZombieGameMode::StaticClass()) &&
                    static_cast<ATslZombieGameMode*>(World->AuthorityGameMode)->ZombieRuleType==EZombieRuleType::BattleRoyale) {
                    static bool WasPlane=false, LeftPlane=false; static double NextReport=0, NextOpen=0;
                    auto* Human=FindRealPlayer(World);
                    if (Human && Human->IsInAircraft() && !Human->SkydivingComponent) {
                        BLOG("[ZOMBIE TEST] Creating human skydive component aboard aircraft");
                        Human->Server_SpawnSkydiveComponent();
                        if (!Human->SkydivingComponent) Human->CreateSkydiveComponent();
                        if (Human->SkydivingComponent) {
                            Human->InitilizeServerSkydiveComp();
                            Human->SkydivingComponent->ParachuteDeploySettings.bUseAutoDeploy=true;
                            Human->SkydivingComponent->ParachuteDeploySettings.AutoDeployAltitude=300.f;
                            static_cast<ATslZombieGameMode*>(World->AuthorityGameMode)->bUseParachuteAutoDeploy=true;
                            Human->ForceNetUpdate();
                            BLOG("[ZOMBIE TEST] Human skydive component created aboard aircraft");
                        }
                    }
                    if (Human && Human->SkydivingComponent && Human->Health>0.f) {
                        auto* Sky=Human->SkydivingComponent; auto* Move=Human->GetTslCharacterMovement();
                        if (Human->IsInAircraft()) WasPlane=true;
                        if (WasPlane && !Human->IsInAircraft() && !LeftPlane && Move) {
                            LeftPlane=true;
                            Sky->SkydiveState=ESkydiveState::Skydive_Freefall;
                            Move->SetMovementMode(EMovementMode::MOVE_Custom,1);
                            Human->ForceNetUpdate();
                            BLOG("[ZOMBIE TEST] Human left aircraft; freefall movement initialized");
                        }
                        if (LeftPlane && !Human->IsInAircraft() && Sky->SkydiveState==ESkydiveState::Skydive_Freefall) {
#ifdef PUBG2019_ENABLE_DIAGNOSTICS
                            static bool ProbeRelocated=false; char ProbeEnabled[4]{}, SafeLanding[4]{};
                            const bool RelocationRequested=(GetEnvironmentVariableA("PUBG2019_ZOMBIE_RESPAWN_PROBE",ProbeEnabled,sizeof(ProbeEnabled)) && ProbeEnabled[0]=='1') ||
                                (GetEnvironmentVariableA("PUBG2019_ZOMBIE_SAFE_LANDING_PROBE",SafeLanding,sizeof(SafeLanding)) && SafeLanding[0]=='1');
                            if (!ProbeRelocated && RelocationRequested) {
                                FVector ProbeSurface{};
                                if (TraceSkyGround(World,Human,V(400000.f,400000.f,200000.f),ProbeSurface)) {
                                    FHitResult Ignored{};
                                    ProbeRelocated=Human->K2_SetActorLocation(ProbeSurface+V(0.f,0.f,50000.f),false,&Ignored,true);
                                    if (ProbeRelocated) BLOG("[ZOMBIE TEST] Diagnostic: relocated freefall above central ground to isolate combat testing");
                                }
                            }
#endif
                            FVector Here=Human->K2_GetActorLocation(), Floor{};
                            if (TraceSkyGround(World,Human,Here,Floor)) {
                                float Clearance=Here.Z-Floor.Z;
                                if (T>=NextReport) {NextReport=T+4;BLOG("[ZOMBIE TEST] Human skydive clearance="+std::to_string((int)Clearance));}
                                if (Clearance>0.f && Clearance<=30000.f && T>=NextOpen) {
                                    NextOpen=T+2; Sky->Server_RequestParachuteOpen(); Human->ForceNetUpdate();
                                    BLOG("[ZOMBIE TEST] Human parachute requested, state="+std::to_string((int)Sky->SkydiveState));
                                }
                            }
                        }
                    }
                }
#ifdef PUBG2019_ENABLE_DIAGNOSTICS
                {
                    char Enabled[4]{};
                    if (GetEnvironmentVariableA("PUBG2019_ZOMBIE_RESPAWN_PROBE",Enabled,sizeof(Enabled)) && Enabled[0]=='1' &&
                        World->AuthorityGameMode && World->AuthorityGameMode->IsA(ATslZombieGameMode::StaticClass())) {
                        static bool Triggered=false, ProbeWasInPlane=false; static double GroundAt=0, NextReport=0;
                        static ATslZombiePlayerState* TestState=nullptr; static int StateIndex=-1;
                        static ATslZombiePlayerController* TestController=nullptr; static int ControllerIndex=-1; static double DiedAt=0; static bool RevivalRequested=false, ReadyProbe=false, RestartProbe=false;
                        static FTransform DeathTransform{};
                        if (!Triggered && GMatchSpawnDone) {
                            auto* Human=FindRealPlayer(World);
                            if (Human && Human->IsInAircraft()) ProbeWasInPlane=true;
                            FVector HumanGround{}, ProbePoint=Human ? Human->K2_GetActorLocation() : FVector{};
                            ProbePoint.Z+=200.f;
                            const bool Grounded=Human && TraceSkyGround(World,Human,ProbePoint,HumanGround) &&
                                Human->K2_GetActorLocation().Z-HumanGround.Z < 250.f;
                            if (Human && ProbeWasInPlane && Grounded && !Human->IsInAircraft() && !Human->IsFallingWithParachute() && Human->GetTslCharacterMovement() && !Human->GetTslCharacterMovement()->IsFalling()) {
                                if (!GroundAt) {GroundAt=T; BLOG("[ZOMBIE TEST] Human landed; preparing isolated respawn probe");}
                                if (T-GroundAt>8) {
                                    auto* HumanState=Human->GetTslPlayerState();
                                    auto* Owner=HumanState ? HumanState->GetOwner() : nullptr;
                                    auto* PC=(Owner && Owner->IsA(ATslPlayerController::StaticClass())) ? static_cast<ATslPlayerController*>(Owner) : nullptr;
                                    if (PC && PC->PlayerState && PC->PlayerState->IsA(ATslZombiePlayerState::StaticClass())) {
                                        FBot* Attacker=nullptr;
                                        for (auto& Candidate:GBots) {
                                            if (Candidate.Pawn && Candidate.Ctrl && LiveBotObject(Candidate.Pawn,Candidate.PawnIndex) &&
                                                Candidate.State!=EBotState::Dead && Candidate.JumpedAt>0 && !Candidate.NativeSkyActive && !Candidate.DropPending &&
                                                !Candidate.Pawn->IsInAircraft() && !Candidate.Pawn->IsFallingWithParachute() && Candidate.Ctrl->PlayerState &&
                                                Candidate.Ctrl->PlayerState->IsA(ATslZombiePlayerState::StaticClass())) { Attacker=&Candidate; break; }
                                        }
                                        if (Attacker) {
                                            TestState=static_cast<ATslZombiePlayerState*>(PC->PlayerState); StateIndex=TestState->GetIndex(); Triggered=true; DiedAt=T;
                                            DeathTransform=Human->GetTransform(); Human->bApplyZombieVirus=true; Human->ForceNetUpdate();
                                            if (PC->IsA(ATslZombiePlayerController::StaticClass())) { TestController=static_cast<ATslZombiePlayerController*>(PC); ControllerIndex=PC->GetIndex(); }
                                            auto* AttackerState=static_cast<ATslZombiePlayerState*>(Attacker->Ctrl->PlayerState);
                                            AttackerState->SetZombieModePlayerState(EZombieModePlayerState::Zombie); AttackerState->ForceNetUpdate();
                                            FHitResult Ignored{}; Attacker->Pawn->K2_SetActorLocation(Human->K2_GetActorLocation()+V(150.f,0.f,0.f),false,&Ignored,true);
                                            BLOG("[ZOMBIE TEST] Diagnostic: landed zombie bot placed near infected human for real combat");
                                        }
                                    }
                                }
                            }
                        }
                        if (Triggered && !RevivalRequested && T-DiedAt>10 && TestController && LiveBotObject(TestController,ControllerIndex) && !TestController->K2_GetPawn()) {
                            RevivalRequested=true;
                            BLOG("[ZOMBIE TEST] Invoking native zombie revival for diagnostic");
                            TestController->ServerZombieRevive();
                        }
                        if (Triggered && !ReadyProbe && T-DiedAt>25 && TestState && LiveBotObject(TestState,StateIndex) && TestController && LiveBotObject(TestController,ControllerIndex) && !TestController->K2_GetPawn()) {
                            ReadyProbe=true;
                            BLOG("[ZOMBIE TEST] Diagnostic: set ready-to-revive state and retry native revival");
                            TestState->SetZombieModePlayerState(EZombieModePlayerState::ReadyToRevive);
                            TestState->ForceNetUpdate();
                            TestController->ServerZombieRevive();
                        }
                        if (Triggered && !RestartProbe && T-DiedAt>40 && TestState && LiveBotObject(TestState,StateIndex) && TestController && LiveBotObject(TestController,ControllerIndex) && !TestController->K2_GetPawn()) {
                            RestartProbe=true;
                            BLOG("[ZOMBIE TEST] Diagnostic: restart zombie controller at recorded death transform");
                            TestState->SetZombieModePlayerState(EZombieModePlayerState::Zombie);
                            BLOG("[ZOMBIE TEST] pre-restart spectator="+std::to_string((int)TestState->bIsSpectator)+" only="+std::to_string((int)TestState->bOnlySpectator)+" killed="+std::to_string((int)TestState->bKilled));
                            TestState->bIsSpectator=0; TestState->bOnlySpectator=0; TestState->bKilled=0;
                            TestState->ForceNetUpdate();
                            World->AuthorityGameMode->RestartPlayerAtTransform(TestController,DeathTransform);
                            auto* NewPawn=TestController->K2_GetPawn();
                            if (NewPawn && NewPawn->IsA(ATslCharacter::StaticClass())) {
                                auto* ZombiePawn=static_cast<ATslCharacter*>(NewPawn);
                                *reinterpret_cast<EZombieModePlayerState*>(reinterpret_cast<uintptr_t>(ZombiePawn)+0x1194)=EZombieModePlayerState::Zombie;
                                ZombiePawn->OnRep_ZombieModePlayerState(); ZombiePawn->ForceNetUpdate();
                                BLOG("[ZOMBIE TEST] Diagnostic: restarted character marked zombie");
                            }
                        }
                        if (Triggered && TestState && LiveBotObject(TestState,StateIndex) && T>=NextReport) {
                            NextReport=T+5;
                            auto* Pawn=TestController && LiveBotObject(TestController,ControllerIndex) ? TestController->K2_GetPawn() : nullptr;
                            BLOG("[ZOMBIE TEST] revival player_state="+std::to_string((int)TestState->GetZombieModePlayerState())+" respawn="+std::to_string(TestState->IsZombieRespawn())+" pawn="+(Pawn ? Pawn->GetName() : std::string("none")));
                        }
                    }
                }
#endif
                const bool ZombieMode = World->AuthorityGameMode && World->AuthorityGameMode->IsA(ATslZombieGameMode::StaticClass());
                // Training has no AI bots; native player loot remains handled by the game.
                if (!IsTrainingWorld(World)) {
                    ScanBuildingTargets(T);
                    ScanGroundLoot(T,!ZombieMode);
                }
                ObserveZoneClosing(World);

                if (GAutoMatchStart && !GMatchSpawnDone && !IsTrainingWorld(World))
                {
                    AGameStateBase* GSB = World->AuthorityGameMode->GameState;
                    if (GSB)
                    {
                        ATslGameState* GS = static_cast<ATslGameState*>(GSB);
                        const FName MS = GS->MatchState;
                        const int32 Rem = GS->RemainingTime;
                        const bool bStateChanged = GMatchStateKnown && (MS.GetComparisonIndex() != GLastMatchState.GetComparisonIndex() || MS.GetNumber() != GLastMatchState.GetNumber());
                        // last seconds of the lobby: take the lobby bots out BEFORE the game counts its starting
                        // players and the plane build seats everyone - they respawn on land after the start
                        if (!S.RegisterTeams && !GPreStartCleared && Rem > 0 && Rem <= 3 && !GBots.empty())
                        {
                            int N = 0;
                            for (const FBot& B : GBots) if (B.State != EBotState::Dead) ++N;
                            RemoveAllBots();
                            GCarryOver += N;
                            GPreStartCleared = true;
                            BLOG("Lobby ending - " + std::to_string(N) + " lobby bots removed, they respawn on land after the plane leaves");
                        }

                        if (bStateChanged && GMatchSpawnDone)
                        {
                            DumpTeams(World, "match state -> " + MS.ToString());
                        }

                        // LeavingMap / WaitingPostMatch are not a new match.
                        if (MS.ToString() == "InProgress")
                        {
                            if (!GRoomMatchAnnounced) {
                                GRoomMatchAnnounced = true;
                                wchar_t Executable[MAX_PATH]{};
                                if(GetModuleFileNameW(nullptr,Executable,MAX_PATH)) {
                                    const auto Dir=std::filesystem::path(Executable).parent_path();
                                    std::error_code Error;
                                    if(std::filesystem::exists(Dir/L"LAN_Teams.ini",Error) && !Error) {
                                        std::ofstream Flag(Dir/L"LAN_MatchStarted.flag",std::ios::trunc);
                                        if(Flag)Flag << "started\n";
                                    }
                                }
                            }
                            BLOG("Detected match start (MatchState " + MS.ToString() + ", RemainingTime " + std::to_string(Rem) + ")");
                            GatherCharacters(World);
                            Bots::OnMatchStarted();
                        }

                        if (MS.ToString() == "WaitingToStart" && GServerTerrainReady && S.RegisterTeams && S.SpawnOnMatchStart && !GLobbySpawned && !GMatchSpawnDone && Rem > 10 && HasRemotePlayer(World) && !(World->AuthorityGameMode && World->AuthorityGameMode->IsA(ATslZombieGameMode::StaticClass()) && static_cast<ATslZombieGameMode*>(World->AuthorityGameMode)->ZombieRuleType==EZombieRuleType::BattleRoyale))
                        {
                            GLobbySpawned = true;
                            GPendingLobbySpawns += (std::max)(0, S.Count);
                            BLOG("Lobby: " + std::to_string(S.Count) + " bots joining now so they board the plane");
                        }

                        GLastMatchState = MS;
                        GLastRemaining = Rem;
                        GMatchStateKnown = true;
                    }
                    else
                    {
                        BLOG("GameState is invalid");
                    }
                }

#ifdef PUBG2019_ENABLE_DIAGNOSTICS
                char ServerOnlyProbe[4]{};
                if (!GServerOnlyZombieTarget && GServerTerrainReady &&
                    GetEnvironmentVariableA("PUBG2019_ZOMBIE_SERVER_ONLY_PROBE",ServerOnlyProbe,sizeof(ServerOnlyProbe)) && ServerOnlyProbe[0]=='1' &&
                    World->AuthorityGameMode && World->AuthorityGameMode->IsA(ATslZombieGameMode::StaticClass()) &&
                    static_cast<ATslZombieGameMode*>(World->AuthorityGameMode)->MatchState.ToString()=="WaitingToStart") {
                    static double NextTargetAttempt=0;
                    if (T>=NextTargetAttempt) {
                        NextTargetAttempt=T+5.0; FVector Ground{};
                        if (FindLandNear(World,400000.f,400000.f,Ground) && SpawnBotAt(World,Ground)) {
                            GServerOnlyZombieTarget=GBots.back().Pawn;
                            BLOG("[ZOMBIE TEST] Server-only stationary surrogate placed on ground; no real client connected");
                            static_cast<ATslZombieGameMode*>(World->AuthorityGameMode)->StartMatch();
                        }
                    }
                }
                char DelayedSpawn[4]{};
                if (GetEnvironmentVariableA("PUBG2019_ZOMBIE_DELAYED_SPAWN_PROBE",DelayedSpawn,sizeof(DelayedSpawn)) && DelayedSpawn[0]=='1' &&
                    World->AuthorityGameMode && World->AuthorityGameMode->IsA(ATslZombieGameMode::StaticClass()) &&
                    static_cast<ATslZombieGameMode*>(World->AuthorityGameMode)->ZombieRuleType==EZombieRuleType::BattleRoyale && GMatchSpawnDone) {
                    static double LandedAt=0, NextSpawn=0; static int ZombiesSpawned=0; static bool WasInPlane=false;
                    char CountText[12]{}; int TargetCount=4;
                    if (GetEnvironmentVariableA("PUBG2019_ZOMBIE_DELAYED_SPAWN_COUNT",CountText,sizeof(CountText)))
                        TargetCount=(std::max)(1,(std::min)(90,std::atoi(CountText)));
                    int LiveZombies=0;
                    for (auto& Existing:GBots) if (Existing.State!=EBotState::Dead && Existing.Ctrl && LiveBotObject(Existing.Ctrl,Existing.CtrlIndex) &&
                        Existing.Ctrl->PlayerState && Existing.Ctrl->PlayerState->IsA(ATslZombiePlayerState::StaticClass()) &&
                        static_cast<ATslZombiePlayerState*>(Existing.Ctrl->PlayerState)->GetZombieModePlayerState()==EZombieModePlayerState::Zombie) ++LiveZombies;
                    auto* Human=FindRealPlayer(World);
                    auto* Movement=Human ? Human->GetTslCharacterMovement() : nullptr;
                    if (Human && (Human->IsInAircraft() || Human==GServerOnlyZombieTarget)) WasInPlane=true;
                    if (!LandedAt && WasInPlane && Human && Human->Health>0 && Movement && !Movement->IsFalling() &&
                        !Human->IsInAircraft() && !Human->IsFallingWithParachute()) {
                        LandedAt=T; BLOG("[ZOMBIE TEST] Delayed-spawn clock started on player landing; delay=120 seconds");
                    }
                    if (LandedAt>0 && Human && Human->Health>0 && T-LandedAt>=120.0 && LiveZombies<TargetCount && T>=NextSpawn) {
                        NextSpawn=T+1.0;
                        const FVector Origin=Human->K2_GetActorLocation(); FVector Ground{}; bool Found=false;
                        for (int Attempt=0;Attempt<32 && !Found;++Attempt) {
                            const float A=RandF(0.f,2.f*kPi), R=RandF(20000.f,80000.f);
                            Found=TraceLand(World,Origin.X+std::cos(A)*R,Origin.Y+std::sin(A)*R,Ground) &&
                                  !IsWaterGround(Ground.Z) && DrySpot(World,Ground);
                        }
                        if (Found && SpawnBotAt(World,Ground)) {
                            auto& SpawnedBot=GBots.back(); SpawnedBot.JumpedAt=T; SpawnedBot.NativeSkyActive=false; SpawnedBot.DropPending=false;
                            if (SpawnedBot.Ctrl && SpawnedBot.Ctrl->PlayerState && SpawnedBot.Ctrl->PlayerState->IsA(ATslZombiePlayerState::StaticClass())) {
                                auto* State=static_cast<ATslZombiePlayerState*>(SpawnedBot.Ctrl->PlayerState);
                                State->SetZombieModePlayerState(EZombieModePlayerState::Zombie); State->ForceNetUpdate();
                                ++ZombiesSpawned;
                                BLOG("[ZOMBIE TEST] delayed ground zombie="+SpawnedBot.Name+" after_landing_seconds="+std::to_string((int)(T-LandedAt))+
                                    " distance_m="+std::to_string((int)(Len(Flat(Ground-Origin))/100.f))+
                                    " location="+std::to_string((int)Ground.X)+","+std::to_string((int)Ground.Y));
                            }
                        }
                    }
                }
#endif
                const bool ZombieSpawn = World->AuthorityGameMode && World->AuthorityGameMode->IsA(ATslZombieGameMode::StaticClass());
                const int SpawnLimit = ZombieSpawn ? (std::min)(1, S.SpawnPerTick) : S.SpawnPerTick;
                int Spawned = 0;
                while (GPendingLobbySpawns > 0 && Spawned < SpawnLimit)
                {
                    if (!SpawnBotNearPlayer(World)) break; // retry when the pawn/floor is ready
                    --GPendingLobbySpawns; ++Spawned;
                }
                if (T < GSpawnNotBefore) Spawned = SpawnLimit; // hold until the plane has everyone
                while (GPendingNearSpawns > 0 && Spawned < SpawnLimit)
                {
                    if (ZombieSpawn) {
                        if ((int)GBots.size() >= S.MaxBots || !SpawnBotNearPlayer(World)) break; // retry when there is capacity and safe ground
                        --GPendingNearSpawns; ++Spawned;
                    } else {
                        --GPendingNearSpawns; ++Spawned;
                        SpawnBotNearPlayer(World);
                    }
                }
                while (GPendingSpawns > 0 && Spawned < SpawnLimit)
                {
                    if (ZombieSpawn) {
                        if ((int)GBots.size() >= S.MaxBots || !SpawnRandomBot(World)) break; // keep requests until ground is available
                        --GPendingSpawns; ++Spawned;
                    } else {
                        --GPendingSpawns; ++Spawned;
                        SpawnRandomBot(World);
                    }
                }

                TryStartZombieGroundMatch(World,T);

                static bool bLoggedFirstThink = false;
                if (!bLoggedFirstThink)
                {
                    bLoggedFirstThink = true;
                    BLOG("First AI think OK (no crash so far)");
                }

                if (!GBots.empty())
                {
                    // Mark dead/removed pawns before all AI passes, not only
                    // inside UpdateBot: hunter and appearance passes use them.
                    for (FBot& B : GBots) {
                        if (B.State == EBotState::Dead || !B.Pawn) continue;
                        if (!LiveBotObject(B.Pawn, B.PawnIndex) || B.Pawn->Health <= 0.f)
                            EnterDead(B, T);
                    }
                    GatherCharacters(World);
                    CountEngaged();
                    // AimOffsets uses the native FRotator-degrees RPC. No format guessing.
                    //UpdateHumanAirborne(World, T);
                    ProcessShots(T);
                    AssignHunters(T);
                    SkinSelection::Tick(World);
                    DressPass();
                    AppearancePass();
                    BodyPass(T);

                    for (FBot& B : GBots)
                    {
                        // The boost is rechecked even when distant AI skips its think.
                        UpdateBotZoneBoost(World, B);
                        // AI LOD: full rate near players / in a fight, slower far away (spread so they don't all think on the same frame)
                        // Aircraft exits, active skydives and swimmers need timely updates.
                        const bool Urgent = B.State != EBotState::Dead && LiveBotObject(B.Pawn, B.PawnIndex) &&
                            (B.NativeSkyActive || B.Pawn->IsInAircraft() || B.Pawn->IsSwimming());
                        if (S.AiLod && GMatchSpawnDone && !Urgent && T < B.NextThinkAt && B.State != EBotState::Combat) continue;
                        UpdateBot(World, B, T);
                        // A synchronous damage event can destroy the pawn during UpdateBot.
                        if (B.State != EBotState::Dead && B.Pawn &&
                            (!LiveBotObject(B.Pawn, B.PawnIndex) || B.Pawn->Health <= 0.f))
                            EnterDead(B, T);
                        if (S.AiLod && GMatchSpawnDone && B.State != EBotState::Dead && B.Pawn)
                        {
                            float HD = 1e9f; FVector HL;
                            NearestHuman(B.Pawn->K2_GetActorLocation(), 1e9f, HD, HL);
                            const double Every = (B.State == EBotState::Combat || HD < 60000.f) ? 0.0 : HD < 150000.f ? 0.3 : 1.0;
                            B.NextThinkAt = T + Every * RandF(0.8f, 1.2f);
                        }
                    }
                    GBots.erase(std::remove_if(GBots.begin(), GBots.end(), [](const FBot& B) { return B.Remove; }), GBots.end());
                }
                else
                {
                    GatherCharacters(World);
                    SkinSelection::Tick(World);
                    DressPass();
                }
#ifdef PUBG2019_ENABLE_DIAGNOSTICS
                if (IsTrainingWorld(World) && GetEnvironmentVariableA("PUBG2019_TRAINING_AUDIT",nullptr,0)>0) {
                    static double NextReport=0; static bool DamageChecked=false;
                    auto* Mode=static_cast<ATslTrainingGameMode*>(World->AuthorityGameMode);
                    auto* State=Mode && Mode->GameState && Mode->GameState->IsA(ATslTrainingGameState::StaticClass()) ? static_cast<ATslTrainingGameState*>(Mode->GameState) : nullptr;
                    if(State && T>=NextReport){NextReport=T+10;BLOG("TRAINING AUDIT state="+Mode->MatchState.ToString()+" elapsed="+std::to_string(State->MatchElapsedTimeSec)+" limit="+std::to_string(State->TimeLimitSeconds)+" humans="+std::to_string(GChars.size()));}
                    if(!DamageChecked && GChars.size()>=2 && GChars[0].Char->Controller && GChars[1].Char->Controller){
                        auto* Victim=GChars[1].Char;float Before=Victim->Health;
                        float Applied=UGameplayStatics::ApplyDamage(Victim,5.f,GChars[0].Char->Controller,GChars[0].Char,TSubclassOf<UDamageType>(nullptr));
                        BLOG("TRAINING AUDIT damage before="+std::to_string(Before)+" after="+std::to_string(Victim->Health)+" applied="+std::to_string(Applied));DamageChecked=true;
                    }
                }
#endif
                if (IsTrainingWorld(World) && World->AuthorityGameMode && !GChars.empty()) {
                    auto* TrainingMode=static_cast<ATslTrainingGameMode*>(World->AuthorityGameMode);
                    if (TrainingMode->GameState && TrainingMode->GameState->IsA(ATslTrainingGameState::StaticClass()) &&
                        static_cast<ATslTrainingGameState*>(TrainingMode->GameState)->TimeLimitSeconds>0.f &&
                        TrainingMode->MatchState.ToString()=="WaitingToStart") {
                        static UWorld* LastTrainingStartWorld=nullptr;
                        if (LastTrainingStartWorld!=World) { LastTrainingStartWorld=World; BLOG("Training: first player arrived, starting match"); TrainingMode->StartMatch(); }
                    }
                }
                HumanRockRecoveryPass(World, T);
                SyncGameCounters(World);
                SyncZombieAircraftPassengerCount(World,T);
                EndGuardThink(World, T);
                LobbyHoldThink(World);

                /*
                if (GMatchSpawnDone)
                {
                    KickFromPlanes(World, T);
                }
                */
            }

            if (bSteer)
            {
                GLastSteer = T;
                SteerStep();
            }
        }
        else
        {
            BLOG("World is invalid");
        }

        const double TickMs = (Now() - T) * 1000.0;
        static double MaxTickMs = 0.0, NextStatusAt = 0.0;
        MaxTickMs = (std::max)(MaxTickMs, TickMs);
        if (GMatchSpawnDone && T >= NextStatusAt)
        {
            NextStatusAt = T + 5.0;
            BLOG("Runtime: humans=" + std::to_string(GHumansAlive) + " bots=" + std::to_string(GBotsAlive) +
                " teams=" + std::to_string(GTeamsAlive) + " max AI tick ms=" + std::to_string(MaxTickMs));
            MaxTickMs = 0.0;
        }
        GInTick = false;
    }
}


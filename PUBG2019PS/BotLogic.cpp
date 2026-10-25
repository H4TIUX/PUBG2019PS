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
#include "IniSettings.h"
#include "SDK.hpp"

#include "BotLogic.h"

#include "../minhook_src/hde/hde64.h"

using namespace SDK;

// =================================================================================
//  Settings  ([Bots] section of settings.ini - every key is optional)
// =================================================================================
struct FBotSettings
{
    bool        Enabled = true;
    int         Count = 20;      // bots spawned by "bots" / F6 / match start
    int         MaxBots = 99;
    bool        SpawnOnMatchStart = true;
    int         SpawnPerTick = 2;       // spawn throttle (per AI think)
    std::string PawnClass = "random"; // random = PlayerFemale_A_C / male class like real players | auto | a class name
    std::string ControllerClass = "AIController"; // AIController | TslAIController | TslBotAIController
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
    float       HitFalloffDist = 6000.f;  // cm - hit chance halves at this range (60 m), ~1/4 at 100 m, ~1/10 at 200 m
    float       LateZoneShare = 0.4f;    // share of bots that stay out in the blue until the gas gets close, then run in
    float       LateZoneMargin = 25000.f; // cm - late bots start running in when the gas is this close (250 m)
    float       HunterPickRange = 150000.f; // cm - hunters are picked from bots within this range of a player (no teleporting)     // share of a normal bot's roam picks that drift toward a player (0 = stay in their own area)
    int         MaxBotsPerPlayer = 1;       // most bots fighting one player at once (a bot that gets shot can always fight back)
    int         MaxResponders = 2;       // how many nearby bots come to check out a burst of gunfire
    float       ZoneLeg = 30000.f; // cm - how far a bot runs per leg when it is outside the safe zone
    bool        AvoidObstacles = true;    // probe ahead while walking: go around walls, hop small stuff, avoid water
    float       FovDegrees = 130.f;
    float       ReactionTime = 0.6f;    // seconds between first seeing someone and shooting
    bool        FightEachOther = true;

    bool        OwnHitRoll = true;    // bots roll their own hits + ApplyPointDamage
    float       Accuracy = 0.18f;   // hit chance per shot at point blank once fully "dialed in" (falls off with range)
    float       AccuracyStart = 0.03f;   // hit chance when a fight has just started
    float       AccuracyRampTime = 10.f;    // seconds of shooting at the same target to go from AccuracyStart to Accuracy
    int         WarningShots = 5;       // first shots of every new fight always miss (you hear / see them)
    bool        ScheduledHits = true;    // hits come in "windows": nothing lands until burst HitBurstMin..Max, then 1-2 hits, then a gap
    int         HitBurstMin = 6, HitBurstMax = 7;
    int         HitsPerWindowMin = 1, HitsPerWindowMax = 2;
    int         WindowGapMin = 3, WindowGapMax = 5;   // bursts between hit windows
    float       LongRangeDist = 10000.f; // cm - beyond this bots fire less: shorter bursts, longer pauses, later / fewer hits
    float       CloseFightDist = 4000.f;  // cm - bots this close to each other fight even during BotPeaceTime
    bool        ReturnFire = true;    // shot by a player -> an early hit window instead of waiting for burst 6-7
    int         ReturnFireHits = 1;       // hits in that early window (then the normal gap)
    int         ReturnFireBurstMin = 1, ReturnFireBurstMax = 2;   // 1 = the very next burst may hit
    float       EvadeTime = 4.f;     // seconds of dodging (crouch spam + side steps) after a player hits it
    bool        EvadeCrouch = true;
    float       MaxFireDistance = 20000.f; // cm - bots only shoot players closer than this (200 m); further away they close in first
    bool        FairFights = true;    // until shot at: one bot shoots a player at a time, only in the open, single taps
    float       PlayerLandingGrace = 10.f;    // seconds after a player lands before bots may shoot them (never while in the air)
    bool        CombatLog = true;    // log who bots spot / hit / why a nearby bot isn't shooting
    int         MaxHitsPerBurst = 1;       // never more hits than this in one burst
    float       MovingTargetFactor = 0.5f;    // hit chance x this when the target is running / driving
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

    bool        Hotkeys = true;
    bool        VerboseLog = false;
    bool        CountAsJoined = true;    // bump the lobby "JOINED" counter (TslGameState::NumJoinPlayers) per bot
    float       SeaLevelZ = -1e9f;   // ground below SeaLevelZ + 150 is never used for spawns; -1e9 = learn it
    std::string BotType = "ai";    // ai = AIController bots | player = CreatePlayer (CRASHES this build)
    bool        RegisterTeams = true;    // give AI bots a PUBG ATeam (+ GameMode Teams entry)
    bool        BotsInPlayerCount = true;    // keep JOINED / "X left" / alive teams = humans + bots
    bool        KeepMatchWhileBotsAlive = true; // don't let the game end the match while bots are still alive
    bool        HoldLobbyWhenEmpty = true; // lobby countdown stays at full until a real player is in the lobby
    float       EndWhenPlayersDead = 5.f;  // seconds after the last real player dies (or leaves) before the match ends (0 = off)
    float       ExitAfterMatch = 5.f;  // seconds after the match ends before the server process closes itself (0 = off)
    bool        NativeEndGuard = true;    // hook the game's own SetMatchState so solo + bots can't auto-finish
    bool        FixPlaneAltitude = true;    // raise the plane's flight path if it is below PlaneMinZ
    bool        SuperheroDrop = true;    // jump only over land, then land instantly (no freefall / chute / fall damage)
    float       JumpSpread = 90.f;    // once the plane is over land, each bot jumps within this many seconds
    float       DropSpread = 150000.f; // cm - land up to this far from the point under the plane (1.5 km)
    bool        ZoneWatch = true;    // log the blue zone's timing + keep it replicating promptly (blue zone snap debugging)
    float       VisibleFall = 5.f;     // seconds a bot is seen skydiving after jumping before it "vanishes" into its simulated drop
    float       HitCooldown = 2.5f;    // seconds after a bot hits you before it can hit you again
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
    bool PlayerBots() { return BotType == "player"; }

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
        S.Count = GetIniInt(B, "Count", S.Count);
        S.MaxBots = GetIniInt(B, "MaxBots", S.MaxBots);
        S.SpawnOnMatchStart = GetIniBool(B, "SpawnOnMatchStart", S.SpawnOnMatchStart);
        S.SpawnPerTick = (std::max)(1, GetIniInt(B, "SpawnPerTick", S.SpawnPerTick));
        S.PawnClass = GetIniString(B, "PawnClass", S.PawnClass.c_str());
        S.ControllerClass = GetIniString(B, "ControllerClass", S.ControllerClass.c_str());
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
        S.HitFalloffDist = GetIniFloat(B, "HitFalloffDist", S.HitFalloffDist);
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
        S.Accuracy = GetIniFloat(B, "Accuracy", S.Accuracy);
        S.AccuracyStart = GetIniFloat(B, "AccuracyStart", S.AccuracyStart);
        S.AccuracyRampTime = (std::max)(0.1f, GetIniFloat(B, "AccuracyRampTime", S.AccuracyRampTime));
        S.WarningShots = GetIniInt(B, "WarningShots", S.WarningShots);
        S.MaxFireDistance = GetIniFloat(B, "MaxFireDistance", S.MaxFireDistance);
        S.ScheduledHits = GetIniBool(B, "ScheduledHits", S.ScheduledHits);
        S.HitBurstMin = (int)GetIniFloat(B, "HitBurstMin", (float)S.HitBurstMin);
        S.HitBurstMax = (int)GetIniFloat(B, "HitBurstMax", (float)S.HitBurstMax);
        S.HitsPerWindowMin = (int)GetIniFloat(B, "HitsPerWindowMin", (float)S.HitsPerWindowMin);
        S.HitsPerWindowMax = (int)GetIniFloat(B, "HitsPerWindowMax", (float)S.HitsPerWindowMax);
        S.WindowGapMin = (int)GetIniFloat(B, "WindowGapMin", (float)S.WindowGapMin);
        S.WindowGapMax = (int)GetIniFloat(B, "WindowGapMax", (float)S.WindowGapMax);
        S.LongRangeDist = GetIniFloat(B, "LongRangeDist", S.LongRangeDist);
        S.CloseFightDist = GetIniFloat(B, "CloseFightDist", S.CloseFightDist);
        S.ReturnFire = GetIniBool(B, "ReturnFire", S.ReturnFire);
        S.ReturnFireHits = (int)GetIniFloat(B, "ReturnFireHits", (float)S.ReturnFireHits);
        S.ReturnFireBurstMin = (int)GetIniFloat(B, "ReturnFireBurstMin", (float)S.ReturnFireBurstMin);
        S.ReturnFireBurstMax = (int)GetIniFloat(B, "ReturnFireBurstMax", (float)S.ReturnFireBurstMax);
        S.EvadeTime = GetIniFloat(B, "EvadeTime", S.EvadeTime);
        S.EvadeCrouch = GetIniBool(B, "EvadeCrouch", S.EvadeCrouch);
        S.FairFights = GetIniBool(B, "FairFights", S.FairFights);
        S.PlayerLandingGrace = GetIniFloat(B, "PlayerLandingGrace", S.PlayerLandingGrace);
        S.CombatLog = GetIniBool(B, "CombatLog", S.CombatLog);
        S.MaxHitsPerBurst = GetIniInt(B, "MaxHitsPerBurst", S.MaxHitsPerBurst);
        S.MovingTargetFactor = GetIniFloat(B, "MovingTargetFactor", S.MovingTargetFactor);
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
        S.Hotkeys = GetIniBool(B, "Hotkeys", S.Hotkeys);
        S.VerboseLog = GetIniBool(B, "VerboseLog", S.VerboseLog);
        S.CountAsJoined = GetIniBool(B, "CountAsJoined", S.CountAsJoined);
        S.SeaLevelZ = GetIniFloat(B, "SeaLevelZ", -1e9f);
        S.BotType = GetIniString(B, "BotType", "ai");
        S.RegisterTeams = GetIniBool(B, "RegisterTeams", S.RegisterTeams);
        S.BotsInPlayerCount = GetIniBool(B, "BotsInPlayerCount", S.BotsInPlayerCount);
        S.KeepMatchWhileBotsAlive = GetIniBool(B, "KeepMatchWhileBotsAlive", S.KeepMatchWhileBotsAlive);
        S.EndWhenPlayersDead = GetIniFloat(B, "EndWhenPlayersDead", S.EndWhenPlayersDead);
        S.HoldLobbyWhenEmpty = GetIniBool(B, "HoldLobbyWhenEmpty", S.HoldLobbyWhenEmpty);
        S.ExitAfterMatch = GetIniFloat(B, "ExitAfterMatch", S.ExitAfterMatch);
        S.NativeEndGuard = GetIniBool(B, "NativeEndGuard", S.NativeEndGuard);
        S.FixPlaneAltitude = GetIniBool(B, "FixPlaneAltitude", S.FixPlaneAltitude);
        S.SuperheroDrop = GetIniBool(B, "SuperheroDrop", S.SuperheroDrop);
        S.JumpSpread = GetIniFloat(B, "JumpSpread", S.JumpSpread);
        S.DropSpread = GetIniFloat(B, "DropSpread", S.DropSpread);
        S.LandingGrace = GetIniFloat(B, "LandingGrace", S.LandingGrace);
        S.DropTimeMin = GetIniFloat(B, "DropTimeMin", S.DropTimeMin);
        S.VisibleFall = GetIniFloat(B, "VisibleFall", S.VisibleFall);
        S.ZoneWatch = GetIniBool(B, "ZoneWatch", S.ZoneWatch);
        S.HitCooldown = GetIniFloat(B, "HitCooldown", S.HitCooldown);
        S.ClaimHold = GetIniFloat(B, "ClaimHold", S.ClaimHold);
        S.DropTimeMax = GetIniFloat(B, "DropTimeMax", S.DropTimeMax);
        S.BotPeaceTime = GetIniFloat(B, "BotPeaceTime", S.BotPeaceTime);
        S.PlaneKickTime = GetIniFloat(B, "PlaneKickTime", S.PlaneKickTime);
        S.PlaneMinZ = GetIniFloat(B, "PlaneMinZ", S.PlaneMinZ);
        S.PlaneZ = GetIniFloat(B, "PlaneZ", S.PlaneZ);
        for (char& c : S.BotType) c = (char)std::tolower((unsigned char)c);
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

    bool ObjValid(const UObject* Obj, int32 Index)
    {
        /*
        if (!Obj || Index < 0)
            return false;
        FUObjectItem* Item = GetItem(Index);
        if (!Item || Item->Object != Obj)
            return false;
        const int32 Flags = *reinterpret_cast<const int32*>(&Item->Pad_8[0]);
        return (Flags & (kInternalPendingKill | kInternalUnreachable)) == 0;
        */
        if (!Obj || Index < 0)
            return false;

        return true;
    }

    bool ObjValid(const UObject* Obj) { return Obj && ObjValid(Obj, Obj->GetIndex()); }

    template<typename T>
    void SetWeak(TWeakObjectPtr<T>& Weak, const UObject* Obj)
    {
        Weak.ObjectIndex = 0;
        Weak.ObjectSerialNumber = 0;
        if (!ObjValid(Obj))
            return;
        FUObjectItem* Item = GetItem(Obj->GetIndex());
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

    std::mutex              GCmdMutex;
    std::deque<std::string> GCmdQueue;
    std::atomic<bool>       GHasCommands{ false };

    // =================================================================================
    //  Bot state
    // =================================================================================
    enum class EBotState : uint8 { Roam, Combat, Dead };

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
        EBotState       State = EBotState::Roam;
        bool            Remove = false;

        // combat
        ATslCharacter* Target = nullptr; int32 TargetIndex = -1;
        double          TargetSeenAt = 0, LastSeenTarget = 0;
        FVector         LastKnownTargetLoc{};
        bool            Firing = false;
        ATslWeapon_Gun* FiringGun = nullptr; int32 FiringGunIndex = -1;
        double          BurstEnd = 0, NextBurst = 0, LastShot = 0, NextStrafe = 0;
        double          AlertUntil = 0;
        float           LastHealth = -1.f;
        ATslCharacter* EngageTarget = nullptr; // who we've been shooting at
        double          EngageStart = 0, LastEngageShot = 0;
        int             ShotsThisEngage = 0, HitsThisBurst = 0;
        int             BurstNo = 0, HitBurst = 6, WindowHits = 0, WindowHitsMax = 1;   // scheduled hits
        bool            WindowRolled = false, WindowLive = false;
        int             BlockedBursts = 0;     // bursts skipped because the target is behind cover
        double          NextWhyLog = 0;
        double          LastHitPlayerAt = -100; // last time this bot landed a hit on a player
        double          SwimResetWindow = 0;    // start of the current run of "stuck swimming" put-backs
        double          LastHurtAt = -100;     // last time this bot took damage (provoked)
        ATslCharacter* ProvokedBy = nullptr;  // the player who last shot it
        double          ProvokedAt = -100;
        bool            ReturnFirePending = false;
        double          EvadeUntil = 0, NextCrouchFlip = 0;
        float           EvadeSide = 1.f;
        uint8           LastHitRep = 0;        // LastTakeHitInfo.EnsureReplicationByte last seen

        // movement
        FVector         Dest{};
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

        // arming
        bool            Armed = false;
        int             ArmStep = 0;
        int             ReequipTries = 0;
        double          NextArmTry = 0;

        double          DiedAt = 0;
        bool            CountedJoin = false;

        // water
        double          SwimSince = 0;
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
    };

    APawn*                     GPawnNearBots;
    bool                       GReady = false;
    bool                       GInTick = false;
    bool                       GFrozen = false;
    UWorld* GWorld = nullptr;
    std::vector<FBot>          GBots;
    std::vector<FCharInfo>     GChars;
    std::vector<FWeaponChoice> GWeapons;
    int                        GPendingSpawns = 0;
    int                        GPendingNearSpawns = 0;
    bool                       GMatchSpawnDone = false;
    bool                       GHasNavMesh = false;
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
    double                     GMatchStartedAt = 0;        // for BotPeaceTime
    double                     GNextKickTry = 0;
    bool                       GKickAnnounced = false;
    int                        GPendingLobbySpawns = 0;    // player bots created in the lobby (no teleport)
    bool                       GLobbySpawned = false;
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
    int                        GHumansAlive = 0, GBotsAlive = 0, GTeamsAlive = 0;
    ATslGameState* GCountGS = nullptr;  int32 GCountGSIndex = -1;
    bool                       GHaveCounts = false, GCountsInMatch = false;
    int32                      GWantJoined = 0, GWantAlive = 0, GWantTeams = 0;
    double                     GLastVetoLog = 0;
    std::vector<int32>         GPlanesSeen;                // aircraft we already logged / fixed
    double                     GNextPlaneCheck = 0;
    bool                       GChuteAltLogged = false;
    int                        GVetoCount = 0;

    AAIController* AIC(const FBot& B) { return B.IsPC ? nullptr : static_cast<AAIController*>(B.Ctrl); }

    // Player bots are removed through the engine (logout), AI bots are just destroyed
    void DestroyBotController(FBot& B)
    {
        if (!ObjValid(B.Ctrl, B.CtrlIndex))
            return;
        if (B.IsPC)
            UGameplayStatics::RemovePlayer(static_cast<APlayerController*>(B.Ctrl), false);
        else
            B.Ctrl->K2_DestroyActor();
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

    FVector GroundAt(UWorld* World, const FVector& P)
    {
        FVector G;
        if (TraceDown(World, P.X, P.Y, P.Z + 3000.f, P.Z - 6000.f, G))
            return G;
        return P;
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

    void GatherCharacters(UWorld* World)
    {
        GChars.clear();

        if (!World)
            return;

        UClass* CharClass = ATslCharacter::StaticClass();

        ATslGameMode* GM = (ATslGameMode*)UGameplayStatics::GetGameMode(UWorld::GetWorld());
        if (GM)
        {
            TArray<APawn*> Pawns;
            GM->GetAllPawns(&Pawns);
            if (Pawns.Num() > 0)
            {
                for (APawn* O : Pawns)
                {
                    if (O->IsA(CharClass))
                    {
                        ATslCharacter* C = static_cast<ATslCharacter*>(O);

                        if (C == UGameplayStatics::GetPlayerCharacter(UWorld::GetWorld(), 0)) continue;

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
            //BLOG("GameMode is invalid");
            return;
        }

        /*
      
        TUObjectArray* Arr = UObject::GObjects.GetTypedPtr();
        if (!Arr)
            return;

        const int32 Num = Arr->Num();
        for (int32 i = 0; i < Num; ++i)
        {
            UObject* O = Arr->GetByIndex(i);

            if (!O || O->IsDefaultObject())
                continue;

            if (O->IsA(CharClass))
            {
                ATslCharacter* C = static_cast<ATslCharacter*>(O);

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
        */
    }
    // Remote client first. The listen server always has its own local player pawn (the "ghost"),
    // which we only fall back to if nobody else is connected.
    ATslCharacter* FindRealPlayer(UWorld* World)
    {
		AGameStateBase* GS = World->AuthorityGameMode->GameState;
        for (APlayerState* PS : GS->PlayerArray)
        {
            ATslPlayerState* TslPS = (ATslPlayerState*)PS;
            if (!TslPS->GetOwningTslPlayerController()->IsLocalPlayerController())
            {
                return (ATslCharacter*)TslPS->GetOwningTslPlayerController()->K2_GetPawn();
            }
		}   
    }

    // a connected client (not the server's own local pawn)
    bool HasRemotePlayer(UWorld* World)
    {
        //GatherCharacters(World);
        /*
        for (const FCharInfo& CI : GChars)
        {
            if (CI.IsBot) continue;

            AController* C = CI.Char->Controller;
            if (C && ObjValid(C) && C->IsA(APlayerController::StaticClass()) && !static_cast<APlayerController*>(C)->bIsLocalPlayerController)
                return true;
            
        }
        */
        AGameStateBase* GS = World->AuthorityGameMode->GameState;
        for (APlayerState* PS : GS->PlayerArray)
        {
            ATslPlayerState* TslPS = (ATslPlayerState*)PS;
            if (!TslPS->GetOwningTslPlayerController()->IsLocalPlayerController())
            {
                return true;
            }
        }
        return false;
    }

    void DetectNavMesh(UWorld* World)
    {
        GHasNavMesh = false;
        /*
        if (World)
        {
            UClass* NavCls = ARecastNavMesh::StaticClass();

            TUObjectArray* Arr = UObject::GObjects.GetTypedPtr();
            if (!Arr)
                return;

            const int32 Num = Arr->Num();
            for (int32 i = 0; i < Num; ++i) {
                UObject* O = Arr->GetByIndex(i);
                if (!O || O->IsDefaultObject())
                    continue;

                if (O && O->IsA(NavCls)) { GHasNavMesh = true; break; }
            }
        }
        */
        BLOG(std::string("Navmesh in persistent level: ") + (GHasNavMesh ? "YES (pathfinding on)" : "NO (straight-line moves)"));
        if (!S.UseNavMesh)
            GHasNavMesh = false;
    }

    // Full navmesh report: the navigation system, every navigation-data object loaded anywhere (any level),
    // and the streaming levels. If a navmesh with data is found, bots switch to real pathfinding.
    void NavCheck(UWorld* World, bool bVerbose)
    {
        if (!World) return;
        UNavigationSystem* NS = World->NavigationSystem;
        std::string L = "navcheck: navigation system ";
        if (!NS || !ObjValid(NS)) L += "NONE (the world has no navigation system)";
        else
        {
            L += "yes | main nav data " + (NS->MainNavData && ObjValid(NS->MainNavData) ? NS->MainNavData->GetName() : std::string("none")) +
                " | registered " + std::to_string(NS->NavDataSet.Num()) + " | queued " + std::to_string(NS->NavDataRegistrationQueue.Num()) +
                " | invokers-only " + (NS->bGenerateNavigationOnlyAroundNavigationInvokers ? "yes" : "no");
        }
        BLOG(L);

        // every navigation data object in memory (any level, loaded or not)
        int Found = 0;
        ANavigationData* Best = nullptr;
            UClass* NDC = ANavigationData::StaticClass();
            for (int32 i = 0; i < UObject::GObjects->Num(); ++i)
            {
                UObject* O = UObject::GObjects->GetByIndex(i);
                if (!O || O->IsDefaultObject() || !O->IsA(NDC)) continue;
                ++Found;
                std::string Outer = O->GetOuter() ? O->GetOuter()->GetName() : std::string("-");
                std::string Pkg = O->GetOuter() && O->GetOuter()->GetOuter() ? O->GetOuter()->GetOuter()->GetName() : std::string("-");
                if (bVerbose || Found <= 8)
                    BLOG("navcheck:   " + O->GetClass()->GetName() + " " + O->GetName() + " in " + Outer + " (" + Pkg + ")");
                if (!Best && O->IsA(ARecastNavMesh::StaticClass())) Best = static_cast<ANavigationData*>(O);
            }
        BLOG("navcheck: " + std::to_string(Found) + " navigation data object(s) loaded");

        // streaming levels: anything that looks like navigation, and how many are loaded
        int Lv = World->StreamingLevels.Num(), Loaded = 0, NavNamed = 0;
        for (int i = 0; i < Lv; ++i)
        {
            ULevelStreaming* SL = World->StreamingLevels[i];
            if (!SL || !ObjValid(SL)) continue;
            if (SL->LoadedLevel) ++Loaded;
            const std::string N = SL->PackageNameToLoad.ToString() + " " + SL->GetName();
            if (Lower(N).find("nav") != std::string::npos)
            {
                ++NavNamed;
                BLOG("navcheck:   streaming level " + SL->GetName() + (SL->LoadedLevel ? " (loaded)" : " (NOT loaded)"));
            }
        }
        BLOG("navcheck: " + std::to_string(Lv) + " streaming levels, " + std::to_string(Loaded) + " loaded, " + std::to_string(World->Levels.Num()) +
            " levels in the world, " + std::to_string(NavNamed) + " with 'nav' in the name");

        // is there navmesh under a player / bot?
        FVector Probe{}; bool bProbe = false;
        for (const FCharInfo& CI : GChars) { Probe = CI.Loc; bProbe = true; if (!CI.IsBot) break; }
        if (bProbe && NS && ObjValid(NS) && (NS->MainNavData || Best))
        {
            const FVector P = UNavigationSystem::ProjectPointToNavigation(World, Probe, NS->MainNavData ? NS->MainNavData : Best,
                TSubclassOf<UNavigationQueryFilter>(nullptr), V(500.f, 500.f, 1000.f));
            const bool bOn = !(P.X == 0.f && P.Y == 0.f && P.Z == 0.f) && Len(P - Probe) < 1500.f;
            BLOG(std::string("navcheck: navmesh under ") + Pos(Probe) + ": " + (bOn ? "YES" : "no"));
            if (bOn && S.UseNavMesh && !GHasNavMesh)
            {
                GHasNavMesh = true;
                BLOG("navcheck: navmesh has data here - bots switch to pathfinding");
            }
        }
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

            if (!O || O->IsDefaultObject() || !O->IsA(WeaponItemCls)) continue;

            UWeaponItem* WI = static_cast<UWeaponItem*>(O);
            UClass* WC = WI->WeaponType;

            if (!WC || !WC->IsSubclassOf(GunCls))
                continue;

            GWeapons.push_back({ O->GetClass(), WC, O->GetClass()->GetName(), WC->GetName() });
        }

        BLOG("Found " + std::to_string(GWeapons.size()) + " gun item classes (type 'weapons' to list them)");
    }

    const FWeaponChoice* PickWeapon()
    {
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
        if (!OutWP || !ObjValid(OutWP))
        {
            OutWP = nullptr;
            return -1;
        }
        const int32 Num = OutWP->EquippedWeapons.Num();
        int32 GunIdx = -1;
        std::string List;
        for (int32 i = 0; i < Num; ++i)
        {
            ATslWeapon* W = OutWP->EquippedWeapons[i];
            if (!W || !ObjValid(W))
                continue;
            if (bLog) List += " [" + std::to_string(i) + "]" + W->GetClass()->GetName();
            if (GunIdx < 0 && W->IsA(ATslWeapon_Gun::StaticClass()))
                GunIdx = i;
        }
        if (bLog)
            BLOG(B.Name + " weapon slots:" + (List.empty() ? std::string(" none") : List) +
                " | in hands index " + std::to_string(OutWP->GetWeaponIndex()));
        return GunIdx;
    }

    void TryArm(FBot& B, double T)
    {
        ATslCharacter* Me = B.Pawn;
        if (Me->GetWeaponGun())
        {
            B.Armed = true;
            if (GWorkingArmStep < 0 && B.LastArmMethod >= 0)
            {
                GWorkingArmStep = B.LastArmMethod;
                static const char* Names[] = { "ServerArmWeapon", "ServerArmWeapon+anim", "SimulateArmWeapon", "OnWeapon_1" };
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
                if (W) { Me->GiveWeapon(TSubclassOf<ATslWeapon>(W->WeaponClass)); if (S.VerboseLog) BLOG(B.Name + " GiveWeapon(" + W->WeaponName + ")"); }
                break;
            case 1:
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
            case 2:
                Me->AdminSetUpWeapon();
                if (S.VerboseLog) BLOG(B.Name + " AdminSetUpWeapon()");
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
        case 0: WP->ServerArmWeapon(Slot, WP->GetWeaponIndex(), false, WP->GetOwner()->K2_GetActorLocation(), 0, 0); break;
        case 1: WP->ServerArmWeapon(Slot, WP->GetWeaponIndex(), true, WP->GetOwner()->K2_GetActorLocation(), 0, 0); break;
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
                    if (CI.IsBot || !ObjValid(CI.Char, CI.Index) || !CI.Char->CustomizableObjectInstance) continue;
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
            if (GPawnFromPlayer && GPawnClass && ObjValid(GPawnClass))
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
                    if (!ObjValid(C, C->GetIndex()) || IsBotChar(C)) continue;
                    AController* Ct = C->GetController();
                    if (!Ct || !ObjValid(Ct) || !Ct->IsA(APlayerController::StaticClass())) continue;
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
            if (GPawnClass && ObjValid(GPawnClass))
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

    UClass* GetControllerClass()
    {
        const std::string C = Lower(S.ControllerClass);
        if (C == "tslaicontroller")    return ATslAIController::StaticClass();
        if (C == "tslbotaicontroller") return ATslBotAIController::StaticClass();
        return AAIController::StaticClass();
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

    ATslGameMode* TslGM(UWorld* World)
    {
        AGameModeBase* GM = World ? World->AuthorityGameMode : nullptr;
        return (GM && GM->IsA(ATslGameMode::StaticClass())) ? static_cast<ATslGameMode*>(GM) : nullptr;
    }

    void DumpTeams(UWorld* World, const std::string& Why)
    {
        ATslGameMode* GM = TslGM(World);
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
                if (!T || !ObjValid(T)) { Line += " [invalid]"; continue; }
                Line += " [" + T->PlayerName.ToString() + " hp " + std::to_string((int)T->Health) +
                    (T->bIsDying ? " DYING" : "") + (T->bQuitter ? " QUIT" : "") +
                    (T->TslCharacter && ObjValid(T->TslCharacter) ? " char" : " nochar") + "]";
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

        ATslGameMode* GM = TslGM(World);
        if (!GM || !B.Ctrl || !B.Ctrl->PlayerState || !B.Ctrl->PlayerState->IsA(ATslPlayerState::StaticClass()))
            return;

        const int32 TeamNum = static_cast<ATslPlayerState*>(B.Ctrl->PlayerState)->TeamNumber;

        const FTransform Xf = MakeXf(B.Pawn->K2_GetActorLocation(), 0.f);
        AActor* A = UGameplayStatics::BeginDeferredActorSpawnFromClass(World, ATeam::StaticClass(), Xf, ESpawnActorCollisionHandlingMethod::AlwaysSpawn, B.Ctrl);
         
        if (!A)
        {
            BLOG(B.Name + ": could not spawn its ATeam actor");
            return;
        }
        ATeam* Tm = static_cast<ATeam*>(A);
        Tm->TslCharacter = B.Pawn;
        Tm->Health = 100; Tm->HealthMax = 100;
        Tm->GroggyHealth = 100; Tm->GroggyHealthMax = 100;
        Tm->MemberIndex = 1;
        SetGameString(&Tm->PlayerName, B.Name);
        SetGameString(&Tm->UniqueId, "BOT-" + std::to_string(B.Id));
        UGameplayStatics::FinishSpawningActor(A, Xf);
		uint64 EncryptedTeam = EncodeTeam((uint64)Tm);
        B.Pawn->Team = EncryptedTeam;
        ATeam* DecodedTeam = (ATeam*)DecodeTeam(EncryptedTeam);
        CUSTOMLOG("Decrypted team: " + DecodedTeam->GetName());
        B.TeamActor = Tm;
        B.TeamActorIndex = Tm->GetIndex();
        // new FTeams { TeamNum, Members = [Tm] } appended to GameMode.Teams
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
        if (ATslGameMode* GM = TslGM(World))
        {
            FRawArray* Arr = reinterpret_cast<FRawArray*>(&GM->Teams);
            FTeams* E = reinterpret_cast<FTeams*>(Arr->Data);
            for (int32 i = 0; E && i < Arr->Num; ++i)
            {
                FRawArray* M = reinterpret_cast<FRawArray*>(&E[i].Members);
                if (M->Num == 1 && M->Data && static_cast<ATeam**>(M->Data)[0] == Tm)
                {
                    if (i < Arr->Num - 1)
                        std::memmove(&E[i], &E[i + 1], (size_t)(Arr->Num - 1 - i) * sizeof(FTeams));
                    Arr->Num -= 1;
                    break;
                }
            }
        }
        if (ObjValid(B.Pawn, B.PawnIndex)) B.Pawn->Team = EncodeTeam(0);
        if (ObjValid(Tm, B.TeamActorIndex)) Tm->K2_DestroyActor();
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

    // A real player on the server (like the server's own local player): normal login, counted by the
    // game, seated in the plane. Tele = where to put it once its pawn exists (nullptr = where the game puts it).
    bool SpawnPlayerBotAt(UWorld* World, const FVector* Tele)
    {
        if ((int)GBots.size() >= S.MaxBots)
        {
            BLOG("MaxBots reached (" + std::to_string(S.MaxBots) + ")");
            return false;
        }
        EnsureLocalPlayerCap();
        APlayerController* PC = UGameplayStatics::CreatePlayer(World, -1, true);
        if (!PC || !ObjValid(PC))
        {
            BLOG("CreatePlayer failed - local player limit reached?");
            return false;
        }
        FBot B;
        B.IsPC = true;
        B.Id = GNextBotId++;
        B.Name = S.NamePrefix + std::to_string(B.Id);
        B.Ctrl = PC;
        B.CtrlIndex = PC->GetIndex();
        B.UseSteer = true; // no path following for player controllers - always input steering
        const double T = Now();
        B.NextArmTry = T + 1.0;
        B.NextRoamPick = T + RandF(0.5f, 2.0f);
        B.ProgressCheckAt = T + 2.0;
        B.SpawnedAt = T;
        B.PawnWaitUntil = T + 15.0;
        if (Tele) { B.HasTeleport = true; B.TeleportTo = *Tele; }
        if (AGameModeBase* GM = UGameplayStatics::GetGameMode(World))
        {
            std::wstring WName(B.Name.begin(), B.Name.end());
            GM->ChangeName(PC, FString(WName.c_str()), false);
        }
        GBots.push_back(B);
        BLOG("Created player bot " + B.Name);
        return true;
    }

    bool SpawnBotAt(UWorld* World, FVector Loc)
    {
        if (S.PlayerBots())
            return SpawnPlayerBotAt(World, &Loc);

        if ((int)GBots.size() >= S.MaxBots)
        {
            BLOG("MaxBots reached (" + std::to_string(S.MaxBots) + ")");
            return false;
        }
        UClass* PawnCls = GetPawnClass(World); // original
        //UClass* PawnCls = Get_PlayerPawn_Class();
        UClass* CtrlCls = GetControllerClass();
        if (!PawnCls || !CtrlCls)
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
                        if (Other && Other != PS && ObjValid(Other) && Other->IsA(ATslPlayerState::StaticClass()))
                            MaxTeam = (std::max)(MaxTeam, static_cast<ATslPlayerState*>(Other)->TeamNumber);
                int32 Team;
                if (S.FightEachOther)
                    Team = MaxTeam + 1;
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

        RegisterBotTeam(World, B);

        GBots.push_back(B);
        if (S.VerboseLog)
            BLOG("Spawned " + B.Name + " at " + std::to_string((int)Loc.X) + "," + std::to_string((int)Loc.Y) + "," + std::to_string((int)Loc.Z));
        return true;
    }

    bool SpawnRandomBot(UWorld* World)
    {
        FVector Loc;
        if (!FindRandomSpawn(World, Loc))
        {
            // Fall back to near a player if the random box gave nothing (levels not streamed in?)
            if (ATslCharacter* P = FindRealPlayer(World))
            {
                FVector Base = P->K2_GetActorLocation();
                const float A = RandF(0.f, 2.f * kPi), R = RandF(3000.f, 8000.f);
                Loc = GroundAt(World, V(Base.X + std::cos(A) * R, Base.Y + std::sin(A) * R, Base.Z));
            }
            else
            {
                BLOG("No valid spawn point found (ground trace missed 25x and no player to spawn near)");
                return false;
            }
        }
        return SpawnBotAt(World, Loc);
    }

    bool SpawnBotNearPlayer(UWorld* World)
    {
        ATslCharacter* P = FindRealPlayer(World);
        if (!P)
        {
            BLOG("No real player pawn found to spawn near - spawning at a random point instead");
            return SpawnRandomBot(World);
        }
        const FVector Base = P->K2_GetActorLocation();
        for (int i = 0; i < 10; ++i)
        {
            const float A = RandF(0.f, 2.f * kPi), R = RandF(1500.f, 3000.f);
            FVector G;
            if (TraceDown(World, Base.X + std::cos(A) * R, Base.Y + std::sin(A) * R, Base.Z - 200.f + 3000.f, -30000.f, G) && !BelowSea(G.Z))
                return SpawnBotAt(World, G);
        }
        return SpawnRandomBot(World);
    }

    // =================================================================================
    //  Bot behaviour
    // =================================================================================
    bool IsBotPS(const APlayerState* PS)
    {
        if (!PS) return false;
        for (const FBot& B : GBots)
            if (B.Ctrl && ObjValid(B.Ctrl, B.CtrlIndex) && B.Ctrl->PlayerState == PS) return true;
        return false;
    }

    std::string CharName(ATslCharacter* C)
    {
        if (!C || !ObjValid(C)) return "?";
        for (const FBot& B : GBots) if (B.Pawn == C) return B.Name;
        if (C->GetTslPlayerState() && ObjValid(C->GetTslPlayerState())) return C->GetTslPlayerState()->PlayerName.ToString();
        return "a player";
    }

    // ---------------------------------------------------------------- replicated aim (AimOffsets)
    // Clients pose a character's gun from ATslCharacter::AimOffsets. Real clients send it every frame;
    // bots never did, so their guns pointed off to the side. Learn from a real player whether the
    // value is a world direction or relative to the body, then write it for bots.
    int    GAimMode = 3;          // 3 = (pitch/180, world yaw/180, 0) [seen from real players], 1 = local dir, 2 = world dir
    bool   GAimLearned = false;
    int    GAimVotesRot = 0, GAimVotesLocal = 0, GAimVotesWorld = 0, GAimVotesOther = 0, GAimSamplesLogged = 0;

    FVector RotToVec(float Pitch, float Yaw)
    {
        const float P = Pitch * kPi / 180.f, Y = Yaw * kPi / 180.f;
        return V(std::cos(P) * std::cos(Y), std::cos(P) * std::sin(Y), std::sin(P));
    }
    float NormAngle(float A) { while (A > 180.f) A -= 360.f; while (A < -180.f) A += 360.f; return A; }

    // confirm the format from real players (it only changes what bots write if players clearly use another one)
    void LearnAimMode()
    {
        if (GAimLearned) return;
        for (const FCharInfo& CI : GChars)
        {
            if (CI.IsBot) continue;
            AController* C = CI.Char->GetController();
            if (!C || !ObjValid(C) || !C->IsA(APlayerController::StaticClass())) continue;
            if (static_cast<APlayerController*>(C)->bIsLocalPlayerController) continue;
            //const FVector A = static_cast<FVector&>(CI.Char->AimOffsets);
            const FVector A = reinterpret_cast<FVector&>(CI.Char->AimOffsets);
            if (A.X == 0.f && A.Y == 0.f && A.Z == 0.f) continue;
            const FRotator CR = C->ControlRotation;
            const float Pitch = NormAngle(CR.Pitch), Yaw = NormAngle(CR.Yaw);
            const float BodyYaw = CI.Char->K2_GetActorRotation().Yaw;
            float DY = std::fabs(A.Y - Yaw / 180.f); if (DY > 1.f) DY = 2.f - DY;   // wrap at +-180
            if (std::fabs(A.X - Pitch / 180.f) < 0.03f && DY < 0.03f && std::fabs(A.Z) < 0.05f) { ++GAimVotesRot; continue; }
            const FVector W = RotToVec(Pitch, Yaw);
            const FVector L = RotToVec(Pitch, NormAngle(Yaw - BodyYaw));
            if (W.Dot(L) > 0.8f) continue;
            if (A.Dot(W) > 0.95f) ++GAimVotesWorld;
            else if (A.Dot(L) > 0.95f) ++GAimVotesLocal;
            else
            {
                ++GAimVotesOther;
                if (GAimSamplesLogged < 3)
                {
                    ++GAimSamplesLogged;
                    char Buf[200];
                    snprintf(Buf, sizeof(Buf), "Aim sample: AimOffsets %.2f %.2f %.2f | control pitch %.0f yaw %.0f | body yaw %.0f",
                        A.X, A.Y, A.Z, Pitch, Yaw, BodyYaw);
                    BLOG(Buf);
                }
            }
        }
        const int Total = GAimVotesRot + GAimVotesWorld + GAimVotesLocal;
        if (Total >= 20)
        {
            GAimLearned = true;
            if (GAimVotesRot >= GAimVotesWorld && GAimVotesRot >= GAimVotesLocal) GAimMode = 3;
            else GAimMode = GAimVotesWorld > GAimVotesLocal ? 2 : 1;
            BLOG(std::string("Aim: player aim data confirmed as ") +
                (GAimMode == 3 ? "pitch/yaw angles" : GAimMode == 2 ? "a world direction" : "a body-relative direction") +
                " (" + std::to_string(GAimVotesRot) + " angles / " + std::to_string(GAimVotesWorld) + " world / " +
                std::to_string(GAimVotesLocal) + " local / " + std::to_string(GAimVotesOther) + " other) - bot guns use the same");
        }
    }

    // aim the bot's gun (as clients see it) along Pitch/Yaw (world)
    void SetBotAim(ATslCharacter* Me, float Pitch, float Yaw)
    {
        FVector D;
        if (GAimMode == 3) D = V((std::clamp)(NormAngle(Pitch), -89.f, 89.f) / 180.f, NormAngle(Yaw) / 180.f, 0.f);
        else if (GAimMode == 2) D = RotToVec(Pitch, Yaw);
        else D = RotToVec(Pitch, NormAngle(Yaw - Me->K2_GetActorRotation().Yaw));
        //static_cast<FVector&>(Me->AimOffsets) = D;
        reinterpret_cast<FVector&>(Me->AimOffsets) = D;
    }

    void StopFire(FBot& B)
    {
        if (B.Firing && B.FiringGun && ObjValid(B.FiringGun, B.FiringGunIndex))
        {
            B.FiringGun->ServerStopFire();
            if (S.ShowBotShots) B.FiringGun->ClientStopSimulatingWeaponFire();
        }
        B.Firing = false;
        B.FiringGun = nullptr;
        B.FiringGunIndex = -1;
    }

    void SetSprint(FBot& B, bool bOn)
    {
        if (!S.Sprint) bOn = false;
        if (B.Sprinting == bOn) return;
        B.Pawn->ServerSetSprinting(bOn);
        B.Sprinting = bOn;
    }

    void MoveTo(UWorld* World, FBot& B, FVector Dest, double T, bool bStrafe)
    {
        Dest = GroundAt(World, Dest);
        B.Dest = Dest;
        B.HasDest = true;
        B.DestSetAt = T;
        if (B.UseSteer)
            return;
        const EPathFollowingRequestResult R = AIC(B)->MoveToLocation(Dest, S.AcceptRadius, false,
            GHasNavMesh, GHasNavMesh, bStrafe, TSubclassOf<UNavigationQueryFilter>(nullptr), true);
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
        return Ct && ObjValid(Ct) && Ct->IsA(APlayerController::StaticClass()) &&
            static_cast<APlayerController*>(Ct)->bIsLocalPlayerController;
    }

    ATslCharacter* NearestHuman(const FVector& Loc, float MaxDist, float& OutDist, FVector& OutLoc)
    {
        ATslCharacter* Best = nullptr;
        OutDist = MaxDist;
        for (const FCharInfo& C : GChars)
        {
            if (C.IsBot || IsGhostChar(C.Char)) continue;
            const float D = Len(C.Loc - Loc);
            if (D < OutDist) { OutDist = D; OutLoc = C.Loc; Best = C.Char; }
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
        if (S.HuntPlayers && GMatchSpawnDone && RandF(0.f, 1.f) < (B.Hunter ? S.HuntChance : S.DriftChance))
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

        if (GHasNavMesh)
        {
            FVector P = UNavigationSystem::GetRandomReachablePointInRadius(World, Center, Radius, nullptr,
                TSubclassOf<UNavigationQueryFilter>(nullptr));
            if (!P.IsZero())
                return P;
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
        APlayerState* PS = (B.Ctrl && ObjValid(B.Ctrl, B.CtrlIndex)) ? B.Ctrl->PlayerState : nullptr;
        if (PS && ObjValid(PS) && PS->IsA(ATslPlayerState::StaticClass()))
            SetWeak(H.PlayerInstigator, PS);
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

    bool RollHit(FBot& B, ATslWeapon_Gun* Gun, ATslCharacter* Target, const FVector& MyLoc, const FVector& TLoc, float Dist)
    {
        const double Tn = Now();
        if (!GGunDamageResolved) ResolveGunDamageType();
        if (Tn - B.LastHitPlayerAt < S.HitCooldown && !IsBotChar(Target)) return false;   // a breather between hits
        const FVector Vel = Target->GetVelocity();
        const bool bMoving = Vel.X * Vel.X + Vel.Y * Vel.Y > 350.f * 350.f; // faster than a jog
        if (S.ScheduledHits)
        {
            // misses until the scheduled burst, then a "window" of 1-2 hits spread over the burst(s)
            if (B.BurstNo < B.HitBurst) return false;
            if (B.WindowHits >= B.WindowHitsMax) return false;
            if (!B.WindowRolled)
            {
                B.WindowRolled = true;
                // long range / moving target: the window may come to nothing
                const float FR = Dist / (std::max)(S.LongRangeDist * 1.5f, 100.f);
                float WChance = Dist > S.LongRangeDist ? 1.f / (1.f + FR * FR) * 2.f : 1.f;   // 150 m: ~1, 250 m: ~0.5
                if (bMoving) WChance *= 0.6f;
                B.WindowLive = RandF(0.f, 1.f) < (std::min)(WChance, 1.f);
            }
            if (!B.WindowLive) return false;
            if (RandF(0.f, 1.f) > 0.45f) return false;   // spread the hit(s) across the burst
        }
        else
        {
            if (B.ShotsThisEngage < S.WarningShots)      return false; // warning shots
            if (B.HitsThisBurst >= S.MaxHitsPerBurst)    return false;
            const float FR = Dist / (std::max)(S.HitFalloffDist, 100.f);
            const float Falloff = 1.f / (1.f + FR * FR);   // 60 m: 1/2, 100 m: ~1/4, 200 m: ~1/10
            const float Ramp = (std::clamp)((float)((Tn - B.EngageStart) / S.AccuracyRampTime), 0.f, 1.f);
            float Chance = (S.AccuracyStart + (S.Accuracy - S.AccuracyStart) * Ramp) * Falloff;
            if (bMoving) Chance *= S.MovingTargetFactor;
            if (RandF(0.f, 1.f) > Chance)
                return false;
        }
        ++B.HitsThisBurst;

        CacheBones();
        const FVector From = MyLoc + V(0.f, 0.f, 60.f);
        // only body parts the bot can actually see can be hit - fully behind cover = a guaranteed miss
        FVector HeadP = TLoc + V(0.f, 0.f, 70.f), BodyP = TLoc + V(0.f, 0.f, 20.f);
        if (Target->Mesh && ObjValid(Target->Mesh))
        {
            HeadP = Target->Mesh->GetSocketLocation(GHeadBone);
            BodyP = Target->Mesh->GetSocketLocation(GSpineBone);
        }
        const bool bSeeHead = ClearShot(B.Pawn, Target, From, HeadP);
        const bool bSeeBody = ClearShot(B.Pawn, Target, From, BodyP);
        if (!bSeeHead && !bSeeBody)
        {
            --B.HitsThisBurst;
            if (S.CombatLog&& GCoverMisses < 20)
            {
                ++GCoverMisses;
                BLOG(B.Name + " would have hit " + CharName(Target) + " but they're behind cover - miss");
            }
            return false;
        }
        const bool bHeadOk = S.HeadshotChance > 0.f;   // HeadshotChance=0: bots never hit heads
        if (!bSeeBody && !bHeadOk)
        {
            --B.HitsThisBurst;
            return false;                               // only their head shows - no shot for us
        }
        const bool bHead = bHeadOk && bSeeHead && (!bSeeBody || RandF(0.f, 1.f) < S.HeadshotChance);
        const float Dmg = S.Damage * (bHead ? S.HeadshotMultiplier : 1.f);
        const FVector To = bHead ? HeadP : BodyP;
        const FVector Dir = SafeNormal(To - From);

        FHitResult Hit{};
        Hit.bBlockingHit = 1;
        Hit.Time = 1.f;
        Hit.Distance = Dist;
        static_cast<FVector&>(Hit.Location) = To;
        static_cast<FVector&>(Hit.ImpactPoint) = To;
        static_cast<FVector&>(Hit.Normal) = Dir * -1.f;
        static_cast<FVector&>(Hit.ImpactNormal) = Dir * -1.f;
        static_cast<FVector&>(Hit.TraceStart) = From;
        static_cast<FVector&>(Hit.TraceEnd) = To + Dir * 100.f;
        Hit.Item = -1;
        Hit.FaceIndex = -1;
        Hit.BoneName = bHead ? GHeadBone : GSpineBone;
        SetWeak(Hit.Actor, Target);
        if (Target->Mesh)
            SetWeak(Hit.Component, Target->Mesh);

        const float Before = Target->Health;
        const uint8 RepBefore = Target->LastTakeHitInfo.EnsureReplicationByte;
        APlayerState* VPS = Target->GetTslPlayerState();
        ATslPlayerState* VTPS = (VPS && ObjValid(VPS) && VPS->IsA(ATslPlayerState::StaticClass())) ? static_cast<ATslPlayerState*>(VPS) : nullptr;
        const float LastHitBefore = VTPS ? VTPS->LastHitTime : 0.f;
        UGameplayStatics::ApplyPointDamage(Target, Dmg, Dir, Hit, B.Ctrl, Gun, GDamageType);
        const float After = ObjValid(Target) ? Target->Health : 0.f;
        if (After < Before && !IsBotChar(Target)) B.LastHitPlayerAt = Tn;
        if (After < Before && IsBotChar(Target)) GBotHurtByBotAt[Target->GetIndex()] = Tn;
        if (S.ScheduledHits && ++B.WindowHits >= B.WindowHitsMax)
        {
            B.HitBurst = B.BurstNo + RandI((std::max)(1, S.WindowGapMin), (std::max)(S.WindowGapMin, S.WindowGapMax));
            B.WindowHits = 0;
            B.WindowHitsMax = RandI((std::max)(1, S.HitsPerWindowMin), (std::max)(S.HitsPerWindowMin, S.HitsPerWindowMax));
            B.WindowRolled = false;
        }
        // "attacked from here": the player state's attack marker (whatever UI this build drives from it)
        if (VTPS && ObjValid(VTPS) && After < Before)
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
        if (ObjValid(Target) && After < Before && Target->LastTakeHitInfo.EnsureReplicationByte == RepBefore)
            ReplicateHitInfo(B, Target, Before - After, From, To, Hit.BoneName, After <= 0.f);

        if (After < Before) ++GHitsApplied;
        else ++GHitsNoEffect;
        if (S.CombatLog && !IsBotChar(Target))
            BLOG(B.Name + " HIT " + CharName(Target) + (bHead ? " in the head" : "") + " for " + std::to_string((int)(Before - After)) +
                " (" + std::to_string((int)(Dist / 100.f)) + " m, hp " + std::to_string((int)After) + " left)");
        if (!GWarnedNoDamage && GHitsApplied == 0 && GHitsNoEffect >= 15)
        {
            GWarnedNoDamage = true;
            BLOG("WARNING: 15 bot hits did no damage. The game may be rejecting it via CanDealDamage/teams, "
                "or wants a TSL damage type. Try [Bots] DamageTypeClass= (see README) or ControllerClass=TslAIController.");
        }
        return true;
    }

    // tell every client "this gun fired toward To" - muzzle flash, sound, tracer (cosmetic only)
    void SimShot(ATslWeapon_Gun* Gun, const FVector& From, const FVector& To)
    {
        if (!S.ShowBotShots || !Gun || !ObjValid(Gun))
            return;
        const FVector Dir = SafeNormal(To - From);
        FHitResult Hit{};
        Hit.bBlockingHit = 1;
        Hit.Time = 1.f;
        Hit.Distance = Len(To - From);
        static_cast<FVector&>(Hit.Location) = To;
        static_cast<FVector&>(Hit.ImpactPoint) = To;
        static_cast<FVector&>(Hit.Normal) = Dir * -1.f;
        static_cast<FVector&>(Hit.ImpactNormal) = Dir * -1.f;
        static_cast<FVector&>(Hit.TraceStart) = From;
        static_cast<FVector&>(Hit.TraceEnd) = To;
        Hit.Item = -1;
        Hit.FaceIndex = -1;
        Gun->ClientSimulateWeaponFire(/*Hit*/);
    }

    FVector ShotAim(const FVector& TLoc, bool bHit)
    {
        if (bHit)
            return TLoc + V(RandF(-20.f, 20.f), RandF(-20.f, 20.f), RandF(0.f, 70.f));
        // near miss: 1.5-4 m around the target, often low so it kicks up the ground
        const float A = RandF(0.f, 2.f * kPi), D = RandF(150.f, 400.f);
        return TLoc + V(std::cos(A) * D, std::sin(A) * D, RandF(-90.f, 160.f));
    }

    // one shot: roll it, then show it to the clients aimed where it actually went
    void TakeShot(FBot& B, ATslWeapon_Gun* Gun, ATslCharacter* Target, const FVector& MyLoc, const FVector& TLoc, float Dist)
    {
        bool bHit = false;
        if (S.OwnHitRoll && ObjValid(Target) && Target->Health > 0.f)
            bHit = RollHit(B, Gun, Target, MyLoc, TLoc, Dist);
        ++B.ShotsThisEngage;
        B.LastEngageShot = Now();
        SimShot(Gun, MyLoc + V(0.f, 0.f, 60.f), ShotAim(TLoc, bHit));
    }

    std::unordered_map<int32, std::pair<int, double>> GShooterSlot; // target char index -> (bot id, last shot time)

    void FireLogic(FBot& B, ATslWeapon_Gun* Gun, ATslCharacter* Target, const FVector& MyLoc, const FVector& TLoc, float Dist, double T, bool bCalm = false)
    {
        //if (S.InfiniteAmmo && Gun->GetCurrentAmmoInClip() < 2)
            //Gun->CurrentAmmoInClip = Gun->AmmoPerClip > 0 ? Gun->AmmoPerClip : 30;

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
                B.HitBurst = RandI(S.HitBurstMin, (std::max)(S.HitBurstMin, S.HitBurstMax)) + (Dist > S.LongRangeDist * 1.5f ? 2 : 0);
                B.WindowHits = 0;
                B.WindowHitsMax = RandI((std::max)(1, S.HitsPerWindowMin), (std::max)(S.HitsPerWindowMin, S.HitsPerWindowMax));
                B.WindowRolled = false;
                B.BlockedBursts = 0;
            }
            // only open fire when a bullet could actually reach them - no endless shooting into cover
            {
                CacheBones();
                FVector HeadP = TLoc + V(0.f, 0.f, 70.f), BodyP = TLoc + V(0.f, 0.f, 20.f);
                if (Target->Mesh && ObjValid(Target->Mesh))
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
            // shot by this player -> one early, small hit window (no stacking on a window that's already open)
            if (B.ReturnFirePending && Target == B.ProvokedBy)
            {
                B.ReturnFirePending = false;
                if (S.ScheduledHits && T - B.ProvokedAt < 10.0)
                {
                    const int Want = B.BurstNo + RandI((std::max)(1, S.ReturnFireBurstMin), (std::max)(S.ReturnFireBurstMin, S.ReturnFireBurstMax));
                    if (Want < B.HitBurst)
                    {
                        B.HitBurst = Want;
                        B.WindowHits = 0;
                        B.WindowHitsMax = (std::max)(1, S.ReturnFireHits);
                        // close / mid range: it will connect; far away it still rolls the normal long-range chance
                        B.WindowRolled = Dist <= S.LongRangeDist * 1.5f;
                        B.WindowLive = B.WindowRolled;
                        if (S.CombatLog) BLOG(B.Name + " returns fire at " + CharName(Target) + " (hit chance in burst " + std::to_string(Want) + ")");
                    }
                }
            }
            // a window that came to nothing moves on to a later burst
            if (S.ScheduledHits && B.WindowRolled && !B.WindowLive)
            {
                B.HitBurst = B.BurstNo + RandI((std::max)(1, S.WindowGapMin), (std::max)(S.WindowGapMin, S.WindowGapMax));
                B.WindowRolled = false;
            }
            ++B.BurstNo;
            B.HitsThisBurst = 0;
            Gun->ServerStartFire();
            B.Firing = true;
            B.FiringGun = Gun;
            B.FiringGunIndex = Gun->GetIndex();
            const bool bFar = Dist > S.LongRangeDist;
            B.BurstEnd = T + ((bCalm || bFar) ? RandF(0.05f, 0.25f) : RandF(S.BurstMin, S.BurstMax)); // calm / far = 1-3 rounds
            B.LastShot = T;
            TakeShot(B, Gun, Target, MyLoc, TLoc, Dist); // first round of the burst
            return;
        }

        float TBS = Gun->WeaponGunData->WeaponGunConfig.TimeBetweenShots;
        if (!(TBS > 0.03f) || TBS > 3.f)
            TBS = 0.1f;
        int Shots = 0;
        while (T - B.LastShot >= TBS && Shots < 6)
        {
            B.LastShot += TBS;
            ++Shots;
            TakeShot(B, Gun, Target, MyLoc, TLoc, Dist);
        }

        if (T >= B.BurstEnd)
        {
            StopFire(B);
            // further away = fewer bursts: up to 3x the pause at 3x LongRangeDist
            const float RangeMul = 1.f + 2.f * (std::clamp)((Dist - S.LongRangeDist) / (std::max)(S.LongRangeDist * 2.f, 100.f), 0.f, 1.f);
            B.NextBurst = T + (bCalm ? RandF(1.5f, 3.5f) : RandF(S.BurstPauseMin, S.BurstPauseMax)) * RangeMul;
        }
    }

    void EnterDead(FBot& B, double T)
    {
        StopFire(B);
        if (ObjValid(B.Ctrl, B.CtrlIndex))
        {
            if (AIC(B)) AIC(B)->K2_ClearFocus();
            B.Ctrl->StopMovement();
        }
        B.State = EBotState::Dead;
        B.DiedAt = T;
        if (B.TeamActor && ObjValid(B.TeamActor, B.TeamActorIndex))
        {
            B.TeamActor->Health = 0;
            B.TeamActor->bIsDying = 1;
        }
        B.Target = nullptr;
        if (S.VerboseLog) BLOG(B.Name + " died");
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
                if (ObjValid(B.Target, B.TargetIndex) && !IsBotChar(B.Target) && B.PawnIndex >= 0)
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
            if (C.Char == B.Pawn)
                continue;
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

    void UpdateBot(UWorld* World, FBot& B, double T)
    {
        if (!ObjValid(B.Ctrl, B.CtrlIndex))
        {
            CUSTOMLOG("UpdateBot: Pawn's controller is invalid");
            B.Remove = true;
            return;
        }

        if (B.IsPC && B.State != EBotState::Dead)
        {
            // player bots: the game spawns the pawn (now or a bit later) - pick it up / follow changes
            APawn* Cur = B.Ctrl->K2_GetPawn();
            if (Cur && Cur != B.Pawn && ObjValid(Cur) && Cur->IsA(ATslCharacter::StaticClass()))
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

        //const bool bPawnOk = ObjValid(B.Pawn, B.PawnIndex);
        //if (B.State != EBotState::Dead && (!bPawnOk || B.Pawn->Health <= 0.f))
            //EnterDead(B, T);

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
            StopFire(B);
            if (B.EjectAt <= 0.0)
            {
                B.EjectAt = T + RandF(S.EjectMin, S.EjectMax);
                if (S.VerboseLog) BLOG(B.Name + " is in the plane, jumping in " + std::to_string((int)(B.EjectAt - T)) + " s");
            }
            else if (T >= B.EjectAt && T >= B.NextEjectTry)
            {
                B.NextEjectTry = T + 2.0;
                bool bFound = false;

                TUObjectArray* Arr = UObject::GObjects.GetTypedPtr();
                if (!Arr)
                    return;

                const int32 Num = Arr->Num();
                for (int32 i = 0; i < Num; ++i)
                {
                    UObject* O = Arr->GetByIndex(i);
                    if (O->IsDefaultObject())
                        continue;

                    if (!O || !O->IsA(ATransportAircraftVehicle::StaticClass()))
                        continue;
                    ATransportAircraftVehicle* Plane = static_cast<ATransportAircraftVehicle*>(O);
                    UTslVehicleSeatComponent* Seat = Plane->VehicleSeatComponent;
                    if (!Seat || !ObjValid(Seat))
                        continue;
                    TArray<ATslCharacter*> Riders = Seat->GetRiders();
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
                            B.EjectAt = T + RandF(0.f, (std::max)(1.f, S.JumpSpread));
                            B.NextEjectTry = B.EjectAt;
                            bFound = true;
                            B.EjectTries = -1;
                            break;
                        }
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
                    bFound = true;
                    if (S.VerboseLog || B.EjectTries == 0)
                        BLOG(B.Name + " jumps out of the plane" + (B.EjectTries ? " (try " + std::to_string(B.EjectTries + 1) + ")" : std::string()));
                    if (bMine) break;

                    if (!bFound && B.EjectTries == 0)
                        BLOG(B.Name + " is in the plane but no plane lists it as a rider - retrying");
                    ++B.EjectTries;

                }
            }
            return;
        }
        B.EjectAt = 0.0;
        B.EjectTries = 0;

        AActor* Chute = (AActor*)Me->ParachuteType;
        const bool bChute = Chute && ObjValid(Chute);

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
                Chute->K2_SetActorLocation(Dest + V(0.f, 0.f, 150.f), false, &Hit, true);
                //Chute->Land();
                Me->Server_SetParachuteLandingAssist(true);
                Me->SetParachuteLandingAssist(true);
                Me->OnLanded(Hit);
            }
            Me->K2_SetActorLocation(Dest, false, &Hit, true);
            if (Me->GetTslCharacterMovement())
                Me->GetTslCharacterMovement()->StopMovementImmediately();
            ++B.DropTries;
            // done once it is no longer on a parachute / falling from height
            AActor* After = (AActor*)Me->ParachuteType;
            const bool bStillChute = After && ObjValid(After) && !Me->IsInAircraft() && Me->IsFallingWithParachute();
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
        if (bChute || Me->IsFallingWithParachute() || (Me->GetTslCharacterMovement() && Me->GetTslCharacterMovement()->IsFalling() &&
            Me->K2_GetActorLocation().Z > 20000.f))
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
            // skydiving: a real player presses F (or the client auto-opens it) - bots have no client, so we open it
            /*
            if (bChute && !Chute->IsReleased())
            {
                float Lo = Me->ForceReleasingParachuteAltitude;
                float Hi = Me->ReleasingParachuteAltitude;
                if (!(Lo > 100.f && Lo < 1e6f)) Lo = 30000.f;
                if (!(Hi > Lo && Hi < 1e6f)) Hi = Lo + 40000.f;
                if (!GChuteAltLogged)
                {
                    GChuteAltLogged = true;
                    BLOG("parachute heights (cm): game opens it at " + std::to_string((int)Me->ReleasingParachuteAltitude) +
                        ", forces it at " + std::to_string((int)Me->ForceReleasingParachuteAltitude) +
                        ", minimum " + std::to_string((int)Me->MinimumParachuteAltitude));
                }
                if (B.ReleaseAlt <= 0.f)
                    B.ReleaseAlt = RandF(Lo * 1.1f, Hi);
                const float Alt = Chute->GetAltitudeFromFloor();
                if (Alt > 0.f && Alt <= B.ReleaseAlt && T >= B.NextReleaseTry)
                {
                    B.NextReleaseTry = T + 0.5;
                    Chute->ServerRelease();
                    if (!B.ChuteLogged)
                    {
                        B.ChuteLogged = true;
                        BLOG(B.Name + " opens its parachute at " + std::to_string((int)(Alt / 100.f)) + " m" +
                            (Chute->CanRelease() ? "" : " (game says it can't yet - forcing)"));
                    }
                }
            }
            */
            B.SpawnedAt = T;
            B.HasDest = false;
            return;
        }
        B.ReleaseAlt = 0.f;
        B.ChuteLogged = false;

        // ---------------------------------------------------------------- water guard
        {
            const bool bWet = Me->IsSwimming(); // IsInWaterVolume is true on land near the coast
            const FVector Here = Me->K2_GetActorLocation();
            if (!bWet)
            {
                B.HasDry = true;
                B.LastDry = Here;
                B.SwimSince = 0;
            }
            else if (B.SwimSince <= 0)
            {
                B.SwimSince = T;
            }
            else if (T - B.SpawnedAt < 8.0 || !B.HasDry)
            {
                // spawned in water: learn where the surface is, then teleport somewhere dry
                const float Surf = Me->GetCurrentWaterSurfaceZ();
                if (Surf > -1e6f && Surf < 1e6f && Surf != 0.f && (GSeaZ < -1e8f || Surf < GSeaZ))
                {
                    GSeaZ = Surf;
                    BLOG("Learned water level Z " + std::to_string((int)Surf) + " - spawn points below that are skipped now");
                }
                if (++B.WaterMoves > 10)
                {
                    BLOG(B.Name + " kept landing in water - removing it");
                    UnregisterBotTeam(World, B);
                    if (ObjValid(B.Pawn, B.PawnIndex)) B.Pawn->K2_DestroyActor();
                    UncountJoin(B);
                    DestroyBotController(B);
                    B.Remove = true;
                    return;
                }
                FVector NewLoc;
                if (FindRandomSpawn(World, NewLoc))
                {
                    NewLoc.Z += 110.f;
                    FHitResult Hit{};
                    Me->K2_SetActorLocation(NewLoc, false, &Hit, true);
                    B.ProgressPos = NewLoc;
                    B.HasDest = false;
                    B.SpawnedAt = T; // give the new spot its own check window
                    BLOG(B.Name + " spawned in water - moved inland to " + std::to_string((int)NewLoc.X) + "," +
                        std::to_string((int)NewLoc.Y) + "," + std::to_string((int)NewLoc.Z) + " (try " + std::to_string(B.WaterMoves) + ")");
                }
                return; // re-check next think before doing anything else
            }
            else if (B.HasDry && T - B.SwimSince > 10.0)
            {
                // input steering doesn't get it out of the water - put it back on its last dry spot
                FHitResult Hit{};
                Me->K2_SetActorLocation(B.LastDry + V(0.f, 0.f, 60.f), false, &Hit, true);
                B.SwimSince = 0;
                B.HasDest = false;
                B.NextRoamPick = T + 1.0;
                B.InlandNext = true;
                if (T - B.SwimResetWindow > 120.0) { B.SwimResetWindow = T; B.SwimResets = 0; }
                ++B.SwimResets;
                if (S.VerboseLog) BLOG(B.Name + " was stuck swimming - put back on land");
                // keeps ending up in the water here (coast / islet): move it once to dry land further inland,
                // only when no player is close enough to see it
                float HD; FVector HL;
                if (B.SwimResets >= 3 && !NearestHuman(B.LastDry, 30000.f, HD, HL))
                {
                    FVector ZC{}; float ZR = 0.f;
                    FVector Toward = GetSafeCircle(World, ZC, ZR) ? ZC : V(408000.f, 408000.f, 0.f);
                    const FVector Dir = SafeNormal(Flat(Toward - B.LastDry));
                    const FVector Aim = B.LastDry + Dir * RandF(20000.f, 50000.f);
                    FVector G;
                    if (FindLandNear(World, Aim.X, Aim.Y, G) && !NearestHuman(G, 30000.f, HD, HL))
                    {
                        Me->K2_SetActorLocation(G + V(0.f, 0.f, 110.f), false, &Hit, true);
                        B.LastDry = G;
                        B.Home = G;
                        B.SwimResets = 0;
                        BLOG(B.Name + " kept walking into water - moved inland to " + Pos(G));
                    }
                }
                return;
            }
            else if (B.HasDry && B.State != EBotState::Combat)
            {
                // walked into water: head back to the last dry spot
                if (T >= B.NextWaterLog && S.VerboseLog)
                {
                    BLOG(B.Name + " is swimming - heading back to land");
                    B.NextWaterLog = T + 30.0;
                }
                if (!B.HasDest || Len(Flat(B.Dest - B.LastDry)) > 50.f)
                    MoveTo(World, B, B.LastDry, T, false);
                B.NextRoamPick = T + 3.0;
            }
        }

        if (B.JumpHeld) { Me->StopJumping(); B.JumpHeld = false; }

        if (GFrozen)
        {
            StopFire(B);
            return;
        }

        if (!B.Armed && T >= B.NextArmTry && (!S.UnarmedInLobby || (GMatchSpawnDone && !Me->IsInAircraft() && !B.DropPending && !B.Hidden)))
            TryArm(B, T);
        ATslWeapon_Gun* Gun = B.Armed ? Me->GetWeaponGun() : nullptr;
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
                    UObject* PSO = Me->LastTakeHitInfo.PlayerInstigator.Get();
                    if (PSO)
                        for (const FCharInfo& C : GChars)
                            if (!C.IsBot && C.Char->GetTslPlayerState() == PSO) { Attacker = C.Char; break; }
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
                        if (D < Best) { Best = D; Attacker = C.Char; }
                    }
                }
            }
            if (Attacker)
            {
                B.ProvokedBy = Attacker;
                B.ProvokedAt = T;
                B.ReturnFirePending = S.ReturnFire;
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

        // ---------------------------------------------------------------- perception
        int32 TIndex = -1; FVector TLoc{}; float TDist = 0.f;
        // unarmed or downed bots don't pick fights - they keep roaming
        ATslCharacter* Target = (bDowned || !Gun || !GMatchSpawnDone || T < B.NoFireUntil) ? nullptr
            : FindTarget(B, MyLoc, Me->GetActorForwardVector(), T < B.AlertUntil, TIndex, TLoc, TDist);

        if (!Target && S.CombatLog && GMatchSpawnDone && T >= B.NextWhyLog && B.State != EBotState::Combat)
        {
            float HD; FVector HL;
            ATslCharacter* H = NearestHuman(MyLoc, 6000.f, HD, HL);
            if (H)
            {
                B.NextWhyLog = T + 10.0;
                const char* Why = !Gun ? "no gun in its hands (re-arming)" : bDowned ? "it is downed" :
                    T < B.NoFireUntil ? "landing grace" :
                    !B.Ctrl->LineOfSightTo(H, V(0, 0, 0), false) ? "no line of sight" : "not facing you";
                BLOG(B.Name + " is " + std::to_string((int)(HD / 100.f)) + " m from " + CharName(H) + " but not fighting: " + Why);
            }
        }

        if (Target)
        {
            if (B.State != EBotState::Combat || Target != B.Target)
            {
                if (S.CombatLog && !IsBotChar(Target))
                    BLOG(B.Name + " spots " + CharName(Target) + " at " + std::to_string((int)(TDist / 100.f)) + " m" +
                        (T < B.AlertUntil ? " (alerted)" : "") + (B.Hunter ? " [hunter]" : ""));
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
            const FRotator Look = UKismetMathLibrary::FindLookAtRotation(MyLoc + V(0, 0, 60.f), TLoc + V(0, 0, 40.f));
            B.Ctrl->SetControlRotationBP(Look);
            Me->K2_SetActorRotation(FRotator{ 0.f, Look.Yaw, 0.f }, false);
            SetBotAim(Me, NormAngle(Look.Pitch), Look.Yaw);

            const bool bHuman = !IsBotChar(Target);
            const bool bTooFar = bHuman && TDist > S.MaxFireDistance;
            const bool bProvoked = T - B.LastHurtAt < 8.0;
            bool bMayFire = !bTooFar;
            if (bMayFire && S.FairFights && bHuman && !bProvoked)
            {
                if (Target->IsIndoor() || Me->IsIndoor())
                    bMayFire = false; // only pick fights out in the open
                else
                {
                    auto It = GShooterSlot.find(TIndex);
                    if (It != GShooterSlot.end() && It->second.first != B.Id && T - It->second.second < 3.0)
                        bMayFire = false; // someone else is already shooting this player
                }
            }
            if (Gun && T - B.TargetSeenAt >= S.ReactionTime && bMayFire)
            {
                if (bHuman) GShooterSlot[TIndex] = { B.Id, T };
                FireLogic(B, Gun, Target, MyLoc, TLoc, TDist, T, S.FairFights && bHuman && !bProvoked);
            }
            else if (!bMayFire)
                StopFire(B);

            const bool bEvading = T < B.EvadeUntil;
            if (bEvading && S.EvadeCrouch && T >= B.NextCrouchFlip)
            {
                if (B.Crouched) Me->UnCrouch(false); else Me->Crouch(false);
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
            StopFire(B);
            if (T - B.LastSeenTarget > 2.5)
            {
                if (AIC(B)) AIC(B)->K2_ClearFocus();
                B.State = EBotState::Roam;
                B.Target = nullptr;
                MoveTo(World, B, B.LastKnownTargetLoc, T, false); // go check where they were
                SetSprint(B, true);
            }
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
            B.NextRoamPick = T + 8.0;
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
                LobbyAntics(B, Me, MyLoc, T);
            else
            {
                // lobby: stand still - the start island is tiny and surrounded by water
                if (B.HasDest) { B.HasDest = false; B.Ctrl->StopMovement(); }
                SetSprint(B, false);
            }
        }
        else if (B.State == EBotState::Roam)
        {
            if (B.Crouched) { Me->UnCrouch(false); B.Crouched = false; } // lobby crouch-spam left over
            if (B.Hunter && T >= B.NextHuntReplan && T >= B.NoFireUntil)
            {
                B.NextHuntReplan = T + S.HunterReplan * RandF(0.8f, 1.2f);
                FVector ZC{}; float ZR = 0.f;
                const bool bOutsideZone = S.FollowZone && GetSafeCircle(World, ZC, ZR) && Len(Flat(ZC - MyLoc)) > ZR * 0.95f;
                float HD; FVector HL;
                // too far away, or keeps ending up in the water (no navmesh = can't path round lakes / the river):
                // put it on land a few hundred metres from its player
                if (S.HunterCatchUp && B.CanCatchUp && T >= B.NextCatchUp && NearestHuman(MyLoc, 1e9f, HD, HL) &&
                    (HD > S.HunterCatchUpDist || B.SwimResets >= 2))
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
            if (B.HasDest && !B.UseSteer && T - B.DestSetAt > 0.75 &&
                AIC(B) && AIC(B)->GetMoveStatus() == EPathFollowingStatus::Idle)
            {
                B.HasDest = false; // arrived (or path following gave up)
                B.NextRoamPick = T + RandF(0.2f, 1.0f);
            }
            if (B.HasDest && T - B.DestSetAt > 90.0)
                B.HasDest = false; // taking forever - pick something else

            if (!B.HasDest && T >= B.NextRoamPick)
            {
                bool bRun = false;
                MoveTo(World, B, PickRoamDestEx(World, B, MyLoc, bRun), T, false);
                SetSprint(B, bRun || RandF(0.f, 1.f) < 0.5f);
            }
        }

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
                    const float Tries[] = { 40.f, -40.f, 75.f, -75.f, 110.f, -110.f, 150.f, -150.f };
                    const float First = RandI(0, 1) ? 1.f : -1.f; // don't always prefer the same side
                    for (float A : Tries)
                    {
                        const FVector D = RotYaw(Want, A * First);
                        bool bL2;
                        if (!PathBlocked(World, Me, MyLoc, D, bL2))
                        {
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
            if (!B.UseSteer || !B.HasDest || B.State == EBotState::Dead || !ObjValid(B.Pawn, B.PawnIndex))
                continue;

            if (B.Pawn->IsInAircraft() || B.Pawn->IsFallingWithParachute())
                continue;

            const FVector Loc = B.Pawn->K2_GetActorLocation();
            const FVector Delta = Flat(B.Dest - Loc);
            if (Len(Delta) <= S.AcceptRadius)
            {
                B.HasDest = false;
                B.NextRoamPick = Now() + RandF(0.2f, 1.0f);
                continue;
            }
            const FVector Dir = (Now() < B.DetourUntil) ? B.DetourDir : SafeNormal(Delta);
            B.Pawn->AddMovementInput(Dir, 1.f, false);
            if (B.State == EBotState::Combat && B.Target && ObjValid(B.Target, B.TargetIndex))
            {
                const FVector TL = B.Target->K2_GetActorLocation();
                const float Yaw = std::atan2(TL.Y - Loc.Y, TL.X - Loc.X) * 180.f / kPi;
                B.Pawn->K2_SetActorRotation(FRotator{ 0.f, Yaw, 0.f }, false);
            }
            else if (B.State != EBotState::Combat)
            {
                const FRotator Face = UKismetMathLibrary::FindLookAtRotation(Loc, Loc + Dir * 100.f);
                if (B.IsPC && ObjValid(B.Ctrl, B.CtrlIndex))
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
            if (ObjValid(B.Pawn, B.PawnIndex)) B.Pawn->K2_DestroyActor();
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

        DetectNavMesh(World);
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

    void OnWorldChanged(UWorld* World)
    {
        // Old objects are gone with the old world - forget them without touching them.
        GBots.clear();
        GChars.clear();
        GPendingSpawns = 0;
        GPendingNearSpawns = 0;
        GMatchSpawnDone = false;
        GBonesCached = false;
        GMatchStateKnown = false;
        GSeaZ = -1e9f;
        GPreStartCleared = false;
        GMatchStartedAt = 0.0;
        GNextKickTry = 0.0;
        GKickAnnounced = false;
        GPlanesSeen.clear();
        GHaveCounts = false;
        GEndPending = false;
        GTraceDumped = false;
        GNatAlive = GNatTeams = -1;
        GNatAliveLogged = GNatTeamsLogged = -2;
        GLastWroteAlive = GLastWroteTeams = -1;
        GLastHumanAliveAt = 0;
        GNoHumansSince = 0;
        GMatchOver = false;
        GDressed.clear();
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
        GBotTeam = -1;
        GWorld = World;
        if (World)
            BLOG("World changed - bot manager reset");
        ApplySettings(World);
    }

    // =================================================================================
    //  Commands
    // =================================================================================
    void PrintHelp()
    {
        BLOG("Commands (type in this console):");
        BLOG("  bots [N]    spawn N bots (lobby: where players spawn / in match: random land points)");
        BLOG("  bot [N]     spawn N bots next to a connected player (default 1)");
        BLOG("  killbots    remove all bots");
        BLOG("  freeze      pause / resume bot AI");
        BLOG("  status      bot count, nav mode, tick rate, damage stats, game player counters");
        BLOG("  where       every character on the server and its position (find your bot)");
        BLOG("  teams       what the GAME has in its team list (who it counts as players)");
        BLOG("  start [N]   lobby: start the match in N seconds (default 5)");
        BLOG("  weapons     list gun classes bots can be given");
        BLOG("  reload      re-read [Bots] from settings.ini");
        BLOG("  nativebot   EXPERIMENTAL: call the game's own UTslCheatManager::SpawnBot()");
        BLOG("  trace       print the last game function calls (debug)");
        BLOG("  lookdump    compare a player's and a bot's character data (appearance debug)");
        BLOG("  navcheck    report navmesh / navigation data loaded anywhere (and use it if found)");
        BLOG("  botflag 0|1 clear / set the 'is a bot' flag on all bots live (appearance test)");
        BLOG("  lookupdate  run the game's appearance rebuild on all bots (appearance test)");
        BLOG("Hotkeys (server window focused): F5 bot  F6 bots  F7 killbots  F8 freeze");
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
        BLOG(std::string("navmesh ") + (GHasNavMesh ? "yes" : "no") + " | frozen " + (GFrozen ? "yes" : "no") +
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
                if (C && ObjValid(C) && C->IsA(APlayerController::StaticClass()))
                    What = static_cast<APlayerController*>(C)->bIsLocalPlayerController ? "SERVER'S OWN LOCAL PAWN (the ghost)" : "real player";
                else
                    What = C ? "AI / other controller" : "no controller";
                if (C && C->PlayerState && ObjValid(C->PlayerState))
                    What += " '" + C->PlayerState->PlayerName.ToString() + "'";
            }
            std::string Team;
            AController* TC = CI.Char->GetController();
            if (TC && ObjValid(TC) && TC->PlayerState && ObjValid(TC->PlayerState) && TC->PlayerState->IsA(ATslPlayerState::StaticClass()))
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
        AGameStateBase* GSB = UGameplayStatics::GetGameState(World);
        if (!GSB || !GSB->IsA(ATslGameState::StaticClass()))
            return;
        ATslGameState* GS = static_cast<ATslGameState*>(GSB);

        std::vector<int32> Teams;
        auto AddTeam = [&Teams](int32 T) { if (std::find(Teams.begin(), Teams.end(), T) == Teams.end()) Teams.push_back(T); };

        int Humans = 0;
        for (const FCharInfo& CI : GChars)
        {
            if (CI.IsBot) continue;
            // a living non-bot character is a human - even while sitting in the plane, where its
            // controller is busy with the seat (counting only PlayerController-possessed characters
            // read 0 humans in the plane and the solo match ended)
            AController* C = CI.Char->Controller;
            const bool bPC = C && ObjValid(C) && C->IsA(APlayerController::StaticClass());
            if (C && ObjValid(C) && !bPC) continue; // some other AI - not a player
            if (bPC && static_cast<APlayerController*>(C)->bIsLocalPlayerController) continue; // listen-server ghost
            APlayerState* PS = bPC ? C->PlayerState : CI.Char->GetTslPlayerState();
            if (!bPC && !(PS && ObjValid(PS)) && !CI.Char->IsInAircraft()) continue;
            ++Humans;
            if (PS->IsA(ATslPlayerState::StaticClass()))
                AddTeam(static_cast<ATslPlayerState*>(PS)->TeamNumber);
            else
                AddTeam(-1000 - Humans);
        }
        // players in a vehicle have no controller on their character - count logged-in, not-killed players too
        {
            int HumansPS = 0;
            for (APlayerState* PS : GS->PlayerArray)
            {
                if (!PS || !ObjValid(PS) || PS->bIsABot || !PS->IsA(ATslPlayerState::StaticClass())) continue;
                ATslPlayerState* TPS = static_cast<ATslPlayerState*>(PS);
                if (TPS->bKilled || TPS->bQuitter) continue;
                AActor* Own = PS->GetOwner();
                if (!Own || !ObjValid(Own) || !Own->IsA(APlayerController::StaticClass())) continue;
                if (static_cast<APlayerController*>(Own)->IsLocalPlayerController()) continue; // listen-server ghost
                ++HumansPS;
                AddTeam(TPS->TeamNumber);
            }
            if (HumansPS > Humans) Humans = HumansPS;
        }
        int Bots = 0;
        for (const FBot& B : GBots)
        {
            if (B.State == EBotState::Dead || !ObjValid(B.Pawn, B.PawnIndex) || B.Pawn->Health <= 0.f) continue;
            ++Bots;
            if (B.Ctrl && ObjValid(B.Ctrl, B.CtrlIndex) && B.Ctrl->PlayerState && ObjValid(B.Ctrl->PlayerState) &&
                B.Ctrl->PlayerState->IsA(ATslPlayerState::StaticClass()))
                AddTeam(static_cast<ATslPlayerState*>(B.Ctrl->PlayerState)->TeamNumber);
            else
                AddTeam(-2000 - B.Id);
        }
        GHumansAlive = Humans;
        if (Humans > 0) GLastHumanAliveAt = Now();
        GBotsAlive = Bots;
        GTeamsAlive = (int)Teams.size();

        if (!S.BotsInPlayerCount)
        {
            GHaveCounts = false;
            return;
        }
        GCountGS = GS;
        GCountGSIndex = GS->GetIndex();
        GCountsInMatch = GMatchSpawnDone;
        if (!GMatchSpawnDone)
        {
            // lobby: everyone the game counted (logged-in player states) + our bots
            int Joined = 0;
            for (APlayerState* PS : GS->PlayerArray)
                if (PS && ObjValid(PS) && !PS->bIsABot && !IsBotPS(PS)) ++Joined;
            GWantJoined = Joined + Bots;
        }
        else
        {
            GWantAlive = Humans + Bots;
            GWantTeams = GTeamsAlive;
            if (Now() - GMatchStartedAt < 90.0)
            {
                GStartPlayers = (std::max)(GStartPlayers, GWantAlive);
                GStartTeams = (std::max)(GStartTeams, GWantTeams);
            }
        }
        GHaveCounts = true;
    }

    // The game rewrites its own counters (probably every frame). We write ours on every
    // ProcessEvent call on the game thread so ours is the last value before replication.
    void ApplyCounters()
    {
        if (!GHaveCounts || !GCountGS || !ObjValid(GCountGS, GCountGSIndex))
            return;

        if (!GCountsInMatch)
        {
            GCountGS->NumJoinPlayers = GWantJoined;
        }
        else
        {
            // anything different from what we wrote last time was written by the game itself
            if (GLastWroteAlive >= 0 && (GCountGS->NumAlivePlayers != GLastWroteAlive || GCountGS->NumAliveTeams != GLastWroteTeams))
            {
                GNatAlive = GCountGS->NumAlivePlayers;
                GNatTeams = GCountGS->NumAliveTeams;
            }

            GCountGS->NumAlivePlayers = GWantAlive;
            GCountGS->NumAliveTeams = GWantTeams;

            if (GStartPlayers > 1) { GCountGS->NumStartPlayers = GStartPlayers; GCountGS->NumStartTeams = GStartTeams; }
            GLastWroteAlive = GWantAlive;
            GLastWroteTeams = GWantTeams;
        }
    }

    // Log every plane once, and lift its flight path if it flies low (plane in the water)
    void CheckPlanes(UWorld* World, double T)
    {
        if (T < GNextPlaneCheck || !World)
            return;

        GNextPlaneCheck = T + 1.0;
        TUObjectArray* Arr = UObject::GObjects.GetTypedPtr();
        if (!Arr)
            return;

        const int32 Num = Arr->Num();
        for (int32 i = 0; i < Num; ++i) {
            UObject* O = Arr->GetByIndex(i);
            if (O->IsDefaultObject())
                continue;

            if (!O->IsA(ATransportAircraftVehicle::StaticClass()))
                continue;

            if (std::find(GPlanesSeen.begin(), GPlanesSeen.end(), O->GetIndex()) != GPlanesSeen.end())
                continue;

            GPlanesSeen.push_back(O->GetIndex());
            ATslAircraftVehicle* Plane = static_cast<ATslAircraftVehicle*>(O);
            const FVector P = Plane->K2_GetActorLocation();
            BLOG("plane at " + Pos(P) + " (Z " + std::to_string((int)(P.Z / 100.f)) + " m)");
            if (!S.FixPlaneAltitude || !Plane->SplineComponent || !ObjValid(Plane->SplineComponent))
            {
                CUSTOMLOG("Spotted plane is invalid");
                continue;
            }
            USplineComponent* Spl = Plane->SplineComponent;
            const int32 N = Spl->GetNumberOfSplinePoints();
            int Raised = 0;
            float MinZ = 1e9f;
            for (int32 i = 0; i < N && i < 64; ++i)
            {
                FVector SP = Spl->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
                MinZ = (std::min)(MinZ, SP.Z);
                if (SP.Z < S.PlaneMinZ)
                {
                    SP.Z = S.PlaneZ;
                    Spl->SetLocationAtSplinePoint(i, SP, ESplineCoordinateSpace::World, i == N - 1);
                    ++Raised;
                }
            }
            BLOG("plane flight path: " + std::to_string(N) + " points, lowest Z " + std::to_string((int)(MinZ / 100.f)) + " m" +
                (Raised ? (" - raised " + std::to_string(Raised) + " point(s) to " + std::to_string((int)(S.PlaneZ / 100.f)) + " m") : std::string()));
            if (P.Z < S.PlaneMinZ)
            {
                AActor* A = (AActor*)O;
                FHitResult Hit{};
                A->K2_SetActorLocation(V(P.X, P.Y, S.PlaneZ), false, &Hit, true);
                BLOG("plane was low - moved up to " + std::to_string((int)(S.PlaneZ / 100.f)) + " m");
            }
        }

    }

    // The plane mod's plane never reaches the end of its route, so nobody is ever forced out.
    // After PlaneKickTime: eject everyone (the game's own EjectAll, then per-rider as backup),
    // waiting for land under the plane for up to 60 s more.
    // ---------------------------------------------------------------- blue zone watch
    // The gas sometimes closes part way on players' screens, then snaps to the new circle. Neither DLL touches
    // the zone, so: log the server's own timing (to compare with what players see), push the game state out
    // promptly while the gas moves, and log server hitches (game time falling behind real time).

    void ZoneWatchThink(UWorld* World, double T)
    {
        if (!S.ZoneWatch) return;
        AGameStateBase* GSB = World->AuthorityGameMode->GameState;
        if (!GSB || !GSB->IsA(ATslGameState::StaticClass())) return;
        ATslGameState* GS = static_cast<ATslGameState*>(GSB);
        const float GameT = UGameplayStatics::GetTimeSeconds(World);

        if (!GZoneNetTuned)
        {
            GZoneNetTuned = true;
            char Buf[200];
            snprintf(Buf, sizeof(Buf), "Zone watch: game state net priority %.1f, update rate %.0f/s -> %.1f, %.0f/s",
                GS->NetPriority, GS->NetUpdateFrequency, (std::max)(GS->NetPriority, 5.f), (std::max)(GS->NetUpdateFrequency, 10.f));
            BLOG(Buf);
            GS->NetPriority = (std::max)(GS->NetPriority, 5.f);
            GS->NetUpdateFrequency = (std::max)(GS->NetUpdateFrequency, 10.f);
        }

        // server hitches: biggest gap between AI thinks, and game time vs real time, per 30 s
        if (GPerfLastThink > 0) GPerfMaxGap = (std::max)(GPerfMaxGap, T - GPerfLastThink);
        GPerfLastThink = T;
        if (GPerfWinStart <= 0) { GPerfWinStart = T; GPerfWinGame = GameT; }
        else if (T - GPerfWinStart >= 30.0)
        {
            const double Real = T - GPerfWinStart, Game = GameT - GPerfWinGame;
            if (GPerfMaxGap > 0.5 || Game < Real * 0.95)
            {
                char Buf[200];
                snprintf(Buf, sizeof(Buf), "Server perf: last %.0f s real = %.1f s game time, worst stall %.2f s", Real, Game, GPerfMaxGap);
                BLOG(Buf);
            }
            GPerfWinStart = T; GPerfWinGame = GameT; GPerfMaxGap = 0;
        }

        const bool bRel = GS->bIsGasRelease;
        char Buf[300];
        if (bRel && !GZoneWasReleasing)
        {
            GZoneStartReal = T; GZoneStartGame = GameT; GZoneNextLog = T + 15.0;
            snprintf(Buf, sizeof(Buf), "Zone: gas starts closing - from %.0f m to %.0f m radius, game says it takes %.0f s (match time %.0f s)",
                GS->SafetyZoneBeginRadius / 100.f, GS->PoisonGasWarningRadius / 100.f, GS->TotalReleaseDuration, T - GMatchStartedAt);
            BLOG(Buf);
            GS->ForceNetUpdate();
        }
        else if (!bRel && GZoneWasReleasing)
        {
            snprintf(Buf, sizeof(Buf), "Zone: gas finished closing after %.0f s real / %.0f s game time (game said %.0f s) - blue now %.0f m",
                T - GZoneStartReal, GameT - GZoneStartGame, GS->TotalReleaseDuration, GS->SafetyZoneRadius / 100.f);
            BLOG(Buf);
            GS->ForceNetUpdate();
        }
        else if (bRel && T >= GZoneNextLog)
        {
            GZoneNextLog = T + 15.0;
            snprintf(Buf, sizeof(Buf), "Zone: closing %.0f/%.0f s (real %.0f s) - blue %.0f m (lerp %.0f m), target %.0f m",
                GS->ElapsedReleaseDuration, GS->TotalReleaseDuration, T - GZoneStartReal,
                GS->SafetyZoneRadius / 100.f, GS->LerpSafetyZoneRadius / 100.f, GS->PoisonGasWarningRadius / 100.f);
            BLOG(Buf);
        }
        if (bRel && T >= GZoneNextPush) { GZoneNextPush = T + 1.0; GS->ForceNetUpdate(); }
        GZoneWasReleasing = bRel;
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
            if (!Seat || !ObjValid(Seat))
                continue;

            TArray<ATslCharacter*> Riders = Seat->GetRiders();
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
                if (R && ObjValid(R) && R->IsInAircraft())
                    Seat->TryToLeave(R, true);
        }
    }

    // ================================================================================
    //  v14: native match-end guard
    //  With one human the game's own (native) alive-team count is 1 - it only counts real
    //  logins - so it finishes the match the moment the plane leaves, without asking
    //  ReadyToEndMatch. Every way of ending a match goes through AGameMode::SetMatchState
    //  (WaitingPostMatch), so we find that function in the game exe and hook it.
    // ================================================================================
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

    bool IsVirtualCall(const hde64s& H, int& Slot)
    {
        // FF /2 (call) or FF /4 (jmp) with [reg], [reg+disp8], [reg+disp32]
        if (H.opcode != 0xFF || (H.modrm_reg != 2 && H.modrm_reg != 4) || H.modrm_mod == 3 || H.modrm_rm == 4 || H.modrm_rm == 5)
            return false;
        int32 Disp = 0;
        if (H.modrm_mod == 1) Disp = static_cast<int8>(H.disp.disp8);
        else if (H.modrm_mod == 2) Disp = static_cast<int32>(H.disp.disp32);
        if (Disp < 0 || (Disp % 8) != 0 || Disp >= 0x2000)
            return false;
        Slot = Disp / 8;
        return true;
    }

    // exec thunk of a UFunction: first virtual slot it calls, and/or its first direct call target
    void ScanThunk(uint8* Code, int MaxBytes, int& Slot, uint8*& Direct)
    {
        Slot = -1; Direct = nullptr;
        int Off = 0;
        while (Off < MaxBytes && Readable(Code + Off, 16))
        {
            hde64s H{};
            const unsigned L = hde64_disasm(Code + Off, &H);
            if (!L || (H.flags & F_ERROR)) break;
            int Sl;
            if (IsVirtualCall(H, Sl)) { if (Slot < 0) Slot = Sl; if (H.modrm_reg == 4) break; }
            else if (H.opcode == 0xE8 || H.opcode == 0xE9)
            {
                if (!Direct) Direct = Code + Off + L + static_cast<int32>(H.imm.imm32);
                if (H.opcode == 0xE9) break;
            }
            else if (H.opcode == 0xC3 || H.opcode == 0xCC || H.opcode == 0xEB) break;
            Off += L;
        }
    }

    // In a function body: a load of the FName "WaitingPostMatch" (mov/lea reg,[rip+X])
    // followed within a few instructions by a call -> that call is SetMatchState.
    bool FindSetMatchState(uint8* Code, int MaxBytes, int& Slot, uint8*& Direct, bool& ByRef, std::vector<uint8*>& Callees)
    {
        int Off = 0, ArmedAt = -1, Cc = 0;
        bool Ref = false;
        while (Off < MaxBytes && Readable(Code + Off, 16))
        {
            hde64s H{};
            const unsigned L = hde64_disasm(Code + Off, &H);
            if (!L || (H.flags & F_ERROR)) break;
            uint8* Ip = Code + Off;
            Cc = (H.opcode == 0xCC) ? Cc + 1 : 0;
            if (Cc >= 2) break; // padding: end of function
            int Sl;
            if ((H.opcode == 0x8B || H.opcode == 0x8D) && H.rex_w && H.modrm_mod == 0 && H.modrm_rm == 5)
            {
                uint8* X = Ip + L + static_cast<int32>(H.disp.disp32);
                if (Readable(X, 8) && SameName(*reinterpret_cast<const FName*>(X), GNameWaitingPost))
                {
                    ArmedAt = Off;
                    Ref = H.opcode == 0x8D;
                }
            }
            else if (ArmedAt >= 0 && Off - ArmedAt <= 48 && IsVirtualCall(H, Sl))
            {
                Slot = Sl; Direct = nullptr; ByRef = Ref;
                return true;
            }
            else if (H.opcode == 0xE8 || H.opcode == 0xE9)
            {
                uint8* Tgt = Ip + L + static_cast<int32>(H.imm.imm32);
                if (ArmedAt >= 0 && Off - ArmedAt <= 48)
                {
                    Slot = -1; Direct = Tgt; ByRef = Ref;
                    return true;
                }
                if (Callees.size() < 12 && InGameExe(Tgt)) Callees.push_back(Tgt);
            }
            Off += L;
        }
        return false;
    }

    bool HoldCondition()
    {
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

    void InstallEndGuard(UWorld* World)
    {
        if (GGuardTried || !S.NativeEndGuard || !World || !World->AuthorityGameMode)
            return;
        AGameModeBase* GM = World->AuthorityGameMode;

        if (!GM->IsA(AGameMode::StaticClass()))
            return;

        GGuardTried = true;
        BLOG("End guard: looking for the game's SetMatchState...");

        for (int i = 0; i < 6; ++i)
        {
            std::wstring W(GKnownStateNames[i], GKnownStateNames[i] + std::strlen(GKnownStateNames[i]));
            GKnownStates[i] = UKismetStringLibrary::Conv_StringToName(FString(W.c_str()));
        }
        GNameInProgress = GKnownStates[2];
        GNameWaitingPost = GKnownStates[3];
        GHaveNames = GNameWaitingPost.GetComparisonIndex() != 0 && GNameInProgress.GetComparisonIndex() != 0;
        if (!GHaveNames) { GGuardWhat = "FName lookup failed"; BLOG("End guard: " + GGuardWhat); return; }

        // sanity: the live MatchState really sits at 0x430
        const FName& Live = static_cast<AGameMode*>(GM)->MatchState;
        bool bKnown = false;
        for (int i = 0; i < 6; ++i) if (SameName(Live, GKnownStates[i])) bKnown = true;
        if (!bKnown) { GGuardWhat = "MatchState offset check failed"; BLOG("End guard: " + GGuardWhat + " - not hooking"); return; }

        void** Vt = *reinterpret_cast<void***>(GM);
        if (!InGameExe(Vt)) { GGuardWhat = "game mode vtable not in exe"; BLOG("End guard: " + GGuardWhat); return; }

        std::vector<uint8*> Found;
        std::vector<int> FoundSlot;
        int Slots = 0;
        for (int i = 0; i < 700; ++i, ++Slots)
        {
            if (!Readable(&Vt[i], 8) || !InGameExe(Vt[i])) break;
            uint8* F = static_cast<uint8*>(Vt[i]);
            bool bSt, bRd;
            ScanFieldUse(F, 0x200, kMatchStateOff, bSt, bRd);
            if (bSt && bRd && std::find(Found.begin(), Found.end(), F) == Found.end())
            {
                Found.push_back(F);
                FoundSlot.push_back(i);
            }
        }
        BLOG("End guard: scanned " + std::to_string(Slots) + " game mode virtuals, " + std::to_string(Found.size()) + " write MatchState");
        for (size_t i = 0; i < Found.size(); ++i)
            BLOG("End guard:   slot " + std::to_string(FoundSlot[i]) + " " + Rva(Found[i]) + " [" + Bytes(Found[i], 20) + "]");
        if (Found.size() != 1)
        {
            GGuardWhat = Found.empty() ? "SetMatchState not found - not hooking" : "several candidates - not hooking (safe)";
            BLOG("End guard: " + GGuardWhat + " (fine - the match-end task is caught instead)");
            return;
        }
        MH_STATUS St = MH_CreateHook(Found[0], reinterpret_cast<void*>(&DetSetMatchState), reinterpret_cast<void**>(&GOrigSetMatchState));
        if (St == MH_OK) St = MH_EnableHook(Found[0]);
        if (St != MH_OK)
        {
            GGuardWhat = std::string("hook failed: ") + MH_StatusToString(St);
            BLOG("End guard: " + GGuardWhat);
            return;
        }
        GGuardOk = true;
        GGuardWhat = "SetMatchState hooked at " + Rva(Found[0]) + " (vtable slot " + std::to_string(FoundSlot[0]) + ")";
        BLOG("End guard: " + GGuardWhat + " - watch for 'SetMatchState -> ...' lines to confirm");
    }

    // every think: install the guard, finish a held match once it's decided, log game counters
    // ---- empty lobby: keep the countdown at full so an empty server never plays a match on its own
    int  GLobbyFullTime = 0;
    bool GLobbyHeld = false;
    void LobbyHoldThink(UWorld* World)
    {
        if (!S.HoldLobbyWhenEmpty || GMatchSpawnDone || GMatchStartedAt > 0) return;

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
            if (!PS || !ObjValid(PS) || PS->bIsABot) continue;
            AActor* Own = PS->GetOwner();
            if (!Own || !ObjValid(Own) || !Own->IsA(APlayerController::StaticClass())) { continue; }
            if (static_cast<APlayerController*>(Own)->bIsLocalPlayerController) { continue; }  // the server's ghost
            ++Humans;
        }
        if (Humans == 0)
        {
            if (GS->RemainingTime != GLobbyFullTime)
            {
                GS->bTimerPaused = true;
                GS->RemainingTime = GLobbyFullTime;
                GM->MatchPreparer->WarmupTime = GLobbyFullTime;
            }
            
            if (!GLobbyHeld) { GLobbyHeld = true; BLOG("Lobby empty - countdown held at " + std::to_string(GLobbyFullTime) + " s until a player joins"); }
        }
        else if (GLobbyHeld)
        {
            GLobbyHeld = false;
            GS->bTimerPaused = false;

            BLOG("A player is in the lobby - countdown running (" + std::to_string(GS->RemainingTime) + " s)");
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

    void EndGuardThink(UWorld* World, double T)
    {
        InstallEndGuard(World);

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
        if (!GHaveNames || !SameName(GM->MatchState, GNameInProgress))
            return;
        BLOG("The match is decided (players alive " + std::to_string(GHumansAlive) + ", bots alive " + std::to_string(GBotsAlive) +
            ") - ending it now");
        ScheduleExit("match decided");
        if (GM->IsA(ATslGameMode::StaticClass()) && GFnFinishMatch)
            static_cast<ATslGameMode*>(GM)->FinishMatch();
        else
            GM->EndMatch();
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
                    if (B.State != EBotState::Roam || !ObjValid(B.Pawn, B.PawnIndex)) continue;
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
            if (N.rfind("Item_", 0) != 0 || N.find("_Lv") != std::string::npos) continue;
            for (FClothSlot& Sl : GClothes)
                if (Sl.Slot == E->EquipSlotID) { Sl.Classes.push_back(O->GetClass()); ++Total; break; }
        }
        BLOG("Clothes: found " + std::to_string(Total) + " clothing items for bots / players");
    }

    void DressCharacter(ATslCharacter* C, const std::string& Who)
    {
        AInventoryFacade* Inv = C->GetInventoryFacade();
        if (!Inv || !ObjValid(Inv) || !Inv->GetEquipment() || !ObjValid(Inv->GetEquipment()))
            return;
        AEquipment* Eq = Inv->GetEquipment();
        std::string Put;
        for (const FClothSlot& Sl : GClothes)
        {
            if (Sl.Classes.empty() || RandF(0.f, 1.f) > Sl.Chance) continue;
            FEquipPosition P{}; P.SlotID = Sl.Slot; P.Index = 0;
            if (Eq->GetItem(P)) continue;                           // already wearing something there
            UClass* Cls = Sl.Classes[RandI(0, (int)Sl.Classes.size() - 1)];
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
            if (B.State == EBotState::Dead || !ObjValid(B.Pawn, B.PawnIndex) || GAppearanced.count(B.PawnIndex)) continue;
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
            if (CI.IsBot || !ObjValid(CI.Char, CI.Index)) continue;
            ATslCharacter* C = CI.Char;
            if (((uint8)C->Gender ? 1 : 0) != G) continue;
            if (!C->CustomizableObjectInstance || !ObjValid(C->CustomizableObjectInstance)) continue;
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
            if (B.State == EBotState::Dead || !ObjValid(B.Pawn, B.PawnIndex)) continue;
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
            if (CI.IsBot ? !S.DressBots : !S.DressPlayers) { GDressed.insert(CI.Index); continue; }
            if (CI.Char->IsInAircraft()) continue;
            if (!CI.Char->GetInventoryFacade()) continue;               // not set up yet - try next think
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
        if (!GMatchSpawnDone || S.HuntersPerPlayer <= 0 || T < GNextHunterAssign || T - GMatchStartedAt < 10.0)
            return;
        GNextHunterAssign = T + 2.0;
        const int Humans = (std::max)(GHumansAlive, 0);
        if (Humans == 0)
            return;
        const int Want = S.HuntersPerPlayer * Humans;
        int Have = 0;
        for (const FBot& B : GBots)
            if (B.Hunter && B.State != EBotState::Dead && !B.Remove && ObjValid(B.Pawn, B.PawnIndex)) ++Have;
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
            if (B.State == EBotState::Dead || B.Remove || !ObjValid(B.Pawn, B.PawnIndex)) continue;
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

    void RunCommand(UWorld* World, const std::string& Line)
    {
        std::stringstream SS(Line);
        std::string Cmd;
        SS >> Cmd;
        Cmd = Lower(Cmd);
        int N = -1;
        SS >> N;

        if (Cmd == "help" || Cmd == "?")       PrintHelp();
        else if (Cmd == "bots")
        {
            const int Want = N > 0 ? N : S.Count;
            if ((S.PlayerBots() || S.RegisterTeams) && !GMatchSpawnDone)
                GPendingLobbySpawns += Want;
            else
                GPendingSpawns += Want;
            BLOG("Queued " + std::to_string(Want) + " bots");
        }
        else if (Cmd == "bot")
        {
            GPendingNearSpawns += N > 0 ? N : 1;
        }
        else if (Cmd == "killbots" || Cmd == "removebots") RemoveAllBots();
        else if (Cmd == "freeze")
        {
            GFrozen = !GFrozen;
            if (GFrozen)
                for (FBot& B : GBots)
                    if (ObjValid(B.Ctrl, B.CtrlIndex)) { StopFire(B); B.Ctrl->StopMovement(); B.HasDest = false; }
            BLOG(std::string("Bots ") + (GFrozen ? "FROZEN" : "resumed"));
        }
        else if (Cmd == "status") {
            PrintStatus(); PrintGameCounters(World);
            BLOG("end guard: " + GGuardWhat + " | match ends held " + std::to_string(GHeldEnds) + "x");
        }
        else if (Cmd == "trace")               DumpTrace("(trace command)");
        else if (Cmd == "botflag")
        {
            const bool bOn = N == 1;
            int N = 0;
            for (FBot& B : GBots)
            {
                if (!B.Ctrl || !ObjValid(B.Ctrl, B.CtrlIndex) || !B.Ctrl->PlayerState) continue;
                B.Ctrl->PlayerState->bIsABot = bOn ? 1 : 0;
                B.Ctrl->PlayerState->ForceNetUpdate();
                ++N;
            }
            BLOG("botflag: bIsABot = " + std::string(bOn ? "1" : "0") + " on " + std::to_string(N) + " bots (watch how they look now)");
        }
        else if (Cmd == "navcheck")
        {
            GatherCharacters(World);
            NavCheck(World, true);
        }
        else if (Cmd == "lookupdate")
        {
            int N = 0;
            for (FBot& B : GBots)
                if (B.State != EBotState::Dead && ObjValid(B.Pawn, B.PawnIndex)) { B.Pawn->UpdateAppearence(); B.Pawn->ForceNetUpdate(); ++N; }
            BLOG("lookupdate: UpdateAppearence on " + std::to_string(N) + " bots");
        }
        else if (Cmd == "lookdump")
        {
            GatherCharacters(World);
            int Humans = 0, BotsShown = 0;
            for (const FCharInfo& CI : GChars)
            {
                if (CI.IsBot ? BotsShown >= 2 : Humans >= 2) continue;
                CI.IsBot ? ++BotsShown : ++Humans;
                ATslCharacter* C = CI.Char;
                FRawArray R; std::memcpy(&R, &C->InstanceDescriptor, sizeof(R));
                std::string Hex;
                char Hb[4];
                for (int i = 0; R.Data && i < R.Num && i < 32; ++i) { snprintf(Hb, sizeof(Hb), "%02X", static_cast<uint8*>(R.Data)[i]); Hex += Hb; }
                APlayerState* PS = C->GetTslPlayerState();
                const bool bNetId = PS && ObjValid(PS) && *reinterpret_cast<void* const*>(reinterpret_cast<const uint8*>(&PS->UniqueId) + 8) != nullptr;
                int EqItems = -1;
                if (C->GetInventoryFacade() && ObjValid(C->GetInventoryFacade()) && C->GetInventoryFacade()->GetEquipment() && ObjValid(C->GetInventoryFacade()->GetEquipment()))
                    EqItems = C->GetInventoryFacade()->GetEquipment()->Items.Num();
                UCustomizableObjectInstance* COI = C->CustomizableObjectInstance;
                std::string Obj = (COI && ObjValid(COI) && COI->CustomizableObject) ? COI->CustomizableObject->GetName() : "-";
                std::string Mesh = (C->CustomizableSkeletalComponent && ObjValid(C->CustomizableSkeletalComponent) &&
                    C->CustomizableSkeletalComponent) ? C->CustomizableSkeletalComponent->GetName() : "-";
                BLOG(std::string(CI.IsBot ? "BOT    " : "PLAYER ") + CharName(C) + " | class " + C->GetClass()->GetName() +
                    " | gender " + ((uint8)C->Gender ? "F" : "M") + " | PS " + (PS ? PS->GetClass()->GetName() : std::string("none")) +
                    " bIsABot " + (PS && PS->bIsABot ? "1" : "0") + " netId " + (bNetId ? "yes" : "no") +
                    " | equip items " + std::to_string(EqItems) + " | mutable obj " + Obj + " mesh " + Mesh +
                    " | record " + std::to_string(R.Num) + " bytes [" + Hex + "]");
            }
            if (!GClothNamesBuilt) BuildClothNames();
            bool Shown[2][2] = { { false, false }, { false, false } };   // [bot][gender]
            for (const FCharInfo& CI : GChars)
            {
                const int G = (uint8)CI.Char->Gender ? 1 : 0;
                if (Shown[CI.IsBot][G]) continue;
                uint8* D; int N;
                if (!GetLook(CI.Char, D, N)) continue;
                Shown[CI.IsBot][G] = true;
                const std::vector<FDescTok> Tk = ParseDesc(D, N);
                const std::unordered_set<std::string>& Cloth = ClothSetFor(Tk);
                std::string L = std::string(CI.IsBot ? "BOT    " : "PLAYER ") + CharName(CI.Char) + (G ? " (F)" : " (M)") + " values:";
                for (const FDescTok& t : Tk)
                {
                    L += " " + t.Name + (Cloth.count(Lower(t.Name)) ? "*" : "") + "=" + DescVal(D + t.Off, t.Len);
                    if (L.size() > 700) { BLOG(L); L = "   "; }
                }
                BLOG(L);
            }
            BLOG("(* = clothing slot, kept as the bot's own; everything else is copied from a player of the same gender)");
        }
        else if (Cmd == "where")               PrintWhere(World);
        else if (Cmd == "teams")               DumpTeams(World, "command");
        else if (Cmd == "start")
        {
            // set the lobby countdown (same field PUBG_InGame sets from [Lobby] WaitTime)
            AGameStateBase* GSB = UGameplayStatics::GetGameState(World);
            if (GMatchSpawnDone)
                BLOG("start: the match has already started");
            else if (GSB && GSB->IsA(ATslGameState::StaticClass()))
            {
                const int Secs = N > 0 ? N : 5;
                static_cast<ATslGameState*>(GSB)->RemainingTime = Secs;
                BLOG("start: match starts in " + std::to_string(Secs) + " s");
            }
            else
                BLOG("start: no TslGameState");
        }
        else if (Cmd == "weapons")             ListWeapons();
        else if (Cmd == "reload")
        {
            ReloadIni();
            LoadSettings();
            ApplySettings(World);
            BLOG("Settings reloaded (applies to existing bots too; PawnClass/ControllerClass only affect new bots)");
        }
        else if (Cmd == "nativebot")           TryNativeSpawnBot(World);
        else if (!Cmd.empty())                 BLOG("Unknown command '" + Cmd + "' - type help");
    }

    void RunQueuedCommands(UWorld* World)
    {
        std::deque<std::string> Local;
        {
            std::lock_guard<std::mutex> Lock(GCmdMutex);
            Local.swap(GCmdQueue);
            GHasCommands = false;
        }
        for (const std::string& C : Local)
            RunCommand(World, C);
    }

    // =================================================================================
    //  Input threads
    // =================================================================================
    DWORD WINAPI ConsoleThread(LPVOID)
    {
        // Raw Win32 console reads: works no matter which C runtime this DLL was built with
        // and whether or not anyone freopen'd stdin.
        HANDLE In = INVALID_HANDLE_VALUE;
        std::string Line;
        char Buf[512];
        for (;;)
        {
            if (In == INVALID_HANDLE_VALUE)
            {
                In = CreateFileA("CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                    nullptr, OPEN_EXISTING, 0, nullptr);
                if (In == INVALID_HANDLE_VALUE) { Sleep(1000); continue; }
            }
            DWORD Read = 0;
            if (!ReadConsoleA(In, Buf, sizeof(Buf), &Read, nullptr))
            {
                CloseHandle(In);
                In = INVALID_HANDLE_VALUE;
                Sleep(500);
                continue;
            }
            for (DWORD i = 0; i < Read; ++i)
            {
                const char c = Buf[i];
                if (c == '\n' || c == '\r')
                {
                    Line.erase(0, Line.find_first_not_of(" \t"));
                    Line.erase(Line.find_last_not_of(" \t") + 1);
                    if (!Line.empty())
                        Bots::QueueCommand(Line);
                    Line.clear();
                }
                else if (Line.size() < 256)
                {
                    Line.push_back(c);
                }
            }
        }
        return 0;
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

    DWORD WINAPI HotkeyThread(LPVOID)
    {
        const int Keys[] = { VK_F5, VK_F6, VK_F7, VK_F8 };
        const char* Cmds[] = { "bot", "bots", "killbots", "freeze" };
        bool Prev[4] = {};
        for (;;)
        {
            Sleep(30);
            const bool bFocus = IsOurWindowFocused();
            for (int i = 0; i < 4; ++i)
            {
                const bool bDown = (GetAsyncKeyState(Keys[i]) & 0x8000) != 0;
                if (bDown && !Prev[i] && bFocus)
                    Bots::QueueCommand(Cmds[i]);
                Prev[i] = bDown;
            }
        }
        return 0;
    }
} // namespace

// =====================================================================================
//  Public API
// =====================================================================================
namespace Bots
{
    void Init(HMODULE Module)
    {
        if (GReady)
            return;
        LoadIni(Module);
        LoadSettings();
        if (!S.Enabled)
        {
            BLOG("Disabled in settings.ini ([Bots] Enabled=0)");
            return;
        }
        GGameThreadId = FindMainThreadId();
        BLOG("Game thread id: " + std::to_string(GGameThreadId));

        CreateThread(nullptr, 0, ConsoleThread, nullptr, 0, nullptr);
        if (S.Hotkeys)
            CreateThread(nullptr, 0, HotkeyThread, nullptr, 0, nullptr);

        GTickRateStart = Now();
        GReady = true;
        PrintHelp();
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

    void QueueCommand(const std::string& Command)
    {
        std::lock_guard<std::mutex> Lock(GCmdMutex);
        GCmdQueue.push_back(Command);
        GHasCommands = true;
    }

    void OnMatchStarted()
    {
        if (!GReady || GMatchSpawnDone)
            return;
        GMatchSpawnDone = true;
        const int Want = GCarryOver + ((S.SpawnOnMatchStart && !GLobbySpawned) ? (std::max)(0, S.Count) : 0);
        GCarryOver = 0;
        GSpawnNotBefore = Now() + 5.0; // let the plane take the players first
        GMatchStartedAt = Now();
        if (S.BotPeaceTime > 0.f)
            BLOG("Bots won't fight each other for the first " + std::to_string((int)(S.BotPeaceTime / 60.f)) + " min");
        DumpTeams(GWorld, "match start");
        if (Want > 0)
        {
            GPendingSpawns += Want;
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
        // a "match over / your result" screen for a player who is still alive while bots live: don't send it
        for (int i = 0; i < GNumResultFuncs; ++i)
        {
            if (GResultFuncs[i] != Func) continue;
            if (!S.KeepMatchWhileBotsAlive || !GMatchSpawnDone || !Obj || !Obj->IsA(AController::StaticClass()) || !HoldCondition())
                return false;
            APawn* P = static_cast<AController*>(Obj)->K2_GetPawn();
            if (!P || !ObjValid(P) || !P->IsA(ATslCharacter::StaticClass()) || static_cast<ATslCharacter*>(P)->Health <= 0.f)
                return false; // dead player: let them see their rank screen
            const double T = Now();
            if (T - GLastResultBlockLog > 10.0)
            {
                GLastResultBlockLog = T;
                BLOG(std::string("Blocked ") + Func->GetName() + " to a living player (bots still alive)");
            }
            return true;
        }
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
                const bool bDead = !P || !ObjValid(P) || !P->IsA(ATslCharacter::StaticClass()) || static_cast<ATslCharacter*>(P)->Health <= 0.f;
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
            if (Shooter && ObjValid(Shooter) && !IsBotPawn(Shooter))
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
            if (KillerPS && ObjValid(KillerPS)) Killer = KillerPS->PlayerName.ToString();
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
                BLOG(std::string("Match state -> WaitingPostMatch (end guard: ") + GGuardWhat + ", held " + std::to_string(GHeldEnds) + "x)");
                ScheduleExit("match state WaitingPostMatch");
                if (!GTraceDumped) { GTraceDumped = true; DumpTrace("before the match ended"); }
            }
            break;
        }
    }

    void OnMatchStarted();

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
        const bool bCmds = GHasCommands.load(std::memory_order_relaxed);
        if (!bThink && !bSteer && !bCmds)
            return;

        GInTick = true;

        static bool bLoggedFirstTick = false;
        if (!bLoggedFirstTick) { bLoggedFirstTick = true; BLOG("First tick on the game thread"); }

        UWorld* World = UWorld::GetWorld();
        if (World != GWorld)
            OnWorldChanged(World);

        if (World)
        {
            if (bCmds)
                RunQueuedCommands(World);

            if (bThink)
            {
                GLastThink = T;

                if (GAutoMatchStart && !GMatchSpawnDone)
                {
                    AGameStateBase* GSB = World->AuthorityGameMode->GameState;
                    if (GSB && GSB->IsA(ATslGameState::StaticClass()))
                    {
                        ATslGameState* GS = static_cast<ATslGameState*>(GSB);
                        const FName MS = GS->MatchState;
                        const int32 Rem = GS->RemainingTime;
                        const bool bStateChanged = GMatchStateKnown && (MS.GetComparisonIndex() != GLastMatchState.GetComparisonIndex() || MS.GetNumber() != GLastMatchState.GetNumber());
                        // last seconds of the lobby: take the lobby bots out BEFORE the game counts its starting
                        // players and the plane build seats everyone - they respawn on land after the start
                        if (!S.PlayerBots() && !S.RegisterTeams && !GPreStartCleared && Rem > 0 && Rem <= 3 && !GBots.empty())
                        {
                            int N = 0;
                            for (const FBot& B : GBots) if (B.State != EBotState::Dead) ++N;
                            RemoveAllBots();
                            GCarryOver += N;
                            GPreStartCleared = true;
                            BLOG("Lobby ending - " + std::to_string(N) + " lobby bots removed, they respawn on land after the plane leaves");
                        }
                        if (bStateChanged && GMatchSpawnDone)
                            DumpTeams(World, "match state -> " + MS.ToString());
                        if (bStateChanged && Rem <= 0 && GLastRemaining <= 0)
                        {
                            BLOG("Detected match start (MatchState " + MS.ToString() + ", RemainingTime " + std::to_string(Rem) + ")");
                            GatherCharacters(World);
                            NavCheck(World, false);   // the map's sublevels are in by now
                            Bots::OnMatchStarted();
                        }
                        if ((S.PlayerBots() || S.RegisterTeams) && S.SpawnOnMatchStart && !GLobbySpawned && !GMatchSpawnDone && Rem > 10 && HasRemotePlayer(World))
                        {
                            GLobbySpawned = true;
                            GPendingLobbySpawns += (std::max)(0, S.Count);
                            BLOG("Lobby: " + std::to_string(S.Count) + " bots joining now so they board the plane");
                        }
                        GLastMatchState = MS;
                        GLastRemaining = Rem;
                        GMatchStateKnown = true;
                    }
                }

                int Spawned = 0;
                while (GPendingLobbySpawns > 0 && Spawned < S.SpawnPerTick)
                {
                    --GPendingLobbySpawns; ++Spawned;
                    if (S.PlayerBots()) SpawnPlayerBotAt(World, nullptr);
                    else SpawnBotNearPlayer(World);
                }
                if (T < GSpawnNotBefore) Spawned = S.SpawnPerTick; // hold until the plane has everyone
                while (GPendingNearSpawns > 0 && Spawned < S.SpawnPerTick)
                {
                    --GPendingNearSpawns; ++Spawned;
                    SpawnBotNearPlayer(World);
                }
                while (GPendingSpawns > 0 && Spawned < S.SpawnPerTick)
                {
                    --GPendingSpawns; ++Spawned;
                    SpawnRandomBot(World);
                }

                static bool bLoggedFirstThink = false;
                if (!bLoggedFirstThink) { bLoggedFirstThink = true; BLOG("First AI think OK (no crash so far)"); }

                if (!GBots.empty())
                {
                    GatherCharacters(World);
                    CountEngaged();
                    LearnAimMode();
                    //UpdateHumanAirborne(World, T);
                    ProcessShots(T);
                    AssignHunters(T);
                    DressPass();
                    AppearancePass();
                    BodyPass(T);
                    for (FBot& B : GBots)
                    {
                        // AI LOD: full rate near players / in a fight, slower far away (spread so they don't all think on the same frame)
                        if (S.AiLod && GMatchSpawnDone && T < B.NextThinkAt && B.State != EBotState::Combat)
                            continue;
                        UpdateBot(World, B, T);
                        if (S.AiLod && GMatchSpawnDone && B.State != EBotState::Dead && ObjValid(B.Pawn, B.PawnIndex))
                        {
                            float HD = 1e9f; FVector HL;
                            NearestHuman(B.Pawn->K2_GetActorLocation(), 1e9f, HD, HL);
                            const double Every = (B.State == EBotState::Combat || HD < 60000.f) ? 0.0
                                : HD < 150000.f ? 0.3 : 1.0;
                            B.NextThinkAt = T + Every * RandF(0.8f, 1.2f);
                        }
                    }
                    GBots.erase(std::remove_if(GBots.begin(), GBots.end(),
                        [](const FBot& B) { return B.Remove; }), GBots.end());
                }
                else
                {
                    GatherCharacters(World);
                    DressPass();
                }
                SyncGameCounters(World);
                EndGuardThink(World, T);
                ZoneWatchThink(World, T);
                LobbyHoldThink(World);
                /*
                if (GMatchSpawnDone)
                {
                    CheckPlanes(World, T);
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

        GInTick = false;
    }
}


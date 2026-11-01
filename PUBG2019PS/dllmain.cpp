#pragma once

#include <Windows.h>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "SDK.hpp"
#include "../simpleini/SimpleIni.h"

#include "MinHook.h"

#include <mutex>
#include <deque>
#include <condition_variable>

#include "../PUBG2019PS/SDK/Engine_parameters.hpp"
#include "../PUBG2019PS/SDK/TslGame_parameters.hpp"

#include "Bots/BotLogic.h"
#include "Damage/SharedWeaponDamage.hpp"
#include "GhilliePolicy.hpp"

using namespace SDK;
#include "NativeNullGuard.inl"

void CUSTOMLOG(std::string LOG)
{
    static std::mutex LogMutex;
    static std::ofstream LogFile("PUBG2019PS_Server.log", std::ios::app);
    std::lock_guard<std::mutex> Lock(LogMutex);
    std::cout << LOG << std::endl;
    if (LogFile) LogFile << LOG << std::endl;
}

SDK::UClass* Get_PlayerPawn_Class();

using ProcessEvent_t = void (*)(UObject*, UFunction*, void*);
ProcessEvent_t GOriginalProcessEvent = nullptr;
static thread_local bool GPreparingPlayerSpawn = false;
static bool GFullTerrainReady = false;
static void UpdateServerTerrain(UWorld* World);
static void UpdateMatchStartup(UWorld* World);
struct FPendingSpawn { AController* Controller; int Index; };
static std::vector<FPendingSpawn> GPendingPlayerSpawns;

static void PreparePlayerSpawn(AController* Controller)
{
    if (!GFullTerrainReady) return;
    if (!Controller || !Controller->IsA(APlayerController::StaticClass())) return;
    auto* PC = static_cast<APlayerController*>(Controller);
    if (!PC || !PC->NetConnection || PC->bIsLocalPlayerController) return;
    auto* Pawn = PC->K2_GetPawn();
    if (!Pawn || !Pawn->IsA(ATslCharacter::StaticClass())) return;
    auto* Character = static_cast<ATslCharacter*>(Pawn);
    auto* World = UWorld::GetWorld();
    if (!World || !World->AuthorityGameMode || !World->AuthorityGameMode->GameState) return;
    if (World->AuthorityGameMode->IsA(ATslTrainingGameMode::StaticClass())) return;
    Character->Health = Character->HealthMax;
    auto* BaseState = World->AuthorityGameMode->GameState;
    if (!BaseState->IsA(ATslGameState::StaticClass())) return;
    auto* State = static_cast<ATslGameState*>(BaseState);
    if (State->MatchState.ToString() != "WaitingToStart" || Character->IsInAircraft()) return;
    const FVector Position = Character->K2_GetActorLocation();
    auto* Movement = Character->GetTslCharacterMovement();
    FFindFloorResult Floor{};
    if (Movement) Movement->K2_FindFloor(Position, &Floor);
    CUSTOMLOG("[PUBG2019PS] Spawn ready: " + Character->GetName() + " at " + std::to_string(Position.X) + "," +
        std::to_string(Position.Y) + "," + std::to_string(Position.Z) + " floor=" + std::to_string((int)Floor.bWalkableFloor));
    if (Floor.bBlockingHit && Floor.bWalkableFloor && !Floor.HitResult.bStartPenetrating && Floor.FloorDist <= 10.f) return;
    FHitResult Hit{};
    TArray<AActor*> Ignore;
    FLinearColor Color{};
    FVector Top = Position, Bottom = Position;
    Top.Z = 100000.f; Bottom.Z = -30000.f;
    const bool Found = UKismetSystemLibrary::LineTraceSingle(Character, Top, Bottom, ETraceTypeQuery::TraceTypeQuery1,
        false, Ignore, EDrawDebugTrace::None, &Hit, true, Color, Color, 0.f);
    if (!Found || !Hit.bBlockingHit || Hit.ImpactNormal.Z < 0.6f) {
        CUSTOMLOG("[PUBG2019PS] Spawn floor not ready; native movement correction retained.");
        return;
    }
    float HalfHeight = 88.f;
    auto* Root = Character->K2_GetRootComponent();
    if (Root && Root->IsA(UCapsuleComponent::StaticClass())) HalfHeight = static_cast<UCapsuleComponent*>(Root)->GetScaledCapsuleHalfHeight();
    FVector Safe = Position; Safe.Z = Hit.ImpactPoint.Z + HalfHeight + 2.f;
    if (Position.Z >= Safe.Z - 5.f) return;
    FHitResult Sweep{};
    if (Character->K2_SetActorLocation(Safe, false, &Sweep, true)) {
        if (Movement) Movement->StopMovementImmediately();
        Character->ForceNetUpdate();
        PC->ClientSetLocation(Safe, Character->K2_GetActorRotation());
        CUSTOMLOG("[PUBG2019PS] Initial lobby spawn raised onto traced floor: " + std::to_string(Position.Z) + " -> " + std::to_string(Safe.Z));
    }
}

    void HookedProcessEvent(UObject* Obj, UFunction* Func, void* Parms)
    {
        // Native events may arrive on worker threads. The add-on reads and
        // modifies game state, so only the game thread may run that logic.
        // Forward the original call unchanged on every other thread.
        if (!Bots::IsGameThread() || !Obj || !Func) {
            GOriginalProcessEvent(Obj, Func, Parms);
            return;
        }
        bool OriginalHitCalled=false;
        // Reject only the two map-inappropriate ghillie variants at the
        // server equipment API. Ordinary clothes and every other map are left
        // to the native equipment implementation.
        static UFunction* TryEquipGhillie = nullptr;
        static UFunction* EquipGhillie = nullptr;
        if ((!TryEquipGhillie || !EquipGhillie) && AEquipment::StaticClass()) {
            TryEquipGhillie=AEquipment::StaticClass()->GetFunction("Equipment","TryEquipItem");
            EquipGhillie=AEquipment::StaticClass()->GetFunction("Equipment","EquipItem");
        }
        if (Parms && (Func==TryEquipGhillie || Func==EquipGhillie) && Obj->IsA(AEquipment::StaticClass())) {
            UItem* Item=Func==TryEquipGhillie ? static_cast<Params::Equipment_TryEquipItem*>(Parms)->Item :
                static_cast<Params::Equipment_EquipItem*>(Parms)->Item;
            if (Item && SharedWeaponDamage::Live(Item,Item->GetIndex()) && GhilliePolicy::IsGhillie(Item->GetClass()->GetName())) {
                UWorld* World=UWorld::GetWorld();
                const std::string Level=World ? UGameplayStatics::GetCurrentLevelName(World,true).ToString() : std::string();
                if (GhilliePolicy::Forbidden(Level,Item->GetClass()->GetName())) {
                    if (Func==TryEquipGhillie) static_cast<Params::Equipment_TryEquipItem*>(Parms)->ReturnValue=false;
                    else static_cast<Params::Equipment_EquipItem*>(Parms)->ReturnValue=false;
                    CUSTOMLOG("[PUBG2019PS] Map ghillie denied " + Level + " " + Item->GetClass()->GetName());
                    return;
                }
            }
        }
        AController* RestartedPlayer = nullptr;
        if (Parms && Func->GetName() == "K2_OnRestartPlayer")
            RestartedPlayer = static_cast<Params::GameModeBase_K2_OnRestartPlayer*>(Parms)->NewPlayer;
        // Retain the engine's native movement corrections, including during
        // client terrain loading. The original blanket suppression is removed.

        if (Parms && Func->GetName()=="ServerNotifyHit" && Obj->IsA(ATslWeapon_Trajectory::StaticClass()))
        {
            auto* Args=static_cast<Params::TslWeapon_Trajectory_ServerNotifyHit*>(Parms);
            auto* Weapon=static_cast<ATslWeapon_Trajectory*>(Obj);
            auto* Object=SharedWeaponDamage::Weak(Args->Impact.Actor);
            auto* Owner=Weapon->GetPawnOwner();
            const bool HumanOwner=Owner && SharedWeaponDamage::Live(Owner,Owner->GetIndex()) && Owner->Controller && Owner->Controller->IsA(APlayerController::StaticClass());
            static int HumanHitDiagnostics=0;
            if (HumanOwner && !Object && HumanHitDiagnostics++ < 20)
                CUSTOMLOG("[PUBG2019PS] HUMAN HIT unresolved actor=" + std::to_string(Args->Impact.Actor.ObjectIndex) + " serial=" + std::to_string(Args->Impact.Actor.ObjectSerialNumber));
            if (Object && Object->IsA(AActor::StaticClass()) && Owner && SharedWeaponDamage::Live(Owner,Owner->GetIndex()))
            {
                auto* Victim=static_cast<AActor*>(Object);
                const int32 VictimIndex=Victim->GetIndex();
                const bool IsChar=Victim->IsA(ATslCharacter::StaticClass());
                // On this build the RPC TraceStart can be unrelated to world
                // coordinates, causing a close hit to appear kilometres away.
                // For characters use the server's authoritative positions.
                // Keep the legacy geometric calculation for other actors.
                const FVector Delta=IsChar ? Victim->K2_GetActorLocation()-Owner->K2_GetActorLocation() : Args->Impact.ImpactPoint-Args->TraceStart;
                const float Straight=std::sqrt(Delta.X*Delta.X+Delta.Y*Delta.Y+Delta.Z*Delta.Z);
                float Damage=0.f; TSubclassOf<UDamageType> Type(nullptr);
                const bool Resolved=std::isfinite(Straight) && Straight>0.f && SharedWeaponDamage::Resolve(Weapon,Straight,Damage,Type);
                if (HumanOwner && IsChar && !Resolved && HumanHitDiagnostics++ < 20)
                    CUSTOMLOG("[PUBG2019PS] HUMAN HIT invalid weapon data victim=" + Victim->GetName() + " distance_cm=" + std::to_string(Straight));
                if (Resolved)
                {
                    bool NativeHandled=false;
                    const float HealthBefore=IsChar ? static_cast<ATslCharacter*>(Victim)->Health : 0.f;
                    float Applied=0.f;
                    if (Victim->IsA(ATslCharacter::StaticClass()))
                    {
                        auto* Character=static_cast<ATslCharacter*>(Victim);
                        const int32 Index=Character->GetIndex();
                        const float HP=Character->Health, Groggy=Character->GroggyHealth;
                        const uint8 HitByte=Character->LastTakeHitInfo.EnsureReplicationByte;
                        GOriginalProcessEvent(Obj,Func,Parms); OriginalHitCalled=true;
                        NativeHandled=!SharedWeaponDamage::Live(Character,Index) || Character->Health<HP ||
                            Character->GroggyHealth<Groggy || Character->LastTakeHitInfo.EnsureReplicationByte!=HitByte;
                    }
                    if (!NativeHandled) {
                        const FVector Direction=Delta/Straight;
                        Applied=UGameplayStatics::ApplyPointDamage(Victim,Damage,Direction,Args->Impact,Owner->Controller,Weapon,Type);
                    }
                    if (HumanOwner && IsChar && HumanHitDiagnostics++ < 20 && SharedWeaponDamage::Live(Victim,VictimIndex))
                        CUSTOMLOG("[PUBG2019PS] HUMAN HIT victim=" + Victim->GetName() + " hp=" + std::to_string(HealthBefore) +
                            "->" + std::to_string(static_cast<ATslCharacter*>(Victim)->Health) + " applied=" + std::to_string(Applied) +
                            " raw=" + std::to_string(Damage) + " native=" + std::to_string(NativeHandled ? 1 : 0) +
                            " bot=" + std::to_string(Bots::IsBotPawn(static_cast<APawn*>(Victim)) ? 1 : 0));
                    static int HitPathLogs=0;
                    if (HitPathLogs++<20) CUSTOMLOG(std::string("[PUBG2019PS] Human hit path=")+(NativeHandled ? "native" : "weapon-data fallback"));
                }
            }
        }

        Bots::OnCall(Obj, Func, Parms);

        if (Bots::ShouldBlockCall(Obj, Func))
            return;

        if (!OriginalHitCalled) GOriginalProcessEvent(Obj, Func, Parms);
        Bots::AfterCall();
        static LONG ReportedNullGuardHits = 0;
        const LONG CurrentNullGuardHits = InterlockedCompareExchange(&GNullGuardHits,0,0);
        if (CurrentNullGuardHits > ReportedNullGuardHits) { ReportedNullGuardHits = CurrentNullGuardHits; CUSTOMLOG("Native null guard intervened, count=" + std::to_string(CurrentNullGuardHits)); }
        static LONG ReportedNullContextHits = 0;
        const LONG CurrentNullContextHits = InterlockedCompareExchange(&GNullContextHits,0,0);
        if (CurrentNullContextHits > ReportedNullContextHits) { ReportedNullContextHits = CurrentNullContextHits; CUSTOMLOG("PRIVATE TEST: second null guard intervened, count=" + std::to_string(CurrentNullContextHits)); }
        static LONG ReportedNullCharacterHits = 0;
        const LONG CurrentNullCharacterHits = InterlockedCompareExchange(&GNullCharacterHits,0,0);
        if (CurrentNullCharacterHits > ReportedNullCharacterHits) { ReportedNullCharacterHits = CurrentNullCharacterHits; CUSTOMLOG("PRIVATE TEST: character null guard intervened, count=" + std::to_string(CurrentNullCharacterHits)); }

        if (!GPreparingPlayerSpawn && Bots::IsGameThread()) UpdateServerTerrain(UWorld::GetWorld());

        if (RestartedPlayer && !GPreparingPlayerSpawn && Bots::IsGameThread()) {
            GPendingPlayerSpawns.push_back({RestartedPlayer, RestartedPlayer->GetIndex()});
        }
        if (GFullTerrainReady && !GPreparingPlayerSpawn && Bots::IsGameThread() && !GPendingPlayerSpawns.empty()) {
            GPreparingPlayerSpawn = true;
            auto Pending = std::move(GPendingPlayerSpawns);
            GPendingPlayerSpawns.clear();
            for (const auto& Entry : Pending) {
                if (Entry.Index >= 0 && Entry.Index < UObject::GObjects->Num() && UObject::GObjects->GetByIndex(Entry.Index) == Entry.Controller)
                    PreparePlayerSpawn(Entry.Controller);
            }
            GPreparingPlayerSpawn = false;
        }

        if (Func && Bots::IsEndMatchQuery(Func))
            Bots::FilterEndMatch(Parms);

        static thread_local bool bInsideTick = false;
        static thread_local double LastTick = 0.0;

        const double CurrentTime = GetTickCount64() / 1000.0;

        if (!GPreparingPlayerSpawn && !bInsideTick && CurrentTime - LastTick >= 0.01)
        {
            bInsideTick = true;
            LastTick = CurrentTime;

            Bots::Tick();
            UpdateMatchStartup(UWorld::GetWorld());

            bInsideTick = false;
        }
    }

    enum ENetMode
    {
        NM_Standalone = 0,
        NM_DedicatedServer = 1,
        NM_ListenServer = 2,
        NM_Client = 3
    };

    __int64 (*GetNetModeO)(SDK::UNetDriver* NetDriver);
    __int64 GetNetModeHook(SDK::UNetDriver* NetDriver)
    {
        return ENetMode::NM_DedicatedServer;
    }

    DWORD HookGetNetMode()
    {
        uintptr_t ProcessEventAddr = (uintptr_t(GetModuleHandle(0)) + 0x38E5810);
        GetNetModeO = decltype(GetNetModeO)(ProcessEventAddr);

        auto HookResult = MH_CreateHook((PVOID&)ProcessEventAddr, GetNetModeHook, reinterpret_cast <LPVOID*> (&GetNetModeO));
        std::string HookResultString = MH_StatusToString(HookResult);

        if (HookResult != MH_STATUS::MH_OK) {
            CUSTOMLOG("GetNetMode Hook CREATED FAILED WITH REASON : " + HookResultString + " !");
            return FALSE;
        }

        CUSTOMLOG("GetNetMode Hook CREATED GOOD!");

        if (MH_EnableHook((PVOID&)ProcessEventAddr) != MH_STATUS::MH_OK)
        {
            CUSTOMLOG("GetNetMode Hook ENABLE FAILED!");
            return FALSE;
        }
        else
        {
            CUSTOMLOG("GetNetMode Hook Enabled!");
        }

        uintptr_t ProcessEventAddr2 = (uintptr_t(GetModuleHandle(0)) + 0x2640460);

        auto HookResult2 = MH_CreateHook((PVOID&)ProcessEventAddr2, GetNetModeHook, reinterpret_cast <LPVOID*> (&GetNetModeO));
        std::string HookResultString2 = MH_StatusToString(HookResult2);

        if (HookResult2 != MH_STATUS::MH_OK) {
            CUSTOMLOG("AActor::GetNetMode Hook CREATED FAILED WITH REASON : " + HookResultString2 + " !");
            return FALSE;
        }

        CUSTOMLOG("AActor::GetNetMode Hook CREATED GOOD!");

        if (MH_EnableHook((PVOID&)ProcessEventAddr2) != MH_STATUS::MH_OK)
        {
            CUSTOMLOG("AActor::GetNetMode Hook ENABLE FAILED!");
            return FALSE;

        }
        else
        {
            CUSTOMLOG("AActor::GetNetMode Hook Enabled!");
        }

        uintptr_t ProcessEventAddr3 =
            (uintptr_t(GetModuleHandle(0)) + 0x1A48A60);

        auto HookResult3 = MH_CreateHook((PVOID&)ProcessEventAddr3, GetNetModeHook, reinterpret_cast <LPVOID*> (&GetNetModeO));
        std::string HookResultString3 = MH_StatusToString(HookResult3);

        if (HookResult3 != MH_STATUS::MH_OK) {
            CUSTOMLOG("UActorComponent::GetNetMode Hook CREATED FAILED WITH REASON : " + HookResultString3 + " !");
            return FALSE;
        }

        CUSTOMLOG("UActorComponent::GetNetMode Hook CREATED GOOD!");

        if (MH_EnableHook((PVOID&)ProcessEventAddr3) != MH_STATUS::MH_OK)
        {
            CUSTOMLOG("UActorComponent::GetNetMode Hook ENABLE FAILED!");
            return FALSE;

        }
        else
        {
            CUSTOMLOG("UActorComponent::GetNetMode Hook Enabled!");
        }
    }

    struct FProp
    {
        SDK::UProperty* Property = nullptr;
        std::string Type;
        int32 Off = 0;
        int32 ByteOff = 0;
        uint8 Mask = 0xFF;

        bool Ok() const
        {
            return Property != nullptr;
        }
    };

    static FProp ObjProp(SDK::UObject* O, const char* Name)
    {
        if (!O || !Name)
            return {};

        static std::unordered_map<std::string, FProp> Cache;
        static std::mutex CacheMutex;

        SDK::UClass* Cls = O->GetClass();
        if (!Cls)
            return {};

        std::string Key = Cls->GetName();
        Key += "::";
        Key += Name;

        {
            std::lock_guard<std::mutex> Lock(CacheMutex);
            auto It = Cache.find(Key);
            if (It != Cache.end())
                return It->second;
        }

        FProp Result{};

        for (SDK::UStruct* S = Cls; S; S = S->Super)
        {
            for (SDK::UField* Field = S->Children; Field; Field = Field->Next)
            {
                if (!Field->IsA(SDK::UProperty::StaticClass()))
                    continue;

                auto* P = static_cast<SDK::UProperty*>(Field);

                if (P->GetName() != Name)
                    continue;

                Result.Property = P;
                Result.Off = P->Offset;

                if (P->IsA(SDK::UBoolProperty::StaticClass()))
                {
                    auto* BP = static_cast<SDK::UBoolProperty*>(P);
                    Result.Type = "BoolProperty";
                    Result.ByteOff = BP->ByteOffset;
                    Result.Mask = BP->FieldMask;
                }
                else
                {
                    Result.Type = "OtherProperty";
                }

                break;
            }

            if (Result.Ok())
                break;
        }

        if (Result.Ok())
        {
            std::lock_guard<std::mutex> Lock(CacheMutex);
            Cache.emplace(Key, Result);
        }

        return Result;
    }

    bool WriteBool(SDK::UObject* O, const char* Name, bool V)
    {
        FProp P = ObjProp(O, Name);

        if (!O || !P.Ok() || P.Type != "BoolProperty")
            return false;

        auto* B = reinterpret_cast<uint8*>(reinterpret_cast<uint8*>(O) + P.Off + P.ByteOff);

        if (P.Mask == 0xFF)
            *B = V ? 1 : 0;
        else
            *B = V ? (*B | P.Mask) : (*B & ~P.Mask);

        return true;
    }

    bool WriteUInt8(SDK::UObject* O, const char* Name, uint8 V)
    {
        FProp P = ObjProp(O, Name);

        if (!O || !P.Ok() || P.Type != "ByteProperty")
            return false;

        auto* B = reinterpret_cast<uint8*>(reinterpret_cast<uint8*>(O) + P.Off + P.ByteOff);

        if (P.Mask == 0xFF)
            *B = V;
        else
            *B = (*B & ~P.Mask) | (V & P.Mask);

        return true;
    }

    static void UpdateServerTerrain(UWorld* World)
    {
        static UWorld* LastWorld = nullptr;
        static ULONGLONG NextCheck = 0, NextLog = 0;
        if (World != LastWorld) {
            LastWorld = World; NextCheck = NextLog = 0;
            GFullTerrainReady = false;
            Bots::SetTerrainReady(false);
            GPendingPlayerSpawns.clear();
        }
        if (!World || GetTickCount64() < NextCheck) return;
        NextCheck = GetTickCount64() + 500;
        // The engine is still constructing the world's initial streaming
        // set during EnteringMap. Pinning it at that point can strand both
        // heightmap LODs and ordinary streamed levels indefinitely.
        if (World->AuthorityGameMode && World->AuthorityGameMode->IsA(AGameMode::StaticClass())) {
            auto* Mode = static_cast<AGameMode*>(World->AuthorityGameMode);
            if (Mode->MatchState.ToString() == "EnteringMap") {
                if (GetTickCount64() >= NextLog) {
                    CUSTOMLOG("[PUBG2019PS] Waiting for native initial map streaming (EnteringMap).");
                    NextLog = GetTickCount64() + 5000;
                }
                return;
            }
        }
        int Terrain = 0, Detailed = 0, Changed = 0;
        for (ULevelStreaming* L : World->StreamingLevels) {
            if (!L) continue;
            const auto Package = L->PackageNameToLoad.ToString();
            if (L->PackageName.ToString().find("eather") != std::string::npos || Package.find("eather") != std::string::npos) continue;
            L->bDisableDistanceStreaming = true;
            L->bShouldBeLoaded = true;
            L->bShouldBeVisible = true;
            L->bShouldBlockOnLoad = false;
            // Different maps spell the same heightmap prefix differently
            // (e.g. Miramar's HeightMap_ and Baltic's Heightmap_).
            const std::string HeightMapPrefix = "heightmap_";
            auto IsSameAscii = [](char A, char B) {
                if (A >= 'A' && A <= 'Z') A = char(A + ('a' - 'A'));
                if (B >= 'A' && B <= 'Z') B = char(B + ('a' - 'A'));
                return A == B;
            };
            if (std::search(Package.begin(), Package.end(), HeightMapPrefix.begin(), HeightMapPrefix.end(), IsSameAscii) == Package.end()) continue;
            ++Terrain;
            // INDEX_NONE selects the original package; index 0 selects _LOD1.
            // Pinning a tile without clearing this index pinned coarse collision.
            if (L->LevelLODIndex != -1) { L->LevelLODIndex = -1; ++Changed; }
            if (L->LoadedLevel && L->LoadedLevel->GetFullName().find("_LOD") == std::string::npos) ++Detailed;
        }
        const bool Ready = Terrain > 0 && Detailed == Terrain;
        if (Ready && !GFullTerrainReady) {
            CUSTOMLOG("[PUBG2019PS] Server terrain ready: " + std::to_string(Detailed) + "/" + std::to_string(Terrain) + " original heightmap packages loaded.");
        } else if (!Ready && GetTickCount64() >= NextLog) {
            CUSTOMLOG("[PUBG2019PS] Server terrain loading: " + std::to_string(Detailed) + "/" + std::to_string(Terrain) + " detailed, LOD selections reset=" + std::to_string(Changed));
            NextLog = GetTickCount64() + 5000;
        }
        GFullTerrainReady = Ready;
        Bots::SetTerrainReady(Ready);
    }

    class AActor* SpawnActorFromClass_Main(UObject* WorldContextObject, TSubclassOf<AActor>ActorClass, FTransform& SpawnTransform, ESpawnActorCollisionHandlingMethod CollisionHandlingOverride, AActor* Owner)
    {
        auto Spawned = UGameplayStatics::BeginDeferredActorSpawnFromClass(WorldContextObject, ActorClass, SpawnTransform, CollisionHandlingOverride, Owner);
        Spawned = UGameplayStatics::FinishSpawningActor(Spawned, SpawnTransform);

        return Spawned;
    }

    void InitUEConsole()
    {
        SDK::UEngine* Engine = SDK::UEngine::GetEngine();
        if (!Engine || !Engine->GameViewport || !Engine->ConsoleClass) return;
        auto* Input = SDK::UInputSettings::GetDefaultObj();
        if (Input && Input->ConsoleKeys.Num() > 0)
            Input->ConsoleKeys[0].KeyName = SDK::UKismetStringLibrary::Conv_StringToName(L"F2");
        SDK::UObject* NewObject = SDK::UGameplayStatics::SpawnObject(Engine->ConsoleClass, Engine->GameViewport);
        if (NewObject) Engine->GameViewport->ViewportConsole = static_cast<SDK::UConsole*>(NewObject);
    }

    DWORD WINAPI MainThread(LPVOID Param)
    {
        HMODULE Module = static_cast<HMODULE>(Param);

        AllocConsole();

        // Keep the server's existing console alive, but display its log in the
        // launcher when that launcher explicitly requests integrated mode.
        char LauncherConsole[4]{};
        if (GetEnvironmentVariableA("PUBG2019_LAUNCHER_LOG", LauncherConsole, sizeof(LauncherConsole)) == 1 && LauncherConsole[0] == '1') {
            HWND Console = GetConsoleWindow();
            if (Console) ShowWindow(Console, SW_HIDE);
        }

        FILE* Dummy;
        freopen_s(&Dummy, "CONOUT$", "w", stderr);
        freopen_s(&Dummy, "CONOUT$", "w", stdout);
        freopen_s(&Dummy, "CONIN$", "r", stdin);

        MH_STATUS St = MH_Initialize();
        if (St != MH_OK && St != MH_ERROR_ALREADY_INITIALIZED)
        {
            CUSTOMLOG(std::string("[PUBG2019PS] MH_Initialize failed: ") + MH_StatusToString(St));
            return 0;
        }

        // Do not alter engine network mode while the native map is still
        // initializing. Wait for its actual lobby, then install our hooks.
        CUSTOMLOG("[PUBG2019PS] Waiting for native server readiness before enabling hooks.");
        UWorld* World = nullptr;
        ATslGameMode* ReadyGM = nullptr;
        for (int i = 0; i < 1200; ++i)
        {
            World = UWorld::GetWorld();
            auto* Mode = World ? World->AuthorityGameMode : nullptr;
            if (Mode && Mode->IsA(ATslGameMode::StaticClass())) {
                auto* Candidate = static_cast<ATslGameMode*>(Mode);
                if (Candidate->GameState && Candidate->TslGameState && Candidate->MatchPreparer) {
                    const std::string State = Candidate->MatchState.ToString();
                    const bool IsTraining = Candidate->IsA(ATslTrainingGameMode::StaticClass());
                    if (State == "WaitingToStart" || (IsTraining && State != "EnteringMap" && State != "Aborted" && State != "LeavingMap")) {
                        ReadyGM = Candidate; break;
                    }
                    if (State == "InProgress" || State == "WaitingPostMatch" || State == "LeavingMap" || State == "Aborted") {
                        CUSTOMLOG("[PUBG2019PS] Native startup refused: unexpected state " + State); return 0;
                    }
                }
            }
            if (i % 10 == 0) CUSTOMLOG("[PUBG2019PS] Native server is still loading.");
            Sleep(500);
        }
        if (!ReadyGM || !World)
        {
            CUSTOMLOG("[PUBG2019PS] No complete server world was found; initialization stopped.");
            return 0;
        }
        HookGetNetMode();
        InitUEConsole();
        CUSTOMLOG("[PUBG2019PS] Native server readiness confirmed.");
        CUSTOMLOG("GameMode: " + ReadyGM->GetName());
        CUSTOMLOG("GameState: " + ReadyGM->GameState->GetName());
        CUSTOMLOG("World: " + World->GetName());
        CUSTOMLOG("[PUBG2019PS] Server world found.");

        Bots::SetAutoDetectMatchStart(true);
        Bots::Init(Module);
        Bots::Prewarm();
        CUSTOMLOG(InstallNullGuard() ? "Native null guard installed" : "Native null guard installation failed");
        CUSTOMLOG(InstallNullContextGuard() ? "PRIVATE TEST: second null guard installed" : "PRIVATE TEST: second null guard installation failed");
        CUSTOMLOG(InstallNullCharacterGuard() ? "PRIVATE TEST: character null guard installed" : "PRIVATE TEST: character null guard installation failed");


        void* Target = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr)) + Offsets::ProcessEvent);
        St = MH_CreateHook(Target, reinterpret_cast<void*>(&HookedProcessEvent), reinterpret_cast<void**>(&GOriginalProcessEvent));
        if (St != MH_OK)
        {
            CUSTOMLOG(std::string("[PUBG2019PS] MH_CreateHook(ProcessEvent) failed: ") + MH_StatusToString(St));
            return 0;
        }
        St = MH_EnableHook(Target);
        if (St != MH_OK)
        {
            CUSTOMLOG(std::string("[PUBG2019PS] MH_EnableHook failed: ") + MH_StatusToString(St));
            return 0;
        }
        CUSTOMLOG("[PUBG2019PS] ProcessEvent hooked - bots are live. Type 'help' in this console.");

        ATslGameMode* GM = (ATslGameMode*)UGameplayStatics::GetGameMode(UWorld::GetWorld());
        CUSTOMLOG("MatchPreparer: " + GM->MatchPreparer->GetName());
        CUSTOMLOG("MatchStartType: " + (uint8)GM->MatchStartType);
        CUSTOMLOG("MatchState: " + GM->MatchState.ToString());

        GM->TslGameState->NumStartPlayers = 0;
        GM->TslGameState->NumStartTeams = 0;

        //GM->MatchPreparer->WarmupTime = 50;
        //GM->MatchPreparer->TimerGameStartCountDown();

        //return 0;
    }

#include "MatchStartup.inl"

SDK::UClass* Get_PlayerPawn_Class()
{
    return static_cast<SDK::UClass*>(SDK::UObject::FindObject("BlueprintGeneratedClass PlayerPawn_v2.PlayerPawn_v2_C"));
}

BOOL APIENTRY DllMain(HMODULE Module, DWORD Reason, LPVOID)
{
    if (Reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(Module);
        CreateThread(nullptr, 0, MainThread, Module, 0, nullptr);
    }
    return TRUE;
}

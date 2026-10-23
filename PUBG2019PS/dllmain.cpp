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

#include "BotLogic.h"
#include "AirplaneLogic.h"

using namespace SDK;

void CUSTOMLOG(std::string LOG)
{
    std::cout << LOG << std::endl;
}

SDK::UClass* Get_PlayerPawn_Class();

constexpr float kPi = 3.14159265f;

std::string GDir;

double NowSec1() { return GetTickCount64() / 1000.0; }

    std::mutex  GLogMutex;
    std::string GLogPath;
    HANDLE      GConOut = INVALID_HANDLE_VALUE;

    using ProcessEvent_t = void (*)(UObject*, UFunction*, void*);
    ProcessEvent_t GOriginalProcessEvent = nullptr;

    void HookedProcessEvent(UObject* Obj, UFunction* Func, void* Parms)
    {
        if (Func->GetName() == "K2_OnSetMatchState")
        {
            auto Params = (Params::GameMode_K2_OnSetMatchState*)Parms;
            if (Params->NewState.ToString() == "InProgress")
            {
                CUSTOMLOG("Loading airplane logic");
                StartAirplane();
            }
        }

        Bots::OnCall(Obj, Func, Parms);

        if (Bots::ShouldBlockCall(Obj, Func))
            return;

        GOriginalProcessEvent(Obj, Func, Parms);

        if (Func && Bots::IsEndMatchQuery(Func))
            Bots::FilterEndMatch(Parms);

        static thread_local bool bInsideTick = false;
        static thread_local double LastTick = 0.0;

        const double CurrentTime = NowSec1();

        if (!bInsideTick && CurrentTime - LastTick >= 0.01)
        {
            bInsideTick = true;
            LastTick = CurrentTime;

            Bots::Tick();

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

    void DisableLevelStreaming(UWorld* World)
    {
        for (ULevelStreaming* L : World->StreamingLevels) {
            if (!L) continue;
             
            if (L->GetName().contains("3")) continue;

            if (L->PackageName.ToString().contains("eather")) continue;
            if (L->PackageNameToLoad.ToString().contains("eather")) continue;

            WriteBool(L, "bDisableDistanceStreaming", true);
            WriteBool(L, "bShouldBeLoaded", true);
            WriteBool(L, "bShouldBeVisible", true);
        }
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
        SDK::UWorld* World = SDK::UWorld::GetWorld();
        SDK::APlayerController* MyController = UGameplayStatics::GetPlayerController(World, 0);
        SDK::UInputSettings::GetDefaultObj()->ConsoleKeys[0].KeyName = SDK::UKismetStringLibrary::Conv_StringToName(L"F2");
        SDK::UObject* NewObject = SDK::UGameplayStatics::SpawnObject(Engine->ConsoleClass, Engine->GameViewport);
        Engine->GameViewport->ViewportConsole = static_cast<SDK::UConsole*>(NewObject);
    }

    DWORD WINAPI MainThread(LPVOID Param)
    {
        HMODULE Module = static_cast<HMODULE>(Param);

        AllocConsole();

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

        HookGetNetMode();
        InitUEConsole();

        CUSTOMLOG("GameMode: " + UGameplayStatics::GetGameMode(UWorld::GetWorld())->GetName());
        CUSTOMLOG("GameState: " + UGameplayStatics::GetGameState(UWorld::GetWorld())->GetName());
        CUSTOMLOG("World: " + UWorld::GetWorld()->GetName());

        // Only plain memory reads on this thread (no ProcessEvent calls) while we wait.
        bool bServer = false;
        for (int i = 0; i < 1200 && !bServer; ++i) // up to 10 minutes
        {
            UWorld* World = UWorld::GetWorld();
            if (World && UGameplayStatics::GetGameMode(UWorld::GetWorld()) && UGameplayStatics::GetGameMode(UWorld::GetWorld())->IsA(ATslGameMode::StaticClass()))
                bServer = true;
            else
                CUSTOMLOG("Current GameMode: " + UGameplayStatics::GetGameMode(UWorld::GetWorld())->GetName());  Sleep(500);
        }
        if (!bServer)
        {
            CUSTOMLOG("[PUBG2019PS] No ATslGameMode found (is this the client, or did the map not load?) - not starting.");
            return 0;
        }
        CUSTOMLOG("[PUBG2019PS] Server world found.");

        Bots::SetAutoDetectMatchStart(true);
        Bots::Init(Module);
        Bots::Prewarm();

        DisableLevelStreaming(UWorld::GetWorld());
        Sleep(15000);

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

        GM->TslGameState->bTimerPaused = false;

        GM->TslGameState->NumStartPlayers = 0;
        GM->TslGameState->NumStartTeams = 0;

        GM->TeamSize = 1;
        GM->bEnableSquadInTeam = false;
        GM->TslGameState->TeamSize = 1;

        //GM->MatchPreparer->WarmupTime = 50;
        //GM->MatchPreparer->TimerGameStartCountDown();

        //return 0;

        while (true)
        {
            Sleep(200);
            PlaneThink(UWorld::GetWorld());
        }
    }

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

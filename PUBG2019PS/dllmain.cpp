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

using namespace SDK;
#include "NativeNullGuard.inl"

void CUSTOMLOG(std::string LOG)
{
    std::cout << LOG << std::endl;
}

class AActor* SpawnActorFromClass(
    class UObject* WorldContextObject, TSubclassOf < class AActor > ActorClass,
    const struct FTransform& SpawnTransform,
    ESpawnActorCollisionHandlingMethod CollisionHandlingOverride,
    class AActor* Owner) {
    auto Spawned = UGameplayStatics::BeginDeferredActorSpawnFromClass(
        WorldContextObject, ActorClass, SpawnTransform, CollisionHandlingOverride,
        Owner);

    Spawned = UGameplayStatics::FinishSpawningActor(Spawned, SpawnTransform);

    return Spawned;
}

SDK::UClass* Get_PlayerPawn_Class();

using ProcessEvent_t = void (*)(UObject*, UFunction*, void*);
ProcessEvent_t GOriginalProcessEvent = nullptr;
UMatchPreparer* GLOB_OLD_MatchPreparer = nullptr;

    void HookedProcessEvent(UObject* Obj, UFunction* Func, void* Parms)
    {
        if (Obj->IsA(UCharacterMovementComponent::StaticClass()))
        {
            UCharacterMovementComponent* CMC = (UCharacterMovementComponent*)Obj;
            CMC->bIgnoreClientMovementErrorChecksAndCorrection = true;
        }

        if (Func->GetName() == "ServerNotifyHit")
        {
            CUSTOMLOG("[PUBG2019PS] ServerNotifyHit by " + Obj->GetName());
            auto Args = (Params::TslWeapon_Trajectory_ServerNotifyHit*)Parms;

            ATslWeapon_Trajectory* Weapon = (ATslWeapon_Trajectory*)Obj;
            auto Config = Weapon->WeaponTrajectoryData->TrajectoryConfig;

            bool bIsHeadShot = 0;
            bool bIsUpperBody = 0;

            float finalDamage = 0.0f; // Final damage calculation
            float HitCoefficient = 0.0f; // damage multiplier value by body part
            float rawDamage = 0.0f;

            if (Args->Impact.BoneName.ToString() == "Head")
            {
                printf("\n Shoot At Head \n");
                HitCoefficient = 1.2f;
                bIsHeadShot = 1;
            }

            if (Args->Impact.BoneName.ToString() == "neck_01")
            {
                printf("\n Shoot At neck_01 \n");
                HitCoefficient = 1.2f;
                bIsHeadShot = 1;
            }

            if (Args->Impact.BoneName.ToString() == "clavicle_r" || Args->Impact.BoneName.ToString() == "clavicle_l")
            {
                printf("\n Shoot At clavicle_ \n");

                HitCoefficient = 0.8f;
            }

            if (Args->Impact.BoneName.ToString() == "spine_02" || Args->Impact.BoneName.ToString() == "spine_03")
            {
                printf("\n Shoot At Spine\n");

                HitCoefficient = 0.75f;
                bIsUpperBody = 1;
            }

            if (Args->Impact.BoneName.ToString() == "pelvis")
            {
                printf("\n Shoot At pelvis\n");

                HitCoefficient = 0.65f;
            }

            if (Args->Impact.BoneName.ToString() == "thigh_r" || Args->Impact.BoneName.ToString() == "thigh_l")
            {
                printf("\n Shoot At thing_\n");

                HitCoefficient = 0.45f;
            }

            if (Args->Impact.BoneName.ToString() == "calf_r" || Args->Impact.BoneName.ToString() == "calf_l")
            {
                printf("\n Shoot At calf_\n");

                HitCoefficient = 0.45f;
            }

            if (Args->Impact.BoneName.ToString() == "foot_r" || Args->Impact.BoneName.ToString() == "foot_l")
            {
                printf("\n Shoot At foot_\n");

                HitCoefficient = 0.25f;
            }

            if (Args->Impact.BoneName.ToString() == "upperarm_r" || Args->Impact.BoneName.ToString() == "upperarm_l")
            {
                printf("\n Shoot At upperarm_\n");

                HitCoefficient = 0.35f;
            }

            if (Args->Impact.BoneName.ToString() == "lowerarm_r" || Args->Impact.BoneName.ToString() == "lowerarm_l")
            {
                printf("\n Shoot At lowerarm_\n");

                HitCoefficient = 0.35f;
            }

            if (Args->Impact.BoneName.ToString() == "hand_r" || Args->Impact.BoneName.ToString() == "hand_l")
            {
                printf("\n Shoot At hand_\n");

                HitCoefficient = 0.15f;
            }

            CUSTOMLOG("Bone hit: " + Args->Impact.BoneName.ToString());

            AActor* ActorHit = Args->Impact.Actor.Get();
            CUSTOMLOG("Actor hit: " + ActorHit->GetFullName());

            FHitResult Final{};
            Final.Actor = Args->Impact.Actor;
            Final.BoneName = Args->Impact.BoneName;
            Final.Location = Args->Impact.Location;
            Final.ImpactPoint = Args->Impact.ImpactPoint;
            Final.PhysMaterial = Args->Impact.PhysMaterial;
            Final.Component = Args->Impact.Component;
            Final.ImpactNormal = Args->Impact.ImpactNormal;

            float WeaponDamage = Config.HitDamage;

            float equipmentDamageReduction = 0.0f;

            float efalloff = 0;

            float multiplier = pow(Config.RangeModifier, Args->TravelDistance / Config.ReferenceDistance);

            efalloff = WeaponDamage * multiplier;

            rawDamage = efalloff;

            finalDamage = rawDamage * (1.0f - equipmentDamageReduction);

            ATslCharacter* OwnerChar = (ATslCharacter*)Weapon->GetPawnOwner();
            ATslPlayerState* ownerPS = (ATslPlayerState*)OwnerChar->GetTslPlayerState();

            SDK::UGameplayStatics::ApplyPointDamage(Args->Impact.Actor.Get(), finalDamage, Args->TraceStart, Final, Weapon->GetPawnOwner()->Controller, Weapon, Config.DamageType);
        }

        Bots::OnCall(Obj, Func, Parms);

        if (Bots::ShouldBlockCall(Obj, Func))
            return;

        GOriginalProcessEvent(Obj, Func, Parms);

        Bots::AfterCall();

        if (Func && Bots::IsEndMatchQuery(Func))
            Bots::FilterEndMatch(Parms);

        static thread_local bool bInsideTick = false;
        static thread_local double LastTick = 0.0;

        const double CurrentTime = GetTickCount64() / 1000.0;

        if (!bInsideTick && CurrentTime - LastTick >= 0.01)
        {
            bInsideTick = true;
            LastTick = CurrentTime;
            
            Bots::Tick();
            Bots::UpdateMatchStartup(UWorld::GetWorld());

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

        uintptr_t ProcessEventAddr3 = (uintptr_t(GetModuleHandle(0)) + 0x1A48A60);

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
        if (O == nullptr || Name == nullptr || *Name == '\0') return {};

        SDK::UClass* Cls = O->GetClass();
        if (Cls == nullptr) return {};

        for (UStruct* S = Cls; S != nullptr; S = S->Super)
        {
            for (UField* Field = S->Children; Field != nullptr; Field = Field->Next)
            {
                if (!Field->IsA(UProperty::StaticClass())) continue;

                auto* P = static_cast<UProperty*>(Field);
                if (P->GetName() != Name) continue;

                FProp Result{};
                Result.Property = P;
                Result.Off = P->Offset;

                if (!P->IsA(UBoolProperty::StaticClass()))
                {
                    Result.Type = "OtherProperty";
                    return Result;
                }

                auto* BP = static_cast<UBoolProperty*>(P);

                Result.Type = "BoolProperty";
                Result.ByteOff = BP->ByteOffset;
                Result.Mask = BP->FieldMask;

                if (Result.Mask == 0 || (Result.Mask != 0xFF && (Result.Mask & (Result.Mask - 1)) != 0)) return {};
                return Result;
            }
        }

        return {};
    }

    static bool WriteBool(SDK::UObject* O, const char* Name, bool Value)
    {
        if (O == nullptr || Name == nullptr || *Name == '\0') return false;

        const FProp P = ObjProp(O, Name);

        if (!P.Ok() || P.Type != "BoolProperty" || P.Property == nullptr) return false;

        auto* Base = reinterpret_cast<std::uint8_t*>(O);
        auto* Byte = Base + P.Off + P.ByteOff;

        if (P.Mask == 0xFF)
        {
            const std::uint8_t NewValue = Value ? 1u : 0u;
            std::memcpy(Byte, &NewValue, sizeof(NewValue));
        }
        else
        {
            std::uint8_t Current{};
            std::memcpy(&Current, Byte, sizeof(Current));
            const std::uint8_t NewValue = Value ? static_cast<std::uint8_t>(Current | P.Mask) : static_cast<std::uint8_t>(Current & static_cast<std::uint8_t>(~P.Mask));
            std::memcpy(Byte, &NewValue, sizeof(NewValue));
        }

        return true;
    }

    void DisableLevelStreaming(UWorld* World)
    {
        if (!World)
        {
            CUSTOMLOG("World is invalid");
            return;
        }

        for (ULevelStreaming* L : World->StreamingLevels)
        {
            if (!L) continue;

            if (L->PackageName.ToString().contains("eather")) continue;
            if (L->PackageNameToLoad.ToString().contains("eather")) continue;

            WriteBool(L, "bDisableDistanceStreaming", true);
            WriteBool(L, "bShouldBeLoaded", true);
            WriteBool(L, "bShouldBeVisible", true);
        }

        Sleep(3000);
        Bots::SetTerrainReady(true);
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

        CUSTOMLOG(InstallNullGuard() ? "Native null guard installed" : "Native null guard installation failed");
        CUSTOMLOG(InstallNullContextGuard() ? "nullptr crash fix: second null guard installed" : "nullptr crash fix: second null guard installation failed");
        CUSTOMLOG(InstallNullCharacterGuard() ? "nullptr crash fix: character null guard installed" : "nullptr crash fix: character null guard installation failed");

        CUSTOMLOG("[PUBG2019PS] Loading all streaming levels, please be patient...");

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

        GM->TslGameState->bTimerPaused = false;
        GM->TslGameState->NumStartPlayers = 0;
        GM->TslGameState->NumStartTeams = 0;
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

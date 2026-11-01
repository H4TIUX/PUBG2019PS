// Native match startup for the supplied 6.0.6.3 executable. Game thread only.
// Observed/validated ABI: preparer game-mode context +0x40, GetWorld virtual
// +0x130, preparation +0x240, countdown registration +0x258, timer handle +0x48.
#include "IniSettings/IniSettings.h"

static void UpdateMatchStartup(UWorld* World)
{
    if (!Bots::IsGameThread()) return;
    static UWorld* LastWorld = nullptr;
    static bool Configured = false, Failed = false;
    static ULONGLONG NextCheck = 0;
    if (World != LastWorld) {
        LastWorld = World; Configured = Failed = false; NextCheck = 0;
    }
    if (!World || !GFullTerrainReady || Configured || Failed || GetTickCount64() < NextCheck) return;
    NextCheck = GetTickCount64() + 1000;
    // Training is a ground-based native mode and has no airborne preparer.
    // Keep its own game mode/preparer instead of manufacturing an aircraft.
    if (World->AuthorityGameMode && World->AuthorityGameMode->IsA(ATslTrainingGameMode::StaticClass())) {
        if (!World->AuthorityGameMode->GameState) return;
        Configured = true;
        CUSTOMLOG("[PUBG2019PS] Match startup ready: native training mode, no aircraft.");
        return;
    }
    if (!World->AuthorityGameMode || !World->AuthorityGameMode->IsA(ATslGameMode::StaticClass())) return;
    auto* GM = static_cast<ATslGameMode*>(World->AuthorityGameMode);
    auto* GS = GM->TslGameState;
    auto* Old = GM->MatchPreparer;
    if (!GS || !GS->LevelAttribute || !Old || GM->MatchState.ToString() != "WaitingToStart") return;
    const auto Base = InSDKUtils::GetImageBase();
    auto Fail = [&](const char* Reason) {
        Failed = true;
        CUSTOMLOG(std::string("[PUBG2019PS] Match startup refused: ") + Reason);
    };
    if (!Old->IsA(UMatchPreparer::StaticClass()) || *reinterpret_cast<ATslGameMode**>(reinterpret_cast<uintptr_t>(Old)+0x40) != GM) {
        Fail("unsupported preparer context"); return;
    }
    UClass* AirborneClass = nullptr;
    for (const auto& Entry : GM->MatchPreparerClasses)
        if (Entry.Type == EMatchStartType::Airborne && Entry.Class) { AirborneClass = Entry.Class; break; }
    if (!AirborneClass || !AirborneClass->IsSubclassOf(UAirborneMatchPreparer::StaticClass())) {
        Fail("airborne preparer class missing"); return;
    }
    const bool ExistingAirborne = Old->IsA(UAirborneMatchPreparer::StaticClass());
    auto* Preparer = ExistingAirborne ? Old : static_cast<UMatchPreparer*>(UGameplayStatics::SpawnObject(AirborneClass, GM));
    if (!Preparer || !Preparer->IsA(UAirborneMatchPreparer::StaticClass())) {
        Fail("airborne preparer creation failed"); return;
    }
    auto** VTable = *reinterpret_cast<void***>(Preparer);
    constexpr unsigned char StartPrologue[]{0x48,0x8b,0xc4,0x55,0x41,0x56,0x41,0x57};
    if (reinterpret_cast<uintptr_t>(VTable[0x258/8]) != Base+0x294a1c0 ||
        reinterpret_cast<uintptr_t>(VTable[0x240/8]) != Base+0x2942db0 ||
        std::memcmp(VTable[0x258/8],StartPrologue,sizeof(StartPrologue)) != 0) {
        Fail("unsupported native startup methods"); return;
    }
    if (!ExistingAirborne) {
        auto** Context = reinterpret_cast<ATslGameMode**>(reinterpret_cast<uintptr_t>(Preparer)+0x40);
        if (*Context) { Fail("new preparer has an unexpected context"); return; }
        // SpawnObject leaves this native (non-reflected) context unset. Bind
        // the same field verified in the original instance and its methods.
        *Context = GM;
    }
    using GetWorldFn = UWorld*(*)(UMatchPreparer*);
    using Action = void(*)(UMatchPreparer*);
    if (reinterpret_cast<GetWorldFn>(VTable[0x130/8])(Preparer) != World) {
        Fail("preparer world binding failed"); return;
    }
    const auto PreviousType = GM->MatchStartType;
    const auto PreviousStateType = GS->MatchStartType;
    GM->MatchPreparer = Preparer;
    GM->MatchStartType = EMatchStartType::Airborne;
    GS->MatchStartType = EMatchStartType::Airborne;
    GS->bTimerPaused = GetIniBool("Bots", "HoldLobbyWhenEmpty", true);
    Preparer->WarmupTime = (std::max)(5, (std::min)(900, GetWaitTime()));
    auto* Airborne = static_cast<UAirborneMatchPreparer*>(Preparer);
    if (!ExistingAirborne || Airborne->Aircrafts.Num() == 0)
        reinterpret_cast<Action>(VTable[0x240/8])(Preparer);
    if (Airborne->Aircrafts.Num() == 0) {
        GM->MatchPreparer=Old; GM->MatchStartType=PreviousType; GS->MatchStartType=PreviousStateType;
        Fail("native aircraft preparation failed"); return;
    }
    auto* Timer = reinterpret_cast<uint64*>(reinterpret_cast<uintptr_t>(Preparer)+0x48);
    if (!*Timer) {
        // The native registration method returns without registering a timer
        // when RemainingTime is already nonzero. Let it initialize the value.
        GS->RemainingTime = 0;
        reinterpret_cast<Action>(VTable[0x258/8])(Preparer);
    }
    if (!*Timer || GS->RemainingTime <= 0) { Fail("native countdown did not register"); return; }
    Configured = true;
    CUSTOMLOG("[PUBG2019PS] Match startup ready: native airborne preparer, aircraft=" + std::to_string(Airborne->Aircrafts.Num()) +
        ", countdown=" + std::to_string(GS->RemainingTime) + " s; waiting for a real player.");
}

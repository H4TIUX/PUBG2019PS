// Native match startup for the supplied 6.0.6.3 executable. Game thread only.
// Observed/validated ABI: preparer game-mode context +0x40, GetWorld virtual
// +0x130, preparation +0x240, countdown registration +0x258, timer handle +0x48.
#include "IniSettings.h"

using FZombieNativeCanDamage = bool(*)(ATslGameMode*, const ATslPlayerState*, const ATslPlayerState*);
static FZombieNativeCanDamage OriginalZombieCanDamage=nullptr;
static bool ZombieNativeCanDamage(ATslGameMode* Mode, const ATslPlayerState* Attacker, const ATslPlayerState* Victim) {
    if (Mode && Attacker && Victim && Attacker!=Victim && Bots::IsGameThread() && Mode->IsA(ATslZombieGameMode::StaticClass()) &&
        Attacker->IsA(ATslZombiePlayerState::StaticClass()) && Victim->IsA(ATslZombiePlayerState::StaticClass())) {
        auto A=*reinterpret_cast<const EZombieModePlayerState*>(reinterpret_cast<uintptr_t>(Attacker)+0x959);
        auto V=*reinterpret_cast<const EZombieModePlayerState*>(reinterpret_cast<uintptr_t>(Victim)+0x959);
        auto* ZombieMode=static_cast<ATslZombieGameMode*>(Mode);
        const int Decision=ZombieCombatPolicy::DamageDecision((int)A,(int)V,ZombieMode->ZombieRuleType==EZombieRuleType::BattleRoyale,ZombieMode->TeamSize);
        if (Decision>=0) return Decision!=0;
    }
    return OriginalZombieCanDamage ? OriginalZombieCanDamage(Mode,Attacker,Victim) : false;
}

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
    // Isolated experiment: let the NPC zombie mode retain its own preparer.
    if (World->AuthorityGameMode && (World->AuthorityGameMode->GetName().find("ZombieNpcGameMode") == 0 || World->AuthorityGameMode->IsA(ATslZombieGameMode::StaticClass()))) {
        if (!World->AuthorityGameMode->GameState) return;
        bool ZombieAirborne=false;
        if (World->AuthorityGameMode->IsA(ATslZombieGameMode::StaticClass())) {
            char ModeValue[8]{}; GetEnvironmentVariableA("PUBG2019_ZOMBIE_RULE", ModeValue, sizeof(ModeValue));
            int Rule = std::atoi(ModeValue); if (Rule < 1 || Rule > 3) Rule = 1;
            ZombieAirborne = Rule==1;
            static_cast<ATslZombieGameMode*>(World->AuthorityGameMode)->ZombieRuleType = static_cast<EZombieRuleType>(Rule);
            static_cast<ATslZombieGameMode*>(World->AuthorityGameMode)->bHumanSkipDBNO = 1;
            if (World->AuthorityGameMode->GameState->IsA(ATslZombieGameState::StaticClass()))
                static_cast<ATslZombieGameState*>(World->AuthorityGameMode->GameState)->ZombieRuleType = static_cast<EZombieRuleType>(Rule);
            CUSTOMLOG("[ZOMBIE TEST] Selected rule=" + std::to_string(Rule));
            auto* ZMode=static_cast<ATslZombieGameMode*>(World->AuthorityGameMode);
            CUSTOMLOG("[ZOMBIE TEST] Mode defaults revive="+std::to_string(ZMode->ZombieRevive)+" respawn="+std::to_string(ZMode->bZombieRespawnEnable)+" virus="+std::to_string((int)ZMode->ApplyZombieVirusType)+" respawnSeconds="+std::to_string(ZMode->ZombieRespawnTime)+" timeLimit="+std::to_string(ZMode->TimeLimitInSec));
            CUSTOMLOG("[ZOMBIE TEST] Mode default modifiers move="+std::to_string(ZMode->ForceMovementSpeedModifier)+" zombieHealth="+std::to_string(ZMode->ZombieHealth)+" virusDps="+std::to_string(ZMode->VirusDamagepersecond));
            CUSTOMLOG("[ZOMBIE TEST] Zombie customization entry count="+std::to_string(ZMode->ZombieCustomizationData.Num()));
            for (int i=0;i<ZMode->ZombieCustomizationData.Num() && i<32;++i) {
                const auto& Entry=ZMode->ZombieCustomizationData[i];
                std::string Values;
                for (int j=0;j<Entry.ZombieOptionValues.Num() && j<16;++j) Values+=(j ? "," : "")+Entry.ZombieOptionValues[j].ToString();
                CUSTOMLOG("[ZOMBIE TEST] Zombie appearance option="+Entry.StringOptionKey.ToString()+" gender="+std::to_string((int)Entry.Gender)+" values="+Values);
            }
            ZMode->bNewZombieGameMode=true;
            ZMode->ZombieRevive=true;
            ZMode->bZombieRespawnEnable=(Rule==2 || Rule==1);
            ZMode->ZombieRespawnMaxNum=(Rule==1 ? 1 : 255);
            ZMode->ApplyZombieVirusType=(Rule==1 ? EZombieVirusType::All : EZombieVirusType::Attacked);
            if (ZMode->GameState && ZMode->GameState->IsA(ATslZombieGameState::StaticClass())) {
                auto* ZState=static_cast<ATslZombieGameState*>(ZMode->GameState);
                ZState->bNewZombieGameMode=true; ZState->ZombieRevive=true; ZState->ForceNetUpdate();
            }
            CUSTOMLOG("[ZOMBIE TEST] Experimental infection settings applied for official rule.");
            auto* GameMode=static_cast<ATslGameMode*>(World->AuthorityGameMode);
            CUSTOMLOG("[ZOMBIE TEST] existing preparer="+(GameMode->MatchPreparer ? GameMode->MatchPreparer->GetFullName() : std::string("none")));
            for (auto& Entry : GameMode->MatchPreparerClasses)
                CUSTOMLOG("[ZOMBIE TEST] preparer choice type="+std::to_string((int)Entry.Type)+" class="+(Entry.Class ? Entry.Class->GetFullName() : std::string("none")));

            auto** VTable=*reinterpret_cast<void***>(World->AuthorityGameMode);
            void* Target=VTable[0x950/sizeof(void*)]; MEMORY_BASIC_INFORMATION Info{};
            if (!OriginalZombieCanDamage && VirtualQuery(Target,&Info,sizeof(Info)) && Info.AllocationBase==GetModuleHandleA(nullptr)) {
                auto Created=MH_CreateHook(Target,reinterpret_cast<void*>(&ZombieNativeCanDamage),reinterpret_cast<void**>(&OriginalZombieCanDamage));
                auto Enabled=Created==MH_OK ? MH_EnableHook(Target) : Created;
                CUSTOMLOG("[ZOMBIE TEST] Native zombie/human damage hook create="+std::to_string((int)Created)+" enable="+std::to_string((int)Enabled));
            }

            auto* Fn=ATslGameMode::StaticClass()->GetFunction("TslGameMode","CanDealDamage");
            if(Fn && Fn->ExecFunction) {
                auto* Bytes=reinterpret_cast<const unsigned char*>(Fn->ExecFunction);
                char Buf[256]{}; for (int n=0;n<64;++n) std::sprintf(Buf+n*2,"%02x",Bytes[n]);
                CUSTOMLOG("[ZOMBIE TEST] CanDealDamage exec offset="+std::to_string(reinterpret_cast<uintptr_t>(Bytes)-InSDKUtils::GetImageBase())+" bytes="+Buf);
            }
        }
        if (!ZombieAirborne) {
        Configured = true;
#ifdef PUBG2019_ENABLE_DIAGNOSTICS
        auto& Objects = UObject::GObjects;
        int Found = 0;
        for (int i = 0; i < Objects->Num() && Found < 100; ++i) {
            auto* Object = Objects->GetByIndex(i);
            if (!Object) continue;
            std::string Name = Object->GetName();
            if ((Name.find("Zombie") != std::string::npos || Name.find("zombie") != std::string::npos || Name.find("Npc") != std::string::npos) &&
                (Object->IsA(UClass::StaticClass()) || Object->IsA(AActor::StaticClass()))) {
                CUSTOMLOG("[ZOMBIE INVENTORY] " + Object->GetFullName()); ++Found;
            }
        }
#endif
        CUSTOMLOG("[PUBG2019PS] Match startup ready: native zombie mode.");
        return;
        }
        CUSTOMLOG("[ZOMBIE TEST] Battle Royale: use native airborne startup.");
    }
    // Training is a ground-based native mode and has no airborne preparer.
    // Keep its own game mode/preparer instead of manufacturing an aircraft.
    if (World->AuthorityGameMode && World->AuthorityGameMode->IsA(ATslTrainingGameMode::StaticClass())) {
        if (!World->AuthorityGameMode->GameState || !World->AuthorityGameMode->GameState->IsA(ATslTrainingGameState::StaticClass())) return;
        auto* TrainingMode=static_cast<ATslTrainingGameMode*>(World->AuthorityGameMode);
        auto* TrainingState=static_cast<ATslTrainingGameState*>(TrainingMode->GameState);
        float TrainingSeconds=1200.f;
#ifdef PUBG2019_ENABLE_DIAGNOSTICS
        char Duration[16]{};if(GetEnvironmentVariableA("PUBG2019_TRAINING_TEST_SECONDS",Duration,sizeof(Duration))){int Value=std::atoi(Duration);if(Value>=30 && Value<=1200)TrainingSeconds=(float)Value;}
#endif
        TrainingMode->TimeLimitSeconds=TrainingSeconds;
        TrainingState->TimeLimitSeconds=TrainingSeconds;
        // Training starts immediately; it does not use the BR lobby countdown.
        // Replicate its absence separately from the training time limit.
        TrainingState->RemainingTime=0;
        TrainingState->ForceNetUpdate();
        Configured = true;
        CUSTOMLOG("[PUBG2019PS] Match startup ready: native training mode, time limit "+std::to_string((int)TrainingSeconds)+" seconds.");
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

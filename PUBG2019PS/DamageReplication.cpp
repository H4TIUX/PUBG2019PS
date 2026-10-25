#include "LegacyDamage.h"
#include "../PUBG2019PS/IniSettings.h"

#include <array>
#include <atomic>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_map>

namespace
{
    enum class DamageRpcKind : size_t
    {
        Trajectory,
        Melee,
        Unarmed,
        Count
    };

    std::atomic_bool InitializationLogged = false;
    std::array<std::atomic_bool,
        static_cast<size_t>(DamageRpcKind::Count)> FirstDispatchLogged{};
    std::array<std::atomic_uint32_t,
        static_cast<size_t>(DamageRpcKind::Count)> FailureCounts{};
    std::atomic<uintptr_t> PointDamageEventVTable{};
    std::atomic_bool FirstNonCharacterTargetLogged = false;
    std::atomic_bool FirstDoorDamageLogged = false;
    std::atomic_bool FirstReactionDoorDamageLogged = false;
    std::atomic_bool FirstUnarmedDoorIgnoredLogged = false;
    std::atomic_bool FirstVehicleDamageLogged = false;
    std::atomic_uint32_t CorrectedBlueZoneDamageCount = 0;
    std::atomic_uint32_t SuppressedSpectatorBlueZoneDamageCount = 0;
    std::atomic_uint32_t SuppressedSafeZoneDamageCount = 0;
    std::atomic_uint32_t EnvironmentalDamageDiagnosticCount = 0;
    std::atomic_bool SpectatorBlueZoneGuardInstalled = false;
    std::atomic_bool FirstMeleeMessagePatchLogged = false;

    struct MeleeDamageContext
    {
        ATslCharacter* Victim = nullptr;
        ATslPlayerState* VictimPlayerState = nullptr;
        ATslWeapon_Melee* Weapon = nullptr;
        UClass* WeaponClass = nullptr;
        UClass* DamageTypeClass = nullptr;
        UClass* KillfeedDamageTypeClass = nullptr;
        bool IsUnarmed = false;
        ULONGLONG CreatedAt = 0;
    };

    thread_local MeleeDamageContext* ActiveMeleeDamageContext = nullptr;

    // Persistent cache: victim PlayerState -> last melee context.
    // BroadcastDeath happens after TakeDamage returns, so ActiveMeleeDamageContext
    // is already null. We cache by victim so killfeed can retrieve weapon info.
    std::unordered_map<ATslPlayerState*, MeleeDamageContext> CachedMeleeContexts;
    std::unordered_map<ATslCharacter*, ULONGLONG> GroggyFeedbackSuppressionUntil;
    constexpr ULONGLONG GroggyFeedbackSuppressionMs = 30000;

    struct BlueZonePhaseGateState
    {
        ABattleRoyaleModeController* Controller = nullptr;
        int32 PhaseIndex = -1;
        bool ReleaseAccepted = false;
    };

    thread_local BlueZonePhaseGateState BlueZonePhaseState{};

    // Verified against TslGame 5.1.4.3 in TslGame_dump.exe.i64. This is the
    // native FTslPointDamageEvent vtable; its type ID is 3, which is required
    // by PUBG's destructible-door damage handlers.
    constexpr uintptr_t PointDamageEventVTableOffset = 0x04C33E58;
    constexpr uintptr_t PointDamageEventFirstVirtualOffset = 0x001ECA70;
    constexpr uintptr_t PointDamageEventGetTypeIdOffset = 0x00209BF0;
    constexpr uintptr_t PointDamageEventIsOfTypeOffset = 0x0036E900;
    constexpr size_t TakeDamageVTableIndex = 0x580 / sizeof(void*);
    constexpr size_t ReceiveComponentDamageVTableIndex = 0x6C0 / sizeof(void*);
    constexpr uintptr_t ReactionDoorReceiveComponentDamageOffset = 0x01201580;
    // The message must follow the same native TakeDamage dispatch. A multi-
    // second cache incorrectly reattributes later bleed-out ticks to the last
    // punch, producing repeated punch hit audio and punch death attribution.
    constexpr ULONGLONG MeleeContextLifetimeMs = 300;

    static_assert(sizeof(FPointDamageEvent) == 0xB0,
        "Unexpected FPointDamageEvent layout for TslGame 5.1.4.3");
    static_assert(sizeof(FTslPointDamageEvent) == 0xB8,
        "Unexpected FTslPointDamageEvent layout for TslGame 5.1.4.3");

    using TakeDamageNative = float(__fastcall*)(AActor*, float,
        const FDamageEvent&, AController*, AActor*);
    using ReceiveComponentDamageNative = void(__fastcall*)(
        UPrimitiveComponent*, float, const FDamageEvent&, AController*, AActor*);

    std::atomic<TakeDamageNative> NativeCharacterTakeDamage = nullptr;

    struct CharacterObserverState
    {
        AController* CurrentController = nullptr;
        AController* LastController = nullptr;
        ATslPlayerState* PlayerState = nullptr;
        APawn* CurrentControllerPawn = nullptr;
        APawn* LastControllerPawn = nullptr;
        bool CurrentSpectating = false;
        bool CurrentObserving = false;
        bool CurrentSpectatorState = false;
        bool LastSpectating = false;
        bool LastObserving = false;
        bool LastSpectatorState = false;
        bool CharacterDead = false;
        bool PlayerKilled = false;
        bool PlayerObserver = false;

        bool IsObserver() const
        {
            return CharacterDead || CurrentSpectating || CurrentObserving ||
                CurrentSpectatorState || LastSpectating || LastObserving ||
                LastSpectatorState || PlayerKilled || PlayerObserver;
        }
    };

    struct AuthoritativeSafeZoneState
    {
        ATslGameState* GameState = nullptr;
        FVector CharacterLocation{};
        FVector RootRelativeLocation{};
        FVector ReplicatedLocation{};
        FVector Center{};
        float Radius = 0.0f;
        float Distance = 0.0f;
        bool UsedReplicatedLocation = false;
        bool IsInside = false;
    };

    bool IsUsableObject(const UObject* Object);
    bool IsUsableActor(const AActor* Actor);
    bool IsUsableVector(const FVector& Vector);
    CharacterObserverState ResolveCharacterObserverState(
        ATslCharacter* Character);
    bool ResolveAuthoritativeSafeZoneState(ATslCharacter* Character,
        AuthoritativeSafeZoneState& State);
    bool IsBlueZoneDamageType(ATslCharacter* Character,
        UClass* DamageTypeClass, AController* InstigatedBy,
        AActor* DamageCauser, std::string* DamageTypeName = nullptr,
        int* DamageTypeCategory = nullptr);


    bool IsUsableObject(const UObject* Object)
    {
        return IsReadableRange(Object, sizeof(UObject));
    }

    bool IsUsableActor(const AActor* Actor)
    {
        return IsReadableRange(Actor, sizeof(AActor));
    }

    UTexture* ReadWeaponCdoIconSafe(UClass* WeaponClass)
    {
        if (!IsUsableObject(WeaponClass))
            return nullptr;

        __try
        {
            UObject* DefaultObject = WeaponClass->DefaultObject;
            if (IsUsableObject(DefaultObject) &&
                DefaultObject->IsA(ATslWeapon::StaticClass()))
                return static_cast<ATslWeapon*>(DefaultObject)->WeaponIcon;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
        return nullptr;
    }

    void LogUnarmedResolutionOnce(ATslCharacter* Attacker,
        UWeaponProcessorComponent* Processor, int32 UnarmedIndex,
        int32 EquippedCount, UClass* ResolvedClass)
    {
        static std::atomic_bool UnarmedResolutionLogged = false;
        if (UnarmedResolutionLogged.exchange(true))
            return;

        const std::string ClassName = IsUsableObject(ResolvedClass)
            ? ResolvedClass->GetName()
            : std::string("<none>");
        char ResolutionMessage[640]{};
        sprintf_s(ResolutionMessage,
            "Unarmed resolution: attacker=%p processor=%p index=%d equipped=%d class=%p name=%s cdo-icon=%p.",
            Attacker, Processor, UnarmedIndex, EquippedCount,
            ResolvedClass, ClassName.c_str(), ReadWeaponCdoIconSafe(ResolvedClass));
        Log::Write(Log::Level::Info, Log::Module::Damage, ResolutionMessage);
    }

    bool IsUsableDamage(float Damage)
    {
        return std::isfinite(Damage) && Damage > 0.0f && Damage < 100000.0f;
    }

    bool IsUsableVector(const FVector& Vector)
    {
        return std::isfinite(Vector.X) && std::isfinite(Vector.Y) &&
            std::isfinite(Vector.Z);
    }

    bool IsInitializedLocation(const FVector& Vector)
    {
        if (!IsUsableVector(Vector))
            return false;

        constexpr float InitializationTolerance = 1.0f;
        return std::fabs(Vector.X) > InitializationTolerance ||
            std::fabs(Vector.Y) > InitializationTolerance ||
            std::fabs(Vector.Z) > InitializationTolerance;
    }

    std::string ToLowerAscii(std::string Value)
    {
        std::transform(Value.begin(), Value.end(), Value.begin(),
            [](unsigned char Character)
            {
                return static_cast<char>(std::tolower(Character));
            });
        return Value;
    }

    bool IsClassObject(UObject* Object)
    {
        if (!Object || !IsReadableRange(Object, sizeof(UObject)))
            return false;

        UClass* Class = Object->GetClass();
        for (int Depth = 0; Class && Depth < 32; ++Depth)
        {
            if (!IsReadableRange(Class, sizeof(UStruct)))
                return false;
            if (Class == UClass::StaticClass())
                return true;
            Class = static_cast<UClass*>(Class->Super);
        }
        return false;
    }

    bool IsWeaponClass(UClass* Candidate)
    {
        for (int Depth = 0; Candidate && Depth < 32; ++Depth)
        {
            if (!IsReadableRange(Candidate, sizeof(UStruct)))
                return false;
            if (Candidate == ATslWeapon::StaticClass())
                return true;
            Candidate = static_cast<UClass*>(Candidate->Super);
        }
        return false;
    }

    // Accepts "/Game/Path/Asset.uasset" or "/Game/Path/Asset" and returns
    // the FindObject-style full object name "/Game/Path/Asset.Asset".
    std::string NormalizeAssetObjectPath(const std::string& RawPath)
    {
        std::string Path = RawPath;
        const std::string Extension = ".uasset";
        const size_t ExtensionPosition = Path.rfind(Extension);
        if (ExtensionPosition != std::string::npos &&
            ExtensionPosition + Extension.size() == Path.size())
        {
            Path.erase(ExtensionPosition);
        }

        const size_t Slash = Path.rfind('/');
        if (Slash == std::string::npos || Slash + 1 >= Path.size())
            return std::string();

        const size_t Dot = Path.rfind('.');
        if (Dot != std::string::npos && Dot > Slash)
            return Path;

        return Path + "." + Path.substr(Slash + 1);
    }

    // One-shot scan of the loaded classes for the unarmed "fists" weapon
    // class the client recognizes. Prefers fist-named candidates whose CDO
    // carries a weapon icon; the base TslWeapon_Melee CDO has none (the icon
    // lives on the fists blueprint asset).
    UClass* FindFistsWeaponClassByScan(bool* FoundByIcon)
    {
        if (FoundByIcon)
            *FoundByIcon = false;

        struct NameTier
        {
            UClass* WithIcon = nullptr;
            UClass* WithoutIcon = nullptr;
        };
        NameTier FistTier{};
        NameTier OtherTier{};
        NameTier MeleeTier{};

        TUObjectArray* Objects = UObject::GObjects.GetTypedPtr();
        if (!IsReadableRange(Objects, sizeof(TUObjectArray)) ||
            Objects->Num() <= 0 || Objects->Num() > 1000000)
        {
            return nullptr;
        }

        FUObjectItem* Items = Objects->GetDecrytedObjPtr();
        if (!IsReadableRange(Items, sizeof(FUObjectItem)))
            return nullptr;

        for (int32 Index = 0; Index < Objects->Num(); ++Index)
        {
            UObject* Object = Items[Index].Object;
            if (!IsClassObject(Object))
                continue;

            UClass* Candidate = static_cast<UClass*>(Object);
            if (Candidate == ATslWeapon_Melee::StaticClass() ||
                !IsWeaponClass(Candidate))
            {
                continue;
            }

            const std::string Lower = ToLowerAscii(Candidate->GetName());
            const bool IsFist = Lower.find("fist") != std::string::npos;
            const bool IsOther = Lower.find("punch") != std::string::npos ||
                Lower.find("unarmed") != std::string::npos;
            const bool IsMelee = Lower.find("melee") != std::string::npos;
            if (!IsFist && !IsOther && !IsMelee)
                continue;

            const bool HasIcon = ReadWeaponCdoIconSafe(Candidate) != nullptr;
            NameTier* Target = nullptr;
            if (IsFist)
                Target = &FistTier;
            else if (IsOther)
                Target = &OtherTier;
            else
                Target = &MeleeTier;

            if (HasIcon && !Target->WithIcon)
                Target->WithIcon = Candidate;
            else if (!Target->WithoutIcon)
                Target->WithoutIcon = Candidate;
        }

        const NameTier* Tiers[] = { &FistTier, &OtherTier, &MeleeTier };
        for (const NameTier* Current : Tiers)
        {
            if (Current->WithIcon)
            {
                if (FoundByIcon)
                    *FoundByIcon = true;
                return Current->WithIcon;
            }
        }
        for (const NameTier* Current : Tiers)
        {
            if (Current->WithoutIcon)
                return Current->WithoutIcon;
        }
        return nullptr;
    }

    // Finds the loaded weapon class whose CDO weapon icon is the given
    // texture; the fists blueprint class references the punch kill-feed icon.
    UClass* FindWeaponClassByCdoIcon(UTexture* IconTexture)
    {
        if (!IsUsableObject(IconTexture))
            return nullptr;

        TUObjectArray* Objects = UObject::GObjects.GetTypedPtr();
        if (!IsReadableRange(Objects, sizeof(TUObjectArray)) ||
            Objects->Num() <= 0 || Objects->Num() > 1000000)
        {
            return nullptr;
        }

        FUObjectItem* Items = Objects->GetDecrytedObjPtr();
        if (!IsReadableRange(Items, sizeof(FUObjectItem)))
            return nullptr;

        for (int32 Index = 0; Index < Objects->Num(); ++Index)
        {
            UObject* Object = Items[Index].Object;
            if (!IsClassObject(Object))
                continue;

            UClass* Candidate = static_cast<UClass*>(Object);
            if (Candidate == ATslWeapon_Melee::StaticClass() ||
                !IsWeaponClass(Candidate))
            {
                continue;
            }

            if (ReadWeaponCdoIconSafe(Candidate) == IconTexture)
                return Candidate;
        }
        return nullptr;
    }

    bool IsWeaponClassSafe(UClass* Candidate)
    {
        if (!Candidate)
            return false;

        __try
        {
            return IsWeaponClass(Candidate);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    UClass* FindConfiguredWeaponClassSafe(const std::string& Path)
    {
        __try
        {
            return UObject::FindClass(Path);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return nullptr;
        }
    }

    UClass* ReadUnarmedDamageType(ATslCharacter* Attacker)
    {
        return Attacker->UnarmedDamageType.Get();
    }

    UClass* ResolveAttackerUnarmedDamageTypeSafe(ATslCharacter* Attacker)
    {
        if (!Attacker)
            return nullptr;

        __try
        {
            return ReadUnarmedDamageType(Attacker);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return nullptr;
        }
    }

    void LogUnarmedDamageTypeResolution(UClass* CharacterType,
        UClass* SelectedType, bool UsedFallback)
    {
        static std::atomic_bool Logged = false;
        if (Logged.exchange(true))
            return;

        int Category = -1;
        UTexture* KillIcon = nullptr;
        if (IsUsableObject(SelectedType))
        {
            UObject* CDO = SelectedType->DefaultObject;
            if (IsUsableObject(CDO) &&
                CDO->IsA(UTslDamageType::StaticClass()))
            {
                auto* DamageType = static_cast<UTslDamageType*>(CDO);
                Category = static_cast<int>(DamageType->DamageTypeCategory);
                KillIcon = DamageType->KillIcon.Texture;
            }
        }

        char Message[512]{};
        sprintf_s(Message,
            "Unarmed damage type resolution: character-class=%p selected=%p fallback=%d category=%d kill-icon=%p.",
            CharacterType, SelectedType, UsedFallback ? 1 : 0,
            Category, KillIcon);
        Log::Write(Log::Level::Info, Log::Module::Damage, Message);
    }

    // UE4.18.1 runtime dump: UObjectGlobals.cpp object loader.
    // VA 0x7FF640FAC8F0 - image base 0x7FF63F7B0000 = RVA 0x017FC8F0.
    // The observed engine call uses (Class, Outer, Name, Filename,
    // LoadFlags=0x4002, Sandbox=null, bAllowObjectReconciliation=true).
    constexpr uintptr_t StaticLoadObjectOffset = 0x017FC8F0;
    using StaticLoadObjectNative = UObject * (__fastcall*)(
        UClass*, UObject*, const wchar_t*, const wchar_t*, uint32,
        void*, bool);

    UClass* LoadFistsDamageTypeNativeSafe()
    {
        const uintptr_t Address = InSDKUtils::GetImageBase() +
            StaticLoadObjectOffset;
        if (!IsReadableRange(reinterpret_cast<const void*>(Address), 1))
            return nullptr;

        const auto StaticLoadObject =
            reinterpret_cast<StaticLoadObjectNative>(Address);
        static constexpr wchar_t AssetPath[] =
            L"/Game/Blueprints/Damage_Type/DmgType_Melee.DmgType_Melee_C";

        UObject* LoadedObject = nullptr;
        __try
        {
            LoadedObject = StaticLoadObject(
                UClass::StaticClass(), nullptr, AssetPath, nullptr,
                0x4002u, nullptr, true);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return nullptr;
        }

        if (!IsUsableObject(LoadedObject) ||
            !LoadedObject->IsA(UClass::StaticClass()))
        {
            return nullptr;
        }

        UClass* LoadedClass = static_cast<UClass*>(LoadedObject);
        if (!IsUsableObject(LoadedClass->DefaultObject) ||
            !LoadedClass->DefaultObject->IsA(UTslDamageType::StaticClass()))
        {
            return nullptr;
        }

        return LoadedClass;
    }
    UClass* ResolveFistsDamageTypeClass()
    {
        // The killfeed table maps Damage_Melee to Icon_killfeed_punch in this
        // build. Use DmgType_Melee_C, not DmgType_Fist_C/Damage_Punch, for the
        // client-facing category lookup.
        static std::atomic<UClass*> CachedResult{ nullptr };
        static std::atomic_bool Resolved = false;
        if (Resolved.load(std::memory_order_acquire))
            return CachedResult.load(std::memory_order_acquire);

        UClass* Result = nullptr;
        const char* Source = "unresolved";
        const std::string Path =
            "/Game/Blueprints/Damage_Type/DmgType_Melee.DmgType_Melee_C";
        Result = FindConfiguredWeaponClassSafe(Path);
        if (!IsUsableObject(Result))
            Result = LoadFistsDamageTypeNativeSafe();
        if (IsUsableObject(Result))
            Source = "DmgType_Melee_C";

        char ResolutionMessage[384]{};
        sprintf_s(ResolutionMessage,
            "Fists damage type resolution: source=%s class=%p.",
            Source, Result);
        Log::Write(Log::Level::Info, Log::Module::Damage, ResolutionMessage);

        if (IsUsableObject(Result))
        {
            CachedResult.store(Result, std::memory_order_release);
            Resolved.store(true, std::memory_order_release);
        }
        return Result;
    }

    UClass* ResolveFistsWeaponClass()
    {
        static std::atomic<UClass*> CachedResult{ nullptr };
        static std::atomic_bool Resolved = false;
        if (Resolved.load(std::memory_order_acquire))
            return CachedResult.load(std::memory_order_acquire);

        UClass* Result = nullptr;
        const char* Source = "canonical";
        const std::string ConfiguredClassPath = GetFistsClassPath();
        if (!ConfiguredClassPath.empty())
        {
            const std::string NormalizedPath =
                NormalizeAssetObjectPath(ConfiguredClassPath);
            Result = FindConfiguredWeaponClassSafe(NormalizedPath);
            if (IsUsableObject(Result) && IsWeaponClassSafe(Result))
                Source = "config-class";
            else
                Result = nullptr;
        }

        if (!IsUsableObject(Result))
            Result = ATslWeapon_Melee::StaticClass();

        char ResolutionMessage[640]{};
        sprintf_s(ResolutionMessage,
            "Fists class resolution: source=%s class=%p (icon reverse scan disabled).",
            Source, Result);
        Log::Write(Log::Level::Info, Log::Module::Damage, ResolutionMessage);

        CachedResult.store(Result, std::memory_order_release);
        Resolved.store(true, std::memory_order_release);
        return Result;
    }

    bool IsNativeBlueZoneDamageCategory(ATslCharacter* Character,
        UClass* DamageTypeClass, AController* InstigatedBy,
        AActor* DamageCauser, int* DamageTypeCategory)
    {
        if (DamageTypeCategory)
            *DamageTypeCategory = -1;
        if (!IsUsableObject(DamageTypeClass))
            return false;

        UObject* DefaultObject = DamageTypeClass->DefaultObject;
        UClass* TslDamageTypeClass = UTslDamageType::StaticClass();
        if (!IsUsableObject(DefaultObject) || !TslDamageTypeClass ||
            !DefaultObject->IsA(TslDamageTypeClass))
        {
            return false;
        }

        const int Category = static_cast<int>(
            static_cast<UTslDamageType*>(DefaultObject)->DamageTypeCategory);
        if (DamageTypeCategory)
            *DamageTypeCategory = Category;
        if (Category != 6 && Category != 7)
            return false;

        if (IsUsableActor(Character) &&
            (DamageTypeClass == Character->GroggyDamageType.Get() ||
                DamageTypeClass == Character->BleedOutDamageType.Get()))
        {
            return false;
        }

        return !InstigatedBy && !DamageCauser;
    }

    bool IsBlueZoneDamageType(ATslCharacter* Character,
        UClass* DamageTypeClass, AController* InstigatedBy,
        AActor* DamageCauser, std::string* DamageTypeName,
        int* DamageTypeCategory)
    {
        if (DamageTypeName)
            DamageTypeName->clear();
        if (DamageTypeCategory)
            *DamageTypeCategory = -1;
        if (!IsUsableObject(DamageTypeClass))
            return false;

        const std::string Name = DamageTypeClass->GetName();
        if (DamageTypeName)
            *DamageTypeName = Name;

        const std::string LowerName = ToLowerAscii(Name);
        if (LowerName.find("bluezone") != std::string::npos ||
            LowerName.find("blue_zone") != std::string::npos ||
            LowerName.find("poison") != std::string::npos ||
            LowerName.find("gaszone") != std::string::npos ||
            LowerName.find("gas_zone") != std::string::npos)
        {
            return true;
        }

        return IsNativeBlueZoneDamageCategory(Character, DamageTypeClass,
            InstigatedBy, DamageCauser, DamageTypeCategory);
    }

    ATslPlayerState* ResolveCharacterPlayerState(ATslCharacter* Character,
        AController* CurrentController, AController* LastController)
    {
        if (!IsUsableActor(Character))
            return nullptr;

        UClass* TslPlayerStateClass = ATslPlayerState::StaticClass();
        if (!TslPlayerStateClass)
            return nullptr;

        auto ResolveCandidate = [TslPlayerStateClass](APlayerState* Candidate)
            -> ATslPlayerState*
            {
                if (!IsUsableObject(Candidate) ||
                    !Candidate->IsA(TslPlayerStateClass))
                {
                    return nullptr;
                }
                return static_cast<ATslPlayerState*>(Candidate);
            };

        if (ATslPlayerState* PlayerState = ResolveCandidate(
            Character->PlayerState.Get()))
        {
            return PlayerState;
        }

        if (IsUsableObject(CurrentController))
        {
            if (ATslPlayerState* PlayerState = ResolveCandidate(
                CurrentController->PlayerState))
            {
                return PlayerState;
            }
        }

        if (IsUsableObject(LastController))
        {
            if (ATslPlayerState* PlayerState = ResolveCandidate(
                LastController->PlayerState))
            {
                return PlayerState;
            }
        }

        if (ATslPlayerState* PlayerState = ResolveCandidate(
            Character->CachedNonNullPlayerState.Get()))
        {
            return PlayerState;
        }

        return nullptr;
    }

    void ResolveControllerObserverState(AController* Controller,
        bool& Spectating, bool& Observing, bool& SpectatorState,
        APawn*& ControlledPawn, ATslCharacter* Character)
    {
        Spectating = false;
        Observing = false;
        SpectatorState = false;
        ControlledPawn = nullptr;
        if (!IsUsableObject(Controller))
            return;

        ControlledPawn = Controller->Pawn.Get();
        SpectatorState = ControlledPawn && ControlledPawn != Character;

        UClass* PlayerControllerClass = APlayerController::StaticClass();
        if (PlayerControllerClass && Controller->IsA(PlayerControllerClass))
        {
            auto* PlayerController = static_cast<APlayerController*>(Controller);
            Spectating = IsUsableActor(PlayerController->SpectatorPawn);
        }

        UClass* TslPlayerControllerClass = ATslPlayerController::StaticClass();
        if (TslPlayerControllerClass && Controller->IsA(TslPlayerControllerClass))
        {
            auto* TslController = static_cast<ATslPlayerController*>(Controller);
            Observing = IsUsableActor(TslController->SpectatorViewCharacter);
        }
    }

    CharacterObserverState ResolveCharacterObserverState(
        ATslCharacter* Character)
    {
        CharacterObserverState State{};
        if (!IsUsableActor(Character))
            return State;

        __try
        {
            // A groggy (downed) character has Health 0 but must still receive
            // blue-zone damage to bleed out. Only mark fully dead characters
            // as observers for the damage guard.
            const bool IsGroggying = std::isfinite(Character->GroggyHealth) &&
                Character->GroggyHealth > 0.0f;
            State.CharacterDead = std::isfinite(Character->Health) &&
                Character->Health <= 0.0f && !IsGroggying;
            State.CurrentController = Character->Controller;
            State.LastController = Character->LastPossessedController;
            ResolveControllerObserverState(State.CurrentController,
                State.CurrentSpectating, State.CurrentObserving,
                State.CurrentSpectatorState, State.CurrentControllerPawn,
                Character);
            if (State.LastController != State.CurrentController)
            {
                ResolveControllerObserverState(State.LastController,
                    State.LastSpectating, State.LastObserving,
                    State.LastSpectatorState, State.LastControllerPawn,
                    Character);
            }

            State.PlayerState = ResolveCharacterPlayerState(Character,
                State.CurrentController, State.LastController);
            if (IsUsableObject(State.PlayerState))
            {
                State.PlayerKilled = State.PlayerState->bKilled;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return CharacterObserverState{};
        }

        return State;
    }

    bool ResolveAuthoritativeSafeZoneState(ATslCharacter* Character,
        AuthoritativeSafeZoneState& State)
    {
        State = {};
        if (!IsUsableActor(Character))
            return false;

        UWorld* World = UWorld::GetWorld();
        if (!IsReadableRange(World, sizeof(UWorld)))
            return false;

        auto* GameMode = static_cast<ATslGameMode*>(World->AuthorityGameMode);
        if (!IsReadableRange(GameMode, sizeof(ATslGameMode)))
            return false;

        __try
        {
            State.GameState = static_cast<ATslGameState*>(GameMode->GameState);
            if (!IsReadableRange(State.GameState, sizeof(ATslGameState)))
                return false;

            USceneComponent* Root = Character->RootComponent.Get();
            if (!IsReadableRange(Root, sizeof(USceneComponent)))
                return false;

            State.RootRelativeLocation = Root->RelativeLocation;
            State.ReplicatedLocation = Character->ReplicatedMovement.Location;

            // RelativeLocation stops being a world-space position when the
            // character root is attached or world-origin rebasing is active.
            // ReplicatedMovement is expressed in the same authoritative map
            // coordinate space as the replicated safety-zone fields.
            State.UsedReplicatedLocation =
                IsInitializedLocation(State.ReplicatedLocation);
            State.CharacterLocation = State.UsedReplicatedLocation ?
                State.ReplicatedLocation : State.RootRelativeLocation;

            // The engine's own damage decision uses the authoritative
            // SafetyZonePosition/SafetyZoneRadius. The Lerp fields can remain
            // stale on the server and describe a pre-phase huge circle (the
            // map half-diagonal), which would classify every player as inside
            // and suppress all blue-zone damage. Use the authoritative fields
            // first and only fall back to Lerp when they are invalid.
            State.Center = State.GameState->SafetyZonePosition;
            State.Radius = State.GameState->SafetyZoneRadius;
            if (!IsUsableVector(State.Center) ||
                !std::isfinite(State.Radius) || State.Radius <= 0.0f)
            {
                State.Center = State.GameState->LerpSafetyZonePosition;
                State.Radius = State.GameState->LerpSafetyZoneRadius;
            }

            // A radius larger than the map half-diagonal is a stale "cover the
            // whole map" placeholder, not a damaging zone. Never suppress
            // damage based on it.
            constexpr float MaximumAuthoritativeZoneRadius = 600000.0f;
            if (!IsUsableVector(State.CharacterLocation) ||
                !IsUsableVector(State.Center) ||
                !std::isfinite(State.Radius) || State.Radius <= 0.0f ||
                State.Radius > MaximumAuthoritativeZoneRadius)
            {
                return false;
            }

            const float DeltaX = State.CharacterLocation.X - State.Center.X;
            const float DeltaY = State.CharacterLocation.Y - State.Center.Y;
            State.Distance = std::sqrt(DeltaX * DeltaX + DeltaY * DeltaY);
            State.IsInside = std::isfinite(State.Distance) &&
                State.Distance <= State.Radius;
            return std::isfinite(State.Distance);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            State = {};
            return false;
        }
    }

    void LogEnvironmentalDamageDiagnostic(uint32_t Count,
        ATslCharacter* Character, float Damage,
        const std::string& DamageTypeName, int DamageTypeCategory,
        AController* InstigatedBy, AActor* DamageCauser,
        bool IsBlueZone, const CharacterObserverState& Observer,
        const AuthoritativeSafeZoneState& SafeZone, bool HasSafeZone)
    {
        char Message[1536]{};
        sprintf_s(Message,
            "Legacy environmental damage: sample=%u, character=%p '%s', health=%.2f, damage=%.3f, type='%s', category=%d, blue=%s, instigator=%p, causer=%p, controller=%p, last-controller=%p, controller-pawn=%p, last-pawn=%p, current-flags=%d/%d/%d, last-flags=%d/%d/%d, player-state=%p, killed=%s, observer=%s, root-relative=(%.1f, %.1f, %.1f), replicated=(%.1f, %.1f, %.1f), selected=%s, zone-center=(%.1f, %.1f), radius=%.1f, distance=%.1f, inside=%s, zone-valid=%s.",
            Count, Character,
            IsUsableObject(Character) ? Character->GetName().c_str() : "<invalid>",
            IsUsableActor(Character) ? Character->Health : 0.0f,
            Damage, DamageTypeName.c_str(), DamageTypeCategory,
            IsBlueZone ? "true" : "false", InstigatedBy, DamageCauser,
            Observer.CurrentController, Observer.LastController,
            Observer.CurrentControllerPawn, Observer.LastControllerPawn,
            Observer.CurrentSpectating, Observer.CurrentObserving,
            Observer.CurrentSpectatorState, Observer.LastSpectating,
            Observer.LastObserving, Observer.LastSpectatorState,
            Observer.PlayerState, Observer.PlayerKilled ? "true" : "false",
            Observer.PlayerObserver ? "true" : "false",
            SafeZone.RootRelativeLocation.X, SafeZone.RootRelativeLocation.Y,
            SafeZone.RootRelativeLocation.Z, SafeZone.ReplicatedLocation.X,
            SafeZone.ReplicatedLocation.Y, SafeZone.ReplicatedLocation.Z,
            SafeZone.UsedReplicatedLocation ? "replicated" : "root-relative",
            SafeZone.Center.X, SafeZone.Center.Y, SafeZone.Radius,
            SafeZone.Distance, SafeZone.IsInside ? "true" : "false",
            HasSafeZone ? "true" : "false");
        Log::Write(Log::Level::Info, Log::Module::Damage, Message);
    }

    float __fastcall LegacyCharacterTakeDamageHook(AActor* DamagedActor,
        float Damage, const FDamageEvent& DamageEvent,
        AController* InstigatedBy, AActor* DamageCauser)
    {
        const TakeDamageNative Original = NativeCharacterTakeDamage.load(
            std::memory_order_acquire);
        if (!Original)
            return 0.0f;

        UClass* CharacterClass = ATslCharacter::StaticClass();
        if (!IsUsableActor(DamagedActor) || !CharacterClass ||
            !DamagedActor->IsA(CharacterClass))
        {
            return Original(DamagedActor, Damage, DamageEvent, InstigatedBy,
                DamageCauser);
        }

        auto* Character = static_cast<ATslCharacter*>(DamagedActor);
        std::string DamageTypeName;
        int DamageTypeCategory = -1;
        UClass* DamageTypeClass = DamageEvent.DamageTypeClass;
        const bool IsBlueZone = IsBlueZoneDamageType(Character,
            DamageTypeClass, InstigatedBy, DamageCauser, &DamageTypeName,
            &DamageTypeCategory);
        if (!IsBlueZone)
        {
            return Original(DamagedActor, Damage, DamageEvent, InstigatedBy,
                DamageCauser);
        }

        const CharacterObserverState Observer =
            ResolveCharacterObserverState(Character);

        // Once a controller has moved to a spectator pawn, its abandoned
        // character must not continue receiving blue-zone pulses. Observer
        // state is resolved only from direct fields here: reflected SDK calls
        // would recursively enter ProcessEvent from inside native TakeDamage.
        if (Observer.IsObserver())
        {
            const uint32_t Count = SuppressedSpectatorBlueZoneDamageCount.fetch_add(
                1, std::memory_order_relaxed) + 1;
            if (Count <= 3 || (Count & (Count - 1)) == 0)
            {
                Log::Write(Log::Level::Info, Log::Module::Damage, "spectator guard: suppressed damage to an observer (count=" +
                    std::to_string(Count) + ", type='" + DamageTypeName +
                    "', category=" + std::to_string(DamageTypeCategory) +
                    ", damage=" + std::to_string(Damage) +
                    ", dead=" + (Observer.CharacterDead ? "true" : "false") +
                    ", killed=" + (Observer.PlayerKilled ? "true" : "false") +
                    ").");
            }
            return 0.0f;
        }

        AuthoritativeSafeZoneState SafeZone{};
        const bool HasSafeZone = ResolveAuthoritativeSafeZoneState(Character,
            SafeZone);
        if (IsUsableDamage(Damage))
        {
            const uint32_t Count = EnvironmentalDamageDiagnosticCount.fetch_add(
                1, std::memory_order_relaxed) + 1;
            if (Count <= 12 || (Count & (Count - 1)) == 0)
            {
                LogEnvironmentalDamageDiagnostic(Count, Character, Damage,
                    DamageTypeName, DamageTypeCategory, InstigatedBy,
                    DamageCauser, IsBlueZone, Observer, SafeZone, HasSafeZone);
            }
        }

        if (IsBlueZone && HasSafeZone && SafeZone.IsInside)
        {
            const uint32_t Count = SuppressedSafeZoneDamageCount.fetch_add(1,
                std::memory_order_relaxed) + 1;
            if (Count <= 3 || (Count & (Count - 1)) == 0)
            {
                Log::Write(Log::Level::Info, Log::Module::Damage, "guard: suppressed damage inside the authoritative safe zone (count=" +
                    std::to_string(Count) + ", type='" + DamageTypeName +
                    "', category=" + std::to_string(DamageTypeCategory) +
                    ", distance=" + std::to_string(SafeZone.Distance) +
                    ", radius=" + std::to_string(SafeZone.Radius) + ").");
            }
            return 0.0f;
        }

        return Original(DamagedActor, Damage, DamageEvent, InstigatedBy,
            DamageCauser);
    }

    bool InstallSpectatorBlueZoneDamageGuard()
    {
        if (SpectatorBlueZoneGuardInstalled.load(std::memory_order_acquire))
            return true;

        ATslCharacter* DefaultCharacter = ATslCharacter::GetDefaultObj();
        if (!IsUsableActor(DefaultCharacter))
            return false;

        auto** VTable = *reinterpret_cast<void***>(DefaultCharacter);
        if (!IsReadableRange(VTable,
            (TakeDamageVTableIndex + 1) * sizeof(void*)))
        {
            return false;
        }

        void* Target = VTable[TakeDamageVTableIndex];
        if (!IsReadableRange(Target, 1))
            return false;

        LPVOID Original = nullptr;
        const MH_STATUS CreateStatus = MH_CreateHook(Target,
            reinterpret_cast<LPVOID>(&LegacyCharacterTakeDamageHook),
            &Original);
        if (CreateStatus != MH_OK)
        {
            Log::Write(Log::Level::Info, Log::Module::Damage, "spectator guard: TakeDamage hook creation failed: " +
                std::string(MH_StatusToString(CreateStatus)) + ".");
            return false;
        }

        NativeCharacterTakeDamage.store(
            reinterpret_cast<TakeDamageNative>(Original),
            std::memory_order_release);
        const MH_STATUS EnableStatus = MH_EnableHook(Target);
        if (EnableStatus != MH_OK && EnableStatus != MH_ERROR_ENABLED)
        {
            NativeCharacterTakeDamage.store(nullptr, std::memory_order_release);
            MH_RemoveHook(Target);
            Log::Write(Log::Level::Info, Log::Module::Damage, "spectator guard: TakeDamage hook enable failed: " +
                std::string(MH_StatusToString(EnableStatus)) + ".");
            return false;
        }

        SpectatorBlueZoneGuardInstalled.store(true, std::memory_order_release);
        const uintptr_t ImageBase = InSDKUtils::GetImageBase();
        char Message[256]{};
        sprintf_s(Message,
            "Legacy blue-zone spectator guard: ATslCharacter::TakeDamage hook installed at TslGame+0x%llX; living-character damage is unchanged.",
            static_cast<unsigned long long>(
                reinterpret_cast<uintptr_t>(Target) - ImageBase));
        Log::Write(Log::Level::Info, Log::Module::Damage, Message);
        return true;
    }

    bool ResolveAuthoritativeBlueZoneContext(UObject* Object,
        ABattleRoyaleModeController*& Controller, ATslGameState*& GameState)
    {
        Controller = nullptr;
        GameState = nullptr;
        if (!IsUsableObject(Object))
            return false;

        UClass* ControllerClass = ABattleRoyaleModeController::StaticClass();
        if (!ControllerClass || !Object->IsA(ControllerClass))
            return false;

        UWorld* World = UWorld::GetWorld();
        if (!IsReadableRange(World, sizeof(UWorld)))
            return false;

        auto* GameMode = static_cast<ATslGameMode*>(World->AuthorityGameMode);
        if (!IsReadableRange(GameMode, sizeof(ATslGameMode)))
            return false;

        __try
        {
            if (GameMode->ModeController != Object)
                return false;

            Controller = static_cast<ABattleRoyaleModeController*>(Object);
            GameState = static_cast<ATslGameState*>(GameMode->GameState);
            return IsReadableRange(Controller,
                sizeof(ABattleRoyaleModeController)) &&
                IsReadableRange(GameState, sizeof(ATslGameState));
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            Controller = nullptr;
            GameState = nullptr;
            return false;
        }
    }

    void LogBlueZonePhase(const char* Action,
        ABattleRoyaleModeController* Controller, ATslGameState* GameState,
        int32 PhaseIndex, bool IsRelease)
    {
        char Message[512]{};
        sprintf_s(Message,
            "Legacy blue-zone phase: %s controller=%p, phase=%d, release=%s, current-radius=%.1f, warning-radius=%.1f, lerp-radius=%.1f, warning-time=%.1f, release-time=%.1f.",
            Action, Controller, PhaseIndex, IsRelease ? "true" : "false",
            GameState->SafetyZoneRadius,
            GameState->PoisonGasWarningRadius,
            GameState->LerpSafetyZoneRadius,
            GameState->ElapsedWarningDuration,
            GameState->ElapsedGasReleaseDuration);
        Log::Write(Log::Level::Info, Log::Module::Damage, Message);
    }

    void LogBlueZoneDamageCorrection(uint32_t Count, float Distance,
        float SuppliedRadius, float WarningRadius, float ReplicatedRadius,
        float PreviousDamage)
    {
        char Message[512]{};
        sprintf_s(Message,
            "Legacy blue-zone damage: corrected safe-area DPS to zero (count=%u, distance=%.1f, supplied-radius=%.1f, warning-radius=%.1f, replicated-radius=%.1f, damage=%.3f).",
            Count, Distance, SuppliedRadius, WarningRadius,
            ReplicatedRadius, PreviousDamage);
        Log::Write(Log::Level::Info, Log::Module::Damage, Message);
    }

    bool ResolvePointDamageAbi()
    {
        const uintptr_t ImageBase = InSDKUtils::GetImageBase();
        auto* VTable = reinterpret_cast<uintptr_t*>(
            ImageBase + PointDamageEventVTableOffset);
        if (!ImageBase || !IsReadableRange(VTable, sizeof(uintptr_t) * 4) ||
            VTable[0] != ImageBase + PointDamageEventFirstVirtualOffset ||
            VTable[1] != ImageBase + PointDamageEventGetTypeIdOffset ||
            VTable[2] != ImageBase + PointDamageEventIsOfTypeOffset)
        {
            return false;
        }

        PointDamageEventVTable.store(reinterpret_cast<uintptr_t>(VTable),
            std::memory_order_release);
        return true;
    }

    FVector GetHitDirection(const FHitResult& Hit)
    {
        FVector Direction = Hit.TraceEnd - Hit.TraceStart;
        if (!IsUsableVector(Direction))
            return {};

        const float MagnitudeSquared = Direction.Dot(Direction);
        if (!std::isfinite(MagnitudeSquared) || MagnitudeSquared <= 0.0001f)
            return {};

        return Direction / std::sqrt(MagnitudeSquared);
    }

    AActor* ResolveHitActor(const FHitResult& Hit)
    {
        AActor* Actor = Hit.Actor.Get();
        if (IsUsableActor(Actor))
            return Actor;

        UPrimitiveComponent* Component = Hit.Component.Get();
        if (!IsUsableObject(Component))
            return nullptr;

        Actor = Component->GetOwner();
        return IsUsableActor(Actor) ? Actor : nullptr;
    }

    AController* ResolveDamageInstigator(AActor* DamageCauser)
    {
        if (!IsUsableActor(DamageCauser))
            return nullptr;

        AController* Controller = DamageCauser->GetInstigatorController();
        if (IsUsableObject(Controller))
            return Controller;

        if (DamageCauser->IsA(APawn::StaticClass()))
        {
            Controller = static_cast<APawn*>(DamageCauser)->GetController();
            if (IsUsableObject(Controller))
                return Controller;
        }

        AActor* Owner = DamageCauser->GetOwner();
        if (IsUsableActor(Owner) && Owner->IsA(APawn::StaticClass()))
        {
            Controller = static_cast<APawn*>(Owner)->GetController();
            if (IsUsableObject(Controller))
                return Controller;
        }

        return nullptr;
    }

    enum class WorldDamageTarget
    {
        Unsupported,
        Door,
        ReactionDoor,
        Vehicle,
        TrainingTarget
    };

    WorldDamageTarget ClassifyWorldDamageTarget(AActor* DamagedActor,
        const FHitResult& Hit)
    {
        if (!IsUsableActor(DamagedActor))
            return WorldDamageTarget::Unsupported;

        UPrimitiveComponent* HitComponent = Hit.Component.Get();
        UClass* ReactionDoorClass = UTslReactionDoorComponent::StaticClass();
        if (IsUsableObject(HitComponent) && ReactionDoorClass &&
            HitComponent->IsA(ReactionDoorClass))
        {
            return WorldDamageTarget::ReactionDoor;
        }

        UClass* DoorClass = ATslDoor::StaticClass();
        if (DoorClass && DamagedActor->IsA(DoorClass))
            return WorldDamageTarget::Door;

        UClass* WheeledVehicleClass = ATslWheeledVehicle::StaticClass();
        UClass* VehicleClass = ATslFloatingVehicle::StaticClass();
        if ((WheeledVehicleClass && DamagedActor->IsA(WheeledVehicleClass)) ||
            (VehicleClass && DamagedActor->IsA(VehicleClass)))
        {
            return WorldDamageTarget::Vehicle;
        }

        if (IsTrainingRangeMatch())
        {
            const std::string ActorName = DamagedActor->GetName();
            const std::string ClassName = DamagedActor->GetClass()
                ? DamagedActor->GetClass()->GetName() : std::string{};
            if (ActorName.find("Target") != std::string::npos ||
                ActorName.find("Dummy") != std::string::npos ||
                ClassName.find("Target") != std::string::npos ||
                ClassName.find("Dummy") != std::string::npos)
            {
                return WorldDamageTarget::TrainingTarget;
            }
        }

        return WorldDamageTarget::Unsupported;
    }

    float ApplyNativeWorldPointDamage(WorldDamageTarget TargetKind,
        AActor* DamagedActor, float BaseDamage, const FVector& Direction,
        const FHitResult& Hit, AController* Instigator, AActor* DamageCauser,
        UClass* DamageTypeClass)
    {
        const float AppliedDamage = UGameplayStatics::ApplyPointDamage(
            DamagedActor,
            BaseDamage,
            Direction,
            Hit,
            Instigator,
            DamageCauser,
            DamageTypeClass);

        std::atomic_bool* FirstLog = &FirstVehicleDamageLogged;
        const char* TargetLabel = "vehicle";
        if (TargetKind == WorldDamageTarget::Door)
        {
            FirstLog = &FirstDoorDamageLogged;
            TargetLabel = "door";
        }
        else if (TargetKind == WorldDamageTarget::ReactionDoor)
        {
            FirstLog = &FirstReactionDoorDamageLogged;
            TargetLabel = "reaction-door";
        }
        else if (TargetKind == WorldDamageTarget::TrainingTarget)
        {
            FirstLog = &FirstVehicleDamageLogged;
            TargetLabel = "training-target";
        }

        bool Expected = false;
        if (FirstLog->compare_exchange_strong(Expected, true,
            std::memory_order_relaxed))
        {
            Log::Write(Log::Level::Info, Log::Module::Damage, std::string("Legacy damage RPC: first ") + TargetLabel +
                " hit forwarded through native GameplayStatics.ApplyPointDamage; base=" +
                std::to_string(BaseDamage) + ", applied=" +
                std::to_string(AppliedDamage) + ".");
        }

        return AppliedDamage;
    }

    bool ApplyNativeReactionDoorPointDamage(UPrimitiveComponent* HitComponent,
        float BaseDamage, const FDamageEvent& DamageEvent,
        AController* Instigator, AActor* DamageCauser)
    {
        UClass* ReactionDoorClass = UTslReactionDoorComponent::StaticClass();
        if (!IsUsableObject(HitComponent) || !ReactionDoorClass ||
            !HitComponent->IsA(ReactionDoorClass))
        {
            return false;
        }

        auto** VTable = *reinterpret_cast<void***>(HitComponent);
        if (!IsReadableRange(VTable,
            (ReceiveComponentDamageVTableIndex + 1) * sizeof(void*)))
        {
            return false;
        }

        const auto ReceiveComponentDamage =
            reinterpret_cast<ReceiveComponentDamageNative>(
                VTable[ReceiveComponentDamageVTableIndex]);
        const uintptr_t ExpectedTarget = InSDKUtils::GetImageBase() +
            ReactionDoorReceiveComponentDamageOffset;
        if (reinterpret_cast<uintptr_t>(ReceiveComponentDamage) !=
            ExpectedTarget ||
            !IsReadableRange(reinterpret_cast<const void*>(
                ReceiveComponentDamage), 1))
        {
            return false;
        }

        ReceiveComponentDamage(HitComponent, BaseDamage, DamageEvent,
            Instigator, DamageCauser);

        bool Expected = false;
        if (FirstReactionDoorDamageLogged.compare_exchange_strong(Expected,
            true, std::memory_order_relaxed))
        {
            Log::Write(Log::Level::Info, Log::Module::Damage, "RPC: first reaction-door weapon hit forwarded through native UTslReactionDoorComponent::ReceiveComponentDamage; authoritative accumulation and replication remain engine-owned.");
        }
        return true;
    }

    void LogFailure(DamageRpcKind Kind, const char* Reason)
    {
        const size_t Index = static_cast<size_t>(Kind);
        const uint32_t Count = FailureCounts[Index].fetch_add(1,
            std::memory_order_relaxed) + 1;
        if (Count <= 3 || (Count & (Count - 1)) == 0)
        {
            Log::Write(Log::Level::Info, Log::Module::Damage, std::string("Legacy damage RPC: ") + Reason +
                " (count=" + std::to_string(Count) + ").");
        }
    }

    void LogFirstDispatch(DamageRpcKind Kind, const char* Label,
        float BaseDamage, float AppliedDamage, const FAttackId& AttackId)
    {
        bool Expected = false;
        if (!FirstDispatchLogged[static_cast<size_t>(Kind)].compare_exchange_strong(
            Expected, true, std::memory_order_relaxed))
        {
            return;
        }

        Log::Write(Log::Level::Info, Log::Module::Damage, std::string("Legacy damage RPC: first ") + Label +
            " hit forwarded through native AActor.TakeDamage; base=" +
            std::to_string(BaseDamage) + ", applied=" +
            std::to_string(AppliedDamage) + ", attack-id=" +
            std::to_string(AttackId.ID) + ".");
    }

    bool ForwardPointDamage(DamageRpcKind Kind, const char* Label,
        AActor* DamageCauser, const FHitResult& Hit, const FVector& Direction,
        float BaseDamage, UClass* DamageTypeClass, const FAttackId& AttackId,
        bool CanDestroyDoors)
    {
        // ProcessEvent also sees locally-issued outbound RPCs. Only the
        // authoritative actor may execute the reconstructed server body.
        if (!IsUsableActor(DamageCauser) ||
            DamageCauser->Role != ENetRole::Value_3)
        {
            return false;
        }

        AActor* DamagedActor = ResolveHitActor(Hit);
        if (!DamagedActor)
        {
            LogFailure(Kind, "hit target could not be resolved from FHitResult");
            return false;
        }

        UClass* CharacterClass = ATslCharacter::StaticClass();
        const bool IsCharacter = CharacterClass &&
            DamagedActor->IsA(CharacterClass);
        const WorldDamageTarget WorldTarget = IsCharacter
            ? WorldDamageTarget::Unsupported
            : ClassifyWorldDamageTarget(DamagedActor, Hit);
        if (!IsCharacter && WorldTarget == WorldDamageTarget::Unsupported)
        {
            bool Expected = false;
            if (FirstNonCharacterTargetLogged.compare_exchange_strong(Expected,
                true, std::memory_order_relaxed))
            {
                Log::Write(Log::Level::Info, Log::Module::Damage, "RPC: skipped an unsupported non-character hit target; door, reaction-door, and vehicle point damage use the engine-native path.");
            }
            return false;
        }

        if ((WorldTarget == WorldDamageTarget::Door ||
            WorldTarget == WorldDamageTarget::ReactionDoor) &&
            !CanDestroyDoors)
        {
            bool Expected = false;
            if (FirstUnarmedDoorIgnoredLogged.compare_exchange_strong(
                Expected, true, std::memory_order_relaxed))
            {
                Log::Write(Log::Level::Info, Log::Module::Damage, "RPC: ignored damage from a weapon that cannot destroy doors.");
            }
            return false;
        }

        if (!IsUsableDamage(BaseDamage))
        {
            LogFailure(Kind, "weapon damage value is invalid");
            return false;
        }
        if (!IsUsableObject(DamageTypeClass))
        {
            LogFailure(Kind, "weapon DamageType class is unavailable");
            return false;
        }

        const uintptr_t EventVTable = PointDamageEventVTable.load(
            std::memory_order_acquire);
        if (!EventVTable)
        {
            LogFailure(Kind, "point-damage ABI is unavailable for this build");
            return false;
        }

        auto** ActorVTable = *reinterpret_cast<void***>(DamagedActor);
        if (!IsReadableRange(ActorVTable,
            (TakeDamageVTableIndex + 1) * sizeof(void*)))
        {
            LogFailure(Kind, "target TakeDamage vtable slot is unreadable");
            return false;
        }

        const auto TakeDamage = reinterpret_cast<TakeDamageNative>(
            ActorVTable[TakeDamageVTableIndex]);
        if (!IsReadableRange(reinterpret_cast<const void*>(TakeDamage), 1))
        {
            LogFailure(Kind, "target TakeDamage implementation is unreadable");
            return false;
        }

        const FVector SafeDirection = IsUsableVector(Direction) ? Direction : FVector{};
        AController* Instigator = ResolveDamageInstigator(DamageCauser);

        FTslPointDamageEvent DamageEvent{};
        *reinterpret_cast<uintptr_t*>(&DamageEvent) = EventVTable;
        DamageEvent.AttackId = AttackId;
        DamageEvent.DamageTypeClass = DamageTypeClass;
        DamageEvent.Damage = BaseDamage;
        DamageEvent.ShotDirection.X = SafeDirection.X;
        DamageEvent.ShotDirection.Y = SafeDirection.Y;
        DamageEvent.ShotDirection.Z = SafeDirection.Z;
        DamageEvent.HitInfo = Hit;
        DamageEvent.bDestructibleDoor = CanDestroyDoors;

        if (WorldTarget == WorldDamageTarget::ReactionDoor)
        {
            UPrimitiveComponent* HitComponent = Hit.Component.Get();
            if (!ApplyNativeReactionDoorPointDamage(HitComponent, BaseDamage,
                DamageEvent, Instigator, DamageCauser))
            {
                LogFailure(Kind,
                    "reaction-door ReceiveComponentDamage ABI verification failed");
                return false;
            }
            return true;
        }

        // Vehicles retain the engine-created point-event path. Actor-owned
        // doors must receive FTslPointDamageEvent directly so their native
        // type-3 and bDestructibleDoor checks can succeed.
        if (WorldTarget == WorldDamageTarget::Vehicle ||
            WorldTarget == WorldDamageTarget::TrainingTarget)
        {
            ApplyNativeWorldPointDamage(WorldTarget, DamagedActor, BaseDamage,
                SafeDirection, Hit, Instigator, DamageCauser, DamageTypeClass);
            return true;
        }

        MeleeDamageContext MeleeContext{};
        MeleeDamageContext* PreviousMeleeContext = ActiveMeleeDamageContext;
        if ((Kind == DamageRpcKind::Melee || Kind == DamageRpcKind::Unarmed)
            && IsCharacter)
        {
            auto* Victim = static_cast<ATslCharacter*>(DamagedActor);
            MeleeContext.Victim = Victim;
            MeleeContext.VictimPlayerState = ResolveCharacterPlayerState(
                Victim, nullptr, nullptr);
            MeleeContext.DamageTypeClass = DamageTypeClass;
            MeleeContext.IsUnarmed = Kind == DamageRpcKind::Unarmed;

            auto* Attacker = static_cast<ATslCharacter*>(DamageCauser);
            if (Kind == DamageRpcKind::Unarmed)
            {
                // Prefer the extracted UE4.18 DmgType_Fist_C asset itself.
                // The character field may contain a generic melee damage type
                // (the observed category 4), which does not select the punch
                // row in BP_KillMessageWidget.
                UClass* FistDamageType = ResolveFistsDamageTypeClass();
                UClass* CharacterDamageType =
                    ResolveAttackerUnarmedDamageTypeSafe(Attacker);
                bool UsedFallbackDamageType = false;
                if (IsUsableObject(FistDamageType))
                {
                    MeleeContext.DamageTypeClass = FistDamageType;
                    UsedFallbackDamageType = true;
                }
                else if (IsUsableObject(CharacterDamageType))
                    MeleeContext.DamageTypeClass = CharacterDamageType;

                LogUnarmedDamageTypeResolution(
                    Attacker->GetClass(), MeleeContext.DamageTypeClass,
                    UsedFallbackDamageType);
            }

            if (Kind == DamageRpcKind::Melee)
            {
                auto* Weapon = static_cast<ATslWeapon_Melee*>(DamageCauser);
                MeleeContext.Weapon = Weapon;
                MeleeContext.WeaponClass = Weapon->GetClass();
            }
            else
            {
                // Unarmed: the attacker is the character. The client resolves
                // the punch kill-feed icon from the DamageCauserClass CDO
                // weapon icon, so send a class the client recognizes.
                MeleeContext.Weapon = nullptr;
                MeleeContext.WeaponClass = nullptr;

                auto* Attacker = static_cast<ATslCharacter*>(DamageCauser);
                UWeaponProcessorComponent* Processor = Attacker->WeaponProcessor;

                int32 UnarmedIndex = -1;
                int32 EquippedCount = -1;
                if (IsUsableObject(Processor))
                {
                    __try
                    {
                        UnarmedIndex = Processor->GetUnarmedWeaponIndex();
                        EquippedCount = Processor->EquippedWeapons.Num();
                        if (UnarmedIndex >= 0 &&
                            Processor->IsUnarmedWeapon(UnarmedIndex))
                        {
                            ATslWeapon* UnarmedWeapon =
                                Processor->GetWeaponByIndex(UnarmedIndex);
                            if (IsUsableObject(UnarmedWeapon) &&
                                UnarmedWeapon->IsA(ATslWeapon::StaticClass()))
                                MeleeContext.WeaponClass =
                                UnarmedWeapon->GetClass();
                        }
                    }
                    __except (EXCEPTION_EXECUTE_HANDLER)
                    {
                        MeleeContext.WeaponClass = nullptr;
                    }
                }

                if (!IsUsableObject(MeleeContext.WeaponClass))
                {
                    // This listen-server build reports no unarmed entry
                    // (index -1), so resolve the class the client uses for
                    // the punch icon: the settings.ini path, then a
                    // loaded-class name scan, then the canonical base melee
                    // class.
                    MeleeContext.WeaponClass = ResolveFistsWeaponClass();
                }

                LogUnarmedResolutionOnce(Attacker, Processor, UnarmedIndex,
                    EquippedCount, MeleeContext.WeaponClass);
            }

            ActiveMeleeDamageContext = &MeleeContext;

            // A later bleed-out must not reuse an old attack context. Replace
            // any prior entry now; the new one is cached after validation.
            if (MeleeContext.VictimPlayerState)
                CachedMeleeContexts.erase(MeleeContext.VictimPlayerState);
        }

        const float AppliedDamage = TakeDamage(
            DamagedActor,
            BaseDamage,
            DamageEvent,
            Instigator,
            DamageCauser);
        ActiveMeleeDamageContext = PreviousMeleeContext;

        if (MeleeContext.VictimPlayerState)
        {
            // Reaching this point means the hit passed all RPC, authority,
            // target, damage, and ABI validation. TakeDamage may legitimately
            // return zero while still emitting a groggy/death message.
            MeleeContext.CreatedAt = GetTickCount64();
            CachedMeleeContexts[MeleeContext.VictimPlayerState] = MeleeContext;
        }

        LogFirstDispatch(Kind, Label, BaseDamage, AppliedDamage, AttackId);
        return true;
    }

    bool HandleTrajectoryHit(ATslWeapon_Trajectory* Weapon, void* Parameters)
    {
        if (!IsUsableActor(Weapon) || !Parameters)
            return false;

        const auto* Parms = static_cast<
            const Params::TslWeapon_Trajectory_ServerNotifyHit*>(Parameters);

        FWeaponTrajectoryConfig Config{};
        if (IsReadableRange(Weapon->WeaponTrajectoryData,
            sizeof(UWeaponTrajectoryData)))
        {
            Config = Weapon->WeaponTrajectoryData->TrajectoryConfig;
        }
        else
        {
            // This reflected getter is only a fallback for weapons whose data
            // asset has not been published on the server actor.
            Config = Weapon->GetWeaponTrajectoryConfig();
        }

        const FVector Direction{
            Parms->ShootDir.X,
            Parms->ShootDir.Y,
            Parms->ShootDir.Z
        };
        return ForwardPointDamage(
            DamageRpcKind::Trajectory,
            "trajectory",
            Weapon,
            Parms->Impact,
            Direction,
            static_cast<float>(Config.HitDamage),
            Config.DamageType.Get(),
            Parms->AttackId,
            Weapon->WeaponConfig.DestructibleDoor);
    }

    bool HandleMeleeHit(ATslWeapon_Melee* Weapon, void* Parameters)
    {
        if (!IsUsableActor(Weapon) || !Parameters)
            return false;

        const auto* Parms = static_cast<
            const Params::TslWeapon_Melee_ServerNotifyHit*>(Parameters);
        const int32 ImpactCount = Parms->Impacts.Num();
        if (ImpactCount <= 0 || ImpactCount > 32 ||
            !IsReadableRange(Parms->Impacts.GetDataPtr(),
                static_cast<size_t>(ImpactCount) * sizeof(FHitResult)))
        {
            LogFailure(DamageRpcKind::Melee,
                "melee impact array is invalid");
            return false;
        }

        std::array<AActor*, 32> DamagedActors{};
        size_t DamagedActorCount = 0;
        bool Forwarded = false;
        for (int32 Index = 0; Index < ImpactCount; ++Index)
        {
            const FHitResult& Hit = Parms->Impacts[Index];
            AActor* HitActor = ResolveHitActor(Hit);
            if (!HitActor)
                continue;

            bool AlreadyDamaged = false;
            for (size_t ActorIndex = 0; ActorIndex < DamagedActorCount; ++ActorIndex)
            {
                if (DamagedActors[ActorIndex] == HitActor)
                {
                    AlreadyDamaged = true;
                    break;
                }
            }
            if (AlreadyDamaged)
                continue;

            DamagedActors[DamagedActorCount++] = HitActor;
            Forwarded |= ForwardPointDamage(
                DamageRpcKind::Melee,
                "melee",
                Weapon,
                Hit,
                GetHitDirection(Hit),
                Weapon->Damage,
                Weapon->DamageType.Get(),
                Parms->AttackId,
                Weapon->WeaponConfig.DestructibleDoor);
        }

        return Forwarded;
    }

    bool HandleUnarmedHit(ATslCharacter* Character, void* Parameters)
    {
        if (!IsUsableActor(Character) || !Parameters)
            return false;

        const auto* Parms = static_cast<
            const Params::TslCharacter_Server_UnarmedHitNotify*>(Parameters);
        const bool IsHeavyPunch =
            Parms->PunchDamageType == EPunchDamageType::Value_1;
        const float Damage = IsHeavyPunch ?
            Character->PunchHeavy_Damage : Character->Punch_Damage;
        FAttackId AttackId{};
        AttackId.ID = -1;

        return ForwardPointDamage(
            DamageRpcKind::Unarmed,
            "unarmed",
            Character,
            Parms->Hit,
            GetHitDirection(Parms->Hit),
            Damage,
            Character->UnarmedDamageType.Get(),
            AttackId,
            false);
    }
}

void PatchLegacyMeleeDamageMessage(UObject* Object, void* Parameters,
    const char* FunctionName)
{
    if (!Object || !Parameters || !FunctionName)
        return;

    if (std::strcmp(FunctionName, "ClientSendGroggyMessage") == 0)
    {
        const auto* Message = static_cast<const Params::
            TslCharacter_ClientSendGroggyMessage*>(Parameters);
        auto* VictimCharacter = IsUsableObject(Object) &&
            Object->IsA(ATslCharacter::StaticClass())
            ? static_cast<ATslCharacter*>(Object) : nullptr;
        const UDamageType* BleedOutDamageType = nullptr;
        const UDamageType* GroggyDamageType = nullptr;
        if (VictimCharacter)
        {
            UClass* BleedOutClass = VictimCharacter->BleedOutDamageType.Get();
            UClass* GroggyClass = VictimCharacter->GroggyDamageType.Get();
            BleedOutDamageType = IsUsableObject(BleedOutClass)
                ? static_cast<const UDamageType*>(BleedOutClass->DefaultObject)
                : nullptr;
            GroggyDamageType = IsUsableObject(GroggyClass)
                ? static_cast<const UDamageType*>(GroggyClass->DefaultObject)
                : nullptr;
        }
        if (VictimCharacter &&
            (Message->GroggyCauserDamageType == BleedOutDamageType ||
                Message->GroggyCauserDamageType == GroggyDamageType))
        {
            // Bleed-out/groggy maintenance messages must retain their native
            // attribution. Never reuse the preceding punch context for them.
            return;
        }
    }

    // Try active context first (within TakeDamage call stack).
    MeleeDamageContext* Context = ActiveMeleeDamageContext;

    // If not active, try a fresh cached context for BroadcastDeath only. The
    // groggy RPC is emitted while the reconstructed TakeDamage call is active;
    // looking it up later by victim incorrectly reuses the previous weapon for
    // bleed-out and subsequent knockdowns.
    MeleeDamageContext CachedContext{};
    if (!Context && std::strcmp(FunctionName, "BroadcastDeath") == 0)
    {
        ATslPlayerState* PlayerState = nullptr;
        bool ConsumeCache = std::strcmp(FunctionName, "BroadcastDeath") == 0;
        if (ConsumeCache)
        {
            PlayerState = static_cast<ATslPlayerState*>(Object);
        }
        else
        {
            const auto* Message = static_cast<
                const Params::TslCharacter_ClientSendGroggyMessage*>(Parameters);
            PlayerState = Message->VictimPlayerState;
        }

        if (IsUsableActor(PlayerState))
        {
            auto It = CachedMeleeContexts.find(PlayerState);
            if (It != CachedMeleeContexts.end())
            {
                const ULONGLONG Age = GetTickCount64() - It->second.CreatedAt;
                if (It->second.CreatedAt != 0 && Age <= MeleeContextLifetimeMs)
                {
                    CachedContext = It->second;
                    Context = &CachedContext;
                }
                if (ConsumeCache || Age > MeleeContextLifetimeMs)
                    CachedMeleeContexts.erase(It);
            }
        }
    }

    // If the reflected RPC was consumed before the widget builds FDeathMessage,
    // recover the newest unarmed context for the local kill-feed consumer.
    if (!Context && (std::strcmp(FunctionName, "SetDeathMessage") == 0 ||
        std::strcmp(FunctionName, "EnqueueDeathMessage") == 0))
    {
        ULONGLONG NewestTimestamp = 0;
        for (const auto& Entry : CachedMeleeContexts)
        {
            const MeleeDamageContext& Candidate = Entry.second;
            if (Candidate.IsUnarmed && Candidate.CreatedAt >= NewestTimestamp)
            {
                NewestTimestamp = Candidate.CreatedAt;
                CachedContext = Candidate;
            }
        }
        if (NewestTimestamp != 0)
            Context = &CachedContext;
    }

    if (!Context || !IsUsableObject(Context->DamageTypeClass))
    {
        return;
    }

    UObject* DamageTypeDefaultObject = Context->DamageTypeClass->DefaultObject;
    const UDamageType* DamageTypeDefault =
        IsUsableObject(DamageTypeDefaultObject)
        ? static_cast<const UDamageType*>(DamageTypeDefaultObject)
        : nullptr;

    UClass* PreviousCauserClass = nullptr;
    const UDamageType* PreviousDamageType = nullptr;
    uint8 PreviousReason = 0xFF;
    uint8 PostReason = 0xFF;

    bool IsUnarmedAttack = Context->IsUnarmed;
    // For unarmed kills the client widget has a dedicated fallback:
    // TslDamageType::DamageTypeCategory -> GetTextureFromDamageTypeCategory.
    // A non-null TslWeapon_Melee class with an empty CDO icon can suppress
    // that fallback, so deliberately omit the causer class for punches.
    UClass* OutgoingCauserClass = IsUnarmedAttack
        ? nullptr : Context->WeaponClass;

    if (std::strcmp(FunctionName, "BroadcastDeath") == 0)
    {
        if (Context->VictimPlayerState &&
            Object != Context->VictimPlayerState)
        {
            return;
        }

        auto* Message = static_cast<
            Params::TslPlayerState_BroadcastDeath*>(Parameters);
        PreviousCauserClass = Message->DamageCauserClass;
        PreviousDamageType = Message->KillerDamageType;
        PreviousReason = static_cast<uint8>(Message->DamageReason);

        Message->DamageCauserClass = OutgoingCauserClass;
        if (DamageTypeDefault)
            Message->KillerDamageType = DamageTypeDefault;
    }
    else if (std::strcmp(FunctionName, "ClientSendGroggyMessage") == 0)
    {
        auto* Message = static_cast<
            Params::TslCharacter_ClientSendGroggyMessage*>(Parameters);
        if (Context->VictimPlayerState && Message->VictimPlayerState &&
            Message->VictimPlayerState != Context->VictimPlayerState)
        {
            return;
        }

        PreviousCauserClass = Message->DamageCauserClass;
        PreviousDamageType = Message->GroggyCauserDamageType;
        PreviousReason = static_cast<uint8>(Message->DamageReason);

        Message->DamageCauserClass = OutgoingCauserClass;
        if (DamageTypeDefault)
            Message->GroggyCauserDamageType = DamageTypeDefault;
    }
    else if (std::strcmp(FunctionName, "SetDeathMessage") == 0 ||
        std::strcmp(FunctionName, "EnqueueDeathMessage") == 0)
    {
        FDeathMessage* DeathMessage = nullptr;
        if (std::strcmp(FunctionName, "SetDeathMessage") == 0)
        {
            DeathMessage = &static_cast<Params::TslKillMessageWidget_SetDeathMessage*>(
                Parameters)->InDeathMessage;
        }
        else
        {
            DeathMessage = &static_cast<Params::TslKillMessageListWidget_EnqueueDeathMessage*>(
                Parameters)->DeathMessage;
        }

        if (!DeathMessage)
            return;

        PreviousDamageType = DeathMessage->DamageType;
        if (IsUnarmedAttack && DamageTypeDefaultObject->IsA(
            UTslDamageType::StaticClass()))
        {
            auto* TslDamageType = static_cast<UTslDamageType*>(
                DamageTypeDefaultObject);
            DeathMessage->DamageType = const_cast<UTslDamageType*>(
                static_cast<const UTslDamageType*>(DamageTypeDefault));
            DeathMessage->DamageCauserIcon = TslDamageType->KillIcon.Texture;
            DeathMessage->DamageCauserIconRatio = 1.0f;
        }
    }
    else
    {
        return;
    }

    bool Expected = false;
    if (FirstMeleeMessagePatchLogged.compare_exchange_strong(Expected, true,
        std::memory_order_relaxed))
    {
        int DamageCategory = -1;
        UTexture* DamageKillIcon = nullptr;

        if (DamageTypeDefaultObject &&
            DamageTypeDefaultObject->IsA(UTslDamageType::StaticClass()))
        {
            auto* TslDamageType = static_cast<UTslDamageType*>(
                DamageTypeDefaultObject);
            DamageCategory = static_cast<int>(
                TslDamageType->DamageTypeCategory);
            DamageKillIcon = TslDamageType->KillIcon.Texture;
        }

        UClass* PostCauserClass = nullptr;
        if (std::strcmp(FunctionName, "BroadcastDeath") == 0)
        {
            PostCauserClass = static_cast<Params::TslPlayerState_BroadcastDeath*>(Parameters)->DamageCauserClass;
            PostReason = static_cast<uint8>(
                static_cast<Params::TslPlayerState_BroadcastDeath*>(Parameters)->DamageReason);
        }
        else if (std::strcmp(FunctionName, "ClientSendGroggyMessage") == 0)
        {
            PostCauserClass = static_cast<Params::TslCharacter_ClientSendGroggyMessage*>(Parameters)->DamageCauserClass;
            PostReason = static_cast<uint8>(
                static_cast<Params::TslCharacter_ClientSendGroggyMessage*>(Parameters)->DamageReason);
        }

        const std::string PreviousCauserName = IsUsableObject(PreviousCauserClass)
            ? PreviousCauserClass->GetName()
            : std::string("<null>");

        char Diagnostic[1024]{};
        sprintf_s(Diagnostic,
            "Legacy melee killfeed: patched %s weapon=%p class=%p unarmed=%d outgoing-causer=%p post-causer=%p, damage-class=%p CDO=%p category=%d kill-icon=%p, prev-causer=%p prev-causer-name=%s prev-damage-type=%p, prev-reason=%d post-reason=%d.",
            FunctionName, Context->Weapon, Context->WeaponClass, IsUnarmedAttack ? 1 : 0,
            OutgoingCauserClass, PostCauserClass, Context->DamageTypeClass,
            DamageTypeDefaultObject, DamageCategory, DamageKillIcon,
            PreviousCauserClass, PreviousCauserName.c_str(), PreviousDamageType,
            PreviousReason, PostReason);
        Log::Write(Log::Level::Info, Log::Module::Damage, Diagnostic);
    }

}

bool ShouldSuppressLegacyBleedOutDamageFeedback(UObject* Object,
    void* Parameters)
{
    if (!Object || !Parameters ||
        !Object->IsA(ATslCharacter::StaticClass()))
    {
        return false;
    }

    auto* Character = static_cast<ATslCharacter*>(Object);
    const auto* Message = static_cast<
        const Params::TslCharacter_ClientDrawDamageInfo*>(Parameters);
    const UDamageType* BleedOutDamageType = nullptr;
    const UDamageType* GroggyDamageType = nullptr;
    UClass* BleedOutClass = Character->BleedOutDamageType.Get();
    UClass* GroggyClass = Character->GroggyDamageType.Get();
    if (IsUsableObject(BleedOutClass))
        BleedOutDamageType = static_cast<const UDamageType*>(
            BleedOutClass->DefaultObject);
    if (IsUsableObject(GroggyClass))
        GroggyDamageType = static_cast<const UDamageType*>(
            GroggyClass->DefaultObject);

    if (Character->IsGroggying())
    {
        // This build sends periodic bleed damage through the same client hit
        // feedback RPC with a generic damage type. Once groggy, suppress that
        // feedback path; death/killfeed RPCs remain untouched.
        return true;
    }

    return Message->DamageType == BleedOutDamageType ||
        Message->DamageType == GroggyDamageType;
}
bool ShouldSuppressLegacyGroggyHitReaction(UObject* Object)
{
    if (!Object || !Object->IsA(ATslCharacter::StaticClass()))
        return false;
    return static_cast<ATslCharacter*>(Object)->IsGroggying();
}

void InitializeLegacyDamageRpcForwarding()
{
    bool Expected = false;
    if (!InitializationLogged.compare_exchange_strong(Expected, true,
        std::memory_order_relaxed))
    {
        return;
    }

    const bool PointDamageReady = ResolvePointDamageAbi();
    if (!PointDamageReady)
    {
        Log::Write(Log::Level::Info, Log::Module::Damage, "native FPointDamageEvent ABI verification failed; hit RPC forwarding is disabled.");
    }

    const bool SpectatorGuardReady = InstallSpectatorBlueZoneDamageGuard();
    if (!SpectatorGuardReady)
    {
        Log::Write(Log::Level::Info, Log::Module::Damage, "spectator guard: ATslCharacter::TakeDamage hook was not installed; observer poison damage is unchanged.");
    }

    if (PointDamageReady)
    {
        Log::Write(Log::Level::Info, Log::Module::Damage, "server-hit RPC forwarding, native reaction-door accumulation, reflected safe-zone correction, and the targeted spectator blue-zone guard are enabled; no Tick work is installed.");
    }
}

bool ShouldProcessLegacyBlueZonePhase(UObject* Object)
{
    ABattleRoyaleModeController* Controller = nullptr;
    ATslGameState* GameState = nullptr;
    if (!ResolveAuthoritativeBlueZoneContext(Object, Controller, GameState))
        return true;

    __try
    {
        const int32 PhaseIndex = Controller->NextPoisonGasIndex;
        const bool IsRelease = GameState->bIsGasRelease;
        if (PhaseIndex < 0 || PhaseIndex > 64)
            return true;

        if (BlueZonePhaseState.Controller != Controller ||
            BlueZonePhaseState.PhaseIndex != PhaseIndex)
        {
            BlueZonePhaseState.Controller = Controller;
            BlueZonePhaseState.PhaseIndex = PhaseIndex;
            BlueZonePhaseState.ReleaseAccepted = IsRelease;
            LogBlueZonePhase("accepted new phase", Controller, GameState,
                PhaseIndex, IsRelease);
            return true;
        }

        if (IsRelease && BlueZonePhaseState.ReleaseAccepted)
        {
            LogBlueZonePhase("suppressed duplicate release", Controller,
                GameState, PhaseIndex, IsRelease);
            return false;
        }

        if (!IsRelease && BlueZonePhaseState.ReleaseAccepted)
        {
            LogBlueZonePhase("suppressed release regression", Controller,
                GameState, PhaseIndex, IsRelease);
            return false;
        }

        if (IsRelease)
        {
            BlueZonePhaseState.ReleaseAccepted = true;
            LogBlueZonePhase("accepted release", Controller, GameState,
                PhaseIndex, IsRelease);
        }
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return true;
    }
}

void CorrectLegacyBlueZoneDamageResult(UObject* Object, void* Parameters)
{
    if (!Parameters)
        return;

    ABattleRoyaleModeController* Controller = nullptr;
    ATslGameState* GameState = nullptr;
    if (!ResolveAuthoritativeBlueZoneContext(Object, Controller, GameState))
        return;

    __try
    {
        auto* Parms = static_cast<
            Params::BattleRoyaleModeController_GetPoisonGasDamagePerSecond*>(
                Parameters);
        if (!std::isfinite(Parms->CharaterToCenterDistance) ||
            !std::isfinite(Parms->SafetyZoneRadius) ||
            !std::isfinite(Parms->PoisonGasWarningRadius) ||
            !std::isfinite(Parms->ReturnValue) ||
            Parms->CharaterToCenterDistance < 0.0f ||
            Parms->SafetyZoneRadius <= 0.0f ||
            Parms->ReturnValue <= 0.0f ||
            Parms->CharaterToCenterDistance > Parms->SafetyZoneRadius)
        {
            return;
        }

        const float PreviousDamage = Parms->ReturnValue;
        Parms->ReturnValue = 0.0f;
        const uint32_t Count = CorrectedBlueZoneDamageCount.fetch_add(1,
            std::memory_order_relaxed) + 1;
        if (Count <= 3 || (Count & (Count - 1)) == 0)
        {
            LogBlueZoneDamageCorrection(Count,
                Parms->CharaterToCenterDistance,
                Parms->SafetyZoneRadius, Parms->PoisonGasWarningRadius,
                GameState->SafetyZoneRadius, PreviousDamage);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

bool HandleLegacyDamageRpc(UObject* Object, void* Parameters,
    const char* FunctionName)
{
    if (!Object || !Parameters || !FunctionName)
        return false;

    if (strcmp(FunctionName, "Server_UnarmedHitNotify") == 0)
    {
        UClass* CharacterClass = ATslCharacter::StaticClass();
        if (CharacterClass && Object->IsA(CharacterClass))
        {
            return HandleUnarmedHit(static_cast<ATslCharacter*>(Object),
                Parameters);
        }
        return false;
    }

    if (strcmp(FunctionName, "ServerNotifyHit") != 0)
        return false;

    UClass* TrajectoryClass = ATslWeapon_Trajectory::StaticClass();
    if (TrajectoryClass && Object->IsA(TrajectoryClass))
    {
        return HandleTrajectoryHit(static_cast<ATslWeapon_Trajectory*>(Object),
            Parameters);
    }

    UClass* MeleeClass = ATslWeapon_Melee::StaticClass();
    if (MeleeClass && Object->IsA(MeleeClass))
    {
        return HandleMeleeHit(static_cast<ATslWeapon_Melee*>(Object), Parameters);
    }

    return false;
}

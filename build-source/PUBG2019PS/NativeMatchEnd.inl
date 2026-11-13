// Verified native entry points for the supplied 6.0.6.3 executable only.
// Reflected EndMatch interception alone misses internal C++ calls.
using NativeMatchEndFn = void(*)(ATslGameMode*);
static NativeMatchEndFn NextNativeFinish = nullptr, NextNativeEnd = nullptr;
static thread_local bool InsideNativeEndCheck = false;

static bool PrematureNativeMatchEnd(ATslGameMode* GM, const char* Reason)
{
    if (InsideNativeEndCheck || !GReady || !S.KeepMatchWhileBotsAlive || GMatchOver ||
        GetCurrentThreadId() != GGameThreadId) return false;
    auto* World = UWorld::GetWorld();
    if (!World || World->AuthorityGameMode != GM || GM->MatchState.ToString() != "InProgress") return false;
    struct Scope {
        Scope() { InsideNativeEndCheck = true; }
        ~Scope() { InsideNativeEndCheck = false; }
    } ScopeGuard;
    SyncGameCounters(World);
    ApplyCounters();
    // This changes the server's decision, not its UI notifications. Legitimate
    // match results still run once the current human/bot team count permits it.
    return HoldMatchEnd(Reason);
}

static void NativeFinishMatch(ATslGameMode* GM)
{
    if (!PrematureNativeMatchEnd(GM, "native FinishMatch")) NextNativeFinish(GM);
}

static void NativeEndMatch(ATslGameMode* GM)
{
    if (!PrematureNativeMatchEnd(GM, "native EndMatch")) NextNativeEnd(GM);
}

static void InstallNativeMatchEndGuard()
{
    if (!S.KeepMatchWhileBotsAlive || !S.BotsInPlayerCount) return;
    auto* World = UWorld::GetWorld();
    if (!World || !World->AuthorityGameMode || !World->AuthorityGameMode->IsA(ATslGameMode::StaticClass())) return;
    auto* GM = static_cast<ATslGameMode*>(World->AuthorityGameMode);
    const auto Base = InSDKUtils::GetImageBase();
    const auto* Dos = reinterpret_cast<IMAGE_DOS_HEADER*>(Base);
    const auto* Nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(Base + Dos->e_lfanew);
    constexpr unsigned char Prefix[]{0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0x01,0x48,0x8b,0xd9};
    auto* Finish = reinterpret_cast<void*>(Base + 0x4d0590);
    auto* End = reinterpret_cast<void*>(Base + 0x216aa90);
    auto** VTable = *reinterpret_cast<void***>(GM);
    if (Nt->FileHeader.TimeDateStamp != 0x5d3ed854 || Nt->OptionalHeader.SizeOfImage != 0x853d000 ||
        !GFnFinishMatch || reinterpret_cast<uintptr_t>(GFnFinishMatch->ExecFunction) != Base + 0x2c8c570 ||
        VTable[0x830/8] != End || std::memcmp(Finish, Prefix, sizeof(Prefix)) != 0 ||
        std::memcmp(End, Prefix, sizeof(Prefix)) != 0)
    {
        BLOG("Native match accounting refused: unsupported match-end entry points");
        return;
    }
    const auto First = MH_CreateHook(Finish, reinterpret_cast<void*>(&NativeFinishMatch), reinterpret_cast<void**>(&NextNativeFinish));
    const auto Second = First == MH_OK ? MH_CreateHook(End, reinterpret_cast<void*>(&NativeEndMatch), reinterpret_cast<void**>(&NextNativeEnd)) : MH_ERROR_NOT_CREATED;
    const auto EnableFirst = Second == MH_OK ? MH_EnableHook(Finish) : MH_ERROR_NOT_CREATED;
    const auto EnableSecond = EnableFirst == MH_OK ? MH_EnableHook(End) : MH_ERROR_NOT_CREATED;
    if (EnableSecond != MH_OK)
    {
        if (EnableFirst == MH_OK) MH_DisableHook(Finish);
        BLOG("Native match accounting refused: hook installation failed");
        return;
    }
    GGuardOk = true;
    GGuardWhat = "validated native FinishMatch and EndMatch";
    BLOG("Native match accounting ready: native FinishMatch/EndMatch guarded; result notifications preserved");
}

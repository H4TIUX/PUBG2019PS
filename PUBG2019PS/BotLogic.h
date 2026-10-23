#pragma once

#include <fstream>
#include <Windows.h>
#include <chrono>
#include <ctime>
#include <iostream>
#include <random>
#include <vector>
#include "SDK.hpp"
#include <string>
#include <minwinbase.h>

using namespace SDK;

namespace Bots
{
    // Call once from the injected MainThread, after you know you are the server.
    void Init(HMODULE Module);

    // Call at the top of the ProcessEvent hook. Cheap when there is nothing to do.
    void Tick();

    // Call when the match starts (K2_OnSetMatchState -> match starting).
    void OnMatchStarted();

    // Standalone DLL: detect match start ourselves by watching AGameState::MatchState /
    // ATslGameState::RemainingTime (same rule as the server code: state change while
    // RemainingTime <= 0). Off by default; the integrated build calls OnMatchStarted itself.
    void SetAutoDetectMatchStart(bool bEnable);

    // Standalone DLL: resolve every class lookup on the injector thread before hooking.
    void Prewarm();

    // ReadyToEndMatch filter (call after the original ProcessEvent returns)
    bool IsEndMatchQuery(SDK::UFunction* Func);
    void FilterEndMatch(void* Parms);
    bool ShouldBlockCall(SDK::UObject* Obj, SDK::UFunction* Func);

    // Top of the ProcessEvent hook (before Tick): call trace + death / match-end logging
    void OnCall(SDK::UObject* Obj, SDK::UFunction* Func, void* Parms);

    // Thread-safe: queue a console command to run on the game thread.
    void QueueCommand(const std::string& Command);

    // True if this pawn is one of our bots (used to skip the join-teleport for bots).
    bool IsBotPawn(SDK::APawn* Pawn);
}

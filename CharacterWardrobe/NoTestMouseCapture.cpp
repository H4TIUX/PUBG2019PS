#include <Windows.h>
#include <string>
#include "MinHook.h"

// Loaded only into isolated test processes. Do not change the active desktop's
// cursor: simply prevent the test process from constraining or warping it.
static BOOL WINAPI IgnoreClipCursor(const RECT*) {
    static volatile LONG reported=0;
    if(InterlockedCompareExchange(&reported,1,0)==0) {
        const std::string path="NoTestMouseCapture_"+std::to_string(GetCurrentProcessId())+".blocked";
        HANDLE marker=CreateFileA(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(marker!=INVALID_HANDLE_VALUE) CloseHandle(marker);
    }
    return TRUE;
}
static BOOL WINAPI IgnoreSetCursorPos(int, int) { return TRUE; }

static DWORD WINAPI Install(void*) {
    if (MH_Initialize()!=MH_OK) return 1;
    if (MH_CreateHookApi(L"user32.dll", "ClipCursor", &IgnoreClipCursor, nullptr)!=MH_OK) return 2;
    if (MH_CreateHookApi(L"user32.dll", "SetCursorPos", &IgnoreSetCursorPos, nullptr)!=MH_OK) return 3;
    if (MH_EnableHook(MH_ALL_HOOKS)!=MH_OK) return 4;
    const std::string path="NoTestMouseCapture_"+std::to_string(GetCurrentProcessId())+".ready";
    HANDLE ready=CreateFileA(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(ready==INVALID_HANDLE_VALUE) return 5;
    CloseHandle(ready);
    return 0;
}
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason==DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        HANDLE thread=CreateThread(nullptr,0,Install,nullptr,0,nullptr);
        if(thread) CloseHandle(thread);
    }
    return TRUE;
}

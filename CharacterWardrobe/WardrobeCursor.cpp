#include <Windows.h>
#include "MinHook.h"

// Interactive editor only: never constrain the cursor to the game rectangle.
// SetCursorPos, SetCapture and all mouse messages keep their normal behavior.
// The stronger hidden-desktop guard is not suitable for interactive editing.
static BOOL WINAPI DoNotConfineCursor(const RECT*) { return TRUE; }
static DWORD WINAPI Install(void*) {
    if (MH_Initialize()!=MH_OK) return 1;
    if (MH_CreateHookApi(L"user32.dll", "ClipCursor", &DoNotConfineCursor, nullptr)!=MH_OK) return 2;
    return MH_EnableHook(MH_ALL_HOOKS)==MH_OK ? 0 : 3;
}
BOOL WINAPI DllMain(HMODULE module,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH){
        DisableThreadLibraryCalls(module);
        HANDLE thread=CreateThread(nullptr,0,Install,nullptr,0,nullptr);
        if(thread)CloseHandle(thread);
    }
    return TRUE;
}

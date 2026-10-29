#include "Windows.h"
#include <winnt.h>
#include <initializer_list>
#include <cstdint>

static volatile LONG GNullGuardHits = 0;
static bool InstallNullGuard()
{
    auto* base = reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    if (!base) return false;
    auto* site = base + 0x3b9eb84;
    auto* resume = base + 0x3b9eb8b;
    auto* fallback = base + 0x3b9ebd4;
    const unsigned char expected[] = { 0x48,0x8b,0x8e,0x40,0x04,0x00,0x00,0x48,0x8b,0x01,0x48,0x8d,0x54,0x24,0x30,0xff };
    const unsigned char fallbackExpected[] = { 0x48,0x8d,0x86,0x48,0x08,0x00,0x00,0x48,0x8d,0x5f,0x50 };
    if (std::memcmp(site, expected, sizeof(expected)) || std::memcmp(fallback, fallbackExpected, sizeof(fallbackExpected))) return false;
    const uintptr_t center = reinterpret_cast<uintptr_t>(site) & ~uintptr_t(0xffff);
    unsigned char* stub = nullptr;
    for (uintptr_t d = 0x10000; d < 0x70000000 && !stub; d += 0x10000)
    {
        for (int sign : {1, -1})
        {
            uintptr_t address = sign > 0 ? center + d : center - d;
            stub = static_cast<unsigned char*>(VirtualAlloc(reinterpret_cast<void*>(address), 4096, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
            if (stub) break;
        }
    }
    if (!stub) return false;
    auto* w = stub;
    auto emit = [&](std::initializer_list<unsigned char> bytes) { for (auto b : bytes) *w++ = b; };
    auto imm = [&](uintptr_t value) { std::memcpy(w, &value, 8); w += 8; };
    emit({ 0x48,0x8b,0x8e,0x40,0x04,0x00,0x00 }); // original load
    emit({ 0x48,0x85,0xc9 });                         // test rcx, rcx
    emit({ 0x75,0x19 });                              // nonzero: skip null path
    emit({ 0x48,0xb8 }); imm(reinterpret_cast<uintptr_t>(&GNullGuardHits));
    emit({ 0xf0,0xff,0x00 });                         // lock inc [rax]
    emit({ 0x48,0xb8 }); imm(reinterpret_cast<uintptr_t>(fallback));
    emit({ 0xff,0xe0 });                              // skip only missing virtual call
    emit({ 0x48,0xb8 }); imm(reinterpret_cast<uintptr_t>(resume));
    emit({ 0xff,0xe0 });
    const intptr_t displacement = stub - (site + 5);
    if (displacement < INT32_MIN || displacement > INT32_MAX) { VirtualFree(stub, 0, MEM_RELEASE); return false; }
    unsigned char patch[7] = { 0xe9,0,0,0,0,0x90,0x90 };
    const int32_t rel = static_cast<int32_t>(displacement); std::memcpy(patch + 1, &rel, 4);
    DWORD old = 0; if (!VirtualProtect(site, sizeof(patch), PAGE_EXECUTE_READWRITE, &old)) { VirtualFree(stub, 0, MEM_RELEASE); return false; }
    std::memcpy(site, patch, sizeof(patch)); FlushInstructionCache(GetCurrentProcess(), site, sizeof(patch));
    DWORD ignored; VirtualProtect(site, sizeof(patch), old, &ignored); return true;
}

static volatile LONG GNullContextHits = 0;
static bool InstallNullContextGuard()
{
    auto* base = reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    if (!base) return false;
    auto* site = base + 0x3b9ae90;
    auto* resume = base + 0x3b9ae95;
    auto* callee = base + 0x1176560;
    const unsigned char expected[] = { 0xe8,0xcb,0xb6,0x5d,0xfd,0x4c,0x8b,0xf0,0x48,0x85,0xf6,0x0f,0x84,0xca,0x00,0x00 };
    const unsigned char calleeExpected[] = { 0x40,0x55,0x56,0x41,0x54,0x41,0x56,0x41,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0x81 };
    if (std::memcmp(site, expected, sizeof(expected)) || std::memcmp(callee, calleeExpected, sizeof(calleeExpected))) return false;
    const uintptr_t center = reinterpret_cast<uintptr_t>(site) & ~uintptr_t(0xffff);
    unsigned char* stub = nullptr;
    for (uintptr_t d = 0x10000; d < 0x70000000 && !stub; d += 0x10000)
    {
        for (int sign : {1, -1})
        {
            uintptr_t address = sign > 0 ? center + d : center - d;
            stub = static_cast<unsigned char*>(VirtualAlloc(reinterpret_cast<void*>(address), 4096, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
            if (stub)break;
        }
    }
    if (!stub)return false;
    auto* w = stub;
    auto emit = [&](std::initializer_list<unsigned char> bytes) {for (auto b : bytes)*w++ = b; };
    auto imm = [&](uintptr_t v) {std::memcpy(w, &v, 8); w += 8; };
    emit({ 0x48,0x85,0xf6 }); // test rsi,rsi
    emit({ 0x75,0x1c });      // if non-null execute original call
    emit({ 0x48,0xb8 }); imm(reinterpret_cast<uintptr_t>(&GNullContextHits));
    emit({ 0xf0,0xff,0x00 }); // record guard activation
    emit({ 0x31,0xc0 });      // null result, as expected by subsequent branch
    emit({ 0x49,0xbb }); imm(reinterpret_cast<uintptr_t>(resume)); emit({ 0x41,0xff,0xe3 });
    emit({ 0x49,0xbb }); imm(reinterpret_cast<uintptr_t>(callee)); emit({ 0x41,0xff,0xd3 });
    emit({ 0x49,0xbb }); imm(reinterpret_cast<uintptr_t>(resume)); emit({ 0x41,0xff,0xe3 });
    const intptr_t displacement = stub - (site + 5);
    if (displacement<INT32_MIN || displacement>INT32_MAX) { VirtualFree(stub, 0, MEM_RELEASE); return false; }
    unsigned char patch[5] = { 0xe9,0,0,0,0 }; const int32_t rel = static_cast<int32_t>(displacement); std::memcpy(patch + 1, &rel, 4);
    DWORD old = 0; if (!VirtualProtect(site, sizeof(patch), PAGE_EXECUTE_READWRITE, &old)) { VirtualFree(stub, 0, MEM_RELEASE); return false; }
    std::memcpy(site, patch, sizeof(patch)); FlushInstructionCache(GetCurrentProcess(), site, sizeof(patch));
    DWORD ignored; VirtualProtect(site, sizeof(patch), old, &ignored); return true;
}

static volatile LONG GNullCharacterHits = 0;
static bool InstallNullCharacterGuard()
{
    auto* base = reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    if (!base) return false;
    auto* site = base + 0x3a49630;
    auto* resume = base + 0x3a49635;
    auto* callee = base + 0x3a1f670;
    const unsigned char expected[] = { 0xe8,0x3b,0x60,0xfd,0xff,0x48,0x85,0xc0,0x74,0x17,0xf3,0x0f,0x10,0x1d,0xc2,0xdc,0x1d,0x01 };
    const unsigned char calleeExpected[] = { 0x48,0x89,0x6c,0x24,0x18,0x56,0x48,0x83,0xec,0x30,0x48,0x8b,0xa9,0x10,0x04,0x00,0x00 };
    if (std::memcmp(site, expected, sizeof(expected)) || std::memcmp(callee, calleeExpected, sizeof(calleeExpected))) return false;
    const uintptr_t center = reinterpret_cast<uintptr_t>(site) & ~uintptr_t(0xffff);
    unsigned char* stub = nullptr;
    for (uintptr_t d = 0x10000; d < 0x70000000 && !stub; d += 0x10000)
    {
        for (int sign : {1, -1})
        {
            uintptr_t address = sign > 0 ? center + d : center - d;
            stub = static_cast<unsigned char*>(VirtualAlloc(reinterpret_cast<void*>(address), 4096, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
            if (stub)break;
        }
    }
    if (!stub)return false;
    auto* w = stub;
    auto emit = [&](std::initializer_list<unsigned char> bytes) {for (auto b : bytes)*w++ = b; };
    auto imm = [&](uintptr_t v) {std::memcpy(w, &v, 8); w += 8; };
    emit({ 0x48,0x85,0xc9 }); // test rcx,rcx
    emit({ 0x75,0x1d });      // non-null: invoke original callee
    emit({ 0x49,0xbb }); imm(reinterpret_cast<uintptr_t>(&GNullCharacterHits));
    emit({ 0xf0,0x41,0xff,0x03 }); // lock inc [r11]
    emit({ 0x31,0xc0 });           // null result, handled by original caller
    emit({ 0x49,0xbb }); imm(reinterpret_cast<uintptr_t>(resume)); emit({ 0x41,0xff,0xe3 });
    emit({ 0x49,0xbb }); imm(reinterpret_cast<uintptr_t>(callee)); emit({ 0x41,0xff,0xd3 });
    emit({ 0x49,0xbb }); imm(reinterpret_cast<uintptr_t>(resume)); emit({ 0x41,0xff,0xe3 });
    const intptr_t displacement = stub - (site + 5);
    if (displacement<INT32_MIN || displacement>INT32_MAX) { VirtualFree(stub, 0, MEM_RELEASE); return false; }
    unsigned char patch[5] = { 0xe9,0,0,0,0 }; const int32_t rel = static_cast<int32_t>(displacement); std::memcpy(patch + 1, &rel, 4);
    DWORD old = 0; if (!VirtualProtect(site, sizeof(patch), PAGE_EXECUTE_READWRITE, &old)) { VirtualFree(stub, 0, MEM_RELEASE); return false; }
    std::memcpy(site, patch, sizeof(patch)); FlushInstructionCache(GetCurrentProcess(), site, sizeof(patch));
    DWORD ignored; VirtualProtect(site, sizeof(patch), old, &ignored); return true;
}

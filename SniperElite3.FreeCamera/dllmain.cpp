#include <windows.h>
#include <cstdint>
#include <cstring>
#include "FreeCamera.h"
#include "FovOverlay.h"

namespace {
bool CameraBuildMatches() {
    const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandle(nullptr));
    const unsigned char expected[] = {0x89, 0x01, 0x89, 0x41, 0x04, 0x89, 0x41, 0x08};
    unsigned char found[8]{};
    SIZE_T n = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(base + 0x5127F), found, 2, &n) && n == 2 &&
           ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(base + 0x51284), found + 2, 3, &n) && n == 3 &&
           ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(base + 0x5128A), found + 5, 3, &n) && n == 3 &&
           std::memcmp(found, expected, sizeof(found)) == 0;
}
DWORD WINAPI Start(LPVOID) {
    // Hooks and the worker live until process exit; don't unload live code.
    HMODULE pinned = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&Start), &pinned)) return 0;
    if (CameraBuildMatches()) FreeCamera::Init();
    else OutputDebugStringW(L"SE3 FreeCamera: camera instructions do not match; camera patch disabled.\n");
    FovOverlay::Init();
    return 0;
}
}
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        HANDLE thread = CreateThread(nullptr, 0, Start, nullptr, 0, nullptr);
        if (thread) CloseHandle(thread);
    }
    return TRUE;
}

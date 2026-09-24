#include "FovControl.h"
#include "FovModel.h"
#include <windows.h>
#include <cstring>

static_assert(sizeof(void*) == 4, "Sniper Elite 3 requires a Win32 build.");

namespace {
bool Read(std::uintptr_t address, void* data, SIZE_T size) {
    SIZE_T count = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address),
                             data, size, &count) && count == size;
}
}

void FovControl::Init() {
    constexpr std::uintptr_t rva = 0x72AD90;
    const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandle(nullptr));
    IMAGE_DOS_HEADER dos{};
    IMAGE_NT_HEADERS32 nt{};
    if (!Read(base, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE ||
        dos.e_lfanew <= 0 || dos.e_lfanew > 0x100000 ||
        !Read(base + dos.e_lfanew, &nt, sizeof(nt)) ||
        nt.Signature != IMAGE_NT_SIGNATURE || nt.FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
        nt.OptionalHeader.SizeOfImage < rva + sizeof(float)) {
        status = "Unsupported game version. FOV changes are disabled.";
        return;
    }
    address = base + rva;
    if (!Read(address, &original, sizeof(original)) || original != FovModel::DefaultConstant) {
        status = "Unexpected FOV value (game version or another mod). Changes are blocked.";
        return;
    }
    lastWritten = original;
    available = true;
    status = "Ready. Reload the checkpoint after changing FOV.";
}

bool FovControl::Apply(float scale, bool enabled) {
    if (!available) return false;
    float current = 0.0f;
    if (!Read(address, &current, sizeof(current)) ||
        std::memcmp(&current, &lastWritten, sizeof(current)) != 0) {
        available = false;
        status = "Another mod changed FOV. Further changes are blocked to avoid a conflict.";
        return false;
    }
    const float next = FovModel::Value(original, scale, enabled);
    if (next == current) return true;
    DWORD protection = 0;
    void* target = reinterpret_cast<void*>(address);
    if (!VirtualProtect(target, sizeof(next), PAGE_READWRITE, &protection)) {
        status = "Could not change memory protection.";
        return false;
    }
    SIZE_T count = 0;
    const bool written = WriteProcessMemory(GetCurrentProcess(), target, &next, sizeof(next), &count)
                      && count == sizeof(next);
    DWORD ignored = 0;
    const bool restored = VirtualProtect(target, sizeof(next), protection, &ignored) != 0;
    if (written) lastWritten = next;
    if (!restored || !written) {
        available = false;
        status = "Memory access failed. FOV control is disabled.";
        return false;
    }
    status = "Value applied. Reload the checkpoint to see the change.";
    return true;
}

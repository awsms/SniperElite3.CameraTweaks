#include "FovOverlay.h"
#include "FovControl.h"
#include "FovModel.h"
#include "SettingsMgr.h"
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>
#include <MinHook.h>
#include <imgui.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx11.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace {
using PresentFn = HRESULT (STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
using SetCursorPosFn = BOOL (WINAPI*)(int, int);
PresentFn originalPresent = nullptr;
SetCursorPosFn originalSetCursorPos = nullptr;
std::atomic<bool> opened{false};
std::atomic<HWND> gameWindow{nullptr};
WNDPROC previousWndProc = nullptr;
ImGuiContext* uiContext = nullptr;
ID3D11Device* device = nullptr;
ID3D11DeviceContext* context = nullptr;
std::mutex renderMutex;
std::mutex inputMutex;
struct Message { UINT id; WPARAM w; LPARAM l; };
std::vector<Message> input;
FovControl fov;
float scale = 1.0f;
bool enabled = true;

void Log(const wchar_t* text) {
    OutputDebugStringW(text);
    OutputDebugStringW(L"\n");
}

bool IsInput(UINT msg) {
    return (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) ||
           (msg >= WM_KEYFIRST && msg <= WM_KEYLAST) || msg == WM_INPUT;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    const bool toggle = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) &&
                        w == static_cast<WPARAM>(SettingsMgr->iFovMenuKey);
    if (toggle) {
        if (!(l & (1LL << 30))) opened.store(!opened.load());
        return 0;
    }
    if ((msg == WM_KEYUP || msg == WM_SYSKEYUP) &&
        w == static_cast<WPARAM>(SettingsMgr->iFovMenuKey)) return 0;
    if (msg == WM_KILLFOCUS) opened.store(false);
    if (opened.load() && msg == WM_KEYDOWN && w == VK_ESCAPE) {
        opened.store(false);
        return 0;
    }
    // The backend and ImGui run only on the render thread. Never hold a render
    // lock while dispatching Win32 messages (Present can itself send messages).
    if (IsInput(msg) || msg == WM_SETFOCUS || msg == WM_KILLFOCUS) {
        std::lock_guard<std::mutex> lock(inputMutex);
        if (input.size() < 2048) input.push_back({msg, w, l});
    }
    if (opened.load() && IsInput(msg)) {
        // DefWindowProc performs WM_INPUT foreground cleanup.
        return msg == WM_INPUT ? DefWindowProcW(hwnd, msg, w, l) : 0;
    }
    if (opened.load() && msg == WM_SETCURSOR) {
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
        return TRUE;
    }
    return CallWindowProcW(previousWndProc, hwnd, msg, w, l);
}

BOOL WINAPI CursorPosHook(int x, int y) {
    if (opened.load() && GetForegroundWindow() == gameWindow.load()) return TRUE;
    return originalSetCursorPos(x, y);
}

bool Setup(IDXGISwapChain* swap) {
    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(swap->GetDesc(&desc)) || !IsWindowVisible(desc.OutputWindow)) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(desc.OutputWindow, &pid);
    if (pid != GetCurrentProcessId()) return false;
    if (FAILED(swap->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&device)))) return false;
    device->GetImmediateContext(&context);
    IMGUI_CHECKVERSION();
    uiContext = ImGui::CreateContext();
    ImGui::SetCurrentContext(uiContext);
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    const bool win32 = ImGui_ImplWin32_Init(desc.OutputWindow);
    const bool dx11 = win32 && ImGui_ImplDX11_Init(device, context);
    if (!dx11) {
        if (win32) ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(uiContext);
        uiContext = nullptr;
        context->Release(); context = nullptr;
        device->Release(); device = nullptr;
        return false;
    }
    // Publish original procedure before installing ours, so another window
    // thread cannot enter it with an uninitialised forwarding pointer.
    previousWndProc = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(desc.OutputWindow, GWLP_WNDPROC));
    SetLastError(0);
    auto old = SetWindowLongPtrW(desc.OutputWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WindowProc));
    if (!old && GetLastError()) {
        ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(uiContext); uiContext = nullptr;
        context->Release(); context = nullptr;
        device->Release(); device = nullptr;
        return false;
    }
    gameWindow.store(desc.OutputWindow);
    fov.Init();
    scale = FovModel::Clamp(SettingsMgr->fFovScale);
    // The saved slider value is a preference. Loading a plugin never changes
    // FOV until the user explicitly applies it or moves the slider.
    Log(L"SE3 FreeCamera: FOV overlay ready.");
    return true;
}

void Draw(IDXGISwapChain* swap) {
    if (!uiContext && !Setup(swap)) return;
    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(swap->GetDesc(&desc)) || desc.OutputWindow != gameWindow.load()) return;
    ID3D11Device* swapDevice = nullptr;
    if (FAILED(swap->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&swapDevice)))) return;
    const bool sameDevice = swapDevice == device;
    swapDevice->Release();
    if (!sameDevice) return; // A newly created device needs a restart of the plugin.
    ImGui::SetCurrentContext(uiContext);
    std::vector<Message> messages;
    {
        std::lock_guard<std::mutex> lock(inputMutex);
        messages.swap(input);
    }
    const bool visible = opened.load() && GetForegroundWindow() == gameWindow.load();
    static bool wasVisible = false;
    static RECT previousClip{};
    if (visible != wasVisible) {
        ImGui::GetIO().ClearInputKeys();
        ImGui::GetIO().ClearEventsQueue();
        if (visible) GetClipCursor(&previousClip);
        else if (GetForegroundWindow() == gameWindow.load()) ClipCursor(&previousClip);
        wasVisible = visible;
    }
    if (!visible) {
        ImGui::GetIO().ClearEventsQueue();
        return;
    }
    for (const auto& msg : messages)
        ImGui_ImplWin32_WndProcHandler(gameWindow.load(), msg.id, msg.w, msg.l);
    ClipCursor(nullptr);
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::GetIO().MouseDrawCursor = true;
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(60, 60), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(470, 0), ImGuiCond_Always);
    bool stayOpen = true;
    if (ImGui::Begin("FreeCamera - FOV Settings", &stayOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Field of view");
        ImGui::TextWrapped("Pause the game before using this menu.");
        ImGui::BeginDisabled(!fov.Available());
        bool changed = ImGui::Checkbox("Enable custom FOV", &enabled);
        changed |= ImGui::SliderFloat("Multiplier", &scale, FovModel::MinScale,
                                      FovModel::MaxScale, "x%.2f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::TextWrapped("1.00 = default value. This multiplier is not an angle in degrees.");
        changed |= ImGui::Button("Apply");
        ImGui::SameLine();
        if (ImGui::Button("Reset")) { scale = 1.0f; enabled = true; changed = true; }
        if (changed) fov.Apply(scale, enabled);
        ImGui::EndDisabled();
        if (ImGui::Button("Save setting")) {
            SettingsMgr->fFovScale = scale;
            SettingsMgr->SaveSettings();
        }
        ImGui::Separator();
        ImGui::TextWrapped("%s", fov.Status());
        ImGui::TextWrapped("Reload the checkpoint after any change, including a reset.");
        ImGui::Text("Close: menu hotkey (VK %d) or Escape.", SettingsMgr->iFovMenuKey);
    }
    ImGui::End();
    if (!stayOpen) opened.store(false);
    ImGui::Render();

    // No backbuffer reference survives Present: ResizeBuffers remains usable
    // without another detour. Preserve the game's entire OM target binding.
    ID3D11Texture2D* backbuffer = nullptr;
    ID3D11RenderTargetView* target = nullptr;
    if (SUCCEEDED(swap->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backbuffer)))) {
        device->CreateRenderTargetView(backbuffer, nullptr, &target);
        backbuffer->Release();
    }
    if (!target) return;
    ID3D11RenderTargetView* previous[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
    ID3D11DepthStencilView* depth = nullptr;
    context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, previous, &depth);
    context->OMSetRenderTargets(1, &target, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, previous, depth);
    for (auto* view : previous) if (view) view->Release();
    if (depth) depth->Release();
    target->Release();
}

HRESULT STDMETHODCALLTYPE PresentHook(IDXGISwapChain* swap, UINT interval, UINT flags) {
    if (!(flags & DXGI_PRESENT_TEST)) {
        std::lock_guard<std::mutex> lock(renderMutex);
        Draw(swap);
    }
    return originalPresent(swap, interval, flags);
}
}

bool FovOverlay::IsOpen() { return opened.load(); }

bool FovOverlay::Init() {
    // Discover the real DXGI Present implementation through the system D3D11,
    // leaving the game's local 3DMigoto d3d11.dll in place and in the call chain.
    wchar_t system[MAX_PATH]{};
    if (!GetSystemDirectoryW(system, MAX_PATH)) return false;
    const std::wstring path = std::wstring(system) + L"\\d3d11.dll";
    HMODULE d3d = LoadLibraryW(path.c_str());
    if (!d3d) return false;
    auto create = reinterpret_cast<decltype(&D3D11CreateDeviceAndSwapChain)>(
        GetProcAddress(d3d, "D3D11CreateDeviceAndSwapChain"));
    if (!create) { FreeLibrary(d3d); return false; }
    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"SE3FreeCameraDXGIProbe";
    if (!RegisterClassW(&wc)) { FreeLibrary(d3d); return false; }
    HWND probe = CreateWindowW(wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW,
                               0, 0, 32, 32, nullptr, nullptr, wc.hInstance, nullptr);
    IDXGISwapChain* swap = nullptr;
    ID3D11Device* probeDevice = nullptr;
    ID3D11DeviceContext* probeContext = nullptr;
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferDesc.Width = 32; desc.BufferDesc.Height = 32;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1; desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 1; desc.OutputWindow = probe; desc.Windowed = TRUE;
    const HRESULT hr = probe ? create(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &desc, &swap, &probeDevice, nullptr, &probeContext) : E_FAIL;
    void* presentAddress = SUCCEEDED(hr) ? (*reinterpret_cast<void***>(swap))[8] : nullptr;
    // Retain the system runtime while the detour exists.
    bool success = false;
    if (presentAddress) {
        const auto init = MH_Initialize();
        if (init == MH_OK || init == MH_ERROR_ALREADY_INITIALIZED) {
            if (MH_CreateHook(presentAddress, reinterpret_cast<void*>(PresentHook),
                reinterpret_cast<void**>(&originalPresent)) == MH_OK) {
                success = MH_EnableHook(presentAddress) == MH_OK;
                if (!success) MH_RemoveHook(presentAddress);
            }
            if (success) {
                void* cursorAddress = reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetCursorPos"));
                if (MH_CreateHook(cursorAddress, reinterpret_cast<void*>(CursorPosHook),
                    reinterpret_cast<void**>(&originalSetCursorPos)) == MH_OK)
                    MH_EnableHook(cursorAddress);
            }
        }
    }
    if (probeContext) probeContext->Release();
    if (probeDevice) probeDevice->Release();
    if (swap) swap->Release();
    if (probe) DestroyWindow(probe);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    if (!success) { FreeLibrary(d3d); Log(L"SE3 FreeCamera: failed to hook DXGI Present."); }
    return success;
}

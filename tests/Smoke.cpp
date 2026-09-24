// Windows x86 synthetic DX11 host. No game files required or modified.
#include <windows.h>
#include <d3d11.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include "FovControl.h"
#include "FovModel.h"

// Linked at RVA 0x72A000 to exercise the real guarded FOV memory writer.
unsigned char fakeFovSection[4096] __attribute__((section(".fov"))) = {1};
int failures = 0;
void Check(bool ok, const char* message) {
    std::printf("%s: %s\n", ok ? "PASS" : "FAIL", message);
    std::fflush(stdout);
    if (!ok) ++failures;
}
LRESULT CALLBACK HostProc(HWND h, UINT m, WPARAM w, LPARAM l) { return DefWindowProcW(h,m,w,l); }

bool Capture(IDXGISwapChain* swap, ID3D11Device* device, ID3D11DeviceContext* context) {
    ID3D11Texture2D *back = nullptr, *staging = nullptr;
    if (FAILED(swap->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back)))) return false;
    D3D11_TEXTURE2D_DESC desc{}; back->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0; desc.MiscFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    if (FAILED(device->CreateTexture2D(&desc, nullptr, &staging))) { back->Release(); return false; }
    context->CopyResource(staging, back); back->Release();
    D3D11_MAPPED_SUBRESOURCE map{};
    bool result = false;
    if (SUCCEEDED(context->Map(staging, 0, D3D11_MAP_READ, 0, &map))) {
        FILE* out = std::fopen("overlay-smoke.ppm", "wb");
        if (out) std::fprintf(out, "P6\n%u %u\n255\n", desc.Width, desc.Height);
        unsigned differing = 0;
        const auto* background = static_cast<unsigned char*>(map.pData);
        for (UINT y = 0; y < desc.Height; ++y) {
            const auto* row = static_cast<unsigned char*>(map.pData) + y * map.RowPitch;
            for (UINT x = 0; x < desc.Width; ++x) {
                if (out) std::fwrite(row + 4*x, 1, 3, out);
                if (std::memcmp(row + 4*x, background, 3)) ++differing;
            }
        }
        if (out) std::fclose(out);
        std::printf("Overlay pixels different from background: %u\n", differing);
        result = differing > 1000;
        context->Unmap(staging, 0);
    }
    staging->Release();
    return result;
}

int main() {
    auto* value = reinterpret_cast<float*>(fakeFovSection + 0xD90);
    Check(reinterpret_cast<std::uintptr_t>(value) == reinterpret_cast<std::uintptr_t>(GetModuleHandle(nullptr)) + 0x72AD90,
          "Synthetic module has the expected FOV RVA");
    if (failures) return 1;
    *value = FovModel::DefaultConstant;
    FovControl control; control.Init();
    Check(control.Available(), "Recognise expected constant");
    DWORD old = 0;
    VirtualProtect(value, sizeof(float), PAGE_READONLY, &old);
    const bool applied = control.Apply(1.25f, true);
    const float expected = FovModel::DefaultConstant * 1.25f;
    std::printf("Scale result: ok=%d actual=%.12g expected=%.12g status=%s\n", applied, *value, expected, control.Status());
    Check(applied && std::memcmp(value, &expected, sizeof(float)) == 0, "Apply x1.25 to read-only memory");
    MEMORY_BASIC_INFORMATION mbi{};
    VirtualQuery(value, &mbi, sizeof(mbi));
    Check(mbi.Protect == PAGE_READONLY, "Restore memory protection");
    Check(control.Apply(1.25f, false) && *value == FovModel::DefaultConstant, "Disable restores original exactly");
    Check(control.Apply(100.f, true) && *value == FovModel::DefaultConstant * 2.f, "Clamp excessive scale");
    Check(control.Apply(NAN, true) && *value == FovModel::DefaultConstant, "Reject non-finite scale");
    DWORD ignored = 0; VirtualProtect(value, sizeof(float), old, &ignored);
    *value = 0.5f;
    Check(!control.Apply(1.2f, true) && *value == 0.5f, "Refuse to overwrite another mod's change");
    FovControl mismatch; mismatch.Init();
    Check(!mismatch.Available(), "Reject unexpected initial constant");
    *value = FovModel::DefaultConstant;

    WNDCLASSW wc{}; wc.lpfnWndProc = HostProc; wc.hInstance = GetModuleHandle(nullptr); wc.lpszClassName = L"SE3Smoke";
    RegisterClassW(&wc);
    HWND window = CreateWindowW(wc.lpszClassName, L"SE3 overlay smoke test", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                0, 0, 900, 650, nullptr, nullptr, wc.hInstance, nullptr);
    SetForegroundWindow(window); SetFocus(window);
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferDesc.Width=900; desc.BufferDesc.Height=650; desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count=1; desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount=1; desc.OutputWindow=window; desc.Windowed=TRUE; desc.SwapEffect=DXGI_SWAP_EFFECT_SEQUENTIAL;
    IDXGISwapChain* swap=nullptr; ID3D11Device* device=nullptr; ID3D11DeviceContext* context=nullptr;
    HRESULT hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,
                                          &desc,&swap,&device,nullptr,&context);
    Check(SUCCEEDED(hr), "Create DX11 test device");
    if (FAILED(hr)) return 1;
    Check(LoadLibraryW(L"SniperElite3.FreeCamera.asi") != nullptr, "Load built 32-bit ASI");
    bool rendered = false;
    for (int frame=0; frame<300; ++frame) {
        MSG msg;
        while (PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        if (frame==100 || frame==180 || frame==200) {
            SendMessageW(window,WM_KEYDOWN,VK_F6,1);
            SendMessageW(window,WM_KEYUP,VK_F6,(1LL<<31)|(1LL<<30)|1);
        }
        if (frame==190) Check(SUCCEEDED(swap->ResizeBuffers(1,900,650,DXGI_FORMAT_UNKNOWN,0)), "Resize after overlay use (no leaked backbuffer)");
        ID3D11Texture2D* back=nullptr; ID3D11RenderTargetView* target=nullptr;
        swap->GetBuffer(0,__uuidof(ID3D11Texture2D),reinterpret_cast<void**>(&back));
        device->CreateRenderTargetView(back,nullptr,&target); back->Release();
        const float color[]={0.05f,0.1f,0.2f,1.f}; context->ClearRenderTargetView(target,color);
        target->Release();
        swap->Present(0,0);
        if (frame==150) rendered=Capture(swap,device,context);
        Sleep(10);
    }
    Check(rendered,"F6 renders visible overlay into game backbuffer");
    Check(*value == FovModel::DefaultConstant,"Opening and closing menu does not silently change FOV");
    std::printf("Failures: %d\n",failures);
    // The ASI deliberately pins itself for process lifetime.
    return failures ? 1 : 0;
}

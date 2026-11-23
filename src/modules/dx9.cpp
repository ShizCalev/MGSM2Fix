#include "m2fix.h"

#include "dx9.h"
#include "color_correction.hpp"

namespace
{
    using PresentFunction = HRESULT (WINAPI *)(IDirect3DDevice9 *, const RECT *, const RECT *, HWND, const RGNDATA *);
    using ResetFunction = HRESULT (WINAPI *)(IDirect3DDevice9 *, D3DPRESENT_PARAMETERS *);
    PresentFunction originalPresent = nullptr;
    ResetFunction originalReset = nullptr;
}

IDirect3D9 * WINAPI DX9::Direct3D::Create9(UINT SDKVersion)
{
    return DX9::GetInstance().Create9(
        DX9::Direct3D::Create9,
        SDKVersion
    );
}

IDirect3D9 * WINAPI DX9::Create9(
    IDirect3D9 * (WINAPI *pFunction)(UINT SDKVersion),
    UINT SDKVersion
) {
    IDirect3D9 *pD3D9 = M2Hook::GetInstance().Invoke<IDirect3D9 *>(
        pFunction,
        SDKVersion
    );

    if (!pD3D9) return pD3D9;

    // IDirect3D9::CreateDevice is always vtable slot 16
    void **d3d9Vtable = *reinterpret_cast<void ***>(pD3D9);
    M2Hook::GetInstance().Hook(
        d3d9Vtable[16],
        DX9::Device::CreateDevice, "[D3D9] IDirect3D9::CreateDevice"
    );

    return pD3D9;
}

HRESULT WINAPI DX9::Device::CreateDevice(
    IDirect3D9             *pD3D9,
    UINT                   Adapter,
    D3DDEVTYPE             DeviceType,
    HWND                   hFocusWindow,
    DWORD                  BehaviorFlags,
    D3DPRESENT_PARAMETERS  *pPresentationParameters,
    IDirect3DDevice9       **ppReturnedDeviceInterface
) {
    return DX9::GetInstance().CreateDevice(
        DX9::Device::CreateDevice,
        pD3D9,
        Adapter,
        DeviceType,
        hFocusWindow,
        BehaviorFlags,
        pPresentationParameters,
        ppReturnedDeviceInterface
    );
}

HRESULT WINAPI DX9::CreateDevice(
    HRESULT (WINAPI *pFunction)(
        IDirect3D9             *pD3D9,
        UINT                   Adapter,
        D3DDEVTYPE             DeviceType,
        HWND                   hFocusWindow,
        DWORD                  BehaviorFlags,
        D3DPRESENT_PARAMETERS  *pPresentationParameters,
        IDirect3DDevice9       **ppReturnedDeviceInterface
    ),
    IDirect3D9             *pD3D9,
    UINT                   Adapter,
    D3DDEVTYPE             DeviceType,
    HWND                   hFocusWindow,
    DWORD                  BehaviorFlags,
    D3DPRESENT_PARAMETERS  *pPresentationParameters,
    IDirect3DDevice9       **ppReturnedDeviceInterface
) {
    HRESULT res = M2Hook::GetInstance().Invoke<HRESULT>(
        pFunction,
        pD3D9,
        Adapter,
        DeviceType,
        hFocusWindow,
        BehaviorFlags,
        pPresentationParameters,
        ppReturnedDeviceInterface
    );

    if (FAILED(res) || Device || !ppReturnedDeviceInterface || !*ppReturnedDeviceInterface) {
        return res;
    }

    HookDevice(*ppReturnedDeviceInterface);

    return res;
}

void DX9::HookDevice(IDirect3DDevice9 *pDevice)
{
    Device = pDevice;

    if (M2Config::iRendererLevel >= 1) {
        spdlog::info("[D3D9] IDirect3D9::CreateDevice() -> {}", fmt::ptr(pDevice));
    }

    // IDirect3DDevice9::Reset is always vtable slot 16, Present is always slot 17.
    void **deviceVtable = *reinterpret_cast<void ***>(pDevice);
    // Steam overlay can sometimes overwrite our present hook.
    DWORD oldProtect;
    if (!VirtualProtect(&deviceVtable[16], 2 * sizeof(void *), PAGE_READWRITE, &oldProtect)) {
        spdlog::warn("[D3D9] Failed to hook Reset and Present: {}", GetLastError());
        return;
    }
    originalReset = reinterpret_cast<ResetFunction>(deviceVtable[16]);
    originalPresent = reinterpret_cast<PresentFunction>(deviceVtable[17]);
    deviceVtable[16] = reinterpret_cast<void *>(&DX9::Device::Reset);
    deviceVtable[17] = reinterpret_cast<void *>(&DX9::Device::Present);
    DWORD unused;
    VirtualProtect(&deviceVtable[16], 2 * sizeof(void *), oldProtect, &unused);

    ColorCorrection::Init();
}

HRESULT WINAPI DX9::Device::Present(
    IDirect3DDevice9 *pDevice,
    const RECT       *pSourceRect,
    const RECT       *pDestRect,
    HWND             hDestWindowOverride,
    const RGNDATA    *pDirtyRegion
) {
    return DX9::GetInstance().Present(
        DX9::Device::Present,
        pDevice,
        pSourceRect,
        pDestRect,
        hDestWindowOverride,
        pDirtyRegion
    );
}

HRESULT WINAPI DX9::Present(
    HRESULT (WINAPI *pFunction)(
        IDirect3DDevice9 *pDevice,
        const RECT       *pSourceRect,
        const RECT       *pDestRect,
        HWND             hDestWindowOverride,
        const RGNDATA    *pDirtyRegion
    ),
    IDirect3DDevice9 *pDevice,
    const RECT       *pSourceRect,
    const RECT       *pDestRect,
    HWND             hDestWindowOverride,
    const RGNDATA    *pDirtyRegion
) {
    ColorCorrection::DrawD3D9(pDevice);

    return originalPresent(
        pDevice,
        pSourceRect,
        pDestRect,
        hDestWindowOverride,
        pDirtyRegion
    );
}

HRESULT WINAPI DX9::Device::Reset(
    IDirect3DDevice9      *pDevice,
    D3DPRESENT_PARAMETERS *pPresentationParameters
) {
    return DX9::GetInstance().Reset(
        DX9::Device::Reset,
        pDevice,
        pPresentationParameters
    );
}

HRESULT WINAPI DX9::Reset(
    HRESULT (WINAPI *pFunction)(
        IDirect3DDevice9      *pDevice,
        D3DPRESENT_PARAMETERS *pPresentationParameters
    ),
    IDirect3DDevice9      *pDevice,
    D3DPRESENT_PARAMETERS *pPresentationParameters
) {


    // D3D9 requires all default-pool textures to be released before resetting device.
    ColorCorrection::ReleaseD3D9();

    return originalReset(
        pDevice,
        pPresentationParameters
    );
}

void DX9::Load()
{
    HMODULE D3DCompiler_47 = LoadLibraryA("D3DCompiler_47.dll");
    if (!D3DCompiler_47) {
        spdlog::warn("[D3D9] Failed to load D3DCompiler_47.dll.");
        return;
    }

    D3DCompile = reinterpret_cast<pD3DCompile>(
        GetProcAddress(D3DCompiler_47, "D3DCompile")
    );
    if (!D3DCompile) {
        spdlog::warn("[D3D9] D3DCompile lookup failed.");
        return;
    }

    HMODULE d3d9 = LoadLibraryA("d3d9.dll");
    if (!d3d9) {
        spdlog::warn("[D3D9] Failed to load d3d9.dll.");
        return;
    }

    typedef IDirect3D9 * (WINAPI *PFN_DIRECT3D_CREATE9)(UINT SDKVersion);

    auto direct3DCreate9 = reinterpret_cast<PFN_DIRECT3D_CREATE9>(
        GetProcAddress(d3d9, "Direct3DCreate9")
    );
    if (!direct3DCreate9) {
        spdlog::info("[D3D9] Direct3DCreate9 lookup failed.");
        return;
    }

    M2Hook::GetInstance().Hook(
        direct3DCreate9,
        DX9::Direct3D::Create9, "[D3D9] Direct3DCreate9"
    );
}

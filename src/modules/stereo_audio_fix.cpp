#include "stdafx.h"
#include "stereo_audio_fix.h"
#include "m2fix.h"

#ifndef _WIN64
#include <objbase.h>

namespace
{
    constexpr GUID XAudioClass = { 0x5a508685, 0xa254, 0x4fba, { 0x9b, 0x82, 0x9a, 0x24, 0xb0, 0x03, 0x06, 0xaf } };
    constexpr GUID XAudioInterface = { 0x8bcf1f58, 0x9fe7, 0x4583, { 0x8a, 0xc6, 0xe2, 0xad, 0xc4, 0x65, 0xc8, 0xbb } };
    safetyhook::InlineHook createHook;
    safetyhook::InlineHook masterHook;
    std::mutex hookMutex;

    HRESULT __stdcall CreateMasteringVoice(void *engine, void **voice, UINT32 channels, UINT32 rate, UINT32 flags, UINT32 device, const void *effects)
    {
        // Contra and Castlevania anniversary allocate 12 matrix coeffs; stereo -> 7.1 needs 16.
        auto hr = masterHook.stdcall<HRESULT>(engine, voice, UINT32(2), rate, flags, device, effects);
        spdlog::info("[Audio] Force Stereo Audio: requested {} channels, using 2 (HRESULT {:08X}).", channels, static_cast<UINT32>(hr));
        return hr;
    }

    HRESULT WINAPI CoCreate(REFCLSID clsid, LPUNKNOWN outer, DWORD context, REFIID iid, LPVOID *object)
    {
        auto hr = createHook.stdcall<HRESULT>(&clsid, outer, context, &iid, object);
        if (FAILED(hr) || clsid != XAudioClass || iid != XAudioInterface || !object || !*object) return hr;

        std::lock_guard lock(hookMutex);
        if (!masterHook) {
            // CreateMasteringVoice is slot 10 in XAudio2 2.7
            auto table = *static_cast<void ***>(*object);
            auto hook = safetyhook::InlineHook::create(table[10], CreateMasteringVoice);
            if (hook) masterHook = std::move(*hook);
            else spdlog::warn("[Audio] Could not hook CreateMasteringVoice for Force Stereo Audio.");
        }
        return hr;
    }
}
#endif

void StereoAudioFix::Setup()
{
#ifndef _WIN64
    if (!M2Config::bForceStereoAudio || createHook) return;
    if (M2Fix::Game() != M2FixGame::Dracula && M2Fix::Game() != M2FixGame::Contra) return;

    auto module = LoadLibraryW(L"ole32.dll");
    auto address = module ? GetProcAddress(module, "CoCreateInstance") : nullptr;
    if (!address) {
        spdlog::warn("[Audio] CoCreateInstance was not found for Force Stereo Audio.");
        return;
    }
    auto hook = safetyhook::InlineHook::create(address, CoCreate);
    if (hook) createHook = std::move(*hook);
    else spdlog::warn("[Audio] Could not hook CoCreateInstance for Force Stereo Audio.");
#endif
}

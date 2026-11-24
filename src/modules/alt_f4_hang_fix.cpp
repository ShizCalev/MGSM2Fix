#include "stdafx.h"
#ifndef _WIN64
#include "alt_f4_hang_fix.h"
#include "m2fix.h"

namespace
{
    BOOL WINAPI PeekMessageHook(LPMSG message, HWND window, UINT minFilter, UINT maxFilter, UINT remove)
    {
        const BOOL result = M2Hook::GetInstance().Invoke<BOOL>(PeekMessageHook, message, window, minFilter, maxFilter, remove);
        if (result && (remove & PM_REMOVE) && message->message == WM_QUIT) { // Steam overlay timing sometimes causes these games to discard their WM_QUIT message after destroying the main window, but before m2's entered the proper shutdown path. we'll just give it a lil shove.
            spdlog::shutdown();
            ExitProcess(static_cast<UINT>(message->wParam));
        }
        return result;
    }
}

void AltF4HangFix::Setup()
{
    switch (M2Fix::Game())
    {
        case M2FixGame::DraculaAdvance:
        case M2FixGame::Darius:
        case M2FixGame::Darius101:
        case M2FixGame::NMA1:
        case M2FixGame::NMA2:
        {
            M2Hook::GetInstance().Hook(
                GetProcAddress(GetModuleHandleW(L"user32.dll"), "PeekMessageA"),
                PeekMessageHook, "[Shutdown] PeekMessageA"
            );
            break;
        }
        default: break;
    }
}
#endif

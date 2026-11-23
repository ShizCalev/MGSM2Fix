#include "mgs1in4.h"
#include "m2fix.h"
#include "psx.h"
#include <psapi.h>

void MGS1in4::Load()
{
    SQEmuState::SetSystem(M2System::PlayStation);
    SQEmuState::paused.store(false, std::memory_order_relaxed);

    M2Utils::DisableWindowsFullscreenOptimization();

    Analog::LoadInstance();

#ifdef _WIN64
    M2Hook::GetInstance().MidHook(
        "4C 89 7C 24 58 48 89 74 24 50 4C 89 7C 24 48 4C", 0x2B,
        MGS4_CreateWindow, "[MGS 4] MGS4::CreateWindow"
    );
#endif

    if (M2Config::bInternalEnabled)
    {
#ifdef _WIN64
        M2Hook::GetInstance().MidHook(
            "BA 01 80 00 00 C7 44 24 70 40 01 00 00 48 8B CE C7 "
            "44 24 74 F0 00 00 00", 0x18,
            MGS4_Draw, "[MGS 4] MGS4::Draw"
        );

        M2Hook::GetInstance().MidHook(
            "41 C7 47 30 40 01 00 00 41 C7 47 34 F0 00 00 00", 0x10,
            MGS4_DisplaySize, "[MGS 4] MGS4::DisplaySize"
        );

        M2Hook::GetInstance().MidHook(
            "8B 84 24 08 01 00 00 45 0F 57 DB F3 4C 0F 2A D8", 0,
            MGS4_DriverPosX, "[MGS 4] MGS4::DriverPosX"
        );

        M2Hook::GetInstance().MidHook(
            "8B 84 24 10 01 00 00 45 0F 57 D2 F3 4C 0F 2A D0", 0,
            MGS4_DriverPosY, "[MGS 4] MGS4::DriverPosY"
        );

        M2Hook::GetInstance().MidHook(
            "66 48 0F 7E C2 FF 50 38 49 8B 96 30 01 00 00 49", 0,
            MGS4_ViewPos, "[MGS 4] MGS4::ViewPos"
        );

        M2Hook::GetInstance().MidHook(
            "49 8B 86 18 01 00 00 F3 0F 11 48 50 F3 0F 11 48", 0,
            MGS4_ViewSize, "[MGS 4] MGS4::ViewSize"
        );
#endif
    }
}


int MGS1in4::MGS4_main(M2_EmuR3000 *cpu, int cycle, unsigned int address)
{
    PSX::main(cpu);
    unsigned int ra = cpu->Reg[31];
    spdlog::info("[MGS 4] __main: 0x{:08x} -> 0x{:08x}.", address, ra);

    PSXFUNCTION main = PSX::UserHandler(cpu, address);
    return main(cpu, cycle, address);
}

void MGS1in4::EPIOnLoadImage(void *image, unsigned int size)
{
    if (MGS4_LoadImageCount == 0) {
        for (auto & Machine : MachineInstances()) {
            Machine.get().BindModules();
        }

        ULONG_PTR pbi[6];
        ULONG size = 0;

        LONG(WINAPI *NtQueryInformationProcess)(
            HANDLE ProcessHandle,
            ULONG ProcessInformationClass,
            PVOID ProcessInformation,
            ULONG ProcessInformationLength,
            PULONG ReturnLength
        );

        *(FARPROC *) &NtQueryInformationProcess = GetProcAddress(LoadLibraryA("NTDLL.DLL"), "NtQueryInformationProcess");
        if (NtQueryInformationProcess) {
            if (NtQueryInformationProcess(GetCurrentProcess(), 0, &pbi, sizeof(pbi), &size) >= 0 && size == sizeof(pbi)) {
				const auto hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pbi[5]);
                char buffer[MAX_PATH];
                GetProcessImageFileName(hProcess, buffer, MAX_PATH);
                std::string name = std::string(buffer);
				std::filesystem::path path = std::filesystem::path(name);
                if (_stricmp(path.filename().string().c_str(), "mgs4.exe") == 0) {
                    TerminateProcess(hProcess, 0);
                }
                CloseHandle(hProcess);
            }
        }
    }
    MGS4_LoadImageCount++;

    MGS4_LoaderPTR = M2Hook::GetInstance().ScanBuffer(
        "00 00 00 00 69 6E 69 74 00 00 00 00",
        -0x30 - reinterpret_cast<uintptr_t>(image),
        image, size, "[MGS 4] mgs_loader_stage"
    );
}

bool MGS1in4::EPIOnMachineCommand(std::any machine, int cmd, unsigned int **args)
{
    struct M2_EmuPSX *psx = std::any_cast<M2_EmuPSX *>(machine);
    struct M2_EmuGPU *gpu = psx->DevGPU;
    bool ret = true;

    switch (cmd)
    {
        case 0x8002: // GET_POSITION
        {
            if (!M2Config::bInternalEnabled) {
                break;
            }

            unsigned int w =  ((gpu->ScreenRangeW >> 12) & 0xFFF) - (gpu->ScreenRangeW & 0xFFF);
            unsigned int h = (((gpu->ScreenRangeH >> 10) & 0x3FF) - (gpu->ScreenRangeH & 0x3FF)) << ((gpu->Status >> 22) & 1);
            unsigned int x = 240 << 1;
            unsigned int y = std::min(h, x);

            *(args[0]) = (2560 - w) >> 1;
            *(args[1]) = (M2Config::iInternalHeight * ((x - y) >> 1)) / x;
            *(args[2]) = (w * M2Config::iInternalHeight) / 240;
            *(args[3]) = (y * M2Config::iInternalHeight) / 240;

            ret = false;
            break;
        }

        case 0x8003: // GET_DIMENSION
        {
            if (!M2Config::bInternalEnabled) {
                break;
            }

            *(args[0]) = (M2Config::iInternalHeight * 320) / 240;
            *(args[1]) =  M2Config::iInternalHeight;

            ret = false;
            break;
        }

        case 0x8004: // SET_DEVICE
        {
            if (!M2Config::bInternalEnabled) {
                break;
            }

            unsigned int *res = args[0];

            res[2] = ((M2Config::iInternalHeight * 320) / 240) * 2;
            res[3] =   M2Config::iInternalHeight * 2;
            break;
        }

        case 0x8007: // SET_VIDEO_MODE
        {
            PSX::VideoMode = reinterpret_cast<unsigned int>(args[0]);
            break;
        }

        default: break;
    }

    return ret;
}

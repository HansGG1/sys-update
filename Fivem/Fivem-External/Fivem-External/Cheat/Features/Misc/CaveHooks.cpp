#include "CaveHooks.hpp"
#include "../../FivemSDK/Fivem.hpp"
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// Damage Cave — intercepts MOVSS [RCX+0x280], XMM0.
//
// Flags page: +0x00 local_player_ped, +0x08 bubbles_active, +0x09 godmode_active
//
// Blocks ALL health reductions on local player when bubbles OR godmode is on.
// Healing writes (new >= current) are always allowed through.
//
// Byte map:
//  [0]  50                      push rax                    1
//  [1]  48 B8 xx*8              mov rax, flagsAddr         10
// [11]  48 3B 08                cmp rcx, [rax]              3
// [14]  75 19                   jnz +25 → [41] .pop_execute 2
// [16]  8A 40 08                mov al, [rax+8]  bubbles     3
// [19]  84 C0                   test al, al                  2
// [21]  75 07                   jnz +7  → [30] .check_dmg   2
// [23]  8A 40 09                mov al, [rax+9]  godmode     3
// [26]  84 C0                   test al, al                  2
// [28]  74 0B                   jz  +11 → [41] .pop_execute  2
// [30]  58                      pop rax          .check_dmg  1
// [31]  0F 2F 81 80 02 00 00    comiss xmm0,[rcx+0x280]      7
// [38]  73 02                   jae +2  → [42] .execute      2
// [40]  C3                      ret  (BLOCKED)               1
// [41]  58                      pop rax          .pop_execute 1
// [42]  F3 0F 11 81 80 02 00 00 original MOVSS   .execute    8
// [50]  FF 25 00 00 00 00       jmp [rip+0]      .return     6
// [56]  xx*8                    return address               8
// Total: 64 bytes
// ─────────────────────────────────────────────────────────────────────────────

namespace Cheat
{
    namespace CaveHooks
    {
        uint64_t g_FlagsPage = 0;
        bool     g_Installed = false;

        static void AppendU64(std::vector<uint8_t>& v, uint64_t val)
        {
            for (int i = 0; i < 8; i++)
                v.push_back(static_cast<uint8_t>((val >> (i * 8)) & 0xFF));
        }

        void Init()
        {
            uint64_t modBase = g_Fivem.GetModuleBase();
            if (!modBase) return;

            size_t imgSize = FrameWork::Memory::GetModuleImageSize(modBase);
            if (imgSize < 0x1000000) return;

            uint64_t hookSite = FrameWork::Memory::PatternScan(
                modBase, imgSize, "F3 0F 11 81 80 02 00 00");
            if (!hookSite) return;

            uint64_t caveBase = FrameWork::Memory::AllocateProcessMemory(
                4096, PAGE_EXECUTE_READWRITE, modBase);
            if (!caveBase) return;

            g_FlagsPage = caveBase + 0xE00;
            uint64_t flagsAddr  = g_FlagsPage;
            uint64_t returnAddr = hookSite + 8;

            std::vector<uint8_t> cave;
            cave.reserve(72);

            // [0]  push rax
            cave.push_back(0x50);
            // [1]  mov rax, flagsAddr
            cave.push_back(0x48); cave.push_back(0xB8);
            AppendU64(cave, flagsAddr);
            // [11] cmp rcx, [rax]
            cave.push_back(0x48); cave.push_back(0x3B); cave.push_back(0x08);
            // [14] jnz +25 → [41] .pop_execute
            cave.push_back(0x75); cave.push_back(0x19);
            // [16] mov al, [rax+8]  (bubbles)
            cave.push_back(0x8A); cave.push_back(0x40); cave.push_back(0x08);
            // [19] test al, al
            cave.push_back(0x84); cave.push_back(0xC0);
            // [21] jnz +7 → [30] .check_dmg
            cave.push_back(0x75); cave.push_back(0x07);
            // [23] mov al, [rax+9]  (godmode)
            cave.push_back(0x8A); cave.push_back(0x40); cave.push_back(0x09);
            // [26] test al, al
            cave.push_back(0x84); cave.push_back(0xC0);
            // [28] jz +11 → [41] .pop_execute
            cave.push_back(0x74); cave.push_back(0x0B);
            // [30] pop rax  .check_dmg
            cave.push_back(0x58);
            // [31] comiss xmm0, [rcx+0x280]
            cave.push_back(0x0F); cave.push_back(0x2F);
            cave.push_back(0x81); cave.push_back(0x80); cave.push_back(0x02);
            cave.push_back(0x00); cave.push_back(0x00);
            // [38] jae +2 → [42] .execute  (healing → allow)
            cave.push_back(0x73); cave.push_back(0x02);
            // [40] ret  (damage blocked)
            cave.push_back(0xC3);
            // [41] pop rax  .pop_execute
            cave.push_back(0x58);
            // [42] original MOVSS  .execute
            cave.push_back(0xF3); cave.push_back(0x0F);
            cave.push_back(0x11); cave.push_back(0x81);
            cave.push_back(0x80); cave.push_back(0x02);
            cave.push_back(0x00); cave.push_back(0x00);
            // [50] jmp [rip+0]  .return
            cave.push_back(0xFF); cave.push_back(0x25);
            cave.push_back(0x00); cave.push_back(0x00);
            cave.push_back(0x00); cave.push_back(0x00);
            // [56] return address
            AppendU64(cave, returnAddr);

            FrameWork::Memory::WriteProcessMemoryImpl(
                caveBase,
                reinterpret_cast<LPVOID>(cave.data()),
                cave.size());

            int32_t rel = static_cast<int32_t>(
                static_cast<int64_t>(caveBase) - static_cast<int64_t>(hookSite) - 5);

            uint8_t patch[8] = {
                0xE9,
                static_cast<uint8_t>( rel        & 0xFF),
                static_cast<uint8_t>((rel >>  8) & 0xFF),
                static_cast<uint8_t>((rel >> 16) & 0xFF),
                static_cast<uint8_t>((rel >> 24) & 0xFF),
                0x90, 0x90, 0x90
            };
            FrameWork::Memory::WriteProcessMemoryImpl(
                hookSite,
                reinterpret_cast<LPVOID>(patch),
                sizeof(patch));

            g_Installed = true;
        }

        void UpdateFlags(uint64_t localPedAddr, bool bubblesActive, bool godmodeActive)
        {
            if (!g_FlagsPage) return;
            FrameWork::Memory::WriteMemory<uint64_t>(g_FlagsPage,     localPedAddr);
            FrameWork::Memory::WriteMemory<uint8_t> (g_FlagsPage + 8, bubblesActive ? 1u : 0u);
            FrameWork::Memory::WriteMemory<uint8_t> (g_FlagsPage + 9, godmodeActive ? 1u : 0u);
        }
    }
}

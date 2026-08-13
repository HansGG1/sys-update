#pragma once
#include <cstdint>

namespace Cheat
{
    namespace CaveHooks
    {
        // Flags page layout:
        //   +0x00  uint64_t  local_player_ped
        //   +0x08  uint8_t   bubbles_active
        //   +0x09  uint8_t   godmode_active
        extern uint64_t g_FlagsPage;
        extern bool     g_Installed;

        void Init();
        void UpdateFlags(uint64_t localPedAddr, bool bubblesActive, bool godmodeActive);
    }
}

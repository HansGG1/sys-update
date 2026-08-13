#include "AntiSilentAim.hpp"
#include "../../Options.hpp"
#include <thread>

namespace Cheat
{
	namespace AntiSilentAim
	{
		void RunThread()
		{
			SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

			while (!g_Options.General.ShutDown)
			{
				auto& LP = g_Options.Misc.Exploits.LocalPlayer;
				bool keyActive = (LP.BubblesBind == 0) || (GetAsyncKeyState(LP.BubblesBind) & 0x8000);
				CPed* lp = g_Fivem.GetLocalPlayerInfo().Ped;

				if (LP.Bubbles && keyActive && lp)
				{
					uint64_t addr = (uint64_t)lp + 0x188;
					uint32_t flags = FrameWork::Memory::ReadMemory<uint32_t>(addr);
					flags |= (1u << 9);
					for (int i = 0; i < 5; ++i)
						FrameWork::Memory::WriteMemory<uint32_t>(addr, flags);
				}

				std::this_thread::sleep_for(std::chrono::microseconds(100));
			}
		}
	}
}

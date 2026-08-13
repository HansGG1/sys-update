#include "AutoPistol.hpp"

#include "../../Options.hpp"

namespace Cheat
{
	void AutoPistol::RunThread()
	{
		while (!g_Options.General.ShutDown)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));

			if (!g_Options.LegitBot.AutoPistol.Enabled)
				continue;

			if (!g_Fivem.GetLocalPlayerInfo().Ped)
				continue;

			if (!GetAsyncKeyState(g_Options.LegitBot.AutoPistol.KeyBind))
				continue;

			if (!g_Fivem.IsPlayerAiming())
				continue;

			INPUT input = {};
			input.type = INPUT_MOUSE;

			input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
			SafeCall(SendInput)(1, &input, sizeof(INPUT));

			std::this_thread::sleep_for(std::chrono::milliseconds(g_Options.LegitBot.AutoPistol.FireRate));

			input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
			SafeCall(SendInput)(1, &input, sizeof(INPUT));

			std::this_thread::sleep_for(std::chrono::milliseconds(g_Options.LegitBot.AutoPistol.FireRate));
		}
	}
}

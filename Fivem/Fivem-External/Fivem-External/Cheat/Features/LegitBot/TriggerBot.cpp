#include "TriggerBot.hpp"

#include <thread>
#include <mutex>

#include "../../Options.hpp"
#include "../Visuals/PlayerESP.hpp"

namespace Cheat
{
	void TriggerBot::RunThread()
	{
		while (!g_Options.General.ShutDown)
		{
			if (!g_Options.LegitBot.Trigger.Enabled)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				continue;
			}

			if (!g_Fivem.GetLocalPlayerInfo().Ped)
				continue;

			static bool Shooting = false;

			bool CanShoot = false;

			if (!GetAsyncKeyState(g_Options.LegitBot.Trigger.KeyBind))
			{
				if (Shooting)
				{
					INPUT input = { 0 };
					input.type = INPUT_MOUSE;
					input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
					SafeCall(SendInput)(1, &input, sizeof(INPUT));

					Shooting = false;
				}

				continue;
			}

			CPed* AimingEnity = g_Fivem.GetAimingEntity();

			// Fallback for builds where the aiming-entity pointer is unavailable:
			// treat any enemy within a tight FOV as the aimed-at entity.
			if (!AimingEnity)
			{
				Entity Closest;
				if (g_Fivem.FindClosestEntity((float)g_Options.LegitBot.Trigger.Fov, g_Options.LegitBot.Trigger.MaxDistance, g_Options.LegitBot.Trigger.ShotNPC, &Closest))
					AimingEnity = Closest.StaticInfo.Ped;
			}

			if (AimingEnity)
			{
				if (g_Fivem.GetLocalPlayerInfo().WorldPos.DistTo(AimingEnity->GetCoordinate()) > g_Options.LegitBot.Trigger.MaxDistance)
					CanShoot = false;
				else
					CanShoot = true;

				if (AimingEnity->IsNPC() && !g_Options.LegitBot.Trigger.ShotNPC)
					CanShoot = false;

				if (g_Options.LegitBot.Trigger.VisibleCheck && !AimingEnity->IsVisible())
					CanShoot = false;

				if (CanShoot && !ESP::IsESPVisible(AimingEnity))
					CanShoot = false;

				if (!g_Fivem.IsPlayerAiming())
					CanShoot = false;

				{
					std::lock_guard<std::mutex> lk(g_Fivem.AllEntitesListMtx);
					auto it = g_Fivem.AllEntitesList.find(AimingEnity);
					if (it != g_Fivem.AllEntitesList.end() && it->second.IsFriend)
						CanShoot = false;
				}
			}
			else
				CanShoot = false;

			if (CanShoot && !Shooting)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(g_Options.LegitBot.Trigger.ReactionTime));

				INPUT input = { 0 };
				input.type = INPUT_MOUSE;
				input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
				SafeCall(SendInput)(1, &input, sizeof(INPUT));

				Shooting = true;
			}
			else if (Shooting && !CanShoot)
			{
				INPUT input = { 0 };
				input.type = INPUT_MOUSE;
				input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
				SafeCall(SendInput)(1, &input, sizeof(INPUT));

				Shooting = false;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
}
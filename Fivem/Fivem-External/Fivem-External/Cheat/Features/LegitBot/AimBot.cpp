#include "AimBot.hpp"

#include "../../Options.hpp"
#include "../Visuals/PlayerESP.hpp"

#include <unordered_map>
#include <chrono>

namespace Cheat
{
	struct VelTracker { Vector3D pos{}; Vector3D vel{}; bool valid = false; };
	static std::unordered_map<CPed*, VelTracker> s_VelMap;

	void AimBot::RunThread()
	{
		auto prevTime = std::chrono::steady_clock::now();

		while (!g_Options.General.ShutDown)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));

			if (!g_Options.LegitBot.AimBot.Enabled)
				continue;

			if (!g_Fivem.GetLocalPlayerInfo().Ped)
				continue;

			auto now = std::chrono::steady_clock::now();
			float dt = std::chrono::duration<float>(now - prevTime).count();
			prevTime = now;

			Entity ClosestEntity;

			if (g_Fivem.FindClosestEntity(g_Options.LegitBot.AimBot.FOV, g_Options.LegitBot.AimBot.MaxDistance, g_Options.LegitBot.AimBot.TargetNPC, &ClosestEntity))
			{
				if (!GetAsyncKeyState(g_Options.LegitBot.AimBot.KeyBind))
					continue;

				if (ClosestEntity.StaticInfo.IsFriend)
					continue;

				if (g_Options.LegitBot.AimBot.VisibleCheck && !g_Fivem.IsEntityVisible(ClosestEntity))
					continue;

				if (!ESP::IsESPVisible(ClosestEntity.StaticInfo.Ped))
					continue;

				Vector3D BonePos;

				switch (g_Options.LegitBot.AimBot.HitBox)
				{
				case 0:
					BonePos = g_Fivem.GetBonePosVec3(ClosestEntity, SKEL_Head);
					break;
				case 1:
					BonePos = g_Fivem.GetBonePosVec3(ClosestEntity, SKEL_Neck_1);
					break;
				case 2:
					BonePos = g_Fivem.GetBonePosVec3(ClosestEntity, SKEL_Spine3);
					break;
				default:
					break;
				}

				Vector3D aimPos = BonePos;

				if (g_Options.LegitBot.AimBot.Prediction)
				{
					CPed* ped  = ClosestEntity.StaticInfo.Ped;
					auto& tr   = s_VelMap[ped];

					if (tr.valid && dt > 0.f && dt < 0.1f)
					{
						// Exponential moving average — reduces jitter from read timing noise
						const float alpha = 0.25f;
						tr.vel.x = alpha * ((BonePos.x - tr.pos.x) / dt) + (1.f - alpha) * tr.vel.x;
						tr.vel.y = alpha * ((BonePos.y - tr.pos.y) / dt) + (1.f - alpha) * tr.vel.y;
						tr.vel.z = alpha * ((BonePos.z - tr.pos.z) / dt) + (1.f - alpha) * tr.vel.z;

						LocalPEDInfo lp = g_Fivem.GetLocalPlayerInfo();
						float dx   = BonePos.x - lp.WorldPos.x;
						float dy   = BonePos.y - lp.WorldPos.y;
						float dist = sqrtf(dx * dx + dy * dy);

						// GTA bullet speed ≈ 900 game-units/s for most rifles/pistols
						float ttt = dist / 900.f;

						aimPos.x += tr.vel.x * ttt;
						aimPos.y += tr.vel.y * ttt;
						aimPos.z += tr.vel.z * ttt;
					}

					tr.pos   = BonePos;
					tr.valid = true;
				}

				g_Fivem.ProcessCameraMovement(aimPos, g_Options.LegitBot.AimBot.SmoothHorizontal, g_Options.LegitBot.AimBot.SmoothVertical);
			}
			else
			{
				// Clear velocity cache for entities no longer targeted to avoid stale data
				if (s_VelMap.size() > 32)
					s_VelMap.clear();
			}
		}
	}
}

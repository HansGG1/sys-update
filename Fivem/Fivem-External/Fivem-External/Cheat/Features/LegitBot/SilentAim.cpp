#include "SilentAim.hpp"

#include "../../Options.hpp"
#include "../Visuals/PlayerESP.hpp"
#include <cstdlib>

namespace Cheat
{
	std::atomic<CPed*> g_SilentAimTarget { nullptr };
	std::atomic<bool>  g_SilentAimLocked { false };

	static uint64_t s_HandleBullet = 0;
	static uint64_t s_AllocPtr     = 0;

	static bool EnsureInit()
	{
		if (!s_HandleBullet)
			s_HandleBullet = g_Fivem.GetHandleBulletAddress();
		if (!s_HandleBullet)
			return false;

		if (!s_AllocPtr)
			s_AllocPtr = FrameWork::Memory::AllocateProcessMemory(
				0x40, PAGE_EXECUTE_READWRITE, s_HandleBullet);

		return s_AllocPtr != 0;
	}

	// Redirects R9 (bullet target) to saPos so the bullet hits the SA target.
	static void WriteCave(Vector3D saPos)
	{
		if (!s_AllocPtr || !s_HandleBullet)
			return;

		union { float f; uint32_t i; } SX, SY, SZ;
		SX.f = saPos.x; SY.f = saPos.y; SZ.f = saPos.z;

		uint32_t J = static_cast<uint32_t>(
			static_cast<intptr_t>((s_HandleBullet + 5) - (s_AllocPtr + 33)));

		uint8_t Cave[33] =
		{
			0x41,0xC7,0x01,
				uint8_t(SX.i),uint8_t(SX.i>>8),uint8_t(SX.i>>16),uint8_t(SX.i>>24),
			0x41,0xC7,0x41,0x04,
				uint8_t(SY.i),uint8_t(SY.i>>8),uint8_t(SY.i>>16),uint8_t(SY.i>>24),
			0x41,0xC7,0x41,0x08,
				uint8_t(SZ.i),uint8_t(SZ.i>>8),uint8_t(SZ.i>>16),uint8_t(SZ.i>>24),
			0xF3,0x41,0x0F,0x10,0x19,
			0xE9,
				uint8_t(J),uint8_t(J>>8),uint8_t(J>>16),uint8_t(J>>24)
		};
		FrameWork::Memory::WriteProcessMemoryImpl(s_AllocPtr, Cave, sizeof(Cave));
	}

	static void ActivateHook()
	{
		if (!s_AllocPtr || !s_HandleBullet)
			return;

		uint32_t Jmp = static_cast<uint32_t>(
			static_cast<intptr_t>(s_AllocPtr - (s_HandleBullet + 5)));

		uint8_t Hook[5] = { 0xE9,
			uint8_t(Jmp), uint8_t(Jmp>>8), uint8_t(Jmp>>16), uint8_t(Jmp>>24) };

		FrameWork::Memory::WriteProcessMemoryImpl(s_HandleBullet, Hook, sizeof(Hook));
	}

	static void DeactivateHook()
	{
		if (!s_HandleBullet)
			return;

		uint8_t Original[17] =
		{
			0xF3,0x41,0x0F,0x10,0x19,
			0xF3,0x41,0x0F,0x10,0x41,0x04,
			0xF3,0x41,0x0F,0x10,0x51,0x08
		};
		FrameWork::Memory::WriteProcessMemoryImpl(s_HandleBullet, Original, sizeof(Original));

		if (s_AllocPtr)
		{
			uint8_t Zeros[56] = {};
			FrameWork::Memory::WriteProcessMemoryImpl(s_AllocPtr, Zeros, sizeof(Zeros));
		}
	}

	void SilentAim::RunThread()
	{
		static bool HookActive = false;
		static bool MissThis   = false;
		static bool wasLMB     = false;

		while (!g_Options.General.ShutDown)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));

			const bool saEnabled = g_Options.LegitBot.SilentAim.Enabled;

			if (!saEnabled)
			{
				if (HookActive) { DeactivateHook(); HookActive = false; }
				continue;
			}

			if (!g_Fivem.GetLocalPlayerInfo().Ped)
			{
				if (HookActive) { DeactivateHook(); HookActive = false; }
				continue;
			}

			if (!EnsureInit())
				continue;

			// ── Scan for target ───────────────────────────────────────────
			Entity   saClosest;
			bool     hasSATarget = false;
			Vector3D saBonePos   = {};

			hasSATarget = g_Fivem.FindClosestEntity(
				(float)g_Options.LegitBot.SilentAim.Fov,
				g_Options.LegitBot.SilentAim.MaxDistance,
				g_Options.LegitBot.SilentAim.ShotNPC,
				&saClosest);

			if (hasSATarget && saClosest.StaticInfo.IsFriend)
				hasSATarget = false;
			if (hasSATarget && g_Options.LegitBot.SilentAim.VisibleCheck && !g_Fivem.IsEntityVisible(saClosest))
				hasSATarget = false;
			if (hasSATarget && !ESP::IsESPVisible(saClosest.StaticInfo.Ped))
				hasSATarget = false;

			if (hasSATarget)
			{
				switch (g_Options.LegitBot.SilentAim.HitBox)
				{
				case 0: saBonePos = g_Fivem.GetBonePosVec3(saClosest, SKEL_Head);   break;
				case 1: saBonePos = g_Fivem.GetBonePosVec3(saClosest, SKEL_Neck_1); break;
				case 2: saBonePos = g_Fivem.GetBonePosVec3(saClosest, SKEL_Spine3); break;
				default: break;
				}
			}

			// ── Key state — 0 = Always On ─────────────────────────────────
			const int  kb      = g_Options.LegitBot.SilentAim.KeyBind;
			const bool keyHeld = (kb == 0) ? true : (GetAsyncKeyState(kb) & 0x8000) != 0;

			// ── Hit chance — re-roll on each LMB click ────────────────────
			const bool lmbHeld = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
			if (lmbHeld && !wasLMB)
				MissThis = (std::rand() % 100) >= g_Options.LegitBot.SilentAim.HitChance;
			wasLMB = lmbHeld;

			const bool saActive = keyHeld && hasSATarget && !MissThis;

			// ── Cave update ───────────────────────────────────────────────
			if (hasSATarget)
				WriteCave(saBonePos);

			// ── Hook control ──────────────────────────────────────────────
			if (saActive)
			{
				ActivateHook();
				HookActive = true;
			}
			else if (HookActive)
			{
				DeactivateHook();
				HookActive = false;
			}

			g_SilentAimTarget.store(hasSATarget ? saClosest.StaticInfo.Ped : nullptr);
			g_SilentAimLocked.store(HookActive && saActive);
		}

		// Restore FiveM's original bytes before the thread dies
		if (HookActive)
			DeactivateHook();
	}
}

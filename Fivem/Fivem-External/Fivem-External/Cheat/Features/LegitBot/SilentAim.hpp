#pragma once

#include <atomic>
#include "../../FivemSDK/Fivem.hpp"

namespace Cheat
{
	extern std::atomic<CPed*> g_SilentAimTarget;
	extern std::atomic<bool>  g_SilentAimLocked;

	namespace SilentAim
	{
		void RunThread();
	}
}
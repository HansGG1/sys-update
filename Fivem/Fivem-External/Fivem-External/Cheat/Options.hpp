#pragma once

namespace Cheat
{
	class Options
	{
	public:
		struct LegitBot
		{
			struct AimBot
			{
				bool Enabled;
				int KeyBind = 0;
				bool TargetNPC;
				bool VisibleCheck;
				bool Prediction = false;
				int HitBox = 0;
				int MaxDistance = 250;
				int FOV = 10;
				int SmoothHorizontal = 30;
				int SmoothVertical = 30;
				float FovColor[4] = { 1.f, 1.f, 1.f, 1.f };
			}AimBot;
			struct SilentAim
			{
				bool Enabled    = false;
				int  KeyBind    = 0;
				bool ShotNPC    = false;
				bool VisibleCheck = false;
				int  MaxDistance = 250;
				int  Fov        = 10;
				int  HitBox     = 0;   // 0=Head 1=Neck 2=Chest
				int  HitChance  = 100;
				float FovColor[4] = { 1.f, 0.2f, 0.2f, 1.f };
			}SilentAim;
			struct TriggerBot
			{
				bool Enabled;
				int KeyBind = 0;
				bool ShotNPC;
				bool VisibleCheck;
				int MaxDistance = 250;
				int ReactionTime;
				int Fov = 3;
				float FovColor[4] = { 1.f, 0.2f, 0.2f, 1.f };
			}Trigger;
		}LegitBot;
		struct Visuals
		{
			struct ESP
			{
				struct Players
				{
					bool Enabled = true;
					bool ShowLocalPlayer = false;
					bool ShowNPCs;
					bool VisibleCheck = false;
					float VisibleColor[4]    = { 1.f, 1.f, 1.f, 1.f };
					float NotVisibleColor[4] = { 1.f, 0.f, 0.f, 0.8f };
					int RenderDistance = 300;
					bool Box;
					bool Skeleton = true;
					float SkeletonColor[4] = { 0.f, 1.f, 0.f, 1.f };
					bool Name = true;
					float NameColor[4] = { 0.f, 1.f, 0.f, 1.f };
					bool HealthBar = true;
					bool ArmorBar;
					bool WeaponName;
					bool Weapon_Misc = true;
					float WeaponNameColor[4] = { 0.f, 1.f, 0.f, 1.f };
					bool Distance;
					float DistanceColor[4] = { 0.f, 1.f, 0.f, 1.f };
					bool SnapLines;
					float SnaplinesColor[4] = { 0.f, 1.f, 0.f, 1.f };
					bool HeadDot;
					int HeadDotSize = 2;
					float HeadDotColor[4] = { 1.f, 1.f, 1.f, 1.f };
				}Players;

				struct Vehicles
				{
					bool Enabled;
					bool IgnoreOccupiedVehicles;
					bool Marker;
					bool Distance;
					bool Name;
					float Color[4]         = { 0.f, 1.f, 0.f, 1.f };
					float NameColor[4]     = { 1.f, 1.f, 1.f, 1.f };
					float DistanceColor[4] = { 0.8f, 0.8f, 0.8f, 1.f };
					int RenderDistance = 300;
				}Vehicles;
			}ESP;
			struct
			{
				bool Enabled;
				int DisplayRadius = 75;
				int Range = 150;
				bool ShowNPC;
				bool RotateWithCamera = true;
				float PlayerColor[4] = { 1.f, 0.2f, 0.2f, 1.f };
				float FriendColor[4] = { 0.2f, 0.8f, 0.2f, 1.f };
			}Radar;
		}Visuals;
		struct Misc
		{
			struct Screen
			{
				bool ShowAimbotFov;
				bool ShowTriggerBotFov;
				bool ShowSilentAimFov = false;
			}Screen;
			struct Exploits
			{
				struct LocalPlayer
				{
					bool Noclip;
					int NoclipBind;
					int NoclipSpeed;

					bool norecoil;
					bool nospread;
					bool noreload;
					bool nosway;
					bool rapidfire;
					bool damagemult;
					float DamageMultiplier = 1.f;
					int health_ammount = 200;

					bool Start_Health = false;

					bool GiveHealth = false;
					int  HealthBind = 0;
					bool GiveArmor = false;
					int  ArmorBind = 0;

					bool Bubbles = false;
					int  BubblesBind = 0;
					bool TeleportWaypoint = false;
				}LocalPlayer;
			}Exploits;
			float FriendColor[4] = { 0.f, 1.f, 0.f, 1.f };
		}Misc;
		struct General
		{
			bool ShutDown = false;
			bool Activate = false;
			bool Destruct = false;
			int PanicKey = 0;
			int MenuKey = VK_DELETE;
			bool CaptureBypass = true;
			bool VSync = true;
			int TargetMonitor = 0;
			int ThreadDelay = 250;
			bool ShowKeybindList = false;
		}General;
	};
}

inline Cheat::Options g_Options;

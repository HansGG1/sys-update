#include "ConfigSystem.hpp"
#include <fstream>
#include <windows.h>

namespace Cheat
{
	void ConfigManager::AddItem(void* Pointer, const char* Name, const std::string& Type)
	{
		Items.push_back(new C_ConfigItem(std::string(Name), Pointer, Type));
	}

	void ConfigManager::SetupItem(int* Pointer, float Value, const std::string& Name)
	{
		AddItem(Pointer, Name.c_str(), XorStr("int"));
		*Pointer = Value;
	}

	void ConfigManager::SetupItem(float* Pointer, float Value, const std::string& Name)
	{
		AddItem(Pointer, Name.c_str(), XorStr("float"));
		*Pointer = Value;
	}

	void ConfigManager::SetupItem(bool* Pointer, float Value, const std::string& Name)
	{
		AddItem(Pointer, Name.c_str(), XorStr("bool"));
		*Pointer = Value;
	}

	void ConfigManager::Setup()
	{
		// Aimbot
		SetupItem(&g_Options.LegitBot.AimBot.Enabled, false, XorStr("abt_enabled"));
		SetupItem(&g_Options.LegitBot.AimBot.KeyBind, 0, XorStr("abt_key"));
		SetupItem(&g_Options.LegitBot.AimBot.TargetNPC, false, XorStr("abt_targetnpc"));
		SetupItem(&g_Options.LegitBot.AimBot.HitBox, 0, XorStr("abt_hitbox"));
		SetupItem(&g_Options.LegitBot.AimBot.MaxDistance, 250, XorStr("abt_maxdistance"));
		SetupItem(&g_Options.LegitBot.AimBot.FOV, 10, XorStr("abt_fov"));
		SetupItem(&g_Options.LegitBot.AimBot.SmoothVertical, 80, XorStr("abt_smoothvertical"));
		SetupItem(&g_Options.LegitBot.AimBot.SmoothHorizontal, 80, XorStr("abt_smoothhorizontal"));
		SetupItem(&g_Options.LegitBot.AimBot.VisibleCheck, false, XorStr("VisibleCheck"));
		SetupItem(&g_Options.LegitBot.AimBot.FovColor[0], 1.f, XorStr("abt_fovcol0"));
		SetupItem(&g_Options.LegitBot.AimBot.FovColor[1], 1.f, XorStr("abt_fovcol1"));
		SetupItem(&g_Options.LegitBot.AimBot.FovColor[2], 1.f, XorStr("abt_fovcol2"));
		SetupItem(&g_Options.LegitBot.AimBot.FovColor[3], 1.f, XorStr("abt_fovcol3"));

		// TriggerBot
		SetupItem(&g_Options.LegitBot.Trigger.Enabled, false, XorStr("trtg_enabled"));
		SetupItem(&g_Options.LegitBot.Trigger.KeyBind, 0, XorStr("trtg_key"));
		SetupItem(&g_Options.LegitBot.Trigger.ShotNPC, false, XorStr("trtg_shotnpc"));
		SetupItem(&g_Options.LegitBot.Trigger.MaxDistance, 250, XorStr("trtg_maxdistance"));
		SetupItem(&g_Options.LegitBot.Trigger.ReactionTime, 0, XorStr("trtg_reactiontime"));
		SetupItem(&g_Options.LegitBot.Trigger.VisibleCheck, false, XorStr("VisibleCheck_trigger"));
		SetupItem(&g_Options.LegitBot.Trigger.Fov, 3, XorStr("trtg_fov"));
		SetupItem(&g_Options.LegitBot.Trigger.FovColor[0], 1.f,  XorStr("trtg_fovcol0"));
		SetupItem(&g_Options.LegitBot.Trigger.FovColor[1], 0.2f, XorStr("trtg_fovcol1"));
		SetupItem(&g_Options.LegitBot.Trigger.FovColor[2], 0.2f, XorStr("trtg_fovcol2"));
		SetupItem(&g_Options.LegitBot.Trigger.FovColor[3], 1.f,  XorStr("trtg_fovcol3"));

		// MISC
		SetupItem(&g_Options.Misc.Exploits.LocalPlayer.norecoil,  false, XorStr("norecoil"));
		SetupItem(&g_Options.Misc.Exploits.LocalPlayer.nospread,  false, XorStr("nospread"));
		SetupItem(&g_Options.Misc.Exploits.LocalPlayer.noreload,  false, XorStr("noreload"));
		SetupItem(&g_Options.Misc.Exploits.LocalPlayer.nosway,          false, XorStr("nosway"));
		SetupItem(&g_Options.Misc.Exploits.LocalPlayer.rapidfire,       false, XorStr("rapidfire"));
		SetupItem(&g_Options.Misc.Exploits.LocalPlayer.damagemult,      false, XorStr("damagemult"));
		SetupItem(&g_Options.Misc.Exploits.LocalPlayer.DamageMultiplier, 1.f,  XorStr("damagemultval"));

		SetupItem(&g_Options.Misc.Exploits.LocalPlayer.Bubbles,     false, XorStr("bubbles_enabled"));
		SetupItem(&g_Options.Misc.Exploits.LocalPlayer.BubblesBind, 0,     XorStr("bubbles_bind"));

		SetupItem(&g_Options.Misc.Exploits.LocalPlayer.Noclip, false, XorStr("Noclip"));
		SetupItem(&g_Options.Misc.Exploits.LocalPlayer.NoclipBind, false, XorStr("NoclipBind"));
		SetupItem(&g_Options.Misc.Exploits.LocalPlayer.NoclipSpeed, 100, XorStr("NoclipSpeed"));
		//

		// Esp-Players
		SetupItem(&g_Options.Visuals.ESP.Players.Enabled, true, XorStr("esp_players_enabled"));
		SetupItem(&g_Options.Visuals.ESP.Players.ShowLocalPlayer, false, XorStr("esp_players_localplayer"));
		SetupItem(&g_Options.Visuals.ESP.Players.ShowNPCs, false, XorStr("esp_players_npscs"));
		SetupItem(&g_Options.Visuals.ESP.Players.RenderDistance, 300, XorStr("esp_players_renderdist"));
		SetupItem(&g_Options.Visuals.ESP.Players.Box, false, XorStr("esp_players_box"));
		SetupItem(&g_Options.Visuals.ESP.Players.Skeleton, true, XorStr("esp_players_skel"));
		SetupItem(&g_Options.Visuals.ESP.Players.Name, true, XorStr("esp_players_name"));
		SetupItem(&g_Options.Visuals.ESP.Players.HealthBar, 0, XorStr("esp_players_healthbar"));
		SetupItem(&g_Options.Visuals.ESP.Players.ArmorBar, 0, XorStr("esp_players_armorbar"));
		SetupItem(&g_Options.Visuals.ESP.Players.WeaponName, 0, XorStr("esp_players_weapname"));
		SetupItem(&g_Options.Visuals.ESP.Players.Distance, false, XorStr("esp_players_distance"));
		SetupItem(&g_Options.Visuals.ESP.Players.SnapLines, false, XorStr("esp_players_snampli"));
		SetupItem(&g_Options.Visuals.ESP.Players.VisibleCheck, false, XorStr("esp_players_vischeck"));

		// Some ESP-pLAYER-cOLOR
		SetupItem(&g_Options.Visuals.ESP.Players.VisibleColor[0],    1.f,  XorStr("esp_players_visco0"));
		SetupItem(&g_Options.Visuals.ESP.Players.VisibleColor[1],    1.f,  XorStr("esp_players_visco1"));
		SetupItem(&g_Options.Visuals.ESP.Players.VisibleColor[2],    1.f,  XorStr("esp_players_visco2"));
		SetupItem(&g_Options.Visuals.ESP.Players.VisibleColor[3],    1.f,  XorStr("esp_players_visco3"));
		SetupItem(&g_Options.Visuals.ESP.Players.NotVisibleColor[0], 1.f,  XorStr("esp_players_notvisco0"));
		SetupItem(&g_Options.Visuals.ESP.Players.NotVisibleColor[1], 0.f,  XorStr("esp_players_notvisco1"));
		SetupItem(&g_Options.Visuals.ESP.Players.NotVisibleColor[2], 0.f,  XorStr("esp_players_notvisco2"));
		SetupItem(&g_Options.Visuals.ESP.Players.NotVisibleColor[3], 0.8f, XorStr("esp_players_notvisco3"));
		SetupItem(&g_Options.Visuals.ESP.Players.SkeletonColor[0], 1.f, XorStr("esp_players_skelco0"));
		SetupItem(&g_Options.Visuals.ESP.Players.SkeletonColor[1], 1.f, XorStr("esp_players_skelco1"));
		SetupItem(&g_Options.Visuals.ESP.Players.SkeletonColor[2], 1.f, XorStr("esp_players_skelco2"));
		SetupItem(&g_Options.Visuals.ESP.Players.SkeletonColor[3], 1.f, XorStr("esp_players_skelco3"));
		SetupItem(&g_Options.Visuals.ESP.Players.NameColor[0], 1.f, XorStr("esp_players_NAMEco0"));
		SetupItem(&g_Options.Visuals.ESP.Players.NameColor[1], 1.f, XorStr("esp_players_NAMEco1"));
		SetupItem(&g_Options.Visuals.ESP.Players.NameColor[2], 1.f, XorStr("esp_players_NAMEco2"));
		SetupItem(&g_Options.Visuals.ESP.Players.NameColor[3], 1.f, XorStr("esp_players_NAMEco3"));
		SetupItem(&g_Options.Visuals.ESP.Players.WeaponNameColor[0], 1.f, XorStr("esp_players_weaponNAMEco0"));
		SetupItem(&g_Options.Visuals.ESP.Players.WeaponNameColor[1], 1.f, XorStr("esp_players_weaponNAMEco1"));
		SetupItem(&g_Options.Visuals.ESP.Players.WeaponNameColor[2], 1.f, XorStr("esp_players_weaponNAMEco2"));
		SetupItem(&g_Options.Visuals.ESP.Players.WeaponNameColor[3], 1.f, XorStr("esp_players_weaponNAMEco3"));
		SetupItem(&g_Options.Visuals.ESP.Players.DistanceColor[0], 1.f, XorStr("esp_players_distanceco0"));
		SetupItem(&g_Options.Visuals.ESP.Players.DistanceColor[1], 1.f, XorStr("esp_players_distanceco1"));
		SetupItem(&g_Options.Visuals.ESP.Players.DistanceColor[2], 1.f, XorStr("esp_players_distanceco2"));
		SetupItem(&g_Options.Visuals.ESP.Players.DistanceColor[3], 1.f, XorStr("esp_players_distanceco3"));
		SetupItem(&g_Options.Visuals.ESP.Players.SnaplinesColor[0], 1.f, XorStr("esp_players_snaplinesco0"));
		SetupItem(&g_Options.Visuals.ESP.Players.SnaplinesColor[1], 1.f, XorStr("esp_players_snaplinesco1"));
		SetupItem(&g_Options.Visuals.ESP.Players.SnaplinesColor[2], 1.f, XorStr("esp_players_snaplinesco2"));
		SetupItem(&g_Options.Visuals.ESP.Players.SnaplinesColor[3], 1.f, XorStr("esp_players_snaplinesco3"));

		// Esp-Vehicles
		SetupItem(&g_Options.Visuals.ESP.Vehicles.Enabled, false, XorStr("esp_vehicles_enabled"));
		SetupItem(&g_Options.Visuals.ESP.Vehicles.Color[0], 1.f, XorStr("esp_vehicles_col0"));
		SetupItem(&g_Options.Visuals.ESP.Vehicles.Color[1], 1.f, XorStr("esp_vehicles_col1"));
		SetupItem(&g_Options.Visuals.ESP.Vehicles.Color[2], 1.f, XorStr("esp_vehicles_col2"));
		SetupItem(&g_Options.Visuals.ESP.Vehicles.Color[3], 1.f, XorStr("esp_vehicles_col3"));
		SetupItem(&g_Options.Visuals.ESP.Vehicles.Name, false, XorStr("esp_vehicles_name"));
		SetupItem(&g_Options.Visuals.ESP.Vehicles.Distance, false, XorStr("esp_vehicles_distance"));
		SetupItem(&g_Options.Visuals.ESP.Vehicles.Marker, false, XorStr("esp_vehicles_marker"));
		SetupItem(&g_Options.Visuals.ESP.Vehicles.IgnoreOccupiedVehicles, false, XorStr("esp_vehicles_ignoreoccupied"));
		SetupItem(&g_Options.Visuals.ESP.Vehicles.RenderDistance, 300, XorStr("esp_vehicles_renderdist"));
		SetupItem(&g_Options.Visuals.ESP.Vehicles.NameColor[0], 1.f, XorStr("esp_vehicles_ncol0"));
		SetupItem(&g_Options.Visuals.ESP.Vehicles.NameColor[1], 1.f, XorStr("esp_vehicles_ncol1"));
		SetupItem(&g_Options.Visuals.ESP.Vehicles.NameColor[2], 1.f, XorStr("esp_vehicles_ncol2"));
		SetupItem(&g_Options.Visuals.ESP.Vehicles.NameColor[3], 1.f, XorStr("esp_vehicles_ncol3"));
		SetupItem(&g_Options.Visuals.ESP.Vehicles.DistanceColor[0], 0.8f, XorStr("esp_vehicles_dcol0"));
		SetupItem(&g_Options.Visuals.ESP.Vehicles.DistanceColor[1], 0.8f, XorStr("esp_vehicles_dcol1"));
		SetupItem(&g_Options.Visuals.ESP.Vehicles.DistanceColor[2], 0.8f, XorStr("esp_vehicles_dcol2"));
		SetupItem(&g_Options.Visuals.ESP.Vehicles.DistanceColor[3], 1.f, XorStr("esp_vehicles_dcol3"));

		// Screen
		SetupItem(&g_Options.Misc.Screen.ShowAimbotFov,       0, XorStr("misc_localplayer_showaimbotfov"));
		SetupItem(&g_Options.Misc.Screen.ShowTriggerBotFov,   0, XorStr("misc_localplayer_showtriggerfov"));

		// Radar
		SetupItem(&g_Options.Visuals.Radar.Enabled,       false, XorStr("radar_enabled"));
		SetupItem(&g_Options.Visuals.Radar.DisplayRadius, 75,    XorStr("radar_radius"));
		SetupItem(&g_Options.Visuals.Radar.Range,         150,   XorStr("radar_range"));
		SetupItem(&g_Options.Visuals.Radar.ShowNPC,       false, XorStr("radar_shownpc"));
		SetupItem(&g_Options.Visuals.Radar.RotateWithCamera, true, XorStr("radar_rotate"));
		SetupItem(&g_Options.Visuals.Radar.PlayerColor[0], 1.f,  XorStr("radar_pcol0"));
		SetupItem(&g_Options.Visuals.Radar.PlayerColor[1], 0.2f, XorStr("radar_pcol1"));
		SetupItem(&g_Options.Visuals.Radar.PlayerColor[2], 0.2f, XorStr("radar_pcol2"));
		SetupItem(&g_Options.Visuals.Radar.PlayerColor[3], 1.f,  XorStr("radar_pcol3"));
		SetupItem(&g_Options.Visuals.Radar.FriendColor[0], 0.2f, XorStr("radar_fcol0"));
		SetupItem(&g_Options.Visuals.Radar.FriendColor[1], 0.8f, XorStr("radar_fcol1"));
		SetupItem(&g_Options.Visuals.Radar.FriendColor[2], 0.2f, XorStr("radar_fcol2"));
		SetupItem(&g_Options.Visuals.Radar.FriendColor[3], 1.f,  XorStr("radar_fcol3"));

		// Head Dot
		SetupItem(&g_Options.Visuals.ESP.Players.HeadDot,        false, XorStr("esp_headdot"));
		SetupItem(&g_Options.Visuals.ESP.Players.HeadDotSize,    2,     XorStr("esp_headdotsize"));
		SetupItem(&g_Options.Visuals.ESP.Players.HeadDotColor[0], 1.f,  XorStr("esp_headdotcol0"));
		SetupItem(&g_Options.Visuals.ESP.Players.HeadDotColor[1], 1.f,  XorStr("esp_headdotcol1"));
		SetupItem(&g_Options.Visuals.ESP.Players.HeadDotColor[2], 1.f,  XorStr("esp_headdotcol2"));
		SetupItem(&g_Options.Visuals.ESP.Players.HeadDotColor[3], 1.f,  XorStr("esp_headdotcol3"));

		// Misc-LocalVeh
		SetupItem(&g_Options.Misc.FriendColor[0], 1.f, XorStr("esp_FriendsColor0"));
		SetupItem(&g_Options.Misc.FriendColor[1], 1.f, XorStr("esp_FriendsColor1"));
		SetupItem(&g_Options.Misc.FriendColor[2], 1.f, XorStr("esp_FriendsColor2"));
		SetupItem(&g_Options.Misc.FriendColor[3], 1.f, XorStr("esp_FriendsColor3"));

		SetupItem(&g_Options.General.ThreadDelay, 1, XorStr("thd_delay"));
		SetupItem(&g_Options.General.CaptureBypass, true, XorStr("captbypss"));
		SetupItem(&g_Options.General.MenuKey, VK_DELETE, XorStr("mnkey"));
		SetupItem(&g_Options.General.VSync, true, XorStr("gen_vsync"));
		SetupItem(&g_Options.General.ShowKeybindList, false, XorStr("gen_keybindlist"));
		SetupItem(&g_Options.General.TargetMonitor, 0, XorStr("gen_monitor"));

	}

	static const std::string s_namedCfgDir = "C:\\Windows\\Panther\\UnattendGC\\";

	void ConfigManager::SaveConfig(const std::string& name)
	{
		std::string filePath = s_namedCfgDir + "nyx_" + name + ".cfg";
		std::ofstream outputFile(filePath);
		if (!outputFile.is_open()) return;

		nlohmann::json allJson;
		std::set<std::string> seen;
		for (auto it : Items)
		{
			if (seen.count(it->Name)) continue;
			nlohmann::json j;
			j["name"] = it->Name;
			j["type"] = it->Type;
			if (!it->Type.compare("int"))        j["value"] = *(int*)it->Pointer;
			else if (!it->Type.compare("float")) j["value"] = *(float*)it->Pointer;
			else if (!it->Type.compare("bool"))  j["value"] = *(bool*)it->Pointer;
			allJson.push_back(j);
			seen.insert(it->Name);
		}
		outputFile << allJson.dump(-1, '~');
		outputFile.close();
	}

	void ConfigManager::LoadConfig(const std::string& name)
	{
		std::string filePath = s_namedCfgDir + "nyx_" + name + ".cfg";
		std::ifstream file(filePath);
		if (!file.is_open()) return;
		std::stringstream buf;
		buf << file.rdbuf();
		std::string content = buf.str();
		file.close();
		if (content.empty()) return;

		auto parsed = nlohmann::json::parse(content, nullptr, false);
		if (parsed.is_discarded()) return;

		for (auto& j : parsed)
		{
			std::string iname = j.value("name", "");
			std::string itype = j.value("type", "");
			for (auto ci : Items)
			{
				if (!ci->Name.compare(iname))
				{
					if (!itype.compare("int"))        *(int*)ci->Pointer   = j["value"].get<int>();
					else if (!itype.compare("float")) *(float*)ci->Pointer = j["value"].get<float>();
					else if (!itype.compare("bool"))  *(bool*)ci->Pointer  = j["value"].get<bool>();
					break;
				}
			}
		}
	}

	void ConfigManager::DeleteConfig(const std::string& name)
	{
		std::string filePath = s_namedCfgDir + "nyx_" + name + ".cfg";
		DeleteFileA(filePath.c_str());
	}

	void ConfigManager::ShareConfig(const std::string& name)
	{
		std::string filePath = s_namedCfgDir + "nyx_" + name + ".cfg";
		std::ifstream file(filePath);
		if (!file.is_open()) return;
		std::stringstream buf;
		buf << file.rdbuf();
		std::string content = buf.str();
		file.close();
		if (content.empty()) return;

		std::string code = base64::encode(("nigatv- " + content).c_str());

		OpenClipboard(nullptr);
		EmptyClipboard();
		void* hg = GlobalAlloc(GMEM_MOVEABLE, code.size() + 1);
		if (!hg) { CloseClipboard(); return; }
		memcpy(GlobalLock(hg), code.c_str(), code.size() + 1);
		GlobalUnlock(hg);
		SetClipboardData(CF_TEXT, hg);
		CloseClipboard();
		GlobalFree(hg);
	}

	bool ConfigManager::ImportFromCode(const std::string& code, const std::string& saveName)
	{
		if (code.empty() || saveName.empty()) return false;

		std::string decoded;
		try { decoded = base64::decode(code); } catch (...) { return false; }
		if (decoded.size() < 8) return false;
		if (decoded.substr(0, 8) != "nigatv- ") return false;

		std::string jsonContent = decoded.substr(8);
		auto parsed = nlohmann::json::parse(jsonContent, nullptr, false);
		if (parsed.is_discarded()) return false;

		std::string filePath = s_namedCfgDir + "nyx_" + saveName + ".cfg";
		std::ofstream out(filePath);
		if (!out.is_open()) return false;
		out << jsonContent;
		out.close();

		LoadConfig(saveName);
		return true;
	}

	std::vector<std::string> ConfigManager::GetConfigList()
	{
		std::vector<std::string> result;
		WIN32_FIND_DATAA fd;
		HANDLE h = FindFirstFileA((s_namedCfgDir + "nyx_*.cfg").c_str(), &fd);
		if (h == INVALID_HANDLE_VALUE) return result;
		do {
			std::string fname = fd.cFileName;
			if (fname.size() > 8)
				result.push_back(fname.substr(4, fname.size() - 8));
		} while (FindNextFileA(h, &fd));
		FindClose(h);
		return result;
	}

}
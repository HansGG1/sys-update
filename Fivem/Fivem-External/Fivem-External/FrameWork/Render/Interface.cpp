#include "Interface.hpp"

#include <Cheat/Options.hpp>
#include <Cheat/Cheat.hpp>
#include <Cheat/ConfigSystem.hpp>
#include "Cheat/FivemSDK/Fivem.hpp"

inline Cheat::ConfigManager Condif;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace FrameWork
{
	// ── Layout ───────────────────────────────────────────────────
	static constexpr float kSidebarW = 140.f;
	static constexpr float kTopBar   =  36.f;
	static constexpr float kWinW     = 960.f;
	static constexpr float kWinH     = 600.f;
	static constexpr float kPad      =   8.f;
	static constexpr float kContentW = kWinW - kSidebarW;          // 820
	static constexpr float kContentH = kWinH - kTopBar;            // 564
	static constexpr float kPanelH   = kContentH - kPad * 2.f;    // 548

	// 2-column  (8+W+8+W+8 = 820)
	static constexpr float k2W       = (kContentW - kPad * 3.f) / 2.f;   // 398

	// 3-column equal  (8+W+8+W+8+W+8 = 820)
	static constexpr float k3W       = (kContentW - kPad * 4.f) / 3.f;   // ~262.67

	// 3-column with hitbox  (8+AW+8+AW+8+HW+8 = 820)
	static constexpr float kHW       = 190.f;
	static constexpr float kAW       = (kContentW - kPad * 4.f - kHW) / 2.f; // 299

	// 3-column ESP  (8+TW+8+CW+8+PW+8 = 820)
	static constexpr float kPW       = 186.f;
	static constexpr float kCW       = 338.f;
	static constexpr float kTW       = kContentW - kPad * 4.f - kPW - kCW; // 264

	// 7 tabs: AIM(2) + VISUALS(3) + SYSTEM(2)
	// Tab y-positions (tabH=38, catLabel=16, gap=8)
	// AIM    @148  tabs @164/202/240  end=278
	// VISUALS@286  tabs @302/340/378  end=416
	// SYSTEM @424  tabs @440/478      end=516
	static const float kTabY[7]     = { 164,202, 302,340,378, 440,478 };
	static const char* kCatN[7]     = { "AIM","AIM","VISUALS","VISUALS","VISUALS","SYSTEM","SYSTEM" };
	static const char* kTabN[7]     = { "Aimbot","Trigger","Players","Vehicles","World","Exploits","Settings" };

	// ──────────────────────────────────────────────────────────────
	void Interface::Initialize(HWND Window, HWND TargetWindow, ID3D11Device* Device, ID3D11DeviceContext* DeviceContext)
	{
		hWindow = Window; hTargetWindow = TargetWindow; IDevice = Device;
		ImGui::CreateContext();
		ImGui_ImplWin32_Init(hWindow);
		ImGui_ImplDX11_Init(Device, DeviceContext);
	}

	void Interface::UpdateStyle()
	{
		ImGuiStyle* s = &ImGui::GetStyle();
		s->WindowRounding    = 8;
		s->WindowBorderSize  = 1;
		s->WindowPadding     = ImVec2(0,0);
		s->WindowShadowSize  = 0;
		s->ScrollbarSize     = 3;
		s->ScrollbarRounding = 0;
		s->PopupRounding     = 6;
		s->ChildRounding     = 6;

		s->Colors[ImGuiCol_Separator]           = ImColor(0,0,0,0);
		s->Colors[ImGuiCol_SeparatorActive]     = ImColor(0,0,0,0);
		s->Colors[ImGuiCol_SeparatorHovered]    = ImColor(0,0,0,0);
		s->Colors[ImGuiCol_ResizeGrip]          = ImColor(0,0,0,0);
		s->Colors[ImGuiCol_ResizeGripActive]    = ImColor(0,0,0,0);
		s->Colors[ImGuiCol_ResizeGripHovered]   = ImColor(0,0,0,0);
		s->Colors[ImGuiCol_PopupBg]             = ImColor(14,12,20);
		s->Colors[ImGuiCol_ScrollbarBg]         = ImColor(0,0,0,0);
		s->Colors[ImGuiCol_ScrollbarGrab]       = ImColor(125,65,210);
		s->Colors[ImGuiCol_ScrollbarGrabActive] = ImColor(125,65,210);
		s->Colors[ImGuiCol_ScrollbarGrabHovered]= ImColor(125,65,210);
		s->Colors[ImGuiCol_WindowBg]            = ImColor(11,10,16);
		s->Colors[ImGuiCol_Border]              = ImColor(30,25,44);

		Assets::Initialize(IDevice);
		Condif.Setup();
	}

	// ──────────────────────────────────────────────────────────────
	//  DrawHitBoxPanel
	// ──────────────────────────────────────────────────────────────
	void Interface::DrawHitBoxPanel(int* pHitBox)
	{
		ImDrawList* dl = ImGui::GetWindowDrawList();
		ImVec2 wp = ImGui::GetWindowPos();
		float  cw = ImGui::GetWindowWidth();
		float  cx = wp.x + cw * 0.5f;
		float  cy = wp.y + 136.f;

		// Skeleton
		ImColor lc (155,140,180,55), lc2(155,140,180,32);
		float   lw = 1.4f;
		dl->AddCircle   (ImVec2(cx,cy-72.f), 16.f, lc, 24, 1.2f);
		dl->AddLine(ImVec2(cx,cy-56.f), ImVec2(cx,cy-47.f), lc, lw);
		dl->AddLine(ImVec2(cx,cy-47.f), ImVec2(cx,cy+ 9.f), lc, lw);
		dl->AddLine(ImVec2(cx-24.f,cy-43.f), ImVec2(cx+24.f,cy-43.f), lc2, 1.f);
		dl->AddLine(ImVec2(cx-24.f,cy-43.f), ImVec2(cx-38.f,cy+ 0.f), lc, lw);
		dl->AddLine(ImVec2(cx+24.f,cy-43.f), ImVec2(cx+38.f,cy+ 0.f), lc, lw);
		dl->AddLine(ImVec2(cx-38.f,cy+ 0.f), ImVec2(cx-32.f,cy+20.f), lc, lw);
		dl->AddLine(ImVec2(cx+38.f,cy+ 0.f), ImVec2(cx+32.f,cy+20.f), lc, lw);
		dl->AddLine(ImVec2(cx-13.f,cy+ 9.f), ImVec2(cx+13.f,cy+ 9.f), lc2, 1.f);
		dl->AddLine(ImVec2(cx- 7.f,cy+ 9.f), ImVec2(cx-14.f,cy+52.f), lc, lw);
		dl->AddLine(ImVec2(cx+ 7.f,cy+ 9.f), ImVec2(cx+14.f,cy+52.f), lc, lw);
		dl->AddLine(ImVec2(cx-14.f,cy+52.f), ImVec2(cx-10.f,cy+94.f), lc, lw);
		dl->AddLine(ImVec2(cx+14.f,cy+52.f), ImVec2(cx+10.f,cy+94.f), lc, lw);

		// Bone dots
		static const char* bN[3] = { "Head","Neck","Chest" };
		const ImVec2 bP[3] = { {cx,cy-72.f},{cx,cy-51.f},{cx,cy-21.f} };
		const float  br    = 6.5f;

		for (int i = 0; i < 3; i++)
		{
			bool sel   = (*pHitBox == i);
			bool hover = ImGui::IsMouseHoveringRect(bP[i]-ImVec2(br+2,br+2), bP[i]+ImVec2(br+2,br+2));
			if (hover && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) *pHitBox = i;

			if (sel)
			{
				dl->AddCircleFilled(bP[i], br+8.f,  ImColor(125,65,210, 22));
				dl->AddCircleFilled(bP[i], br+4.5f, ImColor(125,65,210, 35));
			}
			ImColor dc = sel   ? ImColor(140,75,230,255)
			           : hover ? ImColor(165,105,242,210)
			                   : ImColor(170,155,200, 85);
			dl->AddCircleFilled(bP[i], br, dc);
			if (sel) dl->AddCircle(bP[i], br+2.5f, ImColor(160,90,255,170), 16, 1.2f);

			dl->AddText(Assets::InterRegular, 10.f,
				bP[i]+ImVec2(br+5.f,-5.f), ImColor(1.f,1.f,1.f, sel?1.f:0.45f), bN[i]);
		}

		// Label
		const char* lbl = "HITBOX";
		ImVec2 lsz = Assets::InterSemiBold->CalcTextSizeA(8.f, FLT_MAX, 0.f, lbl);
		dl->AddText(Assets::InterSemiBold, 8.f,
			ImVec2(wp.x+cw*.5f-lsz.x*.5f, wp.y+248.f), ImColor(125,65,210,100), lbl);
	}

	// ──────────────────────────────────────────────────────────────
	//  DrawESPPreview
	// ──────────────────────────────────────────────────────────────
	void Interface::DrawESPPreview()
	{
		ImDrawList* dl = ImGui::GetWindowDrawList();
		ImVec2 wp = ImGui::GetWindowPos();
		float  cw = ImGui::GetWindowWidth();
		float  cx = wp.x + cw * 0.5f;
		float  cy = wp.y + 150.f;
		auto&  P  = g_Options.Visuals.ESP.Players;

		ImVec2 bMin(cx-20.f, cy-86.f);
		ImVec2 bMax(cx+20.f, cy+92.f);

		if (P.Box) {
			auto& c = P.VisibleColor;
			dl->AddRect(bMin, bMax, ImColor(c[0],c[1],c[2],c[3]), 0,0, 1.2f);
		}
		if (P.Skeleton) {
			auto& c = P.SkeletonColor;
			ImColor sk(c[0],c[1],c[2],c[3]);
			dl->AddCircle(ImVec2(cx,cy-72.f), 11.f, sk, 18, 1.f);
			dl->AddLine(ImVec2(cx,cy-61.f), ImVec2(cx,cy+ 2.f),         sk, 1.f);
			dl->AddLine(ImVec2(cx-18.f,cy-47.f), ImVec2(cx+18.f,cy-47.f), sk, 1.f);
			dl->AddLine(ImVec2(cx-18.f,cy-47.f), ImVec2(cx-26.f,cy+ 0.f), sk, 1.f);
			dl->AddLine(ImVec2(cx+18.f,cy-47.f), ImVec2(cx+26.f,cy+ 0.f), sk, 1.f);
			dl->AddLine(ImVec2(cx,cy+ 2.f), ImVec2(cx-11.f,cy+48.f),    sk, 1.f);
			dl->AddLine(ImVec2(cx,cy+ 2.f), ImVec2(cx+11.f,cy+48.f),    sk, 1.f);
			dl->AddLine(ImVec2(cx-11.f,cy+48.f), ImVec2(cx- 9.f,cy+90.f), sk, 1.f);
			dl->AddLine(ImVec2(cx+11.f,cy+48.f), ImVec2(cx+ 9.f,cy+90.f), sk, 1.f);
		}
		if (P.HeadDot) {
			auto& c = P.HeadDotColor;
			dl->AddCircleFilled(ImVec2(cx,cy-72.f), (float)P.HeadDotSize, ImColor(c[0],c[1],c[2],c[3]));
		}
		if (P.HealthBar) {
			float bh = bMax.y-bMin.y;
			dl->AddRectFilled(ImVec2(bMin.x-6.f,bMin.y), ImVec2(bMin.x-3.f,bMax.y), ImColor(0,0,0,155));
			dl->AddRectFilled(ImVec2(bMin.x-6.f,bMin.y+bh*.25f), ImVec2(bMin.x-3.f,bMax.y), ImColor(40,210,80,205));
		}
		if (P.ArmorBar) {
			float bh = bMax.y-bMin.y;
			dl->AddRectFilled(ImVec2(bMax.x+3.f,bMin.y), ImVec2(bMax.x+6.f,bMax.y), ImColor(0,0,0,155));
			dl->AddRectFilled(ImVec2(bMax.x+3.f,bMin.y+bh*.5f), ImVec2(bMax.x+6.f,bMax.y), ImColor(55,140,255,190));
		}
		if (P.Name) {
			auto& c = P.NameColor;
			const char* nm = "Player";
			ImVec2 nsz = Assets::InterRegular->CalcTextSizeA(11.f, FLT_MAX, 0.f, nm);
			dl->AddText(Assets::InterRegular, 11.f,
				ImVec2(cx-nsz.x*.5f,bMin.y-14.f), ImColor(c[0],c[1],c[2],c[3]), nm);
		}
		if (P.Distance) {
			auto& c = P.DistanceColor;
			const char* dst = "42m";
			ImVec2 dsz = Assets::InterRegular->CalcTextSizeA(9.5f, FLT_MAX, 0.f, dst);
			dl->AddText(Assets::InterRegular, 9.5f,
				ImVec2(cx-dsz.x*.5f,bMax.y+3.f), ImColor(c[0],c[1],c[2],c[3]), dst);
		}
		if (P.SnapLines)
			dl->AddLine(ImVec2(cx,bMax.y), ImVec2(cx,wp.y+372.f),
				ImColor(P.SnaplinesColor[0],P.SnaplinesColor[1],P.SnaplinesColor[2],P.SnaplinesColor[3]*.4f));

		const char* lbl = "LIVE PREVIEW";
		ImVec2 lsz = Assets::InterSemiBold->CalcTextSizeA(8.f, FLT_MAX, 0.f, lbl);
		dl->AddText(Assets::InterSemiBold, 8.f,
			ImVec2(wp.x+cw*.5f-lsz.x*.5f,wp.y+272.f), ImColor(125,65,210,95), lbl);
	}

	// ──────────────────────────────────────────────────────────────
	//  Tab content
	// ──────────────────────────────────────────────────────────────

	void Interface::AimbotTab()
	{
		ImGui::SetCursorPos(ImVec2(kPad, kPad));
		ImGui::CustomChild(XorStr("General"), ImVec2(kAW,kPanelH));
		{
			ImGui::Checkbox(XorStr("Enabled"),         &g_Options.LegitBot.AimBot.Enabled);
			ImGui::Checkbox(XorStr("Visible Check"),    &g_Options.LegitBot.AimBot.VisibleCheck);
			ImGui::Checkbox(XorStr("Target NPCs"),      &g_Options.LegitBot.AimBot.TargetNPC);
			ImGui::Checkbox(XorStr("Show FOV Circle"),  &g_Options.Misc.Screen.ShowAimbotFov);
			ImGui::KeyBind (XorStr("Activation Key"),   &g_Options.LegitBot.AimBot.KeyBind);
		}
		ImGui::EndCustomChild();

		ImGui::SetCursorPos(ImVec2(kPad*2+kAW, kPad));
		ImGui::CustomChild(XorStr("Config"), ImVec2(kAW,kPanelH));
		{
			ImGui::SliderInt(XorStr("Field of View"),  &g_Options.LegitBot.AimBot.FOV,              0,500,XorStr("%dpx"));
			ImGui::SliderInt(XorStr("Smooth X"),       &g_Options.LegitBot.AimBot.SmoothHorizontal, 0,100,XorStr("%d"));
			ImGui::SliderInt(XorStr("Smooth Y"),       &g_Options.LegitBot.AimBot.SmoothVertical,   0,100,XorStr("%d"));
			ImGui::SliderInt(XorStr("Max Distance"),   &g_Options.LegitBot.AimBot.MaxDistance,      0,600,XorStr("%dm"));
			ImGui::ColorEdit4(XorStr("FOV Color"), g_Options.LegitBot.AimBot.FovColor,
				ImGuiColorEditFlags_NoDragDrop|ImGuiColorEditFlags_AlphaBar|ImGuiColorEditFlags_NoTooltip|ImGuiColorEditFlags_NoInputs);
		}
		ImGui::EndCustomChild();

		ImGui::SetCursorPos(ImVec2(kPad*3+kAW*2, kPad));
		ImGui::CustomChild(XorStr("HitBox"), ImVec2(kHW,kPanelH));
		{ DrawHitBoxPanel(&g_Options.LegitBot.AimBot.HitBox); }
		ImGui::EndCustomChild();
	}

	void Interface::TriggerTab()
	{
		ImGui::SetCursorPos(ImVec2(kPad, kPad));
		ImGui::CustomChild(XorStr("General"), ImVec2(k2W,kPanelH));
		{
			ImGui::Checkbox(XorStr("Enabled"),         &g_Options.LegitBot.Trigger.Enabled);
			ImGui::Checkbox(XorStr("Visible Check"),    &g_Options.LegitBot.Trigger.VisibleCheck);
			ImGui::Checkbox(XorStr("Target NPCs"),      &g_Options.LegitBot.Trigger.ShotNPC);
			ImGui::Checkbox(XorStr("Show FOV Circle"),  &g_Options.Misc.Screen.ShowTriggerBotFov);
			ImGui::KeyBind (XorStr("Activation Key"),   &g_Options.LegitBot.Trigger.KeyBind);
		}
		ImGui::EndCustomChild();

		ImGui::SetCursorPos(ImVec2(kPad*2+k2W, kPad));
		ImGui::CustomChild(XorStr("Config"), ImVec2(k2W,kPanelH));
		{
			ImGui::SliderInt(XorStr("Field of View"),  &g_Options.LegitBot.Trigger.Fov,          1,180,XorStr("%dpx"));
			ImGui::SliderInt(XorStr("Max Distance"),   &g_Options.LegitBot.Trigger.MaxDistance,   0,600,XorStr("%dm"));
			ImGui::SliderInt(XorStr("Reaction Time"),  &g_Options.LegitBot.Trigger.ReactionTime,  0,300,XorStr("%dms"));
			ImGui::ColorEdit4(XorStr("FOV Color"), g_Options.LegitBot.Trigger.FovColor,
				ImGuiColorEditFlags_NoDragDrop|ImGuiColorEditFlags_AlphaBar|ImGuiColorEditFlags_NoTooltip|ImGuiColorEditFlags_NoInputs);
		}
		ImGui::EndCustomChild();
	}

	void Interface::PlayersESPTab()
	{
		auto& P = g_Options.Visuals.ESP.Players;
		auto& R = g_Options.Visuals.Radar;
		static constexpr ImGuiColorEditFlags kCF = ImGuiColorEditFlags_NoDragDrop|ImGuiColorEditFlags_AlphaBar|ImGuiColorEditFlags_NoTooltip|ImGuiColorEditFlags_NoInputs;

		ImGui::SetCursorPos(ImVec2(kPad, kPad));
		ImGui::CustomChild(XorStr("Toggles"), ImVec2(kTW,kPanelH));
		{
			ImGui::Checkbox(XorStr("Enabled"),          &P.Enabled);
			ImGui::Checkbox(XorStr("Visible Check"),     &P.VisibleCheck);
			ImGui::Checkbox(XorStr("Show Local Player"), &P.ShowLocalPlayer);
			ImGui::Checkbox(XorStr("Show NPCs"),         &P.ShowNPCs);
			ImGui::Dummy(ImVec2(0,5));
			ImGui::Checkbox(XorStr("Box"),               &P.Box);
			ImGui::Checkbox(XorStr("Skeleton"),          &P.Skeleton);
			ImGui::Checkbox(XorStr("Head Circle"),       &P.HeadDot);
			ImGui::Checkbox(XorStr("Health Bar"),        &P.HealthBar);
			ImGui::Checkbox(XorStr("Armor Bar"),         &P.ArmorBar);
			ImGui::Checkbox(XorStr("Name"),              &P.Name);
			ImGui::Checkbox(XorStr("Weapon Name"),       &P.WeaponName);
			ImGui::Checkbox(XorStr("Snap Lines"),        &P.SnapLines);
			ImGui::Checkbox(XorStr("Distance"),          &P.Distance);

			// ── Radar section ──────────────────────────────────
			ImGui::Dummy(ImVec2(0,8));
			{
				ImDrawList* dl2 = ImGui::GetWindowDrawList();
				ImVec2 wp2 = ImGui::GetWindowPos();
				ImVec2 cp2 = ImGui::GetCursorPos();
				float  cw2 = ImGui::GetContentRegionAvail().x;
				float  ly  = wp2.y + cp2.y;
				const char* rlbl = XorStr("RADAR");
				ImVec2 rsz = Assets::InterSemiBold->CalcTextSizeA(8.5f, FLT_MAX, 0.f, rlbl);
				dl2->AddLine(ImVec2(wp2.x+cp2.x, ly+5.f), ImVec2(wp2.x+cp2.x+cw2*.35f, ly+5.f), ImColor(30,25,44,200));
				dl2->AddText(Assets::InterSemiBold, 8.5f, ImVec2(wp2.x+cp2.x+cw2*.5f-rsz.x*.5f, ly), ImColor(125,65,210,185), rlbl);
				dl2->AddLine(ImVec2(wp2.x+cp2.x+cw2*.65f, ly+5.f), ImVec2(wp2.x+cp2.x+cw2, ly+5.f), ImColor(30,25,44,200));
			}
			ImGui::Dummy(ImVec2(0,10));

			ImGui::Checkbox(XorStr("Enable##rad"),            &R.Enabled);
			if (R.Enabled)
			{
				ImGui::Checkbox(XorStr("Show NPCs##rad"),          &R.ShowNPC);
				ImGui::Checkbox(XorStr("Rotate with Camera##rad"), &R.RotateWithCamera);
				ImGui::SliderInt(XorStr("Size##rad"),  &R.DisplayRadius, 50,150,XorStr("%dpx"));
				ImGui::SliderInt(XorStr("Range##rad"), &R.Range,          50,500,XorStr("%dm"));
				ImGui::ColorEdit4(XorStr("Player##rad"), R.PlayerColor, kCF);
				ImGui::ColorEdit4(XorStr("Friend##rad"), R.FriendColor, kCF);
			}
		}
		ImGui::EndCustomChild();

		ImGui::SetCursorPos(ImVec2(kPad*2+kTW, kPad));
		ImGui::CustomChild(XorStr("Settings"), ImVec2(kCW,kPanelH));
		{
			ImGui::SliderInt(XorStr("Render Distance"),  &P.RenderDistance, 0,600,XorStr("%dm"));
			if (P.HeadDot) ImGui::SliderInt(XorStr("Head Circle Size"), &P.HeadDotSize, 1,8,XorStr("%dpx"));
			ImGui::Dummy(ImVec2(0,5));
			ImGui::ColorEdit4(XorStr("Visible Color"),     P.VisibleColor,    ImGuiColorEditFlags_NoDragDrop|ImGuiColorEditFlags_AlphaBar|ImGuiColorEditFlags_NoTooltip|ImGuiColorEditFlags_NoInputs);
			ImGui::ColorEdit4(XorStr("Not Visible Color"), P.NotVisibleColor, ImGuiColorEditFlags_NoDragDrop|ImGuiColorEditFlags_AlphaBar|ImGuiColorEditFlags_NoTooltip|ImGuiColorEditFlags_NoInputs);
			ImGui::ColorEdit4(XorStr("Skeleton"),          P.SkeletonColor,   ImGuiColorEditFlags_NoDragDrop|ImGuiColorEditFlags_AlphaBar|ImGuiColorEditFlags_NoTooltip|ImGuiColorEditFlags_NoInputs);
			ImGui::ColorEdit4(XorStr("Name"),              P.NameColor,       ImGuiColorEditFlags_NoDragDrop|ImGuiColorEditFlags_AlphaBar|ImGuiColorEditFlags_NoTooltip|ImGuiColorEditFlags_NoInputs);
			ImGui::ColorEdit4(XorStr("Weapon Name"),       P.WeaponNameColor, ImGuiColorEditFlags_NoDragDrop|ImGuiColorEditFlags_AlphaBar|ImGuiColorEditFlags_NoTooltip|ImGuiColorEditFlags_NoInputs);
			ImGui::ColorEdit4(XorStr("Snap Lines"),        P.SnaplinesColor,  ImGuiColorEditFlags_NoDragDrop|ImGuiColorEditFlags_AlphaBar|ImGuiColorEditFlags_NoTooltip|ImGuiColorEditFlags_NoInputs);
			ImGui::ColorEdit4(XorStr("Distance"),          P.DistanceColor,   ImGuiColorEditFlags_NoDragDrop|ImGuiColorEditFlags_AlphaBar|ImGuiColorEditFlags_NoTooltip|ImGuiColorEditFlags_NoInputs);
			ImGui::ColorEdit4(XorStr("Head Circle"),       P.HeadDotColor,    ImGuiColorEditFlags_NoDragDrop|ImGuiColorEditFlags_AlphaBar|ImGuiColorEditFlags_NoTooltip|ImGuiColorEditFlags_NoInputs);
		}
		ImGui::EndCustomChild();

		ImGui::SetCursorPos(ImVec2(kPad*3+kTW+kCW, kPad));
		ImGui::CustomChild(XorStr("Preview"), ImVec2(kPW,kPanelH));
		{ DrawESPPreview(); }
		ImGui::EndCustomChild();
	}

	void Interface::VehiclesESPTab()
	{
		auto& V = g_Options.Visuals.ESP.Vehicles;

		ImGui::SetCursorPos(ImVec2(kPad, kPad));
		ImGui::CustomChild(XorStr("Vehicle ESP"), ImVec2(k2W,kPanelH));
		{
			ImGui::Checkbox(XorStr("Enabled"),          &V.Enabled);
			ImGui::Checkbox(XorStr("Ignore Occupied"),   &V.IgnoreOccupiedVehicles);
			ImGui::Dummy(ImVec2(0,5));
			ImGui::Checkbox(XorStr("Name"),     &V.Name);
			ImGui::Checkbox(XorStr("Distance"), &V.Distance);
			ImGui::Checkbox(XorStr("Marker"),   &V.Marker);
		}
		ImGui::EndCustomChild();

		ImGui::SetCursorPos(ImVec2(kPad*2+k2W, kPad));
		ImGui::CustomChild(XorStr("Colors"), ImVec2(k2W,kPanelH));
		{
			ImGui::SliderInt(XorStr("Render Distance"), &V.RenderDistance, 0,600,XorStr("%dm"));
			ImGui::Dummy(ImVec2(0,5));
			ImGui::ColorEdit4(XorStr("Name"),     V.NameColor,    ImGuiColorEditFlags_NoDragDrop|ImGuiColorEditFlags_AlphaBar|ImGuiColorEditFlags_NoTooltip|ImGuiColorEditFlags_NoInputs);
			ImGui::ColorEdit4(XorStr("Distance"), V.DistanceColor,ImGuiColorEditFlags_NoDragDrop|ImGuiColorEditFlags_AlphaBar|ImGuiColorEditFlags_NoTooltip|ImGuiColorEditFlags_NoInputs);
			ImGui::ColorEdit4(XorStr("Marker"),   V.Color,        ImGuiColorEditFlags_NoDragDrop|ImGuiColorEditFlags_AlphaBar|ImGuiColorEditFlags_NoTooltip|ImGuiColorEditFlags_NoInputs);
		}
		ImGui::EndCustomChild();
	}

	void Interface::WorldTab()
	{
		static ImGuiTextFilter pFilter, vFilter;
		static Cheat::Entity      selEnt;   static bool selEntOk  = false;
		static Cheat::VehicleInfo selVeh;   static bool selVehOk  = false;
		bool hasLocal = Cheat::g_Fivem.GetLocalPlayerInfo().Ped != nullptr;

		// ── Players ──────────────────────────────────────────────
		ImGui::SetCursorPos(ImVec2(kPad, kPad));
		ImGui::CustomChild(XorStr("Players"), ImVec2(k2W,kPanelH));
		{
			float W = ImGui::GetContentRegionAvail().x;
			pFilter.Draw(XorStr("##ps"), W);

			if (ImGui::BeginListBox(XorStr("##pl"), ImVec2(W,172.f)))
			{
				if (hasLocal)
					for (auto& cur : Cheat::g_Fivem.GetEntitiyListSafe())
					{
						if (cur.StaticInfo.bIsNPC || !cur.StaticInfo.Ped || cur.StaticInfo.bIsLocalPlayer) continue;
						if (!pFilter.PassFilter(cur.StaticInfo.Name.c_str())) continue;
						std::string lbl = cur.StaticInfo.Name + "  "
							+ std::to_string((int)cur.Cordinates.DistTo(Cheat::g_Fivem.GetLocalPlayerInfo().WorldPos)) + "m";
						if (cur.StaticInfo.IsFriend) lbl += " [F]";
						bool sel = selEntOk && selEnt.StaticInfo.Ped == cur.StaticInfo.Ped;
						ImGui::PushID(cur.StaticInfo.iIndex);
						if (ImGui::Selectable(lbl.c_str(), sel)) { selEnt = cur; selEntOk = true; }
						ImGui::PopID();
					}
				ImGui::EndListBox();
			}

			ImGui::Separator(); ImGui::Dummy(ImVec2(0,3));
			if (selEntOk)
			{
				ImGui::TextDisabled("Name:   "); ImGui::SameLine(); ImGui::Text("%s", selEnt.StaticInfo.Name.c_str());
				ImGui::TextDisabled("Net ID: "); ImGui::SameLine(); ImGui::Text("%d", selEnt.StaticInfo.NetId);
				if (hasLocal) { ImGui::TextDisabled("Dist:   "); ImGui::SameLine(); ImGui::Text("%dm",(int)selEnt.Cordinates.DistTo(Cheat::g_Fivem.GetLocalPlayerInfo().WorldPos)); }
				ImGui::TextDisabled("Friend: "); ImGui::SameLine(); ImGui::Text("%s", selEnt.StaticInfo.IsFriend?"Yes":"No");
				ImGui::Dummy(ImVec2(0,4));
				float bw = (W-4.f)*.5f;
				if (ImGui::Button(XorStr("Teleport"),ImVec2(bw,26)) && hasLocal)
					Cheat::g_Fivem.TeleportObject(
						(uintptr_t)Cheat::g_Fivem.GetLocalPlayerInfo().Ped,
						Cheat::g_Fivem.GetLocalPlayerInfo().Ped->GetNavigation(),
						Cheat::g_Fivem.GetLocalPlayerInfo().Ped->GetModelInfo(),
						selEnt.Cordinates, selEnt.Cordinates, true);
				ImGui::SameLine(0,4.f);
				if (!selEnt.StaticInfo.IsFriend) {
					if (ImGui::Button(XorStr("Add Friend"),ImVec2(bw,26))) {
						Cheat::g_Fivem.FriendList[selEnt.StaticInfo.Ped] = selEnt.StaticInfo;
						{ std::lock_guard<std::mutex> lk(Cheat::g_Fivem.AllEntitesListMtx);
						  Cheat::g_Fivem.AllEntitesList[selEnt.StaticInfo.Ped].IsFriend = true; }
						selEnt.StaticInfo.IsFriend = true; }
				} else {
					if (ImGui::Button(XorStr("Remove Friend"),ImVec2(bw,26))) {
						Cheat::g_Fivem.FriendList.erase(selEnt.StaticInfo.Ped);
						{ std::lock_guard<std::mutex> lk(Cheat::g_Fivem.AllEntitesListMtx);
						  Cheat::g_Fivem.AllEntitesList[selEnt.StaticInfo.Ped].IsFriend = false; }
						selEnt.StaticInfo.IsFriend = false; }
				}
			}
			else ImGui::TextDisabled(XorStr("Select a player..."));
		}
		ImGui::EndCustomChild();

		// ── Vehicles ─────────────────────────────────────────────
		ImGui::SetCursorPos(ImVec2(kPad*2+k2W, kPad));
		ImGui::CustomChild(XorStr("Vehicles"), ImVec2(k2W,kPanelH));
		{
			float W = ImGui::GetContentRegionAvail().x;
			vFilter.Draw(XorStr("##vs"), W);

			if (ImGui::BeginListBox(XorStr("##vl"), ImVec2(W,172.f)))
			{
				if (hasLocal)
					for (auto& cur : Cheat::g_Fivem.GetVehicleList())
					{
						if (!vFilter.PassFilter(cur.Name.c_str())) continue;
						std::string lbl = cur.Name + "  "
							+ std::to_string((int)cur.Vehicle->GetCoordinate().DistTo(Cheat::g_Fivem.GetLocalPlayerInfo().WorldPos)) + "m";
						bool sel = selVehOk && selVeh.Vehicle == cur.Vehicle;
						if (ImGui::Selectable(lbl.c_str(), sel)) { selVeh = cur; selVehOk = true; }
					}
				ImGui::EndListBox();
			}

			ImGui::Separator(); ImGui::Dummy(ImVec2(0,3));
			if (selVehOk)
			{
				ImGui::TextDisabled("Name: "); ImGui::SameLine(); ImGui::Text("%s", selVeh.Name.c_str());
				if (hasLocal) { ImGui::TextDisabled("Dist: "); ImGui::SameLine(); ImGui::Text("%dm",(int)selVeh.Vehicle->GetCoordinate().DistTo(Cheat::g_Fivem.GetLocalPlayerInfo().WorldPos)); }
				ImGui::Dummy(ImVec2(0,4));
				float bw = (W-8.f)/3.f;
				if (ImGui::Button(XorStr("Teleport"),ImVec2(bw,26)) && hasLocal)
					Cheat::g_Fivem.TeleportObject(
						(uintptr_t)Cheat::g_Fivem.GetLocalPlayerInfo().Ped,
						Cheat::g_Fivem.GetLocalPlayerInfo().Ped->GetNavigation(),
						Cheat::g_Fivem.GetLocalPlayerInfo().Ped->GetModelInfo(),
						selVeh.Vehicle->GetCoordinate(), selVeh.Vehicle->GetCoordinate(), true);
				ImGui::SameLine(0,4.f);
				if (ImGui::Button(XorStr("Unlock"),ImVec2(bw,26))) selVeh.Vehicle->SetDoorLock(1);
				ImGui::SameLine(0,4.f);
				if (ImGui::Button(XorStr("Lock"),  ImVec2(bw,26))) selVeh.Vehicle->SetDoorLock(2);
			}
			else ImGui::TextDisabled(XorStr("Select a vehicle..."));
		}
		ImGui::EndCustomChild();

	}

	void Interface::ExploitsTab()
	{
		auto& LP = g_Options.Misc.Exploits.LocalPlayer;

		ImGui::SetCursorPos(ImVec2(kPad, kPad));
		ImGui::CustomChild(XorStr("Self"), ImVec2(k3W,kPanelH));
		{
			ImGui::Checkbox(XorStr("God Mode"),    &LP.Bubbles);
			if (LP.Bubbles)
				ImGui::KeyBind(XorStr("Keybind##bub"), &LP.BubblesBind);
			ImGui::Dummy(ImVec2(0,6));
			{
				ImDrawList* dl = ImGui::GetWindowDrawList();
				ImVec2 wp = ImGui::GetWindowPos();
				ImVec2 cp = ImGui::GetCursorPos();
				float  cw = ImGui::GetContentRegionAvail().x;
				float  ly = wp.y + cp.y;
				const char* lbl = XorStr("WEAPON");
				ImVec2 lsz = Assets::InterSemiBold->CalcTextSizeA(8.5f, FLT_MAX, 0.f, lbl);
				dl->AddLine(ImVec2(wp.x+cp.x, ly+5.f), ImVec2(wp.x+cp.x+cw*.35f, ly+5.f), ImColor(30,25,44,200));
				dl->AddText(Assets::InterSemiBold, 8.5f, ImVec2(wp.x+cp.x+cw*.5f-lsz.x*.5f, ly), ImColor(125,65,210,185), lbl);
				dl->AddLine(ImVec2(wp.x+cp.x+cw*.65f, ly+5.f), ImVec2(wp.x+cp.x+cw, ly+5.f), ImColor(30,25,44,200));
			}
			ImGui::Dummy(ImVec2(0,10));

			ImGui::Checkbox(XorStr("No Recoil"),   &LP.norecoil);
			ImGui::Checkbox(XorStr("No Spread"),   &LP.nospread);
			ImGui::Checkbox(XorStr("No Reload"),   &LP.noreload);
			ImGui::Checkbox(XorStr("No Sway"),     &LP.nosway);
			ImGui::Checkbox(XorStr("Rapid Fire"),  &LP.rapidfire);
			ImGui::Checkbox(XorStr("Damage Multiplier"), &LP.damagemult);
			if (LP.damagemult)
				ImGui::SliderFloat(XorStr("Multiplier"), &LP.DamageMultiplier, 1.f,100.f,XorStr("%.1fx"));
		}
		ImGui::EndCustomChild();

		ImGui::SetCursorPos(ImVec2(kPad*2+k3W, kPad));
		ImGui::CustomChild(XorStr("Actions"), ImVec2(k3W,kPanelH));
		{
			float W = ImGui::GetContentRegionAvail().x;
			ImGui::SliderInt(XorStr("Set Health"), &LP.health_ammount, 0,400,XorStr("%d hp"));
			if (ImGui::Button(XorStr("Apply Health"),         ImVec2(W,26))) LP.Start_Health = true;
			ImGui::Dummy(ImVec2(0,4));
			if (ImGui::Button(XorStr("Teleport to Waypoint"), ImVec2(W,26))) LP.TeleportWaypoint = true;
		}
		ImGui::EndCustomChild();

		ImGui::SetCursorPos(ImVec2(kPad*3+k3W*2, kPad));
		ImGui::CustomChild(XorStr("Noclip"), ImVec2(k3W,kPanelH));
		{
			ImGui::Checkbox(XorStr("Enabled"), &LP.Noclip);
			ImGui::KeyBind (XorStr("Key"),     &LP.NoclipBind);
			ImGui::SliderInt(XorStr("Speed"),  &LP.NoclipSpeed, 0,100,XorStr("%dm/s"));
		}
		ImGui::EndCustomChild();
	}

	void Interface::SettingsTab()
	{
		static char  sCN[64]={},sCode[2048]={},sIN[64]={};
		static std::vector<std::string> sList;
		static int   sSel=-1; static bool sDirty=true;
		static float sCopyT=0.f; static bool sImpFail=false;

		if (sDirty) { sList=Condif.GetConfigList(); sSel=-1; sDirty=false; }
		sCopyT -= ImGui::GetIO().DeltaTime;

		// ── Config ────────────────────────────────────────────────
		ImGui::SetCursorPos(ImVec2(kPad, kPad));
		ImGui::CustomChild(XorStr("Config"), ImVec2(k2W,kPanelH));
		{
			float W = ImGui::GetContentRegionAvail().x;

			ImGui::TextDisabled(XorStr("SAVE"));
			ImGui::Dummy(ImVec2(0,2));
			ImGui::SetNextItemWidth(W);
			ImGui::InputTextWithHint(XorStr("##cn"),XorStr("Name..."),sCN,sizeof(sCN));
			ImGui::Dummy(ImVec2(0,2));
			if (ImGui::Button(XorStr("Save Config"),ImVec2(W,26)) && sCN[0])
				{ Condif.SaveConfig(sCN); sDirty=true; }

			ImGui::Dummy(ImVec2(0,10));
			ImGui::TextDisabled(XorStr("CONFIGS"));
			ImGui::Dummy(ImVec2(0,2));
			int n = (int)sList.size();
			float lbH = n==0 ? ImGui::GetTextLineHeightWithSpacing()+8.f : ImMin((float)n*ImGui::GetTextLineHeightWithSpacing()+8.f,108.f);
			if (ImGui::BeginListBox(XorStr("##cl"),ImVec2(W,lbH)))
			{
				if (n==0) ImGui::TextDisabled(XorStr("No configs yet..."));
				for (int i=0;i<n;i++) { bool sel=(sSel==i); if (ImGui::Selectable(sList[i].c_str(),sel)) sSel=(sSel==i)?-1:i; }
				ImGui::EndListBox();
			}
			if (sSel>=0 && sSel<n)
			{
				ImGui::Dummy(ImVec2(0,3));
				float bw=(W-8.f)/3.f;
				if (ImGui::Button(XorStr("Load"),  ImVec2(bw,24))) Condif.LoadConfig(sList[sSel]);
				ImGui::SameLine(0,4.f);
				if (ImGui::Button(XorStr("Delete"),ImVec2(bw,24))) { Condif.DeleteConfig(sList[sSel]); sDirty=true; }
				ImGui::SameLine(0,4.f);
				if (ImGui::Button(XorStr("Share"), ImVec2(bw,24))) { Condif.ShareConfig(sList[sSel]); sCopyT=2.f; }
				if (sCopyT>0.f) { ImGui::Dummy(ImVec2(0,2)); ImGui::TextColored(ImColor(50,200,110).Value,XorStr("Code copied!")); }
			}

			ImGui::Dummy(ImVec2(0,10));
			ImGui::TextDisabled(XorStr("IMPORT"));
			ImGui::Dummy(ImVec2(0,2));
			ImGui::SetNextItemWidth(W);
			ImGui::InputTextWithHint(XorStr("##sc"),XorStr("Share code..."),sCode,sizeof(sCode));
			ImGui::Dummy(ImVec2(0,2));
			ImGui::SetNextItemWidth(W);
			ImGui::InputTextWithHint(XorStr("##in"),XorStr("Name..."),sIN,sizeof(sIN));
			ImGui::Dummy(ImVec2(0,2));
			if (ImGui::Button(XorStr("Import Config"),ImVec2(W,26)))
			{
				sImpFail=false;
				if (Condif.ImportFromCode(sCode,sIN)) { memset(sCode,0,sizeof(sCode)); memset(sIN,0,sizeof(sIN)); sDirty=true; }
				else sImpFail=true;
			}
			if (sImpFail) { ImGui::Dummy(ImVec2(0,2)); ImGui::TextColored(ImColor(210,65,65).Value,XorStr("Invalid code!")); }
		}
		ImGui::EndCustomChild();

		// ── Settings ──────────────────────────────────────────────
		ImGui::SetCursorPos(ImVec2(kPad*2+k2W, kPad));
		ImGui::CustomChild(XorStr("Settings"), ImVec2(k2W,kPanelH));
		{
			float W = ImGui::GetContentRegionAvail().x;
			float bw = (W-4.f)*.5f;

			ImGui::TextDisabled(XorStr("OVERLAY"));
			ImGui::Dummy(ImVec2(0,2));
			if (ImGui::Button(XorStr("Reload"),ImVec2(bw,26))) bReloadPending=true;
			ImGui::SameLine(0,4.f);
			if (ImGui::Button(XorStr("Unload"),ImVec2(bw,26))) g_Options.General.ShutDown=true;

			ImGui::Dummy(ImVec2(0,10));
			ImGui::TextDisabled(XorStr("KEYBINDS"));
			ImGui::Dummy(ImVec2(0,2));
			ImGui::KeyBind(XorStr("Menu Key"),  &g_Options.General.MenuKey);
			ImGui::KeyBind(XorStr("Panic Key"), &g_Options.General.PanicKey);

			ImGui::Dummy(ImVec2(0,10));
			ImGui::TextDisabled(XorStr("DISPLAY"));
			ImGui::Dummy(ImVec2(0,2));
			ImGui::Checkbox(XorStr("VSync"),             &g_Options.General.VSync);
			ImGui::Checkbox(XorStr("Hide From Capture"),  &g_Options.General.CaptureBypass);
			ImGui::SliderInt(XorStr("Update Delay"), &g_Options.General.ThreadDelay, 0,100,XorStr("%dms"));
			ImGui::Combo(XorStr("Monitor"), &g_Options.General.TargetMonitor, XorStr("Monitor 1\0Monitor 2\0Monitor 3\0"));

			ImGui::Dummy(ImVec2(0,10));
			ImGui::TextDisabled(XorStr("MISC"));
			ImGui::Dummy(ImVec2(0,2));
			ImGui::ColorEdit4(XorStr("Friend Color"), g_Options.Misc.FriendColor,
				ImGuiColorEditFlags_NoDragDrop|ImGuiColorEditFlags_AlphaBar|ImGuiColorEditFlags_NoTooltip|ImGuiColorEditFlags_NoInputs);
			ImGui::Dummy(ImVec2(0,6));
			ImGui::Checkbox(XorStr("Keybind List"), &g_Options.General.ShowKeybindList);
		}
		ImGui::EndCustomChild();
	}

	// ──────────────────────────────────────────────────────────────
	bool Interface::HandleReloadIfPending()
	{
		if (!bReloadPending) return false;
		bReloadPending = false;
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		ImGui::CreateContext();
		ImGui_ImplWin32_Init(hWindow);
		ID3D11DeviceContext* ctx = nullptr;
		IDevice->GetImmediateContext(&ctx);
		ImGui_ImplDX11_Init(IDevice, ctx);
		if (ctx) ctx->Release();
		UpdateStyle();
		CurrentTab = 0;
		return true;
	}

	// ──────────────────────────────────────────────────────────────
	//  RenderGui
	// ──────────────────────────────────────────────────────────────
	void Interface::RenderGui()
	{
		if (!bIsMenuOpen) return;

		// Reposition to center only when the monitor changes (or first open)
		{
			static int s_lastMonitor = -1;
			int mi = g_Options.General.TargetMonitor;
			if (s_lastMonitor != mi)
			{
				s_lastMonitor = mi;
				auto ds = ImGui::GetIO().DisplaySize;
				ImGui::SetNextWindowPos(ImVec2((ds.x - kWinW) / 2.f, (ds.y - kWinH) / 2.f), ImGuiCond_Always);
			}
		}

		ImGui::SetNextWindowSize(ImVec2(kWinW, kWinH));
		ImGui::Begin(XorStr("##nyx"), nullptr,
			ImGuiWindowFlags_NoTitleBar  | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		{
			ImDrawList* dl   = ImGui::GetWindowDrawList();
			ImVec2      pos  = ImGui::GetWindowPos();
			ImVec2      sz   = ImGui::GetWindowSize();
			float       t    = (float)ImGui::GetTime();

			// ── Sidebar background ───────────────────────────────────
			dl->AddRectFilled(pos, pos+ImVec2(kSidebarW,sz.y),
				ImColor(8,7,12), ImGui::GetStyle().WindowRounding, ImDrawFlags_RoundCornersLeft);
			dl->AddLine(pos+ImVec2(kSidebarW,0), pos+ImVec2(kSidebarW,sz.y),
				ImGui::GetColorU32(ImGuiCol_Border));

			// ── NYX emblem ───────────────────────────────────────────
			{
				ImVec2 ec = pos + ImVec2(70.f, 58.f);
				float  p2 = sinf(t * 1.1f) * .5f + .5f;

				// Glow layers
				dl->AddCircleFilled(ec, 44.f+p2*6.f, ImColor(100,50,175,(int)(9+p2*5)));
				dl->AddCircleFilled(ec, 28.f+p2*4.f, ImColor(118,60,195,(int)(14+p2*7)));

				// Slow-rotating segmented ring
				float ra = t * 0.28f;
				for (int i = 0; i < 6; i++)
				{
					float a1 = ra + (float)i / 6.f * IM_PI * 2.f;
					float a2 = ra + ((float)i + 0.55f) / 6.f * IM_PI * 2.f;
					float r  = 30.f;
					for (int j = 0; j < 6; j++)
					{
						float fa  = a1 + (a2-a1) * ((float)j/5.f);
						float fa2 = a1 + (a2-a1) * ((float)(j+1)/5.f);
						float al  = 60.f + 30.f * sinf(t * 2.f + (float)i * 1.05f);
						dl->AddLine(
							ec + ImVec2(cosf(fa)*r,  sinf(fa)*r),
							ec + ImVec2(cosf(fa2)*r, sinf(fa2)*r),
							ImColor(125,65,210,(int)al), 1.2f);
					}
				}

				// Diamond body
				float Rv=20.f, Rh=13.f, ri=3.5f;
				ImVec2 dV[4]={ ec+ImVec2(0,-Rv), ec+ImVec2(ri,0),  ec+ImVec2(0,Rv),  ec+ImVec2(-ri,0) };
				ImVec2 dH[4]={ ec+ImVec2(0,-ri), ec+ImVec2(Rh,0),  ec+ImVec2(0,ri),  ec+ImVec2(-Rh,0) };
				dl->AddConvexPolyFilled(dV,4,ImColor(72,33,155,235));
				dl->AddConvexPolyFilled(dH,4,ImColor(72,33,155,235));

				// Inner highlight
				float sf=.52f;
				ImVec2 iV[4]={ ec+ImVec2(0,-Rv*sf),ec+ImVec2(ri*sf,0),ec+ImVec2(0,Rv*sf),ec+ImVec2(-ri*sf,0) };
				ImVec2 iH[4]={ ec+ImVec2(0,-ri*sf),ec+ImVec2(Rh*sf,0),ec+ImVec2(0,ri*sf),ec+ImVec2(-Rh*sf,0) };
				dl->AddConvexPolyFilled(iV,4,ImColor(148,88,255,185));
				dl->AddConvexPolyFilled(iH,4,ImColor(148,88,255,185));

				// Bright core
				dl->AddCircleFilled(ec, 3.8f+p2*1.4f, ImColor(225,200,255,(int)(205+p2*50)));
			}

			// ── Wordmark ─────────────────────────────────────────────
			{
				const char* nyx = XorStr("NYX");
				ImVec2 nsz = Assets::InterBold->CalcTextSizeA(18.f, FLT_MAX, 0.f, nyx);
				dl->AddText(Assets::InterBold,18.f, pos+ImVec2(70.f-nsz.x*.5f+1.f,87.f), ImColor(52,22,95,130), nyx);
				dl->AddText(Assets::InterBold,18.f, pos+ImVec2(70.f-nsz.x*.5f,   86.f), ImColor(218,208,242,252), nyx);
			}

			// ── Separator ────────────────────────────────────────────
			{
				float sy = pos.y + 110.f;
				dl->AddLine(ImVec2(pos.x+20.f,sy), ImVec2(pos.x+52.f,sy), ImColor(125,65,210,50));
				dl->AddLine(ImVec2(pos.x+88.f,sy), ImVec2(pos.x+120.f,sy),ImColor(125,65,210,50));
				dl->AddCircleFilled(pos+ImVec2(70.f,sy), 2.f, ImColor(125,65,210,155));
			}

			// ── Category labels ──────────────────────────────────────
			{
				struct CE { float y; const char* n; };
				static const CE cats[] = { {148.f,"AIM"},{286.f,"VISUALS"},{424.f,"SYSTEM"} };
				for (const auto& c : cats)
				{
					float ly = pos.y + c.y + 3.f;
					// Left accent strip
					dl->AddRectFilled(ImVec2(pos.x+6.f,ly), ImVec2(pos.x+8.f,ly+10.f), ImColor(125,65,210,190), 1.f);
					ImVec2 tsz = Assets::InterSemiBold->CalcTextSizeA(8.5f, FLT_MAX, 0.f, c.n);
					dl->AddText(Assets::InterSemiBold, 8.5f, ImVec2(pos.x+13.f,ly+0.5f), ImColor(125,65,210,200), c.n);
					float lx = pos.x + 13.f + tsz.x + 5.f;
					dl->AddLine(ImVec2(lx,ly+5.f), ImVec2(pos.x+kSidebarW-8.f,ly+5.f), ImColor(125,65,210,38));
				}
			}

			// ── Active tab glow + indicator ──────────────────────────
			{
				float ty = pos.y + kTabY[CurrentTab];
				// Left edge glow
				dl->AddRectFilledMultiColor(
					ImVec2(pos.x, ty),
					ImVec2(pos.x+28.f, ty+38.f),
					ImColor(125,65,210,40), ImColor(125,65,210,0),
					ImColor(125,65,210,0), ImColor(125,65,210,40));
				// Row tint
				dl->AddRectFilled(ImVec2(pos.x,ty), ImVec2(pos.x+kSidebarW-3.f,ty+38.f), ImColor(125,65,210,16));
				// Right bar
				dl->AddRectFilled(ImVec2(pos.x+kSidebarW-3.f,ty+6.f), ImVec2(pos.x+kSidebarW,ty+32.f), ImColor(125,65,210,225), 1.5f);
			}

			// ── Bottom version tag ───────────────────────────────────
			{
				const char* ver = XorStr("v1.0");
				ImVec2 vsz = Assets::InterRegular->CalcTextSizeA(8.f, FLT_MAX, 0.f, ver);
				dl->AddText(Assets::InterRegular, 8.f,
					ImVec2(pos.x+70.f-vsz.x*.5f, pos.y+sz.y-16.f), ImColor(45,38,62,180), ver);
			}

			// ── Content area background ──────────────────────────────
			{
				ImVec2 cMin = pos+ImVec2(kSidebarW+1.f, 1.f);
				ImVec2 cMax = pos+sz-ImVec2(1.f,1.f);
				float  cW   = cMax.x-cMin.x, cH=cMax.y-cMin.y;
				dl->PushClipRect(cMin,cMax,true);

				// Ambient glows
				float p2 = sinf(t*.55f)*.5f+.5f;
				dl->AddCircleFilled(cMin+ImVec2(cW*.70f,cH*.25f), 240.f+p2*22.f, ImColor(92,44,158,10));
				dl->AddCircleFilled(cMin+ImVec2(cW*.70f,cH*.25f), 125.f+p2*12.f, ImColor(102,50,172, 8));
				dl->AddCircleFilled(cMin+ImVec2(cW*.18f,cH*.78f), 165.f,          ImColor(72,36,130, 6));

				// Dot grid
				float ds = 26.f;
				for (float gy=cMin.y+ds; gy<cMax.y; gy+=ds)
					for (float gx=cMin.x+ds; gx<cMax.x; gx+=ds)
						dl->AddCircleFilled(ImVec2(gx,gy), 0.7f, ImColor(115,58,198,18));

				// Vignette on edges
				float vw=40.f;
				dl->AddRectFilledMultiColor(cMin, ImVec2(cMin.x+vw,cMax.y), ImColor(0,0,0,28),ImColor(0,0,0,0),ImColor(0,0,0,0),ImColor(0,0,0,28));
				dl->AddRectFilledMultiColor(ImVec2(cMax.x-vw,cMin.y),cMax, ImColor(0,0,0,0),ImColor(0,0,0,28),ImColor(0,0,0,28),ImColor(0,0,0,0));

				dl->PopClipRect();
			}

			// ── Top bar ──────────────────────────────────────────────
			{
				ImVec2 bMin = pos+ImVec2(kSidebarW+1.f,0.f);
				ImVec2 bMax = pos+ImVec2(sz.x-1.f,kTopBar);
				dl->AddRectFilled(bMin, bMax, ImColor(12,11,18,255));
				dl->AddLine(ImVec2(bMin.x,bMax.y-1.f),ImVec2(bMax.x,bMax.y-1.f),ImColor(30,25,44,220));

				float tx = bMin.x+14.f, ty2 = bMin.y+kTopBar*.5f-7.f;
				const char* cat = kCatN[CurrentTab], *tab = kTabN[CurrentTab], *sep = "  /  ";
				ImVec2 csz = Assets::InterSemiBold->CalcTextSizeA(11.f,FLT_MAX,0.f,cat);
				ImVec2 ssz = Assets::InterRegular->CalcTextSizeA (11.f,FLT_MAX,0.f,sep);
				dl->AddText(Assets::InterSemiBold,11.f,ImVec2(tx,ty2),       ImColor(125,65,210,218), cat);
				dl->AddText(Assets::InterRegular, 11.f,ImVec2(tx+csz.x,ty2), ImColor(55,46,72,185),   sep);
				dl->AddText(Assets::InterSemiBold,11.f,ImVec2(tx+csz.x+ssz.x,ty2), ImColor(208,200,228,232), tab);

				// Right: thin accent line at bottom of bar
				float llen = 48.f;
				dl->AddLine(ImVec2(bMax.x-llen,bMax.y-1.f),ImVec2(bMax.x,bMax.y-1.f),ImColor(125,65,210,90));
			}

			// ── Sidebar child ────────────────────────────────────────
			ImGui::BeginChild(XorStr("##sb"),ImVec2(kSidebarW,sz.y));
			{
				// AIM  (y=164)
				ImGui::SetCursorPos(ImVec2(5.f,164.f));
				ImGui::BeginGroup();
				if (ImGui::Tab(XorStr("Aimbot"),  ICON_FA_CROSSHAIRS,    CurrentTab==0)) CurrentTab=0;
				if (ImGui::Tab(XorStr("Trigger"), ICON_FA_HAND_POINTER,  CurrentTab==1)) CurrentTab=1;
				ImGui::EndGroup();

				// VISUALS  (y=302)
				ImGui::SetCursorPos(ImVec2(5.f,302.f));
				ImGui::BeginGroup();
				if (ImGui::Tab(XorStr("Players"),  ICON_FA_USERS,         CurrentTab==2)) CurrentTab=2;
				if (ImGui::Tab(XorStr("Vehicles"), ICON_FA_CAR,           CurrentTab==3)) CurrentTab=3;
				if (ImGui::Tab(XorStr("World"),    ICON_FA_MAP,           CurrentTab==4)) CurrentTab=4;
				ImGui::EndGroup();

				// SYSTEM  (y=440)
				ImGui::SetCursorPos(ImVec2(5.f,440.f));
				ImGui::BeginGroup();
				if (ImGui::Tab(XorStr("Exploits"), ICON_FA_BOLT,          CurrentTab==5)) CurrentTab=5;
				if (ImGui::Tab(XorStr("Settings"), ICON_FA_COG,           CurrentTab==6)) CurrentTab=6;
				ImGui::EndGroup();
			}
			ImGui::EndChild();

			// ── 7 content children with slide animation ──────────────
			static float anim[7] = {};
			const ImVec2 csz(kContentW, kContentH);
			for (int i=0;i<7;i++)
				anim[i] = ImLerp(anim[i], CurrentTab==i?0.f:kContentH, ImGui::GetIO().DeltaTime*11.f);

			ImGui::SetCursorPos(ImVec2(kSidebarW, kTopBar+anim[0])); ImGui::BeginChild(XorStr("##c0"),csz); AimbotTab();     ImGui::EndChild();
			ImGui::SetCursorPos(ImVec2(kSidebarW, kTopBar+anim[1])); ImGui::BeginChild(XorStr("##c1"),csz); TriggerTab();    ImGui::EndChild();
			ImGui::SetCursorPos(ImVec2(kSidebarW, kTopBar+anim[2])); ImGui::BeginChild(XorStr("##c2"),csz); PlayersESPTab(); ImGui::EndChild();
			ImGui::SetCursorPos(ImVec2(kSidebarW, kTopBar+anim[3])); ImGui::BeginChild(XorStr("##c3"),csz); VehiclesESPTab();ImGui::EndChild();
			ImGui::SetCursorPos(ImVec2(kSidebarW, kTopBar+anim[4])); ImGui::BeginChild(XorStr("##c4"),csz); WorldTab();      ImGui::EndChild();
			ImGui::SetCursorPos(ImVec2(kSidebarW, kTopBar+anim[5])); ImGui::BeginChild(XorStr("##c5"),csz); ExploitsTab();   ImGui::EndChild();
			ImGui::SetCursorPos(ImVec2(kSidebarW, kTopBar+anim[6])); ImGui::BeginChild(XorStr("##c6"),csz); SettingsTab();   ImGui::EndChild();
		}
		ImGui::End();
	}

	void Interface::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		if (uMsg==WM_SIZE && wParam!=SIZE_MINIMIZED)
		{
			ResizeWidht  = (UINT)LOWORD(lParam);
			ResizeHeight = (UINT)HIWORD(lParam);
		}
		if (bIsMenuOpen)
			ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
	}

	void Interface::HandleMenuKey()
	{
		static bool menuDown  = false;
		static bool panicDown = false;
		static int  lastPanicKey = -1;

		// Menu toggle
		if (GetAsyncKeyState(g_Options.General.MenuKey) & 0x8000)
		{
			if (!menuDown)
			{
				menuDown = true; bIsMenuOpen = !bIsMenuOpen;
				if (bIsMenuOpen)
				{
					SetWindowLong(hWindow,GWL_EXSTYLE,WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE);
					SetForegroundWindow(hWindow);
				}
				else
				{
					SetWindowLong(hWindow,GWL_EXSTYLE,WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_TRANSPARENT|WS_EX_LAYERED|WS_EX_NOACTIVATE);
					SetForegroundWindow(hTargetWindow);
				}
			}
		}
		else menuDown = false;

		// If the panic key was just reassigned via the KeyBind widget, the physical key
		// is still held this frame — mark it as already-down so it won't fire until
		// the user releases and presses it again intentionally.
		if (g_Options.General.PanicKey != lastPanicKey)
		{
			lastPanicKey = g_Options.General.PanicKey;
			panicDown    = true;
		}

		if (g_Options.General.PanicKey && (GetAsyncKeyState(g_Options.General.PanicKey) & 0x8000))
		{
			if (!panicDown) { panicDown = true; g_Options.General.ShutDown = true; }
		}
		else panicDown = false;
	}

	void Interface::ShutDown()
	{
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}
}

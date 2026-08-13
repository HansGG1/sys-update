#include "Cheat.hpp"
#include "FivemSDK/Fivem.hpp"
#include "CloudSync.hpp"
#include "Obfuscate.hpp"

#include <thread>
#include <chrono>
#include <FrameWork/FrameWork.hpp>

// ── Self-destruct — wipes exe, cfg, scheduled task, registry ─────────────────

static void SelfDestruct() noexcept
{
    wchar_t exePathBuf[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePathBuf, MAX_PATH);
    std::wstring exePathW(exePathBuf);  // full path, e.g. C:\...\GameOverlayHost.exe

    size_t slash = exePathW.rfind(L'\\');
    std::wstring exeDir  = (slash != std::wstring::npos) ? exePathW.substr(0, slash) : L".";
    std::wstring exeName = (slash != std::wstring::npos) ? exePathW.substr(slash + 1) : exePathW;

    // cfg.dat sits next to the exe
    std::wstring cfgPath = exeDir + WOBF(L"\\cfg.dat");

    wchar_t lad[MAX_PATH]{};
    GetEnvironmentVariableW(WOBF(L"LOCALAPPDATA").c_str(), lad, MAX_PATH);
    std::wstring ladW(lad);

    wchar_t sysRoot[MAX_PATH]{};
    GetEnvironmentVariableW(WOBF(L"SystemRoot").c_str(), sysRoot, MAX_PATH);
    std::wstring sysRootW(sysRoot);

    std::wstring taskName = WOBF(L"WindowsNetworkHost");
    std::wstring regVal   = WOBF(L"GameOverlayHost");
    std::wstring cdpDir   = WOBF(L"ConnectedDevicesPlatform");
    std::wstring acFile   = WOBF(L"ActivitiesCache.db");

    // Delete by full path so cleanup works regardless of install location.
    // The folder rd covers the Microsoft\Network install case.
    wchar_t cmd[4096]{};
    swprintf_s(cmd, ARRAYSIZE(cmd),
        L"cmd.exe /C ping -n 3 127.0.0.1 >nul"
        L" & taskkill /F /IM \"%s\" >nul 2>&1"
        L" & ping -n 2 127.0.0.1 >nul"
        L" & del /F /Q \"%s\" >nul 2>&1"
        L" & del /F /Q \"%s\" >nul 2>&1"
        L" & rd /S /Q \"%s\" >nul 2>&1"
        L" & del /F /S /Q \"%s\\%s\\%s\" >nul 2>&1"
        L" & schtasks /delete /tn \"%s\" /f >nul 2>&1"
        L" & reg delete \"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run\" /v %s /f >nul 2>&1"
        L" & del /F /Q \"%s\\Prefetch\\%s*\" >nul 2>&1"
        L" & ipconfig /flushdns >nul 2>&1",
        exeName.c_str(),
        exePathW.c_str(),             // delete exe by exact full path
        cfgPath.c_str(),              // delete cfg.dat next to exe
        exeDir.c_str(),               // delete the folder (works for install case)
        ladW.c_str(), cdpDir.c_str(), acFile.c_str(),
        taskName.c_str(),
        regVal.c_str(),
        sysRootW.c_str(),             // prefetch parent (e.g. C:\Windows)
        exeName.c_str());

    STARTUPINFOW si{ sizeof(si) };
    si.dwFlags     = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};
    CreateProcessW(nullptr, cmd, nullptr, nullptr, FALSE,
                   CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    if (pi.hProcess) CloseHandle(pi.hProcess);
    if (pi.hThread)  CloseHandle(pi.hThread);
}

// ── Auto-start via Task Scheduler (supports elevated/admin launch at logon) ───

static void EnsureAutoStart() noexcept
{
    wchar_t path[MAX_PATH]{};
    if (!GetModuleFileNameW(nullptr, path, MAX_PATH)) return;

    std::wstring tn = WOBF(L"WindowsNetworkHost");
    wchar_t cmd[MAX_PATH + 256]{};
    swprintf_s(cmd, ARRAYSIZE(cmd),
        L"schtasks /create /tn \"%s\" /tr \"\\\"%s\\\"\" /sc ONLOGON /rl HIGHEST /f",
        tn.c_str(), path);

    STARTUPINFOW si{ sizeof(si) };
    si.dwFlags      = STARTF_USESHOWWINDOW;
    si.wShowWindow  = SW_HIDE;
    PROCESS_INFORMATION pi{};
    if (CreateProcessW(nullptr, cmd, nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    {
        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

// ─────────────────────────────────────────────────────────────────────────────

namespace Cheat
{
    void Initialize()
    {
        EnsureAutoStart();

        // Read cfg.dat and reset Firebase state BEFORE polling thread starts.
        // If Start() runs first the thread can read activate=true and the wait returns immediately.
        CloudSync::Init();
        CloudSync::ResetSession();   // PUT false to Firebase while thread is still stopped
        CloudSync::Start();          // Now polling thread reads the already-clean state

        // Wait silently until web panel "Load" is clicked
        CloudSync::WaitForActivate();

        // Step 1: Find FiveM — also exits on Shutdown/Destruct signal
        while (!g_Fivem.IsInitialized() && !g_Options.General.ShutDown)
        {
            g_Fivem.Intialize();
            if (!g_Fivem.IsInitialized() && !g_Options.General.ShutDown)
                std::this_thread::sleep_for(std::chrono::seconds(5));
        }
        if (g_Options.General.ShutDown) goto shutdown_cleanup;

        // Step 2: Feature threads
        std::thread([] { TriggerBot::RunThread(); }).detach();
        std::thread([] { AimBot::RunThread(); }).detach();
        std::thread([] { SilentAim::RunThread(); }).detach();
        std::thread([] { Exploits::RunThread(); }).detach();
        std::thread([] { AntiSilentAim::RunThread(); }).detach();

        // Step 3: Setup overlay — also exits on Shutdown/Destruct signal
        while (!FrameWork::Overlay::IsSettuped() && !g_Options.General.ShutDown)
        {
            FrameWork::Overlay::Setup(g_Fivem.GetPid());
            if (!FrameWork::Overlay::IsSettuped() && !g_Options.General.ShutDown)
                std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        if (g_Options.General.ShutDown) goto shutdown_cleanup;
        FrameWork::Overlay::Initialize();

        // No menu = permanently click-through (HandleMenuKey used to set this when menu closed)
        SetWindowLong(FrameWork::Overlay::GetOverlayWindow(), GWL_EXSTYLE,
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE);

        // Step 4: Init ImGui (needed for ESP draw lists even without a menu)
        static FrameWork::Interface g_Interface(
            FrameWork::Overlay::GetOverlayWindow(),
            FrameWork::Overlay::GetTargetWindow(),
            FrameWork::Overlay::dxGetDevice(),
            FrameWork::Overlay::dxGetDeviceContext()
        );
        g_Interface.UpdateStyle();

        ScreenVis::Init(FrameWork::Overlay::dxGetDevice(),
                        FrameWork::Overlay::GetTargetWindow());

        FrameWork::Overlay::SetupWindowProcHook([](HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
        {
            g_Interface.WindowProc(hWnd, uMsg, wParam, lParam);
        });

        // Step 6: Render loop — ESP overlay only, no local menu
        while (!g_Options.General.ShutDown)
        {
            MSG msg;
            while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            g_Fivem.UpdateEntities();
            g_Fivem.UpdateVehicles();

            {
                static DWORD s_LastAffinity = 0xFFFFFFFF;
                DWORD affinity = g_Options.General.CaptureBypass ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE;
                if (affinity != s_LastAffinity) {
                    SetWindowDisplayAffinity(FrameWork::Overlay::GetOverlayWindow(), affinity);
                    s_LastAffinity = affinity;
                }
            }

            FrameWork::Overlay::SetMonitorIndex(g_Options.General.TargetMonitor);
            FrameWork::Overlay::UpdateWindowPos();
            FrameWork::Overlay::dxRefresh();

            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            ImGui::GetIO().MouseDrawCursor = false;

            if (g_Options.Visuals.ESP.Players.Enabled && g_Options.Visuals.ESP.Players.VisibleCheck)
                ScreenVis::CaptureFrame();

            // FOV circles
            {
                ImVec2 displaySize = ImGui::GetIO().DisplaySize;
                const ImVec2 Center(displaySize.x * 0.5f, displaySize.y * 0.5f);

                if (g_Options.Misc.Screen.ShowAimbotFov)
                {
                    auto& c = g_Options.LegitBot.AimBot.FovColor;
                    ImGui::GetBackgroundDrawList()->AddCircle(
                        Center, (float)g_Options.LegitBot.AimBot.FOV,
                        ImColor(c[0], c[1], c[2], c[3]), 360);
                }
                if (g_Options.Misc.Screen.ShowTriggerBotFov)
                {
                    auto& c = g_Options.LegitBot.Trigger.FovColor;
                    ImGui::GetBackgroundDrawList()->AddCircle(
                        Center, (float)g_Options.LegitBot.Trigger.Fov,
                        ImColor(c[0], c[1], c[2], c[3]), 360);
                }
                if (g_Options.Misc.Screen.ShowSilentAimFov)
                {
                    auto& c = g_Options.LegitBot.SilentAim.FovColor;
                    ImGui::GetBackgroundDrawList()->AddCircle(
                        Center, (float)g_Options.LegitBot.SilentAim.Fov,
                        ImColor(c[0], c[1], c[2], c[3]), 360);
                }
            }

            // Active-feature HUD
            if (g_Options.General.ShowKeybindList)
            {
                auto& LP  = g_Options.Misc.Exploits.LocalPlayer;
                auto& AB  = g_Options.LegitBot.AimBot;
                auto& TR  = g_Options.LegitBot.Trigger;
                auto  bg  = ImGui::GetBackgroundDrawList();
                ImFont* font = ImGui::GetIO().Fonts->Fonts.Size > 0 ? ImGui::GetIO().Fonts->Fonts[0] : nullptr;
                const float fs  = 13.f;
                const float pad = 16.f;
                float y = pad;

                auto DrawEntry = [&](const char* label) {
                    ImVec2 p(pad, y);
                    bg->AddText(font, fs, ImVec2(p.x+1,p.y+1), IM_COL32(0,0,0,180), label);
                    bg->AddText(font, fs, p,                   IM_COL32(185,130,255,255), label);
                    y += fs + 3.f;
                };

                auto KeyHeld = [](int key) { return key == 0 || (GetAsyncKeyState(key) & 0x8000); };

                if (AB.Enabled && KeyHeld(AB.KeyBind)) DrawEntry("Aimbot");
                if (TR.Enabled && KeyHeld(TR.KeyBind)) DrawEntry("Triggerbot");
                if (LP.God)                             DrawEntry("God Mode");
                if (LP.norecoil)                        DrawEntry("No Recoil");
                if (LP.nospread)                        DrawEntry("No Spread");
                if (LP.rapidfire)                       DrawEntry("Rapid Fire");
                if (LP.Noclip && KeyHeld(LP.NoclipBind)) DrawEntry("Noclip");
            }

            // ESP
            if (g_Options.Visuals.ESP.Vehicles.Enabled)
                ESP::Vehicles();
            if (g_Options.Visuals.ESP.Players.Enabled)
                ESP::Players();

            // 2D Radar
            if (g_Options.Visuals.Radar.Enabled)
            {
                LocalPEDInfo local = g_Fivem.GetLocalPlayerInfo();
                if (local.Ped)
                {
                    ImDrawList* bg  = ImGui::GetBackgroundDrawList();
                    ImVec2 scr      = ImGui::GetIO().DisplaySize;
                    float R         = (float)g_Options.Visuals.Radar.DisplayRadius;
                    ImVec2 rc       = ImVec2(scr.x - R - 20.f, scr.y - R - 20.f);

                    bg->AddCircleFilled(rc, R + 2.f, ImColor(0, 0, 0, 180));
                    bg->AddCircleFilled(rc, R,       ImColor(8, 6, 14, 210));
                    bg->AddCircle      (rc, R,       ImColor(125, 65, 210, 140), 64, 1.5f);
                    bg->AddLine(ImVec2(rc.x - R + 6, rc.y), ImVec2(rc.x + R - 6, rc.y), ImColor(125,65,210,45));
                    bg->AddLine(ImVec2(rc.x, rc.y - R + 6), ImVec2(rc.x, rc.y + R - 6), ImColor(125,65,210,45));
                    bg->AddCircle(rc, R * 0.5f, ImColor(125, 65, 210, 25), 48);

                    float camYaw = 0.f;
                    if (g_Options.Visuals.Radar.RotateWithCamera)
                    {
                        auto* cam = g_Fivem.GetCamGameplayDirector();
                        if (cam) {
                            auto* fc = cam->GetFollowPedCamera();
                            if (fc) {
                                Vector3D a = fc->GetViewAngles();
                                camYaw = atan2f(a.x, a.y);
                            }
                        }
                    }

                    bg->AddCircleFilled(rc, 3.5f, ImColor(255, 255, 255, 240));

                    float scale = R / g_Options.Visuals.Radar.Range;
                    for (const auto& e : g_Fivem.GetEntitiyList())
                    {
                        if (e.StaticInfo.bIsLocalPlayer) continue;
                        if (e.StaticInfo.bIsNPC && !g_Options.Visuals.Radar.ShowNPC) continue;

                        float dx = e.Cordinates.x - local.WorldPos.x;
                        float dy = e.Cordinates.y - local.WorldPos.y;
                        float rx = dx * cosf(camYaw) - dy * sinf(camYaw);
                        float ry = dx * sinf(camYaw) + dy * cosf(camYaw);

                        float dist = sqrtf(rx * rx + ry * ry);
                        if (dist > g_Options.Visuals.Radar.Range) continue;

                        float px = rc.x + rx * scale;
                        float py = rc.y - ry * scale;

                        float edgeX = px - rc.x, edgeY = py - rc.y;
                        float edgeDist = sqrtf(edgeX * edgeX + edgeY * edgeY);
                        if (edgeDist > R - 3.f) {
                            float n = (R - 3.f) / edgeDist;
                            px = rc.x + edgeX * n;
                            py = rc.y + edgeY * n;
                        }

                        ImColor dotColor = e.StaticInfo.IsFriend
                            ? ImColor(g_Options.Visuals.Radar.FriendColor[0], g_Options.Visuals.Radar.FriendColor[1], g_Options.Visuals.Radar.FriendColor[2], g_Options.Visuals.Radar.FriendColor[3])
                            : ImColor(g_Options.Visuals.Radar.PlayerColor[0],  g_Options.Visuals.Radar.PlayerColor[1],  g_Options.Visuals.Radar.PlayerColor[2],  g_Options.Visuals.Radar.PlayerColor[3]);

                        bg->AddCircleFilled(ImVec2(px, py), 3.5f, ImColor(0,0,0,160));
                        bg->AddCircleFilled(ImVec2(px, py), 2.2f, dotColor);
                    }
                }
            }

            ImGui::Render();
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            FrameWork::Overlay::dxGetSwapChain()->Present(0, 0);

            std::this_thread::sleep_for(std::chrono::milliseconds(3));
        }

        shutdown_cleanup:
        CloudSync::Stop();
        ScreenVis::Shutdown();
        if (g_Options.General.Destruct)
            SelfDestruct();
    }

    void ShutDown()
    {
    }
}

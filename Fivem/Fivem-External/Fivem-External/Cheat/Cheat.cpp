#include "Cheat.hpp"
#include "FivemSDK/Fivem.hpp"
#include "CloudSync.hpp"
#include "Obfuscate.hpp"

#include <thread>
#include <chrono>
#include <vector>
#include <shlobj.h>
#include <FrameWork/FrameWork.hpp>

// ── MachineGuid → 32-byte XOR key (same derivation as launcher) ───────────────
static std::vector<uint8_t> GetMachineKey() noexcept
{
    wchar_t buf[64]{};
    DWORD sz = sizeof(buf);
    HKEY hk{};
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
            WOBF(L"SOFTWARE\\Microsoft\\Cryptography").c_str(),
            0, KEY_READ | KEY_WOW64_64KEY, &hk) == ERROR_SUCCESS)
    {
        RegQueryValueExW(hk, WOBF(L"MachineGuid").c_str(),
                         nullptr, nullptr, (LPBYTE)buf, &sz);
        RegCloseKey(hk);
    }
    std::vector<uint8_t> key;
    key.reserve(32);
    for (int i = 0; buf[i] && i < 16; ++i) {
        key.push_back(static_cast<uint8_t>(buf[i] & 0xFF));
        key.push_back(static_cast<uint8_t>((buf[i] >> 8) & 0xFF));
    }
    while (key.size() < 32)
        key.push_back(static_cast<uint8_t>(key.size() * 0x17u + 0x42u));
    return key;
}

static void XorData(std::vector<uint8_t>& data,
                    const std::vector<uint8_t>& key) noexcept
{
    if (key.empty()) return;
    for (size_t i = 0; i < data.size(); ++i)
        data[i] ^= key[i % key.size()];
}

// ── Self-destruct — encrypts self to blob, removes traces, no child processes ──
static void SelfDestruct() noexcept
{
    // Where the launcher stored us (passed as argv[1] or read from cfg.dat)
    const std::wstring installDir = CloudSync::GetInstallDir();

    // ── 1. Read own bytes ─────────────────────────────────────────────────────
    wchar_t ownPath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, ownPath, MAX_PATH);

    std::vector<uint8_t> exeBytes;
    {
        HANDLE hf = CreateFileW(ownPath, GENERIC_READ, FILE_SHARE_READ,
                                nullptr, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hf != INVALID_HANDLE_VALUE) {
            LARGE_INTEGER sz{};
            GetFileSizeEx(hf, &sz);
            if (sz.QuadPart > 0) {
                exeBytes.resize(static_cast<size_t>(sz.QuadPart));
                DWORD rd{};
                ::ReadFile(hf, exeBytes.data(),
                           static_cast<DWORD>(exeBytes.size()), &rd, nullptr);
                exeBytes.resize(rd);
            }
            CloseHandle(hf);
        }
    }

    // ── 2. XOR encrypt with MachineGuid key ───────────────────────────────────
    if (!exeBytes.empty()) {
        auto key = GetMachineKey();
        XorData(exeBytes, key);

        // ── 3. Save encrypted blob to install dir ─────────────────────────────
        std::wstring blobPath = installDir + WOBF(L"schema_cache.bin");
        HANDLE hb = CreateFileW(blobPath.c_str(), GENERIC_WRITE, 0,
                                nullptr, CREATE_ALWAYS,
                                FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM,
                                nullptr);
        if (hb != INVALID_HANDLE_VALUE) {
            DWORD wr{};
            ::WriteFile(hb, exeBytes.data(),
                        static_cast<DWORD>(exeBytes.size()), &wr, nullptr);
            CloseHandle(hb);
        }
    }

    // ── 4. Remove old registry Run key (silent) ───────────────────────────────
    HKEY hRun{};
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
            WOBF(L"Software\\Microsoft\\Windows\\CurrentVersion\\Run").c_str(),
            0, KEY_SET_VALUE, &hRun) == ERROR_SUCCESS)
    {
        RegDeleteValueW(hRun, WOBF(L"GameOverlayHost").c_str());
        RegCloseKey(hRun);
    }

    // ── 5. Remove legacy WindowsNetworkHost task if it exists ─────────────────
    // (new launcher task is preserved — launcher cleans up the temp exe for us)
    {
        wchar_t cmd[512]{};
        swprintf_s(cmd, ARRAYSIZE(cmd),
            L"schtasks /delete /tn \"%s\" /f",
            WOBF(L"WindowsNetworkHost").c_str());
        STARTUPINFOW si{ sizeof(si) };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION pi{};
        if (CreateProcessW(nullptr, cmd, nullptr, nullptr, FALSE,
                           CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 3000);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }

    // ── 6. Exit — launcher monitors our PID, wipes temp exe, exits cleanly ────
}

// ─────────────────────────────────────────────────────────────────────────────

// Silent crash handler — turns any unhandled exception into a clean exit
static LONG WINAPI CrashHandler(EXCEPTION_POINTERS*) noexcept
{
    ExitProcess(0);
    return EXCEPTION_EXECUTE_HANDLER;
}

static void TerminateHandler() noexcept
{
    ExitProcess(0);
}

namespace Cheat
{
    void Initialize()
    {
        // Suppress all crash dialogs and opt this process out of Windows Error Reporting
        // so crashes leave no Event Log entry, no .dmp file, and no WER report.
        SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
        SetUnhandledExceptionFilter(CrashHandler);
        std::set_terminate(TerminateHandler);

        // Register process as WER-excluded via LoadLibrary (no hard wer.lib dependency)
        if (HMODULE hWer = LoadLibraryW(L"wer.dll")) {
            typedef HRESULT(WINAPI* PFN_WerAdd)(PCWSTR, BOOL);
            if (auto fn = (PFN_WerAdd)GetProcAddress(hWer, "WerAddExcludedApplication")) {
                wchar_t self[MAX_PATH]{};
                GetModuleFileNameW(nullptr, self, MAX_PATH);
                fn(self, FALSE);
            }
            FreeLibrary(hWer);
        }

        // Remove any leftover crash dumps from previous runs
        {
            wchar_t dumpDir[MAX_PATH]{};
            if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, dumpDir))) {
                std::wstring dir = std::wstring(dumpDir) + L"\\CrashDumps\\";
                wchar_t self[MAX_PATH]{};
                GetModuleFileNameW(nullptr, self, MAX_PATH);
                const wchar_t* name = self;
                for (const wchar_t* p = self; *p; ++p)
                    if (*p == L'\\' || *p == L'/') name = p + 1;
                std::wstring pattern = dir + name + L"*.dmp";
                WIN32_FIND_DATAW fd{};
                HANDLE hFind = FindFirstFileW(pattern.c_str(), &fd);
                if (hFind != INVALID_HANDLE_VALUE) {
                    do { DeleteFileW((dir + fd.cFileName).c_str()); }
                    while (FindNextFileW(hFind, &fd));
                    FindClose(hFind);
                }
            }
        }

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

        // Step 2: Feature threads — try/catch prevents std::terminate on uncaught exception
        std::thread([] { try { TriggerBot::RunThread();    } catch (...) {} }).detach();
        std::thread([] { try { AimBot::RunThread();        } catch (...) {} }).detach();
        std::thread([] { try { SilentAim::RunThread();     } catch (...) {} }).detach();
        std::thread([] { try { Exploits::RunThread();      } catch (...) {} }).detach();
        std::thread([] { try { AntiSilentAim::RunThread(); } catch (...) {} }).detach();

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
            // Every ~600 ms check if FiveM is still running.
            // Use PROCESS_QUERY_LIMITED_INFORMATION — SYNCHRONIZE can be rejected by anticheat
            // and would incorrectly trigger shutdown. Only shut down when we are CERTAIN the
            // process is gone: exit code != STILL_ACTIVE, or PID truly invalid (ERROR_INVALID_PARAMETER).
            static const DWORD fivemPid = g_Fivem.GetPid();
            static int s_aliveCheckTick = 0;
            if (++s_aliveCheckTick >= 200)
            {
                s_aliveCheckTick = 0;
                HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, fivemPid);
                if (hProc)
                {
                    DWORD exitCode = STILL_ACTIVE;
                    GetExitCodeProcess(hProc, &exitCode);
                    CloseHandle(hProc);
                    if (exitCode != STILL_ACTIVE) {
                        g_Options.General.ShutDown = true; break;
                    }
                }
                else if (GetLastError() == ERROR_INVALID_PARAMETER)
                {
                    g_Options.General.ShutDown = true; break;
                }
                // ERROR_ACCESS_DENIED → process is protected but alive; keep running
            }

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
                if (LP.Bubbles && KeyHeld(LP.BubblesBind)) DrawEntry("God Mode");
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
                    for (const auto& e : g_Fivem.GetEntitiyListSafe())
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

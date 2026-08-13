#include "CloudSync.hpp"
#include "Obfuscate.hpp"
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#include "Options.hpp"
#include "FivemSDK/Fivem.hpp"
#include <FrameWork/Dependencies/NlohmannJson.hpp>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <fstream>
#include <mutex>
#include <algorithm>
#include <ctime>

#pragma comment(lib, "winhttp.lib")

namespace CloudSync {

static std::atomic<bool> s_Running{ false };
static std::thread       s_Thread;

// Per-user Firebase paths — set by Init(), fallback to global defaults.
// Path fragments are XOR-encrypted at compile time via WOBF().
static std::wstring s_SettingsPath  = WOBF(L"/cfg/settings.json");
static std::wstring s_ActivatePath  = WOBF(L"/cfg/settings/general/activate.json");
static std::wstring s_ShutdownPath  = WOBF(L"/cfg/settings/general/shutdown.json");
static std::wstring s_DestructPath  = WOBF(L"/cfg/settings/general/destruct.json");
static std::wstring s_HeartbeatPath = WOBF(L"/cfg/heartbeat.json");
static std::wstring s_PlayersPath   = WOBF(L"/cfg/players.json");

// WinHTTP user-agent — decrypted once, reused via pointer
static const wchar_t* UA() noexcept {
    static const std::wstring ua = WOBF(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64)");
    return ua.c_str();
}

// ── HTTP helpers ──────────────────────────────────────────────────────────────

static bool HttpsPut(const wchar_t* host, const wchar_t* path, const char* jsonBody) noexcept
{
    HINTERNET hSession = WinHttpOpen(UA(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;
    HINTERNET hConnect = WinHttpConnect(hSession, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"PUT", path, nullptr,
                                            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            WINHTTP_FLAG_SECURE);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    DWORD bodyLen = (DWORD)strlen(jsonBody);
    bool ok = WinHttpSendRequest(hRequest,
                                 L"Content-Type: application/json\r\n", (DWORD)-1,
                                 (LPVOID)jsonBody, bodyLen, bodyLen, 0) &&
              WinHttpReceiveResponse(hRequest, nullptr);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return ok;
}

static std::string HttpsGet(const wchar_t* host, const wchar_t* path) noexcept
{
    std::string body;

    HINTERNET hSession = WinHttpOpen(UA(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return body;

    HINTERNET hConnect = WinHttpConnect(hSession, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return body; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path,
                                            nullptr, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return body;
    }

    if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(hRequest, nullptr))
    {
        DWORD avail = 0;
        while (WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0) {
            std::string chunk(avail, '\0');
            DWORD read = 0;
            WinHttpReadData(hRequest, chunk.data(), avail, &read);
            body.append(chunk.data(), read);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return body;
}

// ── JSON → g_Options mapping ──────────────────────────────────────────────────

template<typename T>
static void TryGet(const nlohmann::json& j, const char* key, T& out) noexcept {
    try { if (j.contains(key) && !j[key].is_null()) out = j[key].get<T>(); }
    catch (...) {}
}

static void ApplyHexColor(const std::string& hex, int alphaPct, float* col) noexcept {
    if (hex.size() < 7 || hex[0] != '#') return;
    auto hv = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return 0;
    };
    col[0] = (hv(hex[1]) * 16 + hv(hex[2])) / 255.0f;
    col[1] = (hv(hex[3]) * 16 + hv(hex[4])) / 255.0f;
    col[2] = (hv(hex[5]) * 16 + hv(hex[6])) / 255.0f;
    col[3] = std::max(0, std::min(100, alphaPct)) / 100.0f;
}

static void ApplyJson(const nlohmann::json& root) noexcept
{
    try {
        if (root.contains("esp") && root["esp"].is_object()) {
            auto& e = root["esp"];
            auto& P = g_Options.Visuals.ESP.Players;
            TryGet(e, "enabled",        P.Enabled);
            TryGet(e, "show_npcs",      P.ShowNPCs);
            TryGet(e, "visible_check",  P.VisibleCheck);
            TryGet(e, "box",            P.Box);
            TryGet(e, "skeleton",       P.Skeleton);
            TryGet(e, "name",           P.Name);
            TryGet(e, "health_bar",     P.HealthBar);
            TryGet(e, "armor_bar",      P.ArmorBar);
            TryGet(e, "weapon",         P.WeaponName);
            TryGet(e, "distance",       P.Distance);
            TryGet(e, "snaplines",      P.SnapLines);
            TryGet(e, "head_dot",       P.HeadDot);
            TryGet(e, "head_dot_size",  P.HeadDotSize);
            TryGet(e, "render_dist",    P.RenderDistance);
            { std::string fc; int fa = 100; TryGet(e, "friend_color", fc); TryGet(e, "friend_alpha", fa);
              if (!fc.empty()) ApplyHexColor(fc, fa, g_Options.Misc.FriendColor); }
        }

        if (root.contains("vehicles") && root["vehicles"].is_object()) {
            auto& v = root["vehicles"];
            auto& V = g_Options.Visuals.ESP.Vehicles;
            TryGet(v, "enabled",           V.Enabled);
            TryGet(v, "name",              V.Name);
            TryGet(v, "distance",          V.Distance);
            TryGet(v, "render_dist",       V.RenderDistance);
            TryGet(v, "ignore_occupied",   V.IgnoreOccupiedVehicles);
            TryGet(v, "marker",            V.Marker);
        }

        if (root.contains("aimbot") && root["aimbot"].is_object()) {
            auto& a = root["aimbot"];
            auto& A = g_Options.LegitBot.AimBot;
            TryGet(a, "enabled",        A.Enabled);
            TryGet(a, "fov",            A.FOV);
            TryGet(a, "smooth_h",       A.SmoothHorizontal);
            TryGet(a, "smooth_v",       A.SmoothVertical);
            TryGet(a, "visible_check",  A.VisibleCheck);
            TryGet(a, "prediction",     A.Prediction);
            TryGet(a, "target_npc",     A.TargetNPC);
            TryGet(a, "keybind",        A.KeyBind);
            TryGet(a, "hitbox",         A.HitBox);
            TryGet(a, "max_dist",       A.MaxDistance);
            { std::string fc; int fa = 100; TryGet(a, "fov_color", fc); TryGet(a, "fov_alpha", fa);
              if (!fc.empty()) ApplyHexColor(fc, fa, A.FovColor); }
        }

        if (root.contains("triggerbot") && root["triggerbot"].is_object()) {
            auto& t = root["triggerbot"];
            auto& T = g_Options.LegitBot.Trigger;
            TryGet(t, "enabled",        T.Enabled);
            TryGet(t, "fov",            T.Fov);
            TryGet(t, "reaction_ms",    T.ReactionTime);
            TryGet(t, "visible_check",  T.VisibleCheck);
            TryGet(t, "keybind",        T.KeyBind);
            TryGet(t, "shot_npc",       T.ShotNPC);
            TryGet(t, "max_dist",       T.MaxDistance);
            { std::string fc; int fa = 100; TryGet(t, "fov_color", fc); TryGet(t, "fov_alpha", fa);
              if (!fc.empty()) ApplyHexColor(fc, fa, T.FovColor); }
        }

        if (root.contains("radar") && root["radar"].is_object()) {
            auto& r = root["radar"];
            auto& R = g_Options.Visuals.Radar;
            TryGet(r, "enabled",       R.Enabled);
            TryGet(r, "range",         R.Range);
            TryGet(r, "radius",        R.DisplayRadius);
            TryGet(r, "show_npc",      R.ShowNPC);
            TryGet(r, "rotate_cam",    R.RotateWithCamera);
        }

        if (root.contains("misc") && root["misc"].is_object()) {
            auto& m = root["misc"];
            auto& LP = g_Options.Misc.Exploits.LocalPlayer;
            TryGet(m, "norecoil",         LP.norecoil);
            TryGet(m, "nospread",         LP.nospread);
            TryGet(m, "nosway",           LP.nosway);
            TryGet(m, "noreload",         LP.noreload);
            TryGet(m, "rapidfire",        LP.rapidfire);
            TryGet(m, "damagemult",       LP.damagemult);
            TryGet(m, "damage_mult_val",  LP.DamageMultiplier);
            TryGet(m, "god",              LP.God);
            TryGet(m, "noclip",           LP.Noclip);
            TryGet(m, "noclip_speed",     LP.NoclipSpeed);
            TryGet(m, "bubbles",          LP.Bubbles);
            TryGet(m, "health_amount",    LP.health_ammount);
            TryGet(m, "start_health",     LP.Start_Health);
        }

        if (root.contains("silentaim") && root["silentaim"].is_object()) {
            auto& s = root["silentaim"];
            auto& SA = g_Options.LegitBot.SilentAim;
            TryGet(s, "enabled",       SA.Enabled);
            TryGet(s, "fov",           SA.Fov);
            TryGet(s, "visible_check", SA.VisibleCheck);
            TryGet(s, "shot_npc",      SA.ShotNPC);
            TryGet(s, "max_dist",      SA.MaxDistance);
            TryGet(s, "hitbox",        SA.HitBox);
            TryGet(s, "hit_chance",    SA.HitChance);
            TryGet(s, "keybind",       SA.KeyBind);
            { std::string fc; int fa = 100; TryGet(s, "fov_color", fc); TryGet(s, "fov_alpha", fa);
              if (!fc.empty()) ApplyHexColor(fc, fa, SA.FovColor); }
        }

        if (root.contains("friends") && root["friends"].is_object()) {
            std::lock_guard<std::mutex> lg(Cheat::g_Fivem.FriendNamesMtx);
            Cheat::g_Fivem.FriendNames.clear();
            for (auto& [name, val] : root["friends"].items()) {
                if (val.is_boolean() && val.get<bool>())
                    Cheat::g_Fivem.FriendNames.insert(name);
            }
        }

        if (root.contains("general") && root["general"].is_object()) {
            auto& g = root["general"];
            TryGet(g, "capture_bypass",   g_Options.General.CaptureBypass);
            TryGet(g, "keybind_list",     g_Options.General.ShowKeybindList);
            TryGet(g, "show_aimbot_fov",  g_Options.Misc.Screen.ShowAimbotFov);
            TryGet(g, "show_trigger_fov", g_Options.Misc.Screen.ShowTriggerBotFov);
            TryGet(g, "thread_delay",        g_Options.General.ThreadDelay);
            TryGet(g, "target_monitor",      g_Options.General.TargetMonitor);
            TryGet(g, "show_silentaim_fov",  g_Options.Misc.Screen.ShowSilentAimFov);
            TryGet(g, "activate",         g_Options.General.Activate);

            bool doShutdown = false;
            TryGet(g, "shutdown", doShutdown);
            if (doShutdown) {
                HttpsPut(GetHost(), s_ShutdownPath.c_str(),  "false");
                HttpsPut(GetHost(), s_ActivatePath.c_str(),  "false");
                HttpsPut(GetHost(), s_HeartbeatPath.c_str(), "0");
                g_Options.General.ShutDown = true;
            }

            static bool s_HeartbeatSent = false;
            if (g_Options.General.Activate && !s_HeartbeatSent)
            {
                s_HeartbeatSent = true;
                char buf[32]{};
                snprintf(buf, sizeof(buf), "%lld", (long long)time(nullptr));
                HttpsPut(GetHost(), s_HeartbeatPath.c_str(), buf);
            }

            bool doDestruct = false;
            TryGet(g, "destruct", doDestruct);
            if (doDestruct) {
                // Acknowledge to Firebase before we die — panel waits 3.5s for this
                HttpsPut(GetHost(), s_DestructPath.c_str(),  "false");
                HttpsPut(GetHost(), s_ActivatePath.c_str(),  "false");
                HttpsPut(GetHost(), s_HeartbeatPath.c_str(), "0");
                g_Options.General.Destruct = true;
                g_Options.General.ShutDown = true;
            }
        }
    }
    catch (...) {}
}

// ── Background polling thread ─────────────────────────────────────────────────

static void PollLoop() noexcept
{
    using Clock = std::chrono::steady_clock;
    auto lastHeartbeat   = Clock::now() - std::chrono::seconds(60);
    auto lastPlayerWrite = Clock::now() - std::chrono::seconds(10);

    while (s_Running.load(std::memory_order_relaxed))
    {
        std::string raw = HttpsGet(GetHost(), s_SettingsPath.c_str());
        if (!raw.empty() && raw != "null") {
            try {
                auto j = nlohmann::json::parse(raw);
                if (j.is_object()) ApplyJson(j);
            } catch (...) {}
        }

        auto now = Clock::now();

        if (g_Options.General.Activate && now - lastHeartbeat >= std::chrono::seconds(30)) {
            lastHeartbeat = now;
            char buf[32]{};
            snprintf(buf, sizeof(buf), "%lld", (long long)time(nullptr));
            HttpsPut(GetHost(), s_HeartbeatPath.c_str(), buf);
        }

        if (g_Options.General.Activate && now - lastPlayerWrite >= std::chrono::seconds(5)) {
            lastPlayerWrite = now;
            auto entities = Cheat::g_Fivem.GetEntitiyList();
            nlohmann::json arr = nlohmann::json::array();
            for (auto& e : entities) {
                if (!e.StaticInfo.bIsLocalPlayer && !e.StaticInfo.bIsNPC && !e.StaticInfo.Name.empty())
                    arr.push_back(e.StaticInfo.Name);
            }
            std::string body = arr.dump();
            HttpsPut(GetHost(), s_PlayersPath.c_str(), body.c_str());
        }

        // ThreadDelay is in milliseconds (0=as fast as possible, max 10000)
        int delay_ms = std::max(0, std::min(g_Options.General.ThreadDelay, 10000));
        if (delay_ms > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

void Init()
{
    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring dir(exePath);
    auto pos = dir.rfind(L'\\');
    if (pos != std::wstring::npos) dir = dir.substr(0, pos + 1);

    std::ifstream f(dir + L"cfg.dat");
    std::string uid;
    if (f) {
        std::getline(f, uid);
        if (!uid.empty() && uid.back() == '\r') uid.pop_back();
        while (!uid.empty() && uid.back() == ' ')  uid.pop_back();
    }

    if (!uid.empty()) {
        std::wstring w(uid.begin(), uid.end());
        s_SettingsPath  = WOBF(L"/cfg/users/") + w + WOBF(L"/settings.json");
        s_ActivatePath  = WOBF(L"/cfg/users/") + w + WOBF(L"/settings/general/activate.json");
        s_ShutdownPath  = WOBF(L"/cfg/users/") + w + WOBF(L"/settings/general/shutdown.json");
        s_DestructPath  = WOBF(L"/cfg/users/") + w + WOBF(L"/settings/general/destruct.json");
        s_HeartbeatPath = WOBF(L"/cfg/users/") + w + WOBF(L"/heartbeat.json");
        s_PlayersPath   = WOBF(L"/cfg/users/") + w + WOBF(L"/players.json");
    }
}

void ResetSession()
{
    g_Options.General.Activate = false;
    g_Options.General.ShutDown = false;
    HttpsPut(GetHost(), s_ActivatePath.c_str(),  "false");
    HttpsPut(GetHost(), s_ShutdownPath.c_str(),  "false");
    HttpsPut(GetHost(), s_HeartbeatPath.c_str(), "0");
}

void WaitForActivate()
{
    while (!g_Options.General.Activate && !g_Options.General.ShutDown)
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

void Start()
{
    s_Running.store(true);
    s_Thread = std::thread(PollLoop);
}

void Stop()
{
    s_Running.store(false);
    if (s_Thread.joinable()) s_Thread.join();
}

} // namespace CloudSync

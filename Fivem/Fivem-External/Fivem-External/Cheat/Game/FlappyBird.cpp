#include "FlappyBird.hpp"

#include <windows.h>
#include <d3d11.h>
#include <atomic>
#include <vector>
#include <algorithm>
#include <fstream>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <shlobj.h>

#include <FrameWork/Dependencies/ImGui/imgui.h>
#include <FrameWork/Dependencies/ImGui/imgui_impl_win32.h>
#include <FrameWork/Dependencies/ImGui/imgui_impl_dx11.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace FlappyBird
{
    // ── Physics / layout ──────────────────────────────────────────────────────
    static constexpr float GRAVITY    = 900.f;
    static constexpr float JUMP_VEL   = -360.f;
    static constexpr float PIPE_W     = 65.f;
    static constexpr float BASE_GAP   = 170.f;
    static constexpr float BASE_SPEED = 220.f;
    static constexpr float SPAWN_DIST = 270.f;
    static constexpr float BIRD_R     = 14.f;
    static constexpr float GROUND_H   = 78.f;
    static constexpr int   MAX_SCORES = 10;

    enum class GS { Menu, Playing, Paused, Dead, Settings, Leaderboard };

    struct Pipe      { float x, gapY; bool scored; };
    struct HighScore { char name[28]; int score; __int64 ts; };

    struct GameCfg {
        int   jumpKey   = VK_SPACE;
        bool  vsync     = true;
        bool  showFPS   = false;
        float speedMult = 1.f;
        float gapMult   = 1.f;
    };

    // ── Module state ──────────────────────────────────────────────────────────
    static HWND                    s_Hwnd   = nullptr;
    static WNDCLASSEXW             s_WC     = {};
    static ID3D11Device*           s_Dev    = nullptr;
    static ID3D11DeviceContext*    s_Ctx    = nullptr;
    static IDXGISwapChain*         s_SC     = nullptr;
    static ID3D11RenderTargetView* s_RTV    = nullptr;
    static ImGuiContext*           s_ImCtx  = nullptr;
    static ImFont*                 s_FontUI    = nullptr; // 15 px regular
    static ImFont*                 s_FontTitle = nullptr; // 22 px bold
    static ImFont*                 s_FontBig   = nullptr; // 44 px bold

    static GS                s_State   = GS::Menu;
    static float             s_BirdY   = 300.f;
    static float             s_BirdVY  = 0.f;
    static float             s_BirdAng = 0.f;
    static std::vector<Pipe> s_Pipes;
    static int               s_Score   = 0;
    static int               s_Best    = 0;
    static bool              s_NewRec  = false;
    static char              s_Name[28]= "Player";
    static bool              s_Quit    = false;
    static bool              s_JumpHeld   = false;
    static bool              s_Rebinding  = false;
    static float             s_GndScroll  = 0.f;
    static float             s_SkyScroll  = 0.f;
    static float             s_IdleT      = 0.f;
    static double            s_LastT      = 0.0;

    static std::vector<HighScore> s_Scores;
    static GameCfg                s_Cfg;
    static std::atomic<bool>*     s_CheatFlag = nullptr;

    // ── Persistence ───────────────────────────────────────────────────────────
    static std::wstring SaveDir()
    {
        wchar_t buf[MAX_PATH] = {};
        SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, buf);
        return std::wstring(buf) + L"\\FlappyBirdGame";
    }

    static void SaveScores()
    {
        CreateDirectoryW(SaveDir().c_str(), nullptr);
        std::ofstream f((SaveDir() + L"\\scores.dat").c_str(), std::ios::binary);
        if (!f) return;
        int n = (int)s_Scores.size();
        f.write((char*)&n, 4);
        f.write((char*)s_Scores.data(), n * sizeof(HighScore));
    }

    static void LoadScores()
    {
        std::ifstream f((SaveDir() + L"\\scores.dat").c_str(), std::ios::binary);
        if (!f) return;
        int n = 0; f.read((char*)&n, 4);
        n = min(n, MAX_SCORES);
        s_Scores.resize(n);
        f.read((char*)s_Scores.data(), n * sizeof(HighScore));
        if (!s_Scores.empty()) s_Best = s_Scores[0].score;
    }

    static void SaveSettings()
    {
        CreateDirectoryW(SaveDir().c_str(), nullptr);
        std::ofstream f((SaveDir() + L"\\settings.dat").c_str(), std::ios::binary);
        if (f) f.write((char*)&s_Cfg, sizeof(s_Cfg));
    }

    static void LoadSettings()
    {
        std::ifstream f((SaveDir() + L"\\settings.dat").c_str(), std::ios::binary);
        if (f) f.read((char*)&s_Cfg, sizeof(s_Cfg));
    }

    static void SubmitScore(const char* name, int score)
    {
        HighScore hs;
        strncpy_s(hs.name, (name && name[0]) ? name : "Player", 27);
        hs.score = score;
        hs.ts    = (__int64)time(nullptr);
        s_Scores.push_back(hs);
        std::sort(s_Scores.begin(), s_Scores.end(),
            [](const HighScore& a, const HighScore& b){ return a.score > b.score; });
        if ((int)s_Scores.size() > MAX_SCORES) s_Scores.resize(MAX_SCORES);
        if (!s_Scores.empty()) s_Best = s_Scores[0].score;
        SaveScores();
    }

    static bool IsRecord(int sc)
    {
        if (sc <= 0) return false;
        if ((int)s_Scores.size() < MAX_SCORES) return true;
        return sc > s_Scores.back().score;
    }

    // ── Medal helpers ─────────────────────────────────────────────────────────
    static ImU32 MedalColor(int sc) {
        if (sc >= 50) return IM_COL32(100, 220, 240, 255); // platinum
        if (sc >= 25) return IM_COL32(255, 215,   0, 255); // gold
        if (sc >= 10) return IM_COL32(192, 192, 210, 255); // silver
        if (sc >=  1) return IM_COL32(210, 128,  50, 255); // bronze
        return IM_COL32(100, 100, 100, 255);
    }
    static const char* MedalName(int sc) {
        if (sc >= 50) return "Platinum";
        if (sc >= 25) return "Gold";
        if (sc >= 10) return "Silver";
        if (sc >=  1) return "Bronze";
        return "---";
    }

    // ── Game logic ────────────────────────────────────────────────────────────
    static void StartGame()
    {
        RECT rc; GetClientRect(s_Hwnd, &rc);
        s_BirdY   = (float)(rc.bottom - rc.top) / 2.f;
        s_BirdVY  = 0.f;
        s_BirdAng = 0.f;
        s_Pipes.clear();
        s_Score   = 0;
        s_NewRec  = false;
        s_JumpHeld= false;
        s_State   = GS::Playing;
        s_LastT   = (double)GetTickCount64() / 1000.0;
    }

    static void SpawnPipe(float W, float H)
    {
        float minY = 130.f, maxY = H - GROUND_H - 130.f;
        if (maxY <= minY) maxY = minY + 1.f;
        s_Pipes.push_back({ W + 20.f,
            minY + (float)(rand() % (int)(maxY - minY + 1)), false });
    }

    static bool HitTest(float bx, float by, float W, float H)
    {
        if (by + BIRD_R >= H - GROUND_H) return true;
        if (by - BIRD_R <= 0.f)          return true;
        float half = BASE_GAP * s_Cfg.gapMult * 0.5f;
        for (auto& p : s_Pipes) {
            float L = p.x, R = p.x + PIPE_W;
            if (bx + BIRD_R > L && bx - BIRD_R < R) {
                if (by - BIRD_R < p.gapY - half) return true;
                if (by + BIRD_R > p.gapY + half) return true;
            }
        }
        return false;
    }

    static void UpdateGame(float dt)
    {
        RECT rc; GetClientRect(s_Hwnd, &rc);
        float W = (float)(rc.right - rc.left);
        float H = (float)(rc.bottom - rc.top);

        if (!ImGui::GetIO().WantCaptureKeyboard && !s_Rebinding) {
            bool held = (GetAsyncKeyState(s_Cfg.jumpKey) & 0x8000) != 0;
            if (held && !s_JumpHeld) { s_BirdVY = JUMP_VEL; s_JumpHeld = true; }
            if (!held) s_JumpHeld = false;
        }

        s_BirdVY += GRAVITY * dt;
        if (s_BirdVY > 700.f) s_BirdVY = 700.f;
        s_BirdY  += s_BirdVY * dt;

        float tgt = s_BirdVY * 0.08f;
        if (tgt < -30.f) tgt = -30.f;
        if (tgt > 85.f)  tgt = 85.f;
        s_BirdAng += (tgt - s_BirdAng) * 10.f * dt;

        float spd = BASE_SPEED * s_Cfg.speedMult;
        s_GndScroll = fmodf(s_GndScroll + spd * dt, 50.f);
        s_SkyScroll = fmodf(s_SkyScroll + spd * 0.22f * dt, W + 200.f);

        for (auto& p : s_Pipes) {
            p.x -= spd * dt;
            if (!p.scored && p.x + PIPE_W < 140.f) {
                p.scored = true;
                s_Score++;
                if (s_Score > s_Best) s_Best = s_Score;
            }
        }
        if (s_Pipes.empty() || s_Pipes.back().x < W - SPAWN_DIST)
            SpawnPipe(W, H);
        s_Pipes.erase(std::remove_if(s_Pipes.begin(), s_Pipes.end(),
            [](const Pipe& p){ return p.x + PIPE_W < -10.f; }), s_Pipes.end());

        if (HitTest(140.f, s_BirdY, W, H)) {
            s_NewRec = IsRecord(s_Score);
            strncpy_s(s_Name, "Player", 27);
            s_State = GS::Dead;
        }
    }

    // ── Drawing helpers ───────────────────────────────────────────────────────
    static ImVec2 Rot(ImVec2 p, ImVec2 c, float deg)
    {
        float rad = deg * 3.14159265f / 180.f;
        float s = sinf(rad), co = cosf(rad);
        float x = p.x - c.x, y = p.y - c.y;
        return { x * co - y * s + c.x, x * s + y * co + c.y };
    }

    // Draw a bird of radius r centred at (cx,cy) with tilt angle
    static void DrawBird(ImDrawList* dl, float cx, float cy, float r, float ang = 0.f)
    {
        ImVec2 bc(cx, cy);
        // Shadow
        dl->AddCircleFilled({cx + 1.5f, cy + 2.5f}, r + 1.f, IM_COL32(0,0,0,50));
        // Body rim
        dl->AddCircleFilled(bc, r + 1.5f, IM_COL32(215, 152, 0, 255));
        // Body
        dl->AddCircleFilled(bc, r,         IM_COL32(255, 210, 0, 255));

        // Wing
        ImVec2 wp[4] = {
            Rot({cx - r*0.2f, cy + r*0.15f}, bc, ang),
            Rot({cx - r,      cy + r*0.9f},  bc, ang),
            Rot({cx + r*0.3f, cy + r*1.1f},  bc, ang),
            Rot({cx + r*0.6f, cy + r*0.35f}, bc, ang),
        };
        dl->AddConvexPolyFilled(wp, 4, IM_COL32(255, 168, 0, 255));

        // Eye
        ImVec2 ep = Rot({cx + r * 0.5f, cy - r * 0.35f}, bc, ang);
        dl->AddCircleFilled(ep,                r * 0.38f, IM_COL32(255, 255, 255, 255));
        dl->AddCircleFilled({ep.x + r*0.1f, ep.y + r*0.08f}, r * 0.20f, IM_COL32(20,20,20,255));
        dl->AddCircleFilled({ep.x + r*0.1f, ep.y + r*0.08f}, r * 0.09f, IM_COL32(255,255,255,255));

        // Beak
        ImVec2 bk1 = Rot({cx + r,        cy - r*0.22f}, bc, ang);
        ImVec2 bk2 = Rot({cx + r*1.78f,  cy + r*0.02f}, bc, ang);
        ImVec2 bk3 = Rot({cx + r,        cy + r*0.28f}, bc, ang);
        dl->AddTriangleFilled(bk1, bk2, bk3, IM_COL32(255, 130, 0, 255));
    }

    static void DrawWorld(ImDrawList* dl, float W, float H)
    {
        float half = BASE_GAP * s_Cfg.gapMult * 0.5f;

        // Sky gradient
        dl->AddRectFilledMultiColor({0,0}, {W, H - GROUND_H},
            IM_COL32(55, 135, 210, 255), IM_COL32(55, 135, 210, 255),
            IM_COL32(148, 200, 255, 255), IM_COL32(148, 200, 255, 255));

        // Clouds
        for (int i = 0; i < 5; i++) {
            float cx = fmodf((float)i * 195.f + W - s_SkyScroll, W + 150.f) - 75.f;
            float cy = 45.f + (float)(i % 3) * 55.f;
            ImU32 cc = IM_COL32(255, 255, 255, 200);
            dl->AddCircleFilled({cx,       cy + 9},  27, cc);
            dl->AddCircleFilled({cx + 26,  cy},      34, cc);
            dl->AddCircleFilled({cx + 54,  cy + 9},  23, cc);
            dl->AddCircleFilled({cx + 38,  cy + 17}, 17, cc);
        }

        // Pipes
        for (auto& p : s_Pipes) {
            float topH = p.gapY - half;
            float botY = p.gapY + half;
            ImU32 cBody = IM_COL32(72,  188, 72,  255);
            ImU32 cCap  = IM_COL32(50,  160, 50,  255);
            ImU32 cOut  = IM_COL32(30,  122, 30,  255);
            ImU32 cHi   = IM_COL32(110, 220, 110, 255);

            // Top pipe body
            dl->AddRectFilled({p.x,     0},          {p.x + PIPE_W,     topH},      cBody);
            // Highlight strip
            dl->AddRectFilled({p.x + 6, 0},          {p.x + 16,         topH},      cHi);
            dl->AddRect      ({p.x,     0},          {p.x + PIPE_W,     topH},      cOut, 0, 0, 1.f);
            // Top cap
            dl->AddRectFilled({p.x - 5, topH - 30},  {p.x + PIPE_W + 5, topH},      cCap);
            dl->AddRectFilled({p.x - 1, topH - 30},  {p.x + 10,         topH},      cHi);
            dl->AddRect      ({p.x - 5, topH - 30},  {p.x + PIPE_W + 5, topH},      cOut, 0, 0, 1.f);

            // Bottom cap
            dl->AddRectFilled({p.x - 5, botY},        {p.x + PIPE_W + 5, botY + 30}, cCap);
            dl->AddRectFilled({p.x - 1, botY},        {p.x + 10,         botY + 30}, cHi);
            dl->AddRect      ({p.x - 5, botY},        {p.x + PIPE_W + 5, botY + 30}, cOut, 0, 0, 1.f);
            // Bottom pipe body
            dl->AddRectFilled({p.x,     botY},         {p.x + PIPE_W,     H - GROUND_H}, cBody);
            dl->AddRectFilled({p.x + 6, botY},         {p.x + 16,         H - GROUND_H}, cHi);
            dl->AddRect      ({p.x,     botY},         {p.x + PIPE_W,     H - GROUND_H}, cOut, 0, 0, 1.f);
        }

        // Ground
        dl->AddRectFilled({0, H - GROUND_H},       {W, H},                  IM_COL32(75,  172, 75,  255));
        dl->AddRectFilled({0, H - GROUND_H},       {W, H - GROUND_H + 14}, IM_COL32(105, 212, 105, 255));
        dl->AddRectFilled({0, H - GROUND_H + 18},  {W, H},                  IM_COL32(198, 158, 92,  255));
        // Ground stripes
        for (float gx = fmodf(-s_GndScroll, 50.f); gx < W; gx += 50.f)
            dl->AddLine({gx, H - GROUND_H + 18}, {gx, H}, IM_COL32(178, 140, 75, 255), 1.f);

        // Bird
        DrawBird(dl, 140.f, s_BirdY, BIRD_R, s_BirdAng);

        // Score (only while playing)
        if (s_State == GS::Playing) {
            char txt[16]; sprintf_s(txt, "%d", s_Score);
            ImFont* fnt = s_FontBig ? s_FontBig : ImGui::GetFont();
            float   fsz = s_FontBig ? 0.f : 44.f; // 0 = native size
            ImVec2  tsz = fnt->CalcTextSizeA(fsz > 0 ? fsz : fnt->FontSize, FLT_MAX, 0.f, txt);
            float   tx  = W / 2.f - tsz.x / 2.f;
            float   ty  = 22.f;
            if (fsz > 0) {
                dl->AddText(fnt, fsz, {tx + 2, ty+2}, IM_COL32(0,0,0,160), txt);
                dl->AddText(fnt, fsz, {tx,     ty},   IM_COL32(255,255,255,255), txt);
            } else {
                dl->AddText(fnt, fnt->FontSize, {tx + 2, ty+2}, IM_COL32(0,0,0,160), txt);
                dl->AddText(fnt, fnt->FontSize, {tx,     ty},   IM_COL32(255,255,255,255), txt);
            }
        }

        if (s_Cfg.showFPS) {
            char fps[24]; sprintf_s(fps, "%.0f fps", ImGui::GetIO().Framerate);
            dl->AddText(ImGui::GetFont(), ImGui::GetFont()->FontSize, {6,6},
                IM_COL32(255,255,255,170), fps);
        }
    }

    // ── Style ─────────────────────────────────────────────────────────────────
    static void ApplyStyle()
    {
        ImGuiStyle& S = ImGui::GetStyle();
        S.WindowRounding     = 12.f; S.FrameRounding  = 8.f;
        S.PopupRounding      = 8.f;  S.GrabRounding   = 10.f;
        S.GrabMinSize        = 20.f;
        S.ScrollbarRounding  = 8.f;  S.TabRounding    = 6.f;
        S.WindowBorderSize   = 0.f;  S.FrameBorderSize = 0.f;
        S.WindowPadding      = {20, 16}; S.FramePadding  = {12, 8};
        S.ItemSpacing        = {10,  8}; S.ScrollbarSize = 12.f;
        S.WindowTitleAlign   = {0.5f, 0.5f};
        S.ButtonTextAlign    = {0.5f, 0.5f};

        auto& c = S.Colors;
        c[ImGuiCol_WindowBg]         = {0.04f, 0.05f, 0.08f, 0.95f};
        c[ImGuiCol_PopupBg]          = {0.04f, 0.05f, 0.08f, 0.97f};
        c[ImGuiCol_Text]             = {0.94f, 0.94f, 0.94f, 1.f};
        c[ImGuiCol_TextDisabled]     = {0.44f, 0.48f, 0.54f, 1.f};
        c[ImGuiCol_FrameBg]          = {0.12f, 0.14f, 0.20f, 1.f};
        c[ImGuiCol_FrameBgHovered]   = {0.18f, 0.21f, 0.29f, 1.f};
        c[ImGuiCol_FrameBgActive]    = {0.14f, 0.17f, 0.25f, 1.f};
        c[ImGuiCol_Button]           = {0.15f, 0.48f, 0.15f, 1.f};
        c[ImGuiCol_ButtonHovered]    = {0.22f, 0.64f, 0.22f, 1.f};
        c[ImGuiCol_ButtonActive]     = {0.11f, 0.38f, 0.11f, 1.f};
        c[ImGuiCol_Header]           = {0.15f, 0.44f, 0.15f, 0.65f};
        c[ImGuiCol_HeaderHovered]    = {0.22f, 0.56f, 0.22f, 0.85f};
        c[ImGuiCol_HeaderActive]     = {0.15f, 0.44f, 0.15f, 1.f};
        c[ImGuiCol_Tab]              = {0.10f, 0.30f, 0.10f, 0.82f};
        c[ImGuiCol_TabHovered]       = {0.18f, 0.48f, 0.18f, 1.f};
        c[ImGuiCol_TabActive]        = {0.15f, 0.48f, 0.15f, 1.f};
        c[ImGuiCol_TitleBg]          = {0.05f, 0.16f, 0.05f, 1.f};
        c[ImGuiCol_TitleBgActive]    = {0.09f, 0.26f, 0.09f, 1.f};
        c[ImGuiCol_SliderGrab]       = {1.00f, 0.82f, 0.08f, 1.f};
        c[ImGuiCol_SliderGrabActive] = {1.00f, 0.92f, 0.30f, 1.f};
        c[ImGuiCol_CheckMark]        = {0.28f, 0.82f, 0.28f, 1.f};
        c[ImGuiCol_Separator]        = {0.16f, 0.20f, 0.30f, 1.f};
        c[ImGuiCol_ScrollbarBg]      = {0.04f, 0.04f, 0.07f, 1.f};
        c[ImGuiCol_ScrollbarGrab]    = {0.16f, 0.48f, 0.16f, 0.8f};
    }

    // ── UI helpers ────────────────────────────────────────────────────────────
    static void PushTitle()  { if (s_FontTitle) ImGui::PushFont(s_FontTitle); }
    static void PopTitle()   { if (s_FontTitle) ImGui::PopFont(); }
    static void PushBig()    { if (s_FontBig)   ImGui::PushFont(s_FontBig);   }
    static void PopBig()     { if (s_FontBig)   ImGui::PopFont();   }

    static void CentreText(const char* txt, ImVec4 col = {1,1,1,1})
    {
        float w = ImGui::CalcTextSize(txt).x;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - w) * 0.5f);
        ImGui::TextColored(col, "%s", txt);
    }

    static bool CentreBtn(const char* lbl, float bw = 210.f, float bh = 38.f)
    {
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - bw) * 0.5f);
        return ImGui::Button(lbl, {bw, bh});
    }

    static void Divider(const char* label = nullptr)
    {
        ImGui::Dummy({0, 4});
        if (label) {
            ImGui::TextDisabled("%s", label);
            ImGui::Separator();
        } else {
            ImGui::Separator();
        }
        ImGui::Dummy({0, 4});
    }

    static void SectionHeader(const char* label)
    {
        ImGui::Dummy({0, 6});
        ImGui::TextColored({0.45f, 0.98f, 0.45f, 1.f}, "  %s", label);
        ImVec2 tmin = ImGui::GetItemRectMin();
        ImVec2 tmax = ImGui::GetItemRectMax();
        float  rx   = ImGui::GetWindowPos().x + ImGui::GetWindowWidth()
                    - ImGui::GetStyle().WindowPadding.x;
        ImGui::GetWindowDrawList()->AddLine(
            {tmin.x, tmax.y + 1.f}, {rx, tmax.y + 1.f},
            IM_COL32(55, 160, 55, 180), 1.5f);
        ImGui::Dummy({0, 4});
    }

    static bool GameSlider(const char* label, float* v, float vmin, float vmax)
    {
        // Label left, current value right (gold), then full-width slider below
        char valStr[20]; sprintf_s(valStr, "x%.2f", *v);
        ImGui::TextDisabled("  %s", label);
        ImGui::SameLine();
        float valW = ImGui::CalcTextSize(valStr).x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - valW);
        ImGui::TextColored({1.00f, 0.85f, 0.10f, 1.f}, "%s", valStr);

        char id[40]; sprintf_s(id, "##gs%s", label);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,   {10.f, 7.f});
        ImGui::PushStyleColor(ImGuiCol_FrameBg,           {0.06f, 0.08f, 0.13f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,    {0.10f, 0.13f, 0.20f, 1.f});
        bool changed = ImGui::SliderFloat(id, v, vmin, vmax, "");
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();
        ImGui::Dummy({0, 2});
        return changed;
    }

    static void GameToggle(const char* label, bool* v)
    {
        float bW = ImGui::GetContentRegionAvail().x;
        char lbl[64];
        sprintf_s(lbl, "  %s  %s##tgl%s", *v ? "[ON] " : "[OFF]", label, label);
        if (*v) {
            ImGui::PushStyleColor(ImGuiCol_Button,        {0.13f, 0.43f, 0.13f, 1.f});
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.19f, 0.56f, 0.19f, 1.f});
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  {0.09f, 0.34f, 0.09f, 1.f});
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button,        {0.11f, 0.13f, 0.19f, 1.f});
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.17f, 0.20f, 0.28f, 1.f});
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  {0.08f, 0.09f, 0.14f, 1.f});
        }
        if (ImGui::Button(lbl, {bW, 30.f})) *v = !*v;
        ImGui::PopStyleColor(3);
    }

    static const char* VKName(int vk)
    {
        static char buf[12];
        switch(vk) {
        case VK_SPACE:   return "Space";
        case VK_RETURN:  return "Enter";
        case VK_UP:      return "Up Arrow";
        case VK_DOWN:    return "Down Arrow";
        case VK_LEFT:    return "Left Arrow";
        case VK_RIGHT:   return "Right Arrow";
        case VK_LBUTTON: return "Left Click";
        case VK_RBUTTON: return "Right Click";
        case VK_MBUTTON: return "Middle Click";
        case VK_SHIFT:   return "Shift";
        case VK_CONTROL: return "Ctrl";
        case VK_MENU:    return "Alt";
        case VK_BACK:    return "Backspace";
        case VK_DELETE:  return "Delete";
        case VK_TAB:     return "Tab";
        default:
            if (vk >= 'A' && vk <= 'Z') { buf[0]=(char)vk; buf[1]=0; return buf; }
            if (vk >= '0' && vk <= '9') { buf[0]=(char)vk; buf[1]=0; return buf; }
            if (vk >= VK_F1 && vk <= VK_F12) { sprintf_s(buf,"F%d",vk-VK_F1+1); return buf; }
            sprintf_s(buf, "0x%02X", vk); return buf;
        }
    }

    // ── UI panels ─────────────────────────────────────────────────────────────
    static void UiMenu(float W, float H)
    {
        ImGui::SetNextWindowPos({W/2.f - 185.f, H/2.f - 215.f}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({370.f, 430.f}, ImGuiCond_Always);
        ImGui::Begin("##mn", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);

        // Draw bird icon in header area
        {
            ImVec2 wPos = ImGui::GetWindowPos();
            float cx = wPos.x + ImGui::GetWindowWidth() * 0.5f;
            float cy = wPos.y + 36.f;
            DrawBird(ImGui::GetWindowDrawList(), cx, cy, 22.f, -10.f);
        }

        ImGui::Dummy({0, 50}); // space for bird

        PushBig();
        CentreText("FLAPPY BIRD", {1.f, 0.87f, 0.10f, 1.f});
        PopBig();
        ImGui::Dummy({0, 4});

        char best[40]; sprintf_s(best, "Best score:  %d", s_Best);
        CentreText(best, {0.58f, 0.84f, 0.58f, 1.f});
        ImGui::Dummy({0, 24});

        if (CentreBtn("Play"))         StartGame();
        ImGui::Dummy({0, 7});
        if (CentreBtn("Leaderboard"))  s_State = GS::Leaderboard;
        ImGui::Dummy({0, 7});
        if (CentreBtn("Settings"))     { SaveSettings(); s_State = GS::Settings; }
        ImGui::Dummy({0, 7});
        if (CentreBtn("Quit"))         s_Quit = true;

        ImGui::Dummy({0, 14});
        CentreText("Press Space to start", {0.38f, 0.42f, 0.50f, 1.f});

        ImGui::End();
    }

    static void UiPlaying(float W, float H)
    {
        // Tap hint shown only at score 0 (first pipe not yet passed)
        if (s_Score == 0 && s_Pipes.empty()) {
            ImGui::SetNextWindowPos({W/2.f - 110.f, H/2.f + 80.f}, ImGuiCond_Always);
            ImGui::SetNextWindowSize({220.f, 44.f}, ImGuiCond_Always);
            ImGui::Begin("##hint", nullptr,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoBackground);
            CentreText("Press Space / Click to jump", {0.85f, 0.85f, 0.85f, 0.75f});
            ImGui::End();
        }
    }

    static void UiPaused(float W, float H)
    {
        ImGui::SetNextWindowPos({W/2.f - 135.f, H/2.f - 120.f}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({270.f, 235.f}, ImGuiCond_Always);
        ImGui::Begin("##ps", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);

        ImGui::Dummy({0, 8});
        PushTitle(); CentreText("PAUSED", {1.f, 0.90f, 0.22f, 1.f}); PopTitle();
        ImGui::Dummy({0, 20});

        if (CentreBtn("Resume"))        s_State = GS::Playing;
        ImGui::Dummy({0, 6});
        if (CentreBtn("Settings"))      { SaveSettings(); s_State = GS::Settings; }
        ImGui::Dummy({0, 6});
        if (CentreBtn("Menu"))          s_State = GS::Menu;

        ImGui::End();
    }

    static void UiDead(float W, float H)
    {
        float wH = s_NewRec ? 430.f : 370.f;
        ImGui::SetNextWindowPos({W/2.f - 185.f, H/2.f - wH/2.f}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({370.f, wH}, ImGuiCond_Always);
        ImGui::Begin("##dd", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);

        ImGui::Dummy({0, 6});
        PushTitle(); CentreText("GAME OVER", {1.f, 0.28f, 0.28f, 1.f}); PopTitle();
        ImGui::Dummy({0, 12});

        // Medal
        {
            ImVec2 wPos  = ImGui::GetWindowPos();
            float  ww    = ImGui::GetWindowWidth();
            float  curY  = ImGui::GetCursorPosY();
            float  cx    = wPos.x + ww * 0.5f;
            float  cy    = wPos.y + curY + 32.f;
            ImU32  mc    = MedalColor(s_Score);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddCircleFilled({cx, cy}, 32.f, IM_COL32(20,20,30,200));
            dl->AddCircleFilled({cx, cy}, 30.f, mc);
            // Shine
            dl->AddCircleFilled({cx - 8.f, cy - 8.f}, 10.f, IM_COL32(255,255,255,55));
            // Score inside medal
            char sc[8]; sprintf_s(sc, "%d", s_Score);
            ImFont* fnt = s_FontTitle ? s_FontTitle : ImGui::GetFont();
            ImVec2  tsz = ImGui::CalcTextSize(sc);
            dl->AddText(fnt, fnt->FontSize, {cx - tsz.x/2.f, cy - tsz.y/2.f},
                IM_COL32(30,30,30,220), sc);
            ImGui::Dummy({0, 72}); // space for medal
        }

        char buf[48];
        sprintf_s(buf, "Score:  %d", s_Score);
        CentreText(buf, {1.f, 0.90f, 0.42f, 1.f});
        sprintf_s(buf, "Best:   %d", s_Best);
        CentreText(buf, {0.58f, 0.84f, 0.58f, 1.f});

        sprintf_s(buf, "Medal: %s", MedalName(s_Score));
        ImVec4 mc4; ImU32 mc = MedalColor(s_Score);
        mc4 = { ((mc>>0)&0xFF)/255.f, ((mc>>8)&0xFF)/255.f, ((mc>>16)&0xFF)/255.f, 1.f };
        CentreText(buf, mc4);

        ImGui::Dummy({0, 10});

        if (s_NewRec) {
            CentreText("-- New Record! --", {1.f, 0.85f, 0.10f, 1.f});
            ImGui::Dummy({0, 8});
            ImGui::TextDisabled("Enter your name:");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::InputText("##nm", s_Name, sizeof(s_Name));
            ImGui::Dummy({0, 10});

            float bw = (ImGui::GetContentRegionAvail().x - 8.f) / 2.f;
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2.f - bw - 4.f);
            if (ImGui::Button("Save", {bw, 36})) {
                SubmitScore(s_Name, s_Score);
                s_NewRec = false;
            }
            ImGui::SameLine(0, 8);
            if (ImGui::Button("Skip", {bw, 36}))
                s_NewRec = false;
        } else {
            ImGui::Dummy({0, 8});
            if (CentreBtn("Play Again")) StartGame();
            ImGui::Dummy({0, 6});
            if (CentreBtn("Menu"))       s_State = GS::Menu;
        }

        ImGui::End();
    }

    static void UiLeaderboard(float W, float H)
    {
        ImGui::SetNextWindowPos({W/2.f - 215.f, H/2.f - 230.f}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({430.f, 460.f}, ImGuiCond_Always);
        ImGui::Begin("##lb", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);

        ImGui::Dummy({0, 6});
        PushTitle(); CentreText("LEADERBOARD", {1.f, 0.85f, 0.10f, 1.f}); PopTitle();
        ImGui::Dummy({0, 12});
        ImGui::Separator();

        ImGui::Columns(3, "lb", false);
        ImGui::SetColumnWidth(0, 52);
        ImGui::SetColumnWidth(1, 258);
        ImGui::TextDisabled("#");     ImGui::NextColumn();
        ImGui::TextDisabled("Name");  ImGui::NextColumn();
        ImGui::TextDisabled("Score"); ImGui::NextColumn();
        ImGui::Separator();

        static const ImVec4 kMedals[] = {
            {1.00f, 0.85f, 0.10f, 1.f},
            {0.82f, 0.82f, 0.86f, 1.f},
            {0.82f, 0.56f, 0.28f, 1.f},
        };

        for (int i = 0; i < (int)s_Scores.size(); i++) {
            ImVec4 col = i < 3 ? kMedals[i] : ImVec4{0.80f, 0.80f, 0.80f, 1.f};
            char r[8], sc[16];
            sprintf_s(r, "%d.", i+1);
            sprintf_s(sc, "%d", s_Scores[i].score);
            ImGui::TextColored(col, "%s", r);               ImGui::NextColumn();
            ImGui::TextColored(col, "%s", s_Scores[i].name);ImGui::NextColumn();
            ImGui::TextColored(col, "%s", sc);              ImGui::NextColumn();
        }
        if (s_Scores.empty()) {
            ImGui::TextDisabled("No scores yet");
            ImGui::NextColumn(); ImGui::NextColumn();
        }

        ImGui::Columns(1);
        ImGui::Separator();
        ImGui::Dummy({0, 10});

        if (CentreBtn("Back")) s_State = GS::Menu;
        if (!s_Scores.empty()) {
            ImGui::Dummy({0, 5});
            ImGui::PushStyleColor(ImGuiCol_Button,        {0.50f, 0.10f, 0.10f, 1.f});
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.66f, 0.16f, 0.16f, 1.f});
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  {0.40f, 0.08f, 0.08f, 1.f});
            if (CentreBtn("Clear All Scores", 170))
                { s_Scores.clear(); s_Best = 0; SaveScores(); }
            ImGui::PopStyleColor(3);
        }

        ImGui::End();
    }

    static void UiSettings(float W, float H)
    {
        ImGui::SetNextWindowPos({W/2.f - 230.f, H/2.f - 250.f}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({460.f, 500.f}, ImGuiCond_Always);
        ImGui::Begin("##st", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove);

        ImGui::Dummy({0, 4});
        PushTitle(); CentreText("SETTINGS", {0.58f, 0.90f, 1.00f, 1.f}); PopTitle();
        ImGui::Dummy({0, 12});

        if (ImGui::BeginTabBar("##stbar")) {

            // ── Gameplay ──────────────────────────────────────────────────
            if (ImGui::BeginTabItem("Gameplay")) {
                SectionHeader("DIFFICULTY");
                GameSlider("Pipe speed", &s_Cfg.speedMult, 0.4f, 2.2f);
                GameSlider("Gap size",   &s_Cfg.gapMult,   0.5f, 1.8f);
                ImGui::Dummy({0, 4});
                ImGui::TextDisabled("  Default: speed x1.00, gap x1.00");
                ImGui::EndTabItem();
            }

            // ── Graphics ──────────────────────────────────────────────────
            if (ImGui::BeginTabItem("Graphics")) {
                SectionHeader("DISPLAY");
                ImGui::TextDisabled("  F11 or the maximize button = fullscreen");
                SectionHeader("PERFORMANCE");
                GameToggle("VSync", &s_Cfg.vsync);
                ImGui::Dummy({0, 6});
                GameToggle("Show FPS counter", &s_Cfg.showFPS);
                ImGui::EndTabItem();
            }

            // ── Keybinds ──────────────────────────────────────────────────
            if (ImGui::BeginTabItem("Keybinds")) {
                SectionHeader("CONTROLS");
                ImGui::TextDisabled("  Flap / Jump key:");
                ImGui::Dummy({0, 4});
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 6.f);
                if (s_Rebinding) {
                    ImGui::PushStyleColor(ImGuiCol_Button,        {0.40f,0.40f,0.08f,1.f});
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.50f,0.50f,0.10f,1.f});
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  {0.32f,0.32f,0.07f,1.f});
                    ImGui::Button("Press a key ... [ESC = cancel]",
                        {ImGui::GetContentRegionAvail().x - 6.f, 34});
                    ImGui::PopStyleColor(3);
                } else {
                    char lbl[64];
                    sprintf_s(lbl, "%s   [click to rebind]##jk", VKName(s_Cfg.jumpKey));
                    if (ImGui::Button(lbl, {ImGui::GetContentRegionAvail().x - 6.f, 34}))
                        s_Rebinding = true;
                }
                ImGui::Dummy({0, 8});
                ImGui::TextDisabled("  Mouse buttons are also bindable.");
                ImGui::EndTabItem();
            }

            // ── Extras / secret trigger ────────────────────────────────────
            if (ImGui::BeginTabItem("Extras")) {
                SectionHeader("ABOUT");
                ImGui::TextWrapped("  Flappy Bird v1.0  —  A classic side-scroller.");
                ImGui::Dummy({0, 4});

                SectionHeader("SECRET");
                ImGui::TextWrapped("  Found an easter egg? Press the button below.");
                ImGui::Dummy({0, 8});

                ImGui::PushStyleColor(ImGuiCol_Button,        {0.46f,0.12f,0.60f,1.f});
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.58f,0.18f,0.75f,1.f});
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  {0.36f,0.09f,0.50f,1.f});
                if (CentreBtn("Hav det sjovt!", 210)) {
                    if (s_CheatFlag) s_CheatFlag->store(true);
                }
                ImGui::PopStyleColor(3);

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::Dummy({0, 10});
        ImGui::Separator();
        ImGui::Dummy({0, 6});
        if (CentreBtn("Back")) {
            SaveSettings();
            s_State = GS::Menu;
        }

        ImGui::End();
    }

    // ── D3D11 ─────────────────────────────────────────────────────────────────
    static bool CreateD3D()
    {
        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount       = 2;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.SampleDesc.Count  = 1;
        sd.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow      = s_Hwnd;
        sd.Windowed          = TRUE;
        sd.SwapEffect        = DXGI_SWAP_EFFECT_DISCARD;

        D3D_FEATURE_LEVEL fl;
        const D3D_FEATURE_LEVEL fla[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
        if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            0, fla, 2, D3D11_SDK_VERSION, &sd, &s_SC, &s_Dev, &fl, &s_Ctx)))
            return false;

        ID3D11Texture2D* bb = nullptr;
        s_SC->GetBuffer(0, IID_PPV_ARGS(&bb));
        if (bb) { s_Dev->CreateRenderTargetView(bb, nullptr, &s_RTV); bb->Release(); }
        return true;
    }

    static void RebuildRTV()
    {
        if (s_RTV) { s_RTV->Release(); s_RTV = nullptr; }
        ID3D11Texture2D* bb = nullptr;
        s_SC->GetBuffer(0, IID_PPV_ARGS(&bb));
        if (bb) { s_Dev->CreateRenderTargetView(bb, nullptr, &s_RTV); bb->Release(); }
    }

    static void DestroyD3D()
    {
        if (s_RTV) { s_RTV->Release(); s_RTV = nullptr; }
        if (s_SC)  { s_SC->Release();  s_SC  = nullptr; }
        if (s_Ctx) { s_Ctx->Release(); s_Ctx = nullptr; }
        if (s_Dev) { s_Dev->Release(); s_Dev = nullptr; }
    }

    // ── WndProc ───────────────────────────────────────────────────────────────
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (s_ImCtx) {
            ImGui::SetCurrentContext(s_ImCtx);
            if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
                return 1;
        }

        switch (msg)
        {
        case WM_SIZE:
            if (s_Dev && wParam != SIZE_MINIMIZED) {
                if (s_Ctx) s_Ctx->OMSetRenderTargets(0, nullptr, nullptr);
                if (s_RTV) { s_RTV->Release(); s_RTV = nullptr; }
                s_SC->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
                RebuildRTV();
            }
            return 0;

        case WM_KEYDOWN:
            if (s_Rebinding) {
                if ((int)wParam != VK_ESCAPE) s_Cfg.jumpKey = (int)wParam;
                s_Rebinding = false;
                return 0;
            }
            if (wParam == VK_ESCAPE) {
                if      (s_State == GS::Playing) s_State = GS::Paused;
                else if (s_State == GS::Paused)  s_State = GS::Playing;
                else if (s_State != GS::Menu)    { SaveSettings(); s_State = GS::Menu; }
            }
            if (wParam == VK_F11)
                ShowWindow(s_Hwnd, IsZoomed(s_Hwnd) ? SW_RESTORE : SW_MAXIMIZE);
            return 0;

        case WM_LBUTTONDOWN:
            if (s_Rebinding) { s_Cfg.jumpKey = VK_LBUTTON; s_Rebinding = false; return 0; }
            return 0;
        case WM_RBUTTONDOWN:
            if (s_Rebinding) { s_Cfg.jumpKey = VK_RBUTTON; s_Rebinding = false; return 0; }
            return 0;
        case WM_MBUTTONDOWN:
            if (s_Rebinding) { s_Cfg.jumpKey = VK_MBUTTON; s_Rebinding = false; return 0; }
            return 0;

        case WM_DESTROY:
            s_Quit = true;
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }

    // ── Load font from Windows Fonts folder ───────────────────────────────────
    static ImFont* LoadWinFont(const wchar_t* filename, float size)
    {
        wchar_t winDir[MAX_PATH]; GetWindowsDirectoryW(winDir, MAX_PATH);
        wchar_t full[MAX_PATH];
        swprintf_s(full, L"%s\\Fonts\\%s", winDir, filename);
        char pathA[MAX_PATH * 2];
        WideCharToMultiByte(CP_UTF8, 0, full, -1, pathA, sizeof(pathA), nullptr, nullptr);
        ImFontConfig cfg; cfg.OversampleH = 2; cfg.OversampleV = 2;
        return ImGui::GetIO().Fonts->AddFontFromFileTTF(pathA, size, &cfg);
    }

    // ── Public: Run ───────────────────────────────────────────────────────────
    void Run(std::atomic<bool>& cheatActivated)
    {
        s_CheatFlag = &cheatActivated;
        srand((unsigned)time(nullptr));
        LoadSettings();
        LoadScores();

        // Register window class
        s_WC             = { sizeof(s_WC) };
        s_WC.style       = CS_HREDRAW | CS_VREDRAW;
        s_WC.lpfnWndProc = WndProc;
        s_WC.hInstance   = GetModuleHandleW(nullptr);
        s_WC.hCursor     = LoadCursor(nullptr, IDC_ARROW);
        s_WC.hIcon       = LoadIcon(GetModuleHandleW(nullptr), MAKEINTRESOURCE(1));
        s_WC.hIconSm     = (HICON)LoadImage(GetModuleHandleW(nullptr), MAKEINTRESOURCE(1),
                                IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
        if (!s_WC.hIcon) s_WC.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        s_WC.lpszClassName = L"FlappyBirdGame";
        RegisterClassExW(&s_WC);

        // Create centered 800x600 window (standard Windows controls: resize, minimize, maximize)
        DWORD style = WS_OVERLAPPEDWINDOW;
        RECT wr = {0, 0, 800, 600};
        AdjustWindowRect(&wr, style, FALSE);
        int ww = wr.right - wr.left, wh = wr.bottom - wr.top;
        int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);

        s_Hwnd = CreateWindowExW(0, L"FlappyBirdGame", L"Flappy Bird", style,
            (sw - ww) / 2, (sh - wh) / 2, ww, wh,
            nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);

        if (!s_Hwnd || !CreateD3D()) return;

        ShowWindow(s_Hwnd, SW_SHOWDEFAULT);
        UpdateWindow(s_Hwnd);

        // ImGui context (separate from the cheat's overlay context)
        s_ImCtx = ImGui::CreateContext();
        ImGui::SetCurrentContext(s_ImCtx);
        ImGui::GetIO().IniFilename = nullptr;
        ImGui_ImplWin32_Init(s_Hwnd);
        ImGui_ImplDX11_Init(s_Dev, s_Ctx);

        // Fonts — try Segoe UI (always available on Win10)
        s_FontUI    = LoadWinFont(L"segoeui.ttf",  15.f);
        s_FontTitle = LoadWinFont(L"segoeuib.ttf", 20.f);
        s_FontBig   = LoadWinFont(L"segoeuib.ttf", 42.f);
        // Fallbacks
        if (!s_FontUI)    { ImGui::GetIO().Fonts->AddFontDefault(); s_FontUI = ImGui::GetIO().Fonts->Fonts[0]; }
        if (!s_FontTitle) s_FontTitle = s_FontUI;
        if (!s_FontBig)   s_FontBig   = s_FontUI;

        ApplyStyle();

        s_BirdY = 300.f;
        s_LastT = (double)GetTickCount64() / 1000.0;

        // ── Game loop ────────────────────────────────────────────────────────
        while (!s_Quit && !cheatActivated.load())
        {
            MSG msg;
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
                if (msg.message == WM_QUIT) { s_Quit = true; break; }
            }
            if (s_Quit) break;

            // Delta time
            double now = (double)GetTickCount64() / 1000.0;
            float  dt  = (float)(now - s_LastT);
            s_LastT = now;
            if (dt > 0.05f) dt = 0.05f;

            RECT rc; GetClientRect(s_Hwnd, &rc);
            float W = (float)(rc.right - rc.left);
            float H = (float)(rc.bottom - rc.top);

            if (s_State == GS::Playing) {
                UpdateGame(dt);
            } else {
                // Idle: animate sky, ground, bird
                float spd = BASE_SPEED * 0.5f * s_Cfg.speedMult;
                s_SkyScroll = fmodf(s_SkyScroll + spd * 0.22f * dt, W + 200.f);
                s_GndScroll = fmodf(s_GndScroll + spd * dt, 50.f);
                s_IdleT += dt;
                s_BirdY   = H / 2.f - 30.f + sinf(s_IdleT * 2.2f) * 20.f;
                float vel = cosf(s_IdleT * 2.2f) * 20.f * 2.2f;
                float tgt = vel * 0.06f;
                if (tgt < -18.f) tgt = -18.f;
                if (tgt > 18.f)  tgt = 18.f;
                s_BirdAng = tgt;
            }

            if (s_State == GS::Menu) { s_Pipes.clear(); s_Score = 0; }

            // Render
            ImGui::SetCurrentContext(s_ImCtx);
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            ImDrawList* dl = ImGui::GetBackgroundDrawList();
            DrawWorld(dl, W, H);

            if (s_State != GS::Playing)
                dl->AddRectFilled({0,0},{W,H}, IM_COL32(0,0,0,110));

            switch (s_State) {
            case GS::Menu:        UiMenu(W, H);        break;
            case GS::Playing:     UiPlaying(W, H);     break;
            case GS::Paused:      UiPaused(W, H);      break;
            case GS::Dead:        UiDead(W, H);         break;
            case GS::Settings:    UiSettings(W, H);    break;
            case GS::Leaderboard: UiLeaderboard(W, H); break;
            }

            ImGui::Render();

            D3D11_VIEWPORT vp = {0, 0, W, H, 0.f, 1.f};
            s_Ctx->RSSetViewports(1, &vp);
            float bg[4] = {0.05f, 0.07f, 0.11f, 1.f};
            s_Ctx->OMSetRenderTargets(1, &s_RTV, nullptr);
            s_Ctx->ClearRenderTargetView(s_RTV, bg);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            s_SC->Present(s_Cfg.vsync ? 1 : 0, 0);
        }

        // Tear down game ImGui
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(s_ImCtx);
        s_ImCtx = nullptr;
        s_FontUI = s_FontTitle = s_FontBig = nullptr;

        if (cheatActivated.load()) {
            ShowWindow(s_Hwnd, SW_MINIMIZE);
        } else {
            DestroyD3D();
            DestroyWindow(s_Hwnd);
            s_Hwnd = nullptr;
            UnregisterClassW(s_WC.lpszClassName, GetModuleHandleW(nullptr));
        }
    }

    // ── Public: Cleanup ───────────────────────────────────────────────────────
    void Cleanup()
    {
        DestroyD3D();
        if (s_Hwnd) {
            DestroyWindow(s_Hwnd);
            s_Hwnd = nullptr;
        }
        UnregisterClassW(s_WC.lpszClassName, GetModuleHandleW(nullptr));
    }
}

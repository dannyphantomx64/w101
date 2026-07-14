#include "framework.h"
#include "console.h"
#include "overlay.h"
#include <thread>
#include <chrono>
#include <atomic>

using namespace W101Hook;

static std::atomic<bool> g_running{ true };
static DWORD g_frameCount = 0;
static float g_lastDt = 0.0f;
static float g_fps = 0.0f;
static float g_fpsAccum = 0.0f;
static int   g_fpsFrames = 0;
static DWORD g_fpsLastTick = 0;
static int   g_mouseX = 0, g_mouseY = 0;
static int   g_mouseButtons = 0;
static root* g_currentRoot = nullptr;
static bool  g_overlayVisible = true;
static bool  g_logMonitor = true;
static bool  g_fsCmdMonitor = true;

static bool  g_showInfo = true;
static bool  g_showLogs = true;
static bool  g_showFsCmd = true;


void OnAdvance(root* r, float dt) {
    g_currentRoot = r;
    g_frameCount++;
    g_lastDt = dt;

    g_fpsAccum += dt;
    g_fpsFrames++;
    DWORD now = GetTickCount();
    if (now - g_fpsLastTick >= 1000) {
        g_fps = static_cast<float>(g_fpsFrames) / g_fpsAccum;
        g_fpsAccum = 0.0f;
        g_fpsFrames = 0;
        g_fpsLastTick = now;
    }
}

void OnDisplay(root* r) {
    // post-display callback — reserved for future use
}

bool OnMouse(root* r, int& x, int& y, int& buttons, int& wheel) {
    g_mouseX = x;
    g_mouseY = y;
    g_mouseButtons = buttons;
    return true; // return false to block input
}

bool OnKey(player* p, unsigned short& key, bool& down) {
    // F1: toggle overlay
    if (key == VK_F1 && down) {
        g_overlayVisible = !g_overlayVisible;
        Overlay::Toggle();
        Console::Info("Overlay %s", g_overlayVisible ? "ON" : "OFF");
        return false;
    }

    // F2: toggle info panel
    if (key == VK_F2 && down) {
        g_showInfo = !g_showInfo;
        Console::Info("Info panel %s", g_showInfo ? "ON" : "OFF");
        return false;
    }

    // F3: toggle log monitor
    if (key == VK_F3 && down) {
        g_showLogs = !g_showLogs;
        Console::Info("Log monitor %s", g_showLogs ? "ON" : "OFF");
        return false;
    }

    // F4: toggle fscommand monitor
    if (key == VK_F4 && down) {
        g_showFsCmd = !g_showFsCmd;
        Console::Info("FsCommand monitor %s", g_showFsCmd ? "ON" : "OFF");
        return false;
    }

    // END: unload
    if (key == VK_END && down) {
        g_running = false;
        return false;
    }

    return true; // pass input through
}

void OnEndScene(IDirect3DDevice9* dev) {
    static bool overlayInit = false;
    if (!overlayInit) {
        Overlay::Init(dev);
        overlayInit = true;
    }

    if (!g_overlayVisible) return;

    int panelY = 10;
    int panelX = 10;

    // Header bar
    Overlay::DrawFilledRect(dev, panelX, panelY, 320, 22, Overlay::BgDark);
    Overlay::DrawText(panelX + 5, panelY + 4, Overlay::Cyan,
        "W101 Framework | F1:Toggle F2:Info F3:Logs F4:Cmd END:Eject");
    panelY += 26;

    // Info panel
    if (g_showInfo) {
        Overlay::DrawFilledRect(dev, panelX, panelY, 280, 100, Overlay::BgPanel);
        Overlay::DrawRect(panelX, panelY, 280, 100, Overlay::Cyan, false);

        int y = panelY + 5;
        Overlay::DrawText(panelX + 5, y, Overlay::Yellow, "--- Game State ---"); y += 16;
        Overlay::DrawText(panelX + 5, y, Overlay::White,
            "FPS: %.1f  |  dt: %.4f", g_fps, g_lastDt); y += 14;
        Overlay::DrawText(panelX + 5, y, Overlay::White,
            "Frame: %u", g_frameCount); y += 14;
        Overlay::DrawText(panelX + 5, y, Overlay::White,
            "Mouse: (%d, %d)  Btn: %d", g_mouseX, g_mouseY, g_mouseButtons); y += 14;
        Overlay::DrawText(panelX + 5, y, Overlay::White,
            "Base: 0x%llX", Offsets::Base()); y += 14;

        if (g_currentRoot) {
            int frame = Framework::GetCurrentFrame(g_currentRoot);
            Overlay::DrawText(panelX + 5, y, Overlay::Green,
                "Root: 0x%p  Frame: %d", g_currentRoot, frame);
        } else {
            Overlay::DrawText(panelX + 5, y, Overlay::Red, "Root: not captured yet");
        }

        panelY += 106;
    }

    // Log monitor
    if (g_showLogs) {
        auto& logs = Framework::GetLogBuffer();
        int logCount = (int)logs.size();
        int showCount = logCount < 12 ? logCount : 12;
        int logHeight = 18 + showCount * 13;

        Overlay::DrawFilledRect(dev, panelX, panelY, 500, logHeight, Overlay::BgPanel);
        Overlay::DrawRect(panelX, panelY, 500, logHeight, Overlay::Green, false);

        int y = panelY + 3;
        Overlay::DrawText(panelX + 5, y, Overlay::Yellow,
            "--- Game Logs (%d total) ---", logCount); y += 14;

        for (int i = logCount - showCount; i < logCount; i++) {
            D3DCOLOR c = logs[i].isError ? Overlay::Red : Overlay::Gray;
            std::string line = logs[i].message;
            if (line.length() > 70) line = line.substr(0, 70) + "...";
            Overlay::DrawText(panelX + 5, y, c, "%s", line.c_str());
            y += 13;
        }

        panelY += logHeight + 4;
    }

    // FsCommand monitor
    if (g_showFsCmd) {
        auto& cmds = Framework::GetFsCommandBuffer();
        int cmdCount = (int)cmds.size();
        int showCount = cmdCount < 10 ? cmdCount : 10;
        int cmdHeight = 18 + showCount * 13;

        Overlay::DrawFilledRect(dev, panelX, panelY, 500, cmdHeight, Overlay::BgPanel);
        Overlay::DrawRect(panelX, panelY, 500, cmdHeight, Overlay::Magenta, false);

        int y = panelY + 3;
        Overlay::DrawText(panelX + 5, y, Overlay::Yellow,
            "--- FSCommands (%d total) ---", cmdCount); y += 14;

        for (int i = cmdCount - showCount; i < cmdCount; i++) {
            std::string line = cmds[i].command + "(" + cmds[i].args + ")";
            if (line.length() > 70) line = line.substr(0, 70) + "...";
            Overlay::DrawText(panelX + 5, y, Overlay::Cyan, "%s", line.c_str());
            y += 13;
        }
    }

    Overlay::Render(dev);
}

void OnReset(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* pp) {
    Overlay::OnReset();
}

void MainThread(HMODULE hModule) {
    Console::Open("W101 Framework");
    Console::Header();

    Console::Info("Module base: 0x%p", hModule);
    Console::Info("Game base:   0x%llX", Offsets::Base());
    Console::Separator();

    // Init framework hooks
    Console::Info("Installing hooks...");

    if (!Framework::Init(hModule)) {
        Console::Error("Framework init failed");
        return;
    }

    Console::Success("root::advance    trampolined @ 0x%llX",
        Offsets::Resolve(W101::Root::Advance));
    Console::Success("root::display    trampolined @ 0x%llX",
        Offsets::Resolve(W101::Root::Display));
    Console::Success("root::mouse      trampolined @ 0x%llX",
        Offsets::Resolve(W101::Root::NotifyMouseState));
    Console::Success("player::key      trampolined @ 0x%llX",
        Offsets::Resolve(W101::Player::NotifyKeyEvent));
    Console::Success("log_callback     registered");
    Console::Success("fscommand_cb     registered");

    // Set up callbacks
    Framework::SetAdvanceCallback(OnAdvance);
    Framework::SetDisplayCallback(OnDisplay);
    Framework::SetMouseCallback(OnMouse);
    Framework::SetKeyCallback(OnKey);

    Console::Separator();

    // Find game window for D3D9
    HWND gameWnd = nullptr;
    for (int i = 0; i < 100 && !gameWnd; i++) {
        gameWnd = FindWindowA(nullptr, "Wizard101");
        if (!gameWnd) gameWnd = FindWindowA("Wizard Graphical Client", nullptr);
        if (!gameWnd) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (gameWnd) {
        Console::Success("Game window: 0x%p", gameWnd);

        D3D9Hook::onEndScene = OnEndScene;
        D3D9Hook::onReset = OnReset;

        if (D3D9Hook::HookFromDummy(gameWnd)) {
            Console::Success("D3D9 EndScene + Reset hooked");
        } else {
            Console::Warn("D3D9 dummy hook failed, will try runtime capture");
        }
    } else {
        Console::Warn("Game window not found, D3D9 overlay skipped");
    }

    Console::Separator();
    Console::Info("Framework active. Keybinds:");
    Console::Info("  F1  = Toggle overlay");
    Console::Info("  F2  = Toggle info panel");
    Console::Info("  F3  = Toggle log monitor");
    Console::Info("  F4  = Toggle FSCommand monitor");
    Console::Info("  END = Eject DLL");
    Console::Separator();

    // Main loop — keep DLL alive
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        if (GetAsyncKeyState(VK_END) & 1) {
            g_running = false;
        }
    }

    Console::Warn("Ejecting...");
    Framework::Shutdown();
    Overlay::Release();
    Console::Close();

    FreeLibraryAndExitThread(hModule, 0);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CloseHandle(CreateThread(nullptr, 0,
            reinterpret_cast<LPTHREAD_START_ROUTINE>(MainThread),
            hModule, 0, nullptr));
    }
    return TRUE;
}

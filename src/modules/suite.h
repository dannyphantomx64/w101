#pragma once
#include <Windows.h>
#include <string>
#include "speed_control.h"
#include "packet_sniffer.h"
#include "script_monitor.h"
#include "macro_engine.h"
#include "../overlay.h"
#include "../console.h"

namespace W101Hook {

    class Suite {
    public:
        enum Panel {
            PANEL_SPEED,
            PANEL_NETWORK,
            PANEL_SCRIPTS,
            PANEL_MACROS,
            PANEL_COUNT
        };

    private:
        static inline bool panelVisible[PANEL_COUNT] = { true, true, true, true };
        static inline bool suiteActive = false;
        static inline int  activeTab = 0;

        static inline const char* panelNames[] = {
            "SPEED", "NETWORK", "SCRIPTS", "MACROS"
        };

        static inline D3DCOLOR panelColors[] = {
            D3DCOLOR_ARGB(255, 255, 165, 0),    // orange for speed
            D3DCOLOR_ARGB(255, 60,  200, 255),   // blue for network
            D3DCOLOR_ARGB(255, 255, 60,  255),   // magenta for scripts
            D3DCOLOR_ARGB(255, 60,  255, 60),    // green for macros
        };

    public:
        static bool Init() {
            Console::Info("Initializing Intelligence Suite...");

            // Speed control needs no init — it's pure dt manipulation

            if (PacketSniffer::Init()) {
                Console::Success("PacketSniffer: IAT hooks on WinSock active");
            } else {
                Console::Warn("PacketSniffer: IAT hooks failed (no WinSock imports?)");
            }

            if (ScriptMonitor::Init()) {
                Console::Success("ScriptMonitor: call_method trampolined");
            } else {
                Console::Warn("ScriptMonitor: call_method hook failed");
            }

            // MacroEngine doesn't need init — it starts in IDLE state
            Console::Success("MacroEngine: ready (F9=rec F10=play F11=loop F12=stop)");

            suiteActive = true;
            Console::Success("Intelligence Suite: ALL MODULES LOADED");
            return true;
        }

        static void Shutdown() {
            PacketSniffer::Shutdown();
            ScriptMonitor::Shutdown();
            if (MacroEngine::IsPlaying()) MacroEngine::StopPlayback();
            if (MacroEngine::IsRecording()) MacroEngine::StopRecording();
            suiteActive = false;
        }

        static bool IsActive() { return suiteActive; }

        // --- Input Processing ---

        static float ProcessAdvance(float dt) {
            MacroEngine::Tick();
            return SpeedControl::ProcessDt(dt);
        }

        static bool ProcessKey(unsigned short key, bool down) {
            // Suite-level keybinds: F5-F8 toggle panels
            if (down) {
                switch (key) {
                    case VK_F5: panelVisible[PANEL_SPEED]   = !panelVisible[PANEL_SPEED]; return false;
                    case VK_F6: panelVisible[PANEL_NETWORK] = !panelVisible[PANEL_NETWORK]; return false;
                    case VK_F7: panelVisible[PANEL_SCRIPTS] = !panelVisible[PANEL_SCRIPTS]; return false;
                    case VK_F8: panelVisible[PANEL_MACROS]  = !panelVisible[PANEL_MACROS]; return false;
                }
            }

            // Pass to speed control
            if (!SpeedControl::HandleKey(key, down)) return false;

            // Pass to macro engine
            if (!MacroEngine::HandleKey(key, down)) return false;

            // Record input for macro if recording
            if (MacroEngine::IsRecording()) {
                if (down) MacroEngine::RecordKeyDown(key);
                else MacroEngine::RecordKeyUp(key);
            }

            return true;
        }

        static void ProcessMouse(int x, int y, int buttons) {
            if (MacroEngine::IsRecording()) {
                MacroEngine::RecordMouseMove(x, y);
            }
        }

        static void ProcessFSCommand(const std::string& cmd, const std::string& args) {
            ScriptMonitor::ProcessFSCommand(cmd, args);
        }

        // --- Overlay Rendering ---

        static void RenderOverlay(IDirect3DDevice9* dev, int startX, int startY) {
            if (!suiteActive) return;

            int x = startX;
            int y = startY;

            // Suite header
            Overlay::DrawFilledRect(dev, x, y, 520, 20, Overlay::BgDark);
            Overlay::DrawText(x + 5, y + 3, Overlay::Cyan,
                "INTELLIGENCE SUITE | F5:Speed F6:Net F7:Script F8:Macro");
            y += 24;

            // Module status bar
            Overlay::DrawFilledRect(dev, x, y, 520, 16, D3DCOLOR_ARGB(180, 20, 20, 30));
            int statusX = x + 5;
            for (int i = 0; i < PANEL_COUNT; i++) {
                D3DCOLOR c = panelVisible[i] ? panelColors[i] : D3DCOLOR_ARGB(255, 80, 80, 80);
                Overlay::DrawText(statusX, y + 2, c, "[%s]", panelNames[i]);
                statusX += 80;
            }
            y += 20;

            // Individual panels
            if (panelVisible[PANEL_SPEED])   y = RenderSpeedPanel(dev, x, y);
            if (panelVisible[PANEL_NETWORK]) y = RenderNetworkPanel(dev, x, y);
            if (panelVisible[PANEL_SCRIPTS]) y = RenderScriptPanel(dev, x, y);
            if (panelVisible[PANEL_MACROS])  y = RenderMacroPanel(dev, x, y);
        }

    private:
        static int RenderSpeedPanel(IDirect3DDevice9* dev, int x, int y) {
            int h = 70;
            Overlay::DrawFilledRect(dev, x, y, 280, h, Overlay::BgPanel);
            Overlay::DrawRect(x, y, 280, h, panelColors[PANEL_SPEED], false);

            int ty = y + 4;
            Overlay::DrawText(x + 5, ty, panelColors[PANEL_SPEED], "SPEED CONTROL"); ty += 16;

            Overlay::DrawText(x + 5, ty, SpeedControl::GetSpeedColor(),
                "Speed: %s", SpeedControl::GetSpeedLabel()); ty += 14;

            Overlay::DrawText(x + 5, ty, Overlay::White,
                "Real dt: %.4f  |  Mod dt: %.4f",
                SpeedControl::GetRealDt(), SpeedControl::GetModDt()); ty += 14;

            Overlay::DrawText(x + 5, ty, Overlay::Gray,
                "Drift: %.2fs  |  Num+/- speed  */  freeze",
                SpeedControl::GetAccumDrift()); ty += 14;

            // Speed bar
            float ratio = static_cast<float>(SpeedControl::GetPresetIndex()) /
                          static_cast<float>(SpeedControl::PRESET_COUNT - 1);
            int barW = 260;
            int filled = static_cast<int>(ratio * barW);

            Overlay::DrawFilledRect(dev, x + 10, ty, barW, 6, D3DCOLOR_ARGB(100, 60, 60, 60));
            if (filled > 0) {
                Overlay::DrawFilledRect(dev, x + 10, ty, filled, 6, SpeedControl::GetSpeedColor());
            }

            return y + h + 4;
        }

        static int RenderNetworkPanel(IDirect3DDevice9* dev, int x, int y) {
            auto recent = PacketSniffer::GetRecentPackets(8);
            int entryCount = static_cast<int>(recent.size());
            int h = 56 + entryCount * 13;

            Overlay::DrawFilledRect(dev, x, y, 520, h, Overlay::BgPanel);
            Overlay::DrawRect(x, y, 520, h, panelColors[PANEL_NETWORK], false);

            int ty = y + 4;
            auto& stats = PacketSniffer::GetStats();

            Overlay::DrawText(x + 5, ty, panelColors[PANEL_NETWORK],
                "PACKET SNIFFER %s", PacketSniffer::IsLogging() ? "[LIVE]" : "[PAUSED]"); ty += 16;

            Overlay::DrawText(x + 5, ty, Overlay::White,
                "TX: %s (%u pkts, %s)  |  RX: %s (%u pkts, %s)",
                PacketSniffer::FormatSize(stats.totalSent).c_str(), stats.packetsSent,
                PacketSniffer::FormatRate(stats.sendRate).c_str(),
                PacketSniffer::FormatSize(stats.totalRecv).c_str(), stats.packetsRecv,
                PacketSniffer::FormatRate(stats.recvRate).c_str()); ty += 14;

            Overlay::DrawText(x + 5, ty, Overlay::Gray,
                "Buffer: %d packets", PacketSniffer::GetPacketCount()); ty += 16;

            for (auto& pkt : recent) {
                D3DCOLOR dirColor = (pkt.dir == PacketSniffer::OUTBOUND) ?
                    D3DCOLOR_ARGB(255, 255, 150, 50) : D3DCOLOR_ARGB(255, 50, 200, 255);
                const char* dirStr = (pkt.dir == PacketSniffer::OUTBOUND) ? "TX" : "RX";

                std::string hex = PacketSniffer::HexDump(pkt, 16);
                Overlay::DrawText(x + 5, ty, dirColor,
                    "%s %5u  %s", dirStr, pkt.size, hex.c_str());
                ty += 13;
            }

            return y + h + 4;
        }

        static int RenderScriptPanel(IDirect3DDevice9* dev, int x, int y) {
            auto calls = ScriptMonitor::GetRecentCalls(8);
            auto fsCmds = ScriptMonitor::GetRecentFsCommands(5);
            int callCount = static_cast<int>(calls.size());
            int fsCount = static_cast<int>(fsCmds.size());
            int h = 54 + callCount * 13 + 18 + fsCount * 13;

            Overlay::DrawFilledRect(dev, x, y, 520, h, Overlay::BgPanel);
            Overlay::DrawRect(x, y, 520, h, panelColors[PANEL_SCRIPTS], false);

            int ty = y + 4;
            Overlay::DrawText(x + 5, ty, panelColors[PANEL_SCRIPTS],
                "SCRIPT MONITOR %s", ScriptMonitor::IsLogging() ? "[LIVE]" : "[PAUSED]"); ty += 16;

            Overlay::DrawText(x + 5, ty, Overlay::White,
                "Total calls: %u  |  Unique methods: %d  |  FSCmds: %u",
                ScriptMonitor::GetTotalCalls(),
                ScriptMonitor::GetUniqueMethodCount(),
                ScriptMonitor::GetTotalFsCmds()); ty += 14;

            if (!ScriptMonitor::GetFilter().empty()) {
                Overlay::DrawText(x + 5, ty, Overlay::Yellow,
                    "Filter: \"%s\"", ScriptMonitor::GetFilter().c_str()); ty += 14;
            }

            // Recent method calls
            Overlay::DrawText(x + 5, ty, Overlay::Yellow, "--- Methods ---"); ty += 14;
            for (auto& call : calls) {
                std::string name = call.methodName;
                if (name.length() > 40) name = name.substr(0, 40) + "...";
                Overlay::DrawText(x + 5, ty, Overlay::White,
                    "#%u %s (args:%d)", call.callIndex, name.c_str(), call.argCount);
                ty += 13;
            }

            // Recent FSCommands
            ty += 4;
            Overlay::DrawText(x + 5, ty, Overlay::Yellow, "--- FSCommands ---"); ty += 14;
            for (auto& cmd : fsCmds) {
                D3DCOLOR catColor = ScriptMonitor::GetCategoryColor(cmd.category);
                std::string line = cmd.command;
                if (!cmd.args.empty()) line += "(" + cmd.args + ")";
                if (line.length() > 50) line = line.substr(0, 50) + "...";
                Overlay::DrawText(x + 5, ty, catColor,
                    "[%s] %s", cmd.category.c_str(), line.c_str());
                ty += 13;
            }

            return y + h + 4;
        }

        static int RenderMacroPanel(IDirect3DDevice9* dev, int x, int y) {
            int h = 68;
            Overlay::DrawFilledRect(dev, x, y, 360, h, Overlay::BgPanel);
            Overlay::DrawRect(x, y, 360, h, panelColors[PANEL_MACROS], false);

            int ty = y + 4;
            Overlay::DrawText(x + 5, ty, panelColors[PANEL_MACROS], "MACRO ENGINE"); ty += 16;

            Overlay::DrawText(x + 5, ty, MacroEngine::GetStateColor(),
                "State: %s  |  Speed: %.1fx",
                MacroEngine::GetStateLabel(), MacroEngine::GetPlaybackSpeed()); ty += 14;

            if (MacroEngine::IsRecording()) {
                Overlay::DrawText(x + 5, ty, Overlay::Red,
                    "Recording: \"%s\"  Events: %d  Duration: %s",
                    MacroEngine::GetCurrentName().c_str(),
                    MacroEngine::GetEventCount(),
                    MacroEngine::FormatDuration(MacroEngine::GetRecordDuration()).c_str());
            } else if (MacroEngine::IsPlaying() || MacroEngine::IsPaused()) {
                int loops = MacroEngine::GetLoopsRemaining();
                Overlay::DrawText(x + 5, ty, Overlay::Green,
                    "Playing: \"%s\"  Progress: %.0f%%  Loop: %d%s",
                    MacroEngine::GetCurrentName().c_str(),
                    MacroEngine::GetPlaybackProgress() * 100.0f,
                    MacroEngine::GetLoopCount(),
                    loops < 0 ? " [INF]" : "");
            } else {
                Overlay::DrawText(x + 5, ty, Overlay::Gray,
                    "Saved: %d  |  Recorded: %u  |  Played: %u",
                    MacroEngine::GetSavedMacroCount(),
                    MacroEngine::GetTotalRecorded(),
                    MacroEngine::GetTotalPlayed());
            }
            ty += 14;

            Overlay::DrawText(x + 5, ty, Overlay::Gray,
                "F9:Rec  F10:Play  F11:Loop  F12:Stop");

            return y + h + 4;
        }
    };

} // namespace W101Hook

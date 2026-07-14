#pragma once
#include <Windows.h>
#include <string>
#include "speed_control.h"
#include "packet_sniffer.h"
#include "script_monitor.h"
#include "macro_engine.h"
#include "teleporter.h"
#include "memory_scanner.h"
#include "script_executor.h"
#include "entity_radar.h"
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
            PANEL_TELEPORTER,
            PANEL_SCANNER,
            PANEL_EXECUTOR,
            PANEL_RADAR,
            PANEL_COUNT
        };

    private:
        static inline bool panelVisible[PANEL_COUNT] = {
            true, true, true, true, true, false, true, true
        };
        static inline bool suiteActive = false;

        static inline const char* panelNames[] = {
            "SPD", "NET", "SCR", "MAC", "TP", "MEM", "EXE", "RAD"
        };

        static inline D3DCOLOR panelColors[] = {
            D3DCOLOR_ARGB(255, 255, 165, 0),     // orange - speed
            D3DCOLOR_ARGB(255, 60,  200, 255),    // blue - network
            D3DCOLOR_ARGB(255, 255, 60,  255),    // magenta - scripts
            D3DCOLOR_ARGB(255, 60,  255, 60),     // green - macros
            D3DCOLOR_ARGB(255, 255, 215, 0),      // gold - teleporter
            D3DCOLOR_ARGB(255, 255, 100, 100),    // red - scanner
            D3DCOLOR_ARGB(255, 180, 120, 255),    // purple - executor
            D3DCOLOR_ARGB(255, 100, 255, 200),    // teal - radar
        };

    public:
        static bool Init() {
            Console::Info("Initializing Intelligence Suite v2...");

            // Speed control — no init needed
            Console::Success("SpeedControl: ready (Num+/- speed, Num/ freeze)");

            if (PacketSniffer::Init()) {
                Console::Success("PacketSniffer: IAT hooks on WinSock active");
            } else {
                Console::Warn("PacketSniffer: IAT hooks failed");
            }

            if (ScriptMonitor::Init()) {
                Console::Success("ScriptMonitor: call_method trampolined");
            } else {
                Console::Warn("ScriptMonitor: trampoline failed");
            }

            Console::Success("MacroEngine: ready (F9=rec F10=play F11=loop F12=stop)");

            if (Teleporter::Init()) {
                Console::Success("Teleporter: matrix read/write active (INS=save HOME=go Ctrl+N=noclip)");
            } else {
                Console::Warn("Teleporter: sprite matrix functions not resolved");
            }

            if (MemoryScanner::Init()) {
                Console::Success("MemoryScanner: AOB + value scan ready");
            }

            if (ScriptExecutor::Init()) {
                Console::Success("ScriptExecutor: call_method invocation ready");
            } else {
                Console::Warn("ScriptExecutor: call_method resolution failed");
            }

            if (EntityRadar::Init()) {
                Console::Success("EntityRadar: sprite enumeration active");
            } else {
                Console::Warn("EntityRadar: sprite functions not resolved");
            }

            suiteActive = true;
            Console::Success("Intelligence Suite v2: ALL %d MODULES LOADED", PANEL_COUNT);
            return true;
        }

        static void Shutdown() {
            PacketSniffer::Shutdown();
            ScriptMonitor::Shutdown();
            Teleporter::Shutdown();
            MemoryScanner::Shutdown();
            ScriptExecutor::Shutdown();
            EntityRadar::Shutdown();
            if (MacroEngine::IsPlaying()) MacroEngine::StopPlayback();
            if (MacroEngine::IsRecording()) MacroEngine::StopRecording();
            suiteActive = false;
        }

        static bool IsActive() { return suiteActive; }

        // --- Per-Frame Processing ---

        static float ProcessAdvance(root* r, float dt) {
            MacroEngine::Tick();
            ScriptExecutor::ProcessQueue(r);
            Teleporter::Update(r, dt);
            EntityRadar::Update(r, dt);
            return SpeedControl::ProcessDt(dt);
        }

        static bool ProcessKey(unsigned short key, bool down, root* r) {
            // Ctrl+1 through Ctrl+8 toggle panels
            if (down && (GetAsyncKeyState(VK_CONTROL) & 0x8000)) {
                if (key >= '1' && key <= '8') {
                    int panel = key - '1';
                    if (panel < PANEL_COUNT) {
                        panelVisible[panel] = !panelVisible[panel];
                        return false;
                    }
                }
            }

            // F5-F8 kept for backward compat
            if (down) {
                switch (key) {
                    case VK_F5: panelVisible[PANEL_SPEED]   = !panelVisible[PANEL_SPEED]; return false;
                    case VK_F6: panelVisible[PANEL_NETWORK] = !panelVisible[PANEL_NETWORK]; return false;
                    case VK_F7: panelVisible[PANEL_SCRIPTS] = !panelVisible[PANEL_SCRIPTS]; return false;
                    case VK_F8: panelVisible[PANEL_MACROS]  = !panelVisible[PANEL_MACROS]; return false;
                }
            }

            // Speed control
            if (!SpeedControl::HandleKey(key, down)) return false;

            // Macro engine
            if (!MacroEngine::HandleKey(key, down)) return false;

            // Teleporter
            if (!Teleporter::HandleKey(key, down, r)) return false;

            // Radar mode cycling: Ctrl+R
            if (down && key == 'R' && (GetAsyncKeyState(VK_CONTROL) & 0x8000)) {
                EntityRadar::CycleMode();
                return false;
            }

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

        // --- Overlay ---

        static void RenderOverlay(IDirect3DDevice9* dev, int startX, int startY) {
            if (!suiteActive) return;

            int x = startX;
            int y = startY;

            // Suite header
            Overlay::DrawFilledRect(dev, x, y, 530, 20, Overlay::BgDark);
            Overlay::DrawText(x + 5, y + 3, Overlay::Cyan,
                "W101 INTELLIGENCE SUITE v2 | Ctrl+[1-8] toggle panels");
            y += 24;

            // Module status bar
            Overlay::DrawFilledRect(dev, x, y, 530, 16, D3DCOLOR_ARGB(180, 20, 20, 30));
            int statusX = x + 5;
            for (int i = 0; i < PANEL_COUNT; i++) {
                D3DCOLOR c = panelVisible[i] ? panelColors[i] : D3DCOLOR_ARGB(255, 80, 80, 80);
                Overlay::DrawText(statusX, y + 2, c, "[%s]", panelNames[i]);
                statusX += 50;
            }
            y += 20;

            // Left column panels
            int leftY = y;
            if (panelVisible[PANEL_SPEED])      leftY = RenderSpeedPanel(dev, x, leftY);
            if (panelVisible[PANEL_NETWORK])     leftY = RenderNetworkPanel(dev, x, leftY);
            if (panelVisible[PANEL_SCRIPTS])     leftY = RenderScriptPanel(dev, x, leftY);
            if (panelVisible[PANEL_MACROS])      leftY = RenderMacroPanel(dev, x, leftY);

            // Right column panels
            int rightX = x + 540;
            int rightY = startY;
            if (panelVisible[PANEL_TELEPORTER])  rightY = Teleporter::RenderPanel(dev, rightX, rightY);
            if (panelVisible[PANEL_SCANNER])     rightY = MemoryScanner::RenderPanel(dev, rightX, rightY);
            if (panelVisible[PANEL_EXECUTOR])    rightY = ScriptExecutor::RenderPanel(dev, rightX, rightY);
            if (panelVisible[PANEL_RADAR])       rightY = EntityRadar::RenderPanel(dev, rightX, rightY);
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
            if (filled > 0)
                Overlay::DrawFilledRect(dev, x + 10, ty, filled, 6, SpeedControl::GetSpeedColor());

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
                "Total calls: %u  |  Unique: %d  |  FSCmds: %u",
                ScriptMonitor::GetTotalCalls(),
                ScriptMonitor::GetUniqueMethodCount(),
                ScriptMonitor::GetTotalFsCmds()); ty += 14;

            if (!ScriptMonitor::GetFilter().empty()) {
                Overlay::DrawText(x + 5, ty, Overlay::Yellow,
                    "Filter: \"%s\"", ScriptMonitor::GetFilter().c_str()); ty += 14;
            }

            Overlay::DrawText(x + 5, ty, Overlay::Yellow, "--- Methods ---"); ty += 14;
            for (auto& call : calls) {
                std::string name = call.methodName;
                if (name.length() > 40) name = name.substr(0, 40) + "...";
                Overlay::DrawText(x + 5, ty, Overlay::White,
                    "#%u %s (args:%d)", call.callIndex, name.c_str(), call.argCount);
                ty += 13;
            }

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

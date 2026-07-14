#pragma once
#include <Windows.h>
#include <d3d9.h>
#include <string>
#include <deque>
#include <algorithm>
#include "../framework.h"
#include "../overlay.h"

namespace W101Hook {

    class AutoDialogue {
    public:
        struct DialogueEvent {
            std::string npcName;
            std::string text;
            DWORD       timestamp;
            bool        skipped;
        };

    private:
        static inline bool   active = false;
        static inline bool   enabled = false;
        static inline bool   inDialogue = false;
        static inline DWORD  lastSkipTime = 0;
        static inline DWORD  skipDelay = 200;     // 200ms between skips (human-like)
        static inline DWORD  dialogueStartTime = 0;
        static inline uint32_t totalSkipped = 0;
        static inline uint32_t totalDialogues = 0;
        static inline std::string currentNPC;
        static inline std::deque<DialogueEvent> history;
        static inline int maxHistory = 30;

        // W101 dialogue is SWF-based, shown as flash UI panels
        // Clicking advances dialogue, pressing X or ESC can close
        // The "next" or "continue" area is at the bottom of the dialogue box

        static void SkipDialogue() {
            // Method 1: Press SPACE or X to advance dialogue
            INPUT inputs[2] = {};
            inputs[0].type = INPUT_KEYBOARD;
            inputs[0].ki.wVk = VK_SPACE;
            inputs[0].ki.dwFlags = 0;

            inputs[1].type = INPUT_KEYBOARD;
            inputs[1].ki.wVk = VK_SPACE;
            inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

            SendInput(2, inputs, sizeof(INPUT));
        }

        static void CloseDialogue() {
            // Press X key to close/skip dialogue
            INPUT inputs[2] = {};
            inputs[0].type = INPUT_KEYBOARD;
            inputs[0].ki.wVk = 'X';
            inputs[0].ki.dwFlags = 0;

            inputs[1].type = INPUT_KEYBOARD;
            inputs[1].ki.wVk = 'X';
            inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

            SendInput(2, inputs, sizeof(INPUT));
        }

        static void ClickDialogueBox() {
            // Click the bottom area of the screen where dialogue boxes appear
            // W101 dialogue boxes are typically at the bottom-center
            int clickX = 512;  // center-ish
            int clickY = 550;  // dialogue area

            INPUT inputs[2] = {};
            inputs[0].type = INPUT_MOUSE;
            inputs[0].mi.dx = clickX * (65535 / GetSystemMetrics(SM_CXSCREEN));
            inputs[0].mi.dy = clickY * (65535 / GetSystemMetrics(SM_CYSCREEN));
            inputs[0].mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE | MOUSEEVENTF_LEFTDOWN;

            inputs[1].type = INPUT_MOUSE;
            inputs[1].mi.dx = inputs[0].mi.dx;
            inputs[1].mi.dy = inputs[0].mi.dy;
            inputs[1].mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE | MOUSEEVENTF_LEFTUP;

            SendInput(2, inputs, sizeof(INPUT));
        }

        static void LogEvent(const std::string& npc, const std::string& text, bool skipped) {
            DialogueEvent evt;
            evt.npcName = npc;
            evt.text = text;
            evt.timestamp = GetTickCount();
            evt.skipped = skipped;

            history.push_front(evt);
            if ((int)history.size() > maxHistory) history.pop_back();
        }

    public:
        static bool Init() {
            active = true;
            return true;
        }

        static void Shutdown() { active = false; enabled = false; }
        static bool IsActive() { return active; }
        static bool IsEnabled() { return enabled; }
        static bool IsInDialogue() { return inDialogue; }

        static void ProcessFSCommand(const std::string& cmd, const std::string& args) {
            if (!active) return;

            std::string lowerCmd = cmd;
            std::transform(lowerCmd.begin(), lowerCmd.end(), lowerCmd.begin(), ::tolower);

            // Detect dialogue open/close events
            // W101 uses FSCommands for UI state changes including NPC dialogue
            if (lowerCmd.find("dialog") != std::string::npos ||
                lowerCmd.find("dialogue") != std::string::npos ||
                lowerCmd.find("npc") != std::string::npos ||
                lowerCmd.find("talk") != std::string::npos ||
                lowerCmd.find("conversation") != std::string::npos ||
                lowerCmd.find("quest_text") != std::string::npos ||
                lowerCmd.find("speech") != std::string::npos) {

                if (lowerCmd.find("open") != std::string::npos ||
                    lowerCmd.find("show") != std::string::npos ||
                    lowerCmd.find("start") != std::string::npos ||
                    lowerCmd.find("begin") != std::string::npos) {

                    inDialogue = true;
                    dialogueStartTime = GetTickCount();
                    totalDialogues++;

                    // Try to extract NPC name from args
                    if (!args.empty()) {
                        // First comma-separated field is often the NPC name
                        size_t comma = args.find(',');
                        currentNPC = comma != std::string::npos ?
                            args.substr(0, comma) : args;
                    }

                    LogEvent(currentNPC, "dialogue started", false);
                }

                if (lowerCmd.find("close") != std::string::npos ||
                    lowerCmd.find("hide") != std::string::npos ||
                    lowerCmd.find("end") != std::string::npos ||
                    lowerCmd.find("finish") != std::string::npos) {

                    if (inDialogue) {
                        LogEvent(currentNPC, "dialogue ended", false);
                        inDialogue = false;
                        currentNPC.clear();
                    }
                }
            }

            // Detect quest accept/decline options
            if (lowerCmd.find("quest") != std::string::npos) {
                if (lowerCmd.find("accept") != std::string::npos ||
                    lowerCmd.find("offer") != std::string::npos) {
                    inDialogue = true;
                    if (!args.empty()) currentNPC = args;
                    LogEvent(currentNPC, "quest offered", false);
                }

                if (lowerCmd.find("complete") != std::string::npos) {
                    LogEvent(currentNPC, "quest complete", false);
                }
            }

            // Detect generic UI popups that need clicking through
            if (lowerCmd.find("popup") != std::string::npos ||
                lowerCmd.find("notification") != std::string::npos ||
                lowerCmd.find("message") != std::string::npos) {

                if (lowerCmd.find("show") != std::string::npos) {
                    inDialogue = true;
                    LogEvent("POPUP", args.substr(0, 40), false);
                }
                if (lowerCmd.find("close") != std::string::npos ||
                    lowerCmd.find("dismiss") != std::string::npos) {
                    inDialogue = false;
                }
            }
        }

        static void Update(float dt) {
            if (!active || !enabled) return;

            DWORD now = GetTickCount();

            if (inDialogue && (now - lastSkipTime >= skipDelay)) {
                lastSkipTime = now;
                totalSkipped++;

                // Cycle through skip methods
                int method = totalSkipped % 3;
                switch (method) {
                    case 0: SkipDialogue();     break;
                    case 1: ClickDialogueBox(); break;
                    case 2: CloseDialogue();    break;
                }

                LogEvent(currentNPC, "auto-skipped", true);

                // If dialogue has been going for too long, assume it's stuck
                if (now - dialogueStartTime > 30000) {
                    // Force close with ESC
                    INPUT inputs[2] = {};
                    inputs[0].type = INPUT_KEYBOARD;
                    inputs[0].ki.wVk = VK_ESCAPE;
                    inputs[0].ki.dwFlags = 0;
                    inputs[1].type = INPUT_KEYBOARD;
                    inputs[1].ki.wVk = VK_ESCAPE;
                    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
                    SendInput(2, inputs, sizeof(INPUT));

                    inDialogue = false;
                    LogEvent("SYSTEM", "force-closed stuck dialogue", true);
                }
            }
        }

        static void Toggle() { enabled = !enabled; }

        static void SetSkipDelay(DWORD ms) {
            skipDelay = std::clamp(ms, (DWORD)100, (DWORD)2000);
        }

        static uint32_t GetTotalSkipped() { return totalSkipped; }
        static uint32_t GetTotalDialogues() { return totalDialogues; }

        static bool HandleKey(unsigned short key, bool down) {
            if (!down || !active) return true;

            // Ctrl+D = toggle auto dialogue skip
            if (key == 'D' && (GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
                !(GetAsyncKeyState(VK_SHIFT) & 0x8000)) {
                Toggle();
                return false;
            }

            return true;
        }

        static int RenderPanel(IDirect3DDevice9* dev, int x, int y) {
            int histShow = (int)history.size() < 5 ? (int)history.size() : 5;
            int h = 68 + histShow * 13;
            Overlay::DrawFilledRect(dev, x, y, 360, h, Overlay::BgPanel);
            Overlay::DrawRect(x, y, 360, h, D3DCOLOR_ARGB(255, 200, 200, 60), false);

            int ty = y + 4;
            D3DCOLOR headerColor = enabled ?
                D3DCOLOR_ARGB(255, 200, 200, 60) : D3DCOLOR_ARGB(255, 140, 140, 60);
            Overlay::DrawText(x + 5, ty, headerColor,
                "AUTO DIALOGUE %s", enabled ? "[ACTIVE]" : "[OFF]"); ty += 16;

            Overlay::DrawText(x + 5, ty, Overlay::White,
                "Dialogues: %u  |  Skipped: %u  |  Delay: %ums",
                totalDialogues, totalSkipped, skipDelay); ty += 14;

            if (inDialogue) {
                Overlay::DrawText(x + 5, ty, Overlay::Yellow,
                    "IN DIALOGUE: %s",
                    currentNPC.empty() ? "Unknown NPC" : currentNPC.c_str());
            } else {
                Overlay::DrawText(x + 5, ty, Overlay::Gray, "Not in dialogue");
            }
            ty += 14;

            Overlay::DrawText(x + 5, ty, Overlay::Gray,
                "Ctrl+D toggle auto-skip"); ty += 16;

            for (int i = 0; i < histShow; i++) {
                auto& evt = history[i];
                D3DCOLOR c = evt.skipped ?
                    D3DCOLOR_ARGB(180, 180, 180, 60) :
                    D3DCOLOR_ARGB(200, 200, 200, 200);
                std::string line = evt.npcName;
                if (!evt.text.empty()) {
                    line += ": " + evt.text;
                }
                if (line.length() > 45) line = line.substr(0, 45) + "...";
                Overlay::DrawText(x + 5, ty, c, "%s", line.c_str());
                ty += 13;
            }

            return y + h + 4;
        }
    };

} // namespace W101Hook

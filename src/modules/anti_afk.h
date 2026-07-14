#pragma once
#include <Windows.h>
#include <d3d9.h>
#include "../overlay.h"

namespace W101Hook {

    class AntiAFK {
    private:
        static inline bool   active = false;
        static inline bool   enabled = false;
        static inline DWORD  intervalMs = 120000; // 2 minutes default
        static inline DWORD  lastAction = 0;
        static inline DWORD  lastInput = 0;
        static inline uint32_t totalActions = 0;
        static inline int    actionType = 0; // cycles through different inputs

        // Intervals
        static constexpr DWORD INTERVAL_MIN = 30000;   // 30 sec
        static constexpr DWORD INTERVAL_MAX = 600000;   // 10 min
        static constexpr DWORD INTERVAL_STEP = 30000;    // 30 sec increments

        static void PerformAntiAFK() {
            INPUT input = {};

            switch (actionType % 4) {
                case 0:
                    // Tiny mouse jiggle (1 pixel right then back)
                    input.type = INPUT_MOUSE;
                    input.mi.dwFlags = MOUSEEVENTF_MOVE;
                    input.mi.dx = 1;
                    input.mi.dy = 0;
                    SendInput(1, &input, sizeof(INPUT));
                    Sleep(50);
                    input.mi.dx = -1;
                    SendInput(1, &input, sizeof(INPUT));
                    break;

                case 1:
                    // Press and release shift (harmless)
                    input.type = INPUT_KEYBOARD;
                    input.ki.wVk = VK_SHIFT;
                    input.ki.dwFlags = 0;
                    SendInput(1, &input, sizeof(INPUT));
                    Sleep(30);
                    input.ki.dwFlags = KEYEVENTF_KEYUP;
                    SendInput(1, &input, sizeof(INPUT));
                    break;

                case 2:
                    // Mouse move 0,0 (no visible movement)
                    input.type = INPUT_MOUSE;
                    input.mi.dwFlags = MOUSEEVENTF_MOVE;
                    input.mi.dx = 0;
                    input.mi.dy = 0;
                    SendInput(1, &input, sizeof(INPUT));
                    break;

                case 3:
                    // Press and release ctrl (harmless)
                    input.type = INPUT_KEYBOARD;
                    input.ki.wVk = VK_CONTROL;
                    input.ki.dwFlags = 0;
                    SendInput(1, &input, sizeof(INPUT));
                    Sleep(30);
                    input.ki.dwFlags = KEYEVENTF_KEYUP;
                    SendInput(1, &input, sizeof(INPUT));
                    break;
            }

            actionType++;
            totalActions++;
            lastAction = GetTickCount();
        }

    public:
        static bool Init() {
            lastAction = GetTickCount();
            lastInput = GetTickCount();
            active = true;
            return true;
        }

        static void Shutdown() {
            active = false;
            enabled = false;
        }

        static bool IsActive() { return active; }
        static bool IsEnabled() { return enabled; }

        // Call every frame from advance hook
        static void Update(float dt) {
            if (!active || !enabled) return;

            DWORD now = GetTickCount();

            // Check if it's time to perform anti-AFK action
            if (now - lastAction >= intervalMs) {
                PerformAntiAFK();
            }
        }

        // Call from mouse/key callbacks to reset timer on real input
        static void OnRealInput() {
            lastInput = GetTickCount();
            lastAction = GetTickCount(); // reset anti-AFK timer
        }

        // --- Controls ---
        static void Toggle() { enabled = !enabled; lastAction = GetTickCount(); }

        static void IncreaseInterval() {
            intervalMs = (std::min)(intervalMs + INTERVAL_STEP, INTERVAL_MAX);
        }

        static void DecreaseInterval() {
            intervalMs = (std::max)(intervalMs - INTERVAL_STEP, INTERVAL_MIN);
        }

        static void SetInterval(DWORD ms) {
            intervalMs = std::clamp(ms, INTERVAL_MIN, INTERVAL_MAX);
        }

        static DWORD GetInterval() { return intervalMs; }
        static uint32_t GetTotalActions() { return totalActions; }

        static DWORD GetTimeSinceLastAction() {
            return GetTickCount() - lastAction;
        }

        static DWORD GetTimeSinceLastInput() {
            return GetTickCount() - lastInput;
        }

        static DWORD GetTimeUntilNext() {
            if (!enabled) return 0;
            DWORD elapsed = GetTickCount() - lastAction;
            if (elapsed >= intervalMs) return 0;
            return intervalMs - elapsed;
        }

        static bool HandleKey(unsigned short key, bool down) {
            if (!down) return true;

            // Ctrl+A = toggle anti-AFK
            if (key == 'A' && (GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
                (GetAsyncKeyState(VK_SHIFT) & 0x8000)) {
                Toggle();
                return false;
            }

            return true;
        }

        static std::string FormatTime(DWORD ms) {
            char buf[32];
            if (ms < 1000)
                snprintf(buf, sizeof(buf), "%ums", ms);
            else if (ms < 60000)
                snprintf(buf, sizeof(buf), "%us", ms / 1000);
            else
                snprintf(buf, sizeof(buf), "%um%02us", ms / 60000, (ms % 60000) / 1000);
            return buf;
        }

        // --- Overlay ---
        static int RenderPanel(IDirect3DDevice9* dev, int x, int y) {
            int h = 54;
            Overlay::DrawFilledRect(dev, x, y, 300, h, Overlay::BgPanel);
            Overlay::DrawRect(x, y, 300, h, D3DCOLOR_ARGB(255, 200, 200, 60), false);

            int ty = y + 4;
            D3DCOLOR titleColor = enabled ?
                D3DCOLOR_ARGB(255, 60, 255, 60) : D3DCOLOR_ARGB(255, 200, 200, 60);
            Overlay::DrawText(x + 5, ty, titleColor,
                "ANTI-AFK: %s", enabled ? "ACTIVE" : "OFF"); ty += 16;

            if (enabled) {
                DWORD nextIn = GetTimeUntilNext();
                Overlay::DrawText(x + 5, ty, Overlay::White,
                    "Next action in: %s  |  Interval: %s",
                    FormatTime(nextIn).c_str(),
                    FormatTime(intervalMs).c_str()); ty += 14;
            } else {
                Overlay::DrawText(x + 5, ty, Overlay::Gray,
                    "Interval: %s  |  Total actions: %u",
                    FormatTime(intervalMs).c_str(), totalActions); ty += 14;
            }

            Overlay::DrawText(x + 5, ty, Overlay::Gray,
                "Ctrl+Shift+A toggle  |  Idle: %s",
                FormatTime(GetTimeSinceLastInput()).c_str());

            return y + h + 4;
        }
    };

} // namespace W101Hook

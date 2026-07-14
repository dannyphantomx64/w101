#pragma once
#include <Windows.h>
#include <algorithm>
#include <cmath>

namespace W101Hook {

    class SpeedControl {
    public:
        static constexpr float SPEED_PRESETS[] = {
            0.1f, 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f, 12.0f, 16.0f
        };
        static constexpr int PRESET_COUNT = sizeof(SPEED_PRESETS) / sizeof(float);
        static constexpr int DEFAULT_INDEX = 4; // 1.0x

    private:
        static inline float  multiplier   = 1.0f;
        static inline int    presetIndex  = DEFAULT_INDEX;
        static inline bool   frozen       = false;
        static inline bool   enabled      = true;
        static inline float  accumDt      = 0.0f;
        static inline DWORD  frameCount   = 0;
        static inline float  realDtLast   = 0.0f;
        static inline float  modDtLast    = 0.0f;

    public:
        static float ProcessDt(float originalDt) {
            realDtLast = originalDt;
            frameCount++;
            if (!enabled) { modDtLast = originalDt; return originalDt; }
            if (frozen) { modDtLast = 0.0f; return 0.0f; }

            float modified = originalDt * multiplier;

            // clamp to prevent physics explosions at extreme speeds
            if (modified > 0.5f) modified = 0.5f;
            if (modified < 0.0f) modified = 0.0f;

            accumDt += modified - originalDt;
            modDtLast = modified;
            return modified;
        }

        static bool HandleKey(unsigned short key, bool down) {
            if (!down) return true;

            switch (key) {
                case VK_ADD:      // Numpad+ : speed up
                    SpeedUp();
                    return false;

                case VK_SUBTRACT: // Numpad- : slow down
                    SlowDown();
                    return false;

                case VK_MULTIPLY: // Numpad* : reset to 1x
                    Reset();
                    return false;

                case VK_DIVIDE:   // Numpad/ : toggle freeze
                    ToggleFreeze();
                    return false;

                case VK_NUMPAD0:  // Numpad0 : toggle speed control
                    enabled = !enabled;
                    return false;

                // Direct speed presets via numpad
                case VK_NUMPAD1: SetPreset(0); return false;  // 0.1x
                case VK_NUMPAD2: SetPreset(2); return false;  // 0.5x
                case VK_NUMPAD3: SetPreset(4); return false;  // 1.0x
                case VK_NUMPAD4: SetPreset(6); return false;  // 2.0x
                case VK_NUMPAD5: SetPreset(8); return false;  // 4.0x
                case VK_NUMPAD6: SetPreset(10); return false; // 8.0x
                case VK_NUMPAD7: SetPreset(12); return false; // 16.0x
            }

            return true;
        }

        static void SpeedUp() {
            if (presetIndex < PRESET_COUNT - 1) {
                presetIndex++;
                multiplier = SPEED_PRESETS[presetIndex];
                frozen = false;
            }
        }

        static void SlowDown() {
            if (presetIndex > 0) {
                presetIndex--;
                multiplier = SPEED_PRESETS[presetIndex];
                frozen = false;
            }
        }

        static void Reset() {
            presetIndex = DEFAULT_INDEX;
            multiplier = 1.0f;
            frozen = false;
            accumDt = 0.0f;
        }

        static void SetPreset(int index) {
            if (index >= 0 && index < PRESET_COUNT) {
                presetIndex = index;
                multiplier = SPEED_PRESETS[index];
                frozen = false;
            }
        }

        static void ToggleFreeze() { frozen = !frozen; }

        // Getters
        static float  GetMultiplier() { return multiplier; }
        static float  GetRealDt()     { return realDtLast; }
        static float  GetModDt()      { return modDtLast; }
        static float  GetAccumDrift() { return accumDt; }
        static bool   IsFrozen()      { return frozen; }
        static bool   IsEnabled()     { return enabled; }
        static int    GetPresetIndex(){ return presetIndex; }
        static DWORD  GetFrameCount() { return frameCount; }

        static const char* GetSpeedLabel() {
            if (frozen) return "FROZEN";
            if (!enabled) return "OFF";

            static char buf[32];
            if (multiplier == static_cast<int>(multiplier)) {
                snprintf(buf, sizeof(buf), "%.0fx", multiplier);
            } else {
                snprintf(buf, sizeof(buf), "%.2fx", multiplier);
            }
            return buf;
        }

        static D3DCOLOR GetSpeedColor() {
            if (frozen) return D3DCOLOR_ARGB(255, 100, 100, 255);
            if (!enabled) return D3DCOLOR_ARGB(255, 160, 160, 160);
            if (multiplier < 1.0f) return D3DCOLOR_ARGB(255, 60, 180, 255);
            if (multiplier > 1.0f) return D3DCOLOR_ARGB(255, 255, 80, 60);
            return D3DCOLOR_ARGB(255, 60, 255, 60);
        }

        static void DrawSpeedBar(int x, int y, int barWidth = 200) {
            // visual speed indicator bar
            float ratio = static_cast<float>(presetIndex) / static_cast<float>(PRESET_COUNT - 1);
            int filled = static_cast<int>(ratio * barWidth);

            // background
            for (int i = 0; i < barWidth; i++) {
                // this gets rendered via overlay
            }
        }
    };

} // namespace W101Hook

#pragma once
#include <Windows.h>
#include <vector>
#include <string>
#include <mutex>
#include <algorithm>

namespace W101Hook {

    class MacroEngine {
    public:
        enum EventType {
            EVT_KEY_DOWN,
            EVT_KEY_UP,
            EVT_MOUSE_MOVE,
            EVT_MOUSE_DOWN,
            EVT_MOUSE_UP,
            EVT_MOUSE_WHEEL,
            EVT_DELAY
        };

        struct InputEvent {
            EventType   type;
            DWORD       timestamp;
            union {
                struct { unsigned short key; }                   keyData;
                struct { int x, y, button; }                     mouseData;
                struct { int delta; }                            wheelData;
                struct { DWORD ms; }                             delayData;
            };
        };

        struct Macro {
            std::string          name;
            std::vector<InputEvent> events;
            DWORD                recordStart;
            DWORD                totalDuration;
            bool                 valid;
        };

        enum State {
            IDLE,
            RECORDING,
            PLAYING,
            PAUSED
        };

    private:
        static inline State                 state = IDLE;
        static inline Macro                 currentMacro = {};
        static inline std::vector<Macro>    savedMacros;
        static inline std::mutex            macroMtx;

        // Playback state
        static inline int       playIndex = 0;
        static inline DWORD     playStartTime = 0;
        static inline DWORD     nextEventTime = 0;
        static inline int       loopCount = 0;
        static inline int       loopsRemaining = 0;
        static inline bool      infiniteLoop = false;
        static inline float     playbackSpeed = 1.0f;

        // Recording state
        static inline DWORD     recordStartTime = 0;
        static inline DWORD     lastEventTime = 0;
        static inline int       maxEventsPerMacro = 50000;

        // Stats
        static inline uint32_t  totalPlayed = 0;
        static inline uint32_t  totalRecorded = 0;

        static void InsertDelay(DWORD now) {
            if (lastEventTime == 0) {
                lastEventTime = now;
                return;
            }
            DWORD gap = now - lastEventTime;
            if (gap > 5) {
                InputEvent delay = {};
                delay.type = EVT_DELAY;
                delay.timestamp = now - recordStartTime;
                delay.delayData.ms = gap;
                currentMacro.events.push_back(delay);
            }
            lastEventTime = now;
        }

        static void ExecuteEvent(const InputEvent& evt) {
            INPUT input = {};

            switch (evt.type) {
                case EVT_KEY_DOWN:
                    input.type = INPUT_KEYBOARD;
                    input.ki.wVk = evt.keyData.key;
                    input.ki.dwFlags = 0;
                    SendInput(1, &input, sizeof(INPUT));
                    break;

                case EVT_KEY_UP:
                    input.type = INPUT_KEYBOARD;
                    input.ki.wVk = evt.keyData.key;
                    input.ki.dwFlags = KEYEVENTF_KEYUP;
                    SendInput(1, &input, sizeof(INPUT));
                    break;

                case EVT_MOUSE_MOVE:
                    SetCursorPos(evt.mouseData.x, evt.mouseData.y);
                    break;

                case EVT_MOUSE_DOWN:
                    input.type = INPUT_MOUSE;
                    if (evt.mouseData.button == 0)
                        input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
                    else if (evt.mouseData.button == 1)
                        input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
                    else
                        input.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
                    SendInput(1, &input, sizeof(INPUT));
                    break;

                case EVT_MOUSE_UP:
                    input.type = INPUT_MOUSE;
                    if (evt.mouseData.button == 0)
                        input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
                    else if (evt.mouseData.button == 1)
                        input.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
                    else
                        input.mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
                    SendInput(1, &input, sizeof(INPUT));
                    break;

                case EVT_MOUSE_WHEEL:
                    input.type = INPUT_MOUSE;
                    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
                    input.mi.mouseData = evt.wheelData.delta;
                    SendInput(1, &input, sizeof(INPUT));
                    break;

                case EVT_DELAY:
                    break;
            }
        }

    public:
        // --- Recording ---

        static void StartRecording(const std::string& name = "macro") {
            if (state == PLAYING) StopPlayback();

            currentMacro = {};
            currentMacro.name = name;
            currentMacro.valid = false;
            recordStartTime = GetTickCount();
            currentMacro.recordStart = recordStartTime;
            lastEventTime = 0;
            state = RECORDING;
        }

        static void StopRecording() {
            if (state != RECORDING) return;

            currentMacro.totalDuration = GetTickCount() - recordStartTime;
            currentMacro.valid = !currentMacro.events.empty();
            state = IDLE;

            if (currentMacro.valid) {
                totalRecorded++;
                std::lock_guard<std::mutex> lock(macroMtx);
                savedMacros.push_back(currentMacro);
                if (savedMacros.size() > 32) {
                    savedMacros.erase(savedMacros.begin());
                }
            }
        }

        static void RecordKeyDown(unsigned short key) {
            if (state != RECORDING) return;
            if (static_cast<int>(currentMacro.events.size()) >= maxEventsPerMacro) return;

            DWORD now = GetTickCount();
            InsertDelay(now);

            InputEvent evt = {};
            evt.type = EVT_KEY_DOWN;
            evt.timestamp = now - recordStartTime;
            evt.keyData.key = key;
            currentMacro.events.push_back(evt);
        }

        static void RecordKeyUp(unsigned short key) {
            if (state != RECORDING) return;
            if (static_cast<int>(currentMacro.events.size()) >= maxEventsPerMacro) return;

            DWORD now = GetTickCount();
            InsertDelay(now);

            InputEvent evt = {};
            evt.type = EVT_KEY_UP;
            evt.timestamp = now - recordStartTime;
            evt.keyData.key = key;
            currentMacro.events.push_back(evt);
        }

        static void RecordMouseMove(int x, int y) {
            if (state != RECORDING) return;
            if (static_cast<int>(currentMacro.events.size()) >= maxEventsPerMacro) return;

            DWORD now = GetTickCount();

            // Throttle mouse moves — skip if same position
            if (!currentMacro.events.empty()) {
                auto& last = currentMacro.events.back();
                if (last.type == EVT_MOUSE_MOVE &&
                    last.mouseData.x == x && last.mouseData.y == y) {
                    return;
                }
                // Throttle to ~60Hz mouse recording
                if (last.type == EVT_MOUSE_MOVE && (now - lastEventTime) < 16) {
                    last.mouseData.x = x;
                    last.mouseData.y = y;
                    return;
                }
            }

            InsertDelay(now);

            InputEvent evt = {};
            evt.type = EVT_MOUSE_MOVE;
            evt.timestamp = now - recordStartTime;
            evt.mouseData.x = x;
            evt.mouseData.y = y;
            currentMacro.events.push_back(evt);
        }

        static void RecordMouseButton(int button, bool down) {
            if (state != RECORDING) return;
            if (static_cast<int>(currentMacro.events.size()) >= maxEventsPerMacro) return;

            DWORD now = GetTickCount();
            InsertDelay(now);

            InputEvent evt = {};
            evt.type = down ? EVT_MOUSE_DOWN : EVT_MOUSE_UP;
            evt.timestamp = now - recordStartTime;
            evt.mouseData.button = button;
            currentMacro.events.push_back(evt);
        }

        // --- Playback ---

        static bool StartPlayback(int macroIndex = -1, int loops = 1) {
            if (state == RECORDING) StopRecording();

            Macro* target = nullptr;

            if (macroIndex >= 0) {
                std::lock_guard<std::mutex> lock(macroMtx);
                if (macroIndex < static_cast<int>(savedMacros.size())) {
                    currentMacro = savedMacros[macroIndex];
                    target = &currentMacro;
                }
            } else if (currentMacro.valid) {
                target = &currentMacro;
            }

            if (!target || target->events.empty()) return false;

            playIndex = 0;
            playStartTime = GetTickCount();
            nextEventTime = playStartTime;

            if (loops <= 0) {
                infiniteLoop = true;
                loopsRemaining = 1;
            } else {
                infiniteLoop = false;
                loopsRemaining = loops;
            }

            loopCount = 0;
            state = PLAYING;
            totalPlayed++;
            return true;
        }

        static void StopPlayback() {
            state = IDLE;
            playIndex = 0;
        }

        static void PausePlayback() {
            if (state == PLAYING) state = PAUSED;
            else if (state == PAUSED) state = PLAYING;
        }

        // Called every frame from the advance hook
        static void Tick() {
            if (state != PLAYING) return;
            if (currentMacro.events.empty()) { state = IDLE; return; }

            DWORD now = GetTickCount();

            while (playIndex < static_cast<int>(currentMacro.events.size())) {
                auto& evt = currentMacro.events[playIndex];

                if (evt.type == EVT_DELAY) {
                    DWORD scaledDelay = static_cast<DWORD>(evt.delayData.ms / playbackSpeed);
                    if (now < nextEventTime + scaledDelay) return;
                    nextEventTime += scaledDelay;
                    playIndex++;
                    continue;
                }

                ExecuteEvent(evt);
                playIndex++;
            }

            // Loop handling
            loopCount++;
            loopsRemaining--;

            if (infiniteLoop || loopsRemaining > 0) {
                playIndex = 0;
                nextEventTime = GetTickCount();
            } else {
                state = IDLE;
            }
        }

        // --- Getters ---
        static State GetState() { return state; }
        static bool IsRecording() { return state == RECORDING; }
        static bool IsPlaying() { return state == PLAYING; }
        static bool IsPaused() { return state == PAUSED; }
        static bool IsIdle() { return state == IDLE; }

        static float GetPlaybackSpeed() { return playbackSpeed; }
        static void SetPlaybackSpeed(float speed) {
            playbackSpeed = std::clamp(speed, 0.1f, 10.0f);
        }

        static int GetEventCount() {
            return static_cast<int>(currentMacro.events.size());
        }

        static int GetPlayIndex() { return playIndex; }
        static int GetLoopCount() { return loopCount; }
        static int GetLoopsRemaining() { return infiniteLoop ? -1 : loopsRemaining; }
        static uint32_t GetTotalPlayed() { return totalPlayed; }
        static uint32_t GetTotalRecorded() { return totalRecorded; }
        static const std::string& GetCurrentName() { return currentMacro.name; }

        static DWORD GetRecordDuration() {
            if (state == RECORDING) return GetTickCount() - recordStartTime;
            return currentMacro.totalDuration;
        }

        static float GetPlaybackProgress() {
            if (!IsPlaying() || currentMacro.events.empty()) return 0.0f;
            return static_cast<float>(playIndex) / static_cast<float>(currentMacro.events.size());
        }

        static int GetSavedMacroCount() {
            std::lock_guard<std::mutex> lock(macroMtx);
            return static_cast<int>(savedMacros.size());
        }

        static std::vector<std::string> GetSavedMacroNames() {
            std::lock_guard<std::mutex> lock(macroMtx);
            std::vector<std::string> names;
            for (auto& m : savedMacros) names.push_back(m.name);
            return names;
        }

        static void DeleteSavedMacro(int index) {
            std::lock_guard<std::mutex> lock(macroMtx);
            if (index >= 0 && index < static_cast<int>(savedMacros.size())) {
                savedMacros.erase(savedMacros.begin() + index);
            }
        }

        static bool HandleKey(unsigned short key, bool down) {
            if (!down) return true;

            switch (key) {
                case VK_F9:
                    if (IsRecording()) {
                        StopRecording();
                    } else if (IsIdle()) {
                        char name[32];
                        snprintf(name, sizeof(name), "macro_%u", totalRecorded + 1);
                        StartRecording(name);
                    }
                    return false;

                case VK_F10:
                    if (IsPlaying()) {
                        StopPlayback();
                    } else if (IsIdle() && currentMacro.valid) {
                        StartPlayback(-1, 1);
                    }
                    return false;

                case VK_F11:
                    if (IsIdle() && currentMacro.valid) {
                        StartPlayback(-1, 0); // infinite loop
                    } else if (IsPlaying()) {
                        PausePlayback();
                    } else if (IsPaused()) {
                        PausePlayback();
                    }
                    return false;

                case VK_F12:
                    if (IsPlaying() || IsPaused()) {
                        StopPlayback();
                    }
                    return false;
            }

            return true;
        }

        static const char* GetStateLabel() {
            switch (state) {
                case RECORDING: return "REC";
                case PLAYING:   return "PLAY";
                case PAUSED:    return "PAUSE";
                default:        return "IDLE";
            }
        }

        static D3DCOLOR GetStateColor() {
            switch (state) {
                case RECORDING: return D3DCOLOR_ARGB(255, 255, 60, 60);
                case PLAYING:   return D3DCOLOR_ARGB(255, 60, 255, 60);
                case PAUSED:    return D3DCOLOR_ARGB(255, 255, 255, 60);
                default:        return D3DCOLOR_ARGB(255, 160, 160, 160);
            }
        }

        static std::string FormatDuration(DWORD ms) {
            char buf[64];
            if (ms < 1000)
                snprintf(buf, sizeof(buf), "%ums", ms);
            else if (ms < 60000)
                snprintf(buf, sizeof(buf), "%.1fs", ms / 1000.0f);
            else
                snprintf(buf, sizeof(buf), "%dm%ds", ms / 60000, (ms % 60000) / 1000);
            return buf;
        }
    };

} // namespace W101Hook

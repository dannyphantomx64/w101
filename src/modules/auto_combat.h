#pragma once
#include <Windows.h>
#include <d3d9.h>
#include <string>
#include <vector>
#include <deque>
#include <algorithm>
#include "../framework.h"
#include "../overlay.h"

namespace W101Hook {

    class AutoCombat {
    public:
        enum CombatState {
            COMBAT_IDLE,
            COMBAT_WAITING_TURN,
            COMBAT_SELECTING_SPELL,
            COMBAT_SELECTING_TARGET,
            COMBAT_CASTING,
            COMBAT_FLEEING
        };

        enum SpellStrategy {
            STRAT_STRONGEST_FIRST,
            STRAT_AOE_PRIORITY,
            STRAT_HEAL_PRIORITY,
            STRAT_BLADE_THEN_HIT,
            STRAT_PASS_ONLY
        };

        struct CombatStats {
            uint32_t totalBattles;
            uint32_t totalRounds;
            uint32_t spellsCast;
            uint32_t passedTurns;
            uint32_t flees;
            uint32_t autoWins;
            DWORD    battleStartTime;
            DWORD    totalBattleTime;
        };

        struct SpellSlot {
            int      index;       // 0-6 spell deck position
            bool     available;
            bool     isAOE;
            bool     isHeal;
            bool     isBlade;
            bool     isTrap;
            int      pipCost;
        };

    private:
        static inline bool          active = false;
        static inline bool          enabled = false;
        static inline bool          inCombat = false;
        static inline CombatState   state = COMBAT_IDLE;
        static inline SpellStrategy strategy = STRAT_STRONGEST_FIRST;
        static inline CombatStats   stats = {};
        static inline DWORD         lastActionTime = 0;
        static inline DWORD         actionDelay = 1500;
        static inline DWORD         roundDelay = 2000;
        static inline int           currentPips = 0;
        static inline int           maxPips = 0;
        static inline bool          hasPowerPip = false;
        static inline int           currentHealth = 0;
        static inline int           maxHealth = 0;
        static inline int           currentMana = 0;
        static inline float         fleeThreshold = 0.15f; // flee at 15% HP
        static inline bool          autoFlee = false;
        static inline int           bladeStack = 0;
        static inline int           roundNumber = 0;
        static inline std::deque<std::string> combatLog;
        static inline int           maxLogEntries = 20;

        // W101 combat works through FSCommands and keyboard
        // Key combat actions mapped to spell deck positions
        // The game processes spell selection through the UI

        static void LogAction(const char* fmt, ...) {
            char buf[256];
            va_list args;
            va_start(args, fmt);
            vsnprintf(buf, sizeof(buf), fmt, args);
            va_end(args);

            combatLog.push_front(std::string(buf));
            if ((int)combatLog.size() > maxLogEntries)
                combatLog.pop_back();
        }

        // Simulate spell selection via keyboard input
        static void SelectSpell(int slotIndex) {
            // In W101, clicking a spell card or pressing its hotkey selects it
            // The deck is typically slots 1-7, navigated by clicking
            // We simulate mouse clicks at card positions
            // Card positions are roughly at the bottom of the screen
            // The combat UI shows cards in a row, each ~80px wide starting around x=300

            int cardX = 350 + (slotIndex * 85);
            int cardY = 680; // approximate bottom of spell deck

            // Simulate the click
            INPUT inputs[2] = {};
            inputs[0].type = INPUT_MOUSE;
            inputs[0].mi.dx = cardX * (65535 / GetSystemMetrics(SM_CXSCREEN));
            inputs[0].mi.dy = cardY * (65535 / GetSystemMetrics(SM_CYSCREEN));
            inputs[0].mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE | MOUSEEVENTF_LEFTDOWN;

            inputs[1].type = INPUT_MOUSE;
            inputs[1].mi.dx = inputs[0].mi.dx;
            inputs[1].mi.dy = inputs[0].mi.dy;
            inputs[1].mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE | MOUSEEVENTF_LEFTUP;

            SendInput(2, inputs, sizeof(INPUT));

            stats.spellsCast++;
            LogAction("Cast spell slot %d", slotIndex);
        }

        // Click the pass button
        static void PassTurn() {
            // The pass button is typically at the right side of the combat UI
            int passX = 780;
            int passY = 580;

            INPUT inputs[2] = {};
            inputs[0].type = INPUT_MOUSE;
            inputs[0].mi.dx = passX * (65535 / GetSystemMetrics(SM_CXSCREEN));
            inputs[0].mi.dy = passY * (65535 / GetSystemMetrics(SM_CYSCREEN));
            inputs[0].mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE | MOUSEEVENTF_LEFTDOWN;

            inputs[1].type = INPUT_MOUSE;
            inputs[1].mi.dx = inputs[0].mi.dx;
            inputs[1].mi.dy = inputs[0].mi.dy;
            inputs[1].mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE | MOUSEEVENTF_LEFTUP;

            SendInput(2, inputs, sizeof(INPUT));

            stats.passedTurns++;
            LogAction("Passed turn");
        }

        // Click the flee button
        static void FleeBattle() {
            // Flee button is typically near the pass button
            int fleeX = 60;
            int fleeY = 580;

            INPUT inputs[2] = {};
            inputs[0].type = INPUT_MOUSE;
            inputs[0].mi.dx = fleeX * (65535 / GetSystemMetrics(SM_CXSCREEN));
            inputs[0].mi.dy = fleeY * (65535 / GetSystemMetrics(SM_CYSCREEN));
            inputs[0].mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE | MOUSEEVENTF_LEFTDOWN;

            inputs[1].type = INPUT_MOUSE;
            inputs[1].mi.dx = inputs[0].mi.dx;
            inputs[1].mi.dy = inputs[0].mi.dy;
            inputs[1].mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE | MOUSEEVENTF_LEFTUP;

            SendInput(2, inputs, sizeof(INPUT));

            stats.flees++;
            LogAction("FLED from battle!");
            state = COMBAT_FLEEING;
        }

        // Select enemy target (click on the enemy)
        static void SelectTarget(int targetIndex) {
            // Enemy positions in combat are roughly at the top-center area
            // In 4v4 duels: enemies at positions across the top
            int targetX = 300 + (targetIndex * 140);
            int targetY = 300;

            INPUT inputs[2] = {};
            inputs[0].type = INPUT_MOUSE;
            inputs[0].mi.dx = targetX * (65535 / GetSystemMetrics(SM_CXSCREEN));
            inputs[0].mi.dy = targetY * (65535 / GetSystemMetrics(SM_CYSCREEN));
            inputs[0].mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE | MOUSEEVENTF_LEFTDOWN;

            inputs[1].type = INPUT_MOUSE;
            inputs[1].mi.dx = inputs[0].mi.dx;
            inputs[1].mi.dy = inputs[0].mi.dy;
            inputs[1].mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE | MOUSEEVENTF_LEFTUP;

            SendInput(2, inputs, sizeof(INPUT));
            LogAction("Selected target %d", targetIndex);
        }

        static int PickSpellSlot() {
            switch (strategy) {
                case STRAT_STRONGEST_FIRST:
                    // Pick the last available spell (usually highest rank)
                    for (int i = 6; i >= 0; i--) {
                        return i;
                    }
                    break;

                case STRAT_AOE_PRIORITY:
                    // W101: AOE spells are usually in later deck slots
                    for (int i = 6; i >= 4; i--) return i;
                    for (int i = 3; i >= 0; i--) return i;
                    break;

                case STRAT_BLADE_THEN_HIT:
                    // First 2 rounds: cast blades (usually slot 0-1), then biggest hit
                    if (bladeStack < 2) {
                        bladeStack++;
                        return bladeStack - 1;
                    }
                    return 6; // biggest spell
                    break;

                case STRAT_HEAL_PRIORITY:
                    // Heal if low, otherwise hit
                    if (maxHealth > 0 && currentHealth < maxHealth / 3) {
                        return 0; // heals are often first slots
                    }
                    return 6;

                case STRAT_PASS_ONLY:
                    return -1; // signal to pass
            }
            return 0;
        }

    public:
        static bool Init() {
            memset(&stats, 0, sizeof(stats));
            active = true;
            return true;
        }

        static void Shutdown() {
            active = false;
            enabled = false;
        }

        static bool IsActive() { return active; }
        static bool IsEnabled() { return enabled; }
        static bool IsInCombat() { return inCombat; }

        // Called from FSCommand processor to detect combat state
        static void ProcessFSCommand(const std::string& cmd, const std::string& args) {
            if (!active) return;

            std::string lowerCmd = cmd;
            std::transform(lowerCmd.begin(), lowerCmd.end(), lowerCmd.begin(), ::tolower);

            // Detect entering combat
            if (lowerCmd.find("duel") != std::string::npos ||
                lowerCmd.find("combat") != std::string::npos ||
                lowerCmd.find("battle") != std::string::npos) {

                if (lowerCmd.find("start") != std::string::npos ||
                    lowerCmd.find("begin") != std::string::npos ||
                    lowerCmd.find("enter") != std::string::npos ||
                    lowerCmd.find("init") != std::string::npos) {

                    if (!inCombat) {
                        inCombat = true;
                        state = COMBAT_WAITING_TURN;
                        roundNumber = 0;
                        bladeStack = 0;
                        stats.totalBattles++;
                        stats.battleStartTime = GetTickCount();
                        LogAction("=== COMBAT STARTED ===");
                    }
                }

                if (lowerCmd.find("end") != std::string::npos ||
                    lowerCmd.find("finish") != std::string::npos ||
                    lowerCmd.find("complete") != std::string::npos ||
                    lowerCmd.find("victory") != std::string::npos ||
                    lowerCmd.find("defeat") != std::string::npos) {

                    if (inCombat) {
                        DWORD duration = GetTickCount() - stats.battleStartTime;
                        stats.totalBattleTime += duration;

                        if (lowerCmd.find("victory") != std::string::npos ||
                            lowerCmd.find("win") != std::string::npos)
                            stats.autoWins++;

                        LogAction("=== COMBAT ENDED (%.1fs) ===", duration / 1000.0f);
                        inCombat = false;
                        state = COMBAT_IDLE;
                    }
                }
            }

            // Detect turn phase
            if (lowerCmd.find("planning") != std::string::npos ||
                lowerCmd.find("select") != std::string::npos ||
                lowerCmd.find("yourturn") != std::string::npos ||
                lowerCmd.find("turn") != std::string::npos) {

                if (inCombat) {
                    state = COMBAT_WAITING_TURN;
                    roundNumber++;
                    stats.totalRounds++;
                }
            }

            // Parse pip info from combat commands
            if (lowerCmd.find("pip") != std::string::npos) {
                int pips = 0;
                if (sscanf(args.c_str(), "%d", &pips) == 1) {
                    currentPips = pips;
                }
            }

            // Parse health updates
            if (lowerCmd.find("health") != std::string::npos ||
                lowerCmd.find("hp") != std::string::npos) {
                int hp = 0, maxhp = 0;
                if (sscanf(args.c_str(), "%d,%d", &hp, &maxhp) >= 1) {
                    currentHealth = hp;
                    if (maxhp > 0) maxHealth = maxhp;
                }
            }
        }

        static void Update(float dt) {
            if (!active || !enabled || !inCombat) return;

            DWORD now = GetTickCount();
            if (now - lastActionTime < actionDelay) return;

            switch (state) {
                case COMBAT_WAITING_TURN: {
                    // Check if we should flee
                    if (autoFlee && maxHealth > 0) {
                        float hpRatio = static_cast<float>(currentHealth) / maxHealth;
                        if (hpRatio <= fleeThreshold) {
                            FleeBattle();
                            lastActionTime = now;
                            return;
                        }
                    }

                    // Pick and cast a spell
                    int slot = PickSpellSlot();
                    if (slot < 0) {
                        PassTurn();
                    } else {
                        SelectSpell(slot);
                        state = COMBAT_SELECTING_TARGET;
                    }
                    lastActionTime = now;
                    break;
                }

                case COMBAT_SELECTING_TARGET: {
                    // After selecting spell, pick target (enemy 0 = first enemy)
                    SelectTarget(0);
                    state = COMBAT_CASTING;
                    lastActionTime = now;
                    break;
                }

                case COMBAT_CASTING: {
                    // Wait for casting animation to finish, then back to waiting
                    if (now - lastActionTime >= roundDelay) {
                        state = COMBAT_WAITING_TURN;
                        lastActionTime = now;
                    }
                    break;
                }

                case COMBAT_FLEEING:
                case COMBAT_IDLE:
                    break;
                default: break;
            }
        }

        static void Toggle() { enabled = !enabled; }

        static void CycleStrategy() {
            strategy = static_cast<SpellStrategy>((strategy + 1) % 5);
            bladeStack = 0;
            LogAction("Strategy: %s", GetStrategyName());
        }

        static void ToggleAutoFlee() { autoFlee = !autoFlee; }
        static void SetFleeThreshold(float pct) { fleeThreshold = std::clamp(pct, 0.05f, 0.5f); }
        static void SetActionDelay(DWORD ms) { actionDelay = std::clamp(ms, (DWORD)500, (DWORD)5000); }

        static const char* GetStateName() {
            switch (state) {
                case COMBAT_IDLE:             return "IDLE";
                case COMBAT_WAITING_TURN:     return "WAITING TURN";
                case COMBAT_SELECTING_SPELL:  return "SELECTING SPELL";
                case COMBAT_SELECTING_TARGET: return "SELECTING TARGET";
                case COMBAT_CASTING:          return "CASTING";
                case COMBAT_FLEEING:          return "FLEEING";
                default:                      return "UNKNOWN";
            }
        }

        static const char* GetStrategyName() {
            switch (strategy) {
                case STRAT_STRONGEST_FIRST: return "STRONGEST";
                case STRAT_AOE_PRIORITY:    return "AOE";
                case STRAT_HEAL_PRIORITY:   return "HEAL FIRST";
                case STRAT_BLADE_THEN_HIT:  return "BLADE+HIT";
                case STRAT_PASS_ONLY:       return "PASS";
                default:                    return "???";
            }
        }

        static bool HandleKey(unsigned short key, bool down) {
            if (!down || !active) return true;

            // Ctrl+G = toggle auto combat
            if (key == 'G' && (GetAsyncKeyState(VK_CONTROL) & 0x8000)) {
                if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
                    CycleStrategy();
                } else {
                    Toggle();
                }
                return false;
            }

            return true;
        }

        static int RenderPanel(IDirect3DDevice9* dev, int x, int y) {
            int logShow = (int)combatLog.size() < 5 ? (int)combatLog.size() : 5;
            int h = 84 + logShow * 13;
            Overlay::DrawFilledRect(dev, x, y, 360, h, Overlay::BgPanel);
            Overlay::DrawRect(x, y, 360, h, D3DCOLOR_ARGB(255, 255, 80, 80), false);

            int ty = y + 4;
            D3DCOLOR headerColor = enabled ?
                D3DCOLOR_ARGB(255, 255, 60, 60) : D3DCOLOR_ARGB(255, 180, 80, 80);
            Overlay::DrawText(x + 5, ty, headerColor,
                "AUTO COMBAT %s", enabled ? "[ACTIVE]" : "[OFF]"); ty += 16;

            Overlay::DrawText(x + 5, ty, Overlay::White,
                "State: %s  |  Strategy: %s",
                GetStateName(), GetStrategyName()); ty += 14;

            Overlay::DrawText(x + 5, ty, Overlay::White,
                "Battles: %u  Rounds: %u  Spells: %u  Wins: %u",
                stats.totalBattles, stats.totalRounds,
                stats.spellsCast, stats.autoWins); ty += 14;

            if (maxHealth > 0) {
                float hpPct = (float)currentHealth / maxHealth * 100.0f;
                D3DCOLOR hpColor = hpPct > 50 ? Overlay::Green :
                    (hpPct > 25 ? Overlay::Yellow : Overlay::Red);
                Overlay::DrawText(x + 5, ty, hpColor,
                    "HP: %d/%d (%.0f%%)  Pips: %d  AutoFlee: %s",
                    currentHealth, maxHealth, hpPct, currentPips,
                    autoFlee ? "ON" : "OFF");
            } else {
                Overlay::DrawText(x + 5, ty, Overlay::Gray,
                    "HP: ???  Pips: %d  AutoFlee: %s",
                    currentPips, autoFlee ? "ON" : "OFF");
            }
            ty += 14;

            Overlay::DrawText(x + 5, ty, Overlay::Gray,
                "Ctrl+G toggle  Ctrl+Sh+G strategy"); ty += 16;

            // Combat log
            for (int i = 0; i < logShow; i++) {
                Overlay::DrawText(x + 5, ty, D3DCOLOR_ARGB(200, 200, 200, 200),
                    "%s", combatLog[i].c_str());
                ty += 13;
            }

            return y + h + 4;
        }
    };

} // namespace W101Hook

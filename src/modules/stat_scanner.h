#pragma once
#include <Windows.h>
#include <d3d9.h>
#include <string>
#include <vector>
#include <cstring>
#include "../framework.h"
#include "../overlay.h"

namespace W101Hook {

    class StatScanner {
    public:
        enum StatID {
            STAT_HEALTH,
            STAT_MANA,
            STAT_PIPS,
            STAT_POWER_PIPS,
            STAT_GOLD,
            STAT_ENERGY,
            STAT_LEVEL,
            STAT_XP,
            STAT_COUNT
        };

        struct TrackedStat {
            StatID      id;
            const char* name;
            int         currentValue;
            int         maxValue;
            int         lastValue;
            bool        frozen;
            int         frozenValue;
            uintptr_t   address;
            bool        found;
            D3DCOLOR    color;
        };

    private:
        static inline bool active = false;
        static inline bool scanning = false;
        static inline DWORD lastScanTime = 0;
        static inline DWORD scanInterval = 1000;

        static inline TrackedStat stats[STAT_COUNT] = {
            { STAT_HEALTH,     "Health",     0, 0, 0, false, 0, 0, false, D3DCOLOR_ARGB(255, 255, 60, 60)  },
            { STAT_MANA,       "Mana",       0, 0, 0, false, 0, 0, false, D3DCOLOR_ARGB(255, 60, 120, 255) },
            { STAT_PIPS,       "Pips",       0, 0, 0, false, 0, 0, false, D3DCOLOR_ARGB(255, 255, 200, 60) },
            { STAT_POWER_PIPS, "Power Pips", 0, 0, 0, false, 0, 0, false, D3DCOLOR_ARGB(255, 255, 100, 255)},
            { STAT_GOLD,       "Gold",       0, 0, 0, false, 0, 0, false, D3DCOLOR_ARGB(255, 255, 215, 0)  },
            { STAT_ENERGY,     "Energy",     0, 0, 0, false, 0, 0, false, D3DCOLOR_ARGB(255, 60, 255, 60)  },
            { STAT_LEVEL,      "Level",      0, 0, 0, false, 0, 0, false, D3DCOLOR_ARGB(255, 200, 200, 200)},
            { STAT_XP,         "XP",         0, 0, 0, false, 0, 0, false, D3DCOLOR_ARGB(255, 180, 120, 255)},
        };

        // Known patterns for stat memory locations
        // W101 stores player stats in a character data structure
        // These are found through the wire protocol / fscommand data
        static inline bool fsCommandMode = true; // true = read from FSCommands, false = memory scan

        // Memory scan state for manual discovery
        struct ScanCandidate {
            uintptr_t address;
            int       lastValue;
        };
        static inline std::vector<ScanCandidate> scanCandidates;
        static inline StatID scanTarget = STAT_HEALTH;
        static inline int scanPhase = 0; // 0=initial, 1=narrowing

        static bool SafeRead(uintptr_t addr, void* buf, size_t sz) {
            __try {
                MEMORY_BASIC_INFORMATION mbi;
                if (!VirtualQuery(reinterpret_cast<void*>(addr), &mbi, sizeof(mbi)))
                    return false;
                if (mbi.State != MEM_COMMIT) return false;
                if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
                memcpy(buf, reinterpret_cast<void*>(addr), sz);
                return true;
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
        }

        static bool SafeWrite(uintptr_t addr, const void* buf, size_t sz) {
            __try {
                DWORD oldProt;
                if (!VirtualProtect(reinterpret_cast<void*>(addr), sz, PAGE_READWRITE, &oldProt))
                    return false;
                memcpy(reinterpret_cast<void*>(addr), buf, sz);
                VirtualProtect(reinterpret_cast<void*>(addr), sz, oldProt, &oldProt);
                return true;
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
        }

        // Full memory scan for a specific integer value
        static void InitialScan(int targetValue) {
            scanCandidates.clear();
            scanPhase = 0;

            HMODULE hMod = GetModuleHandleA("WizardGraphicalClient.exe");
            if (!hMod) return;

            uintptr_t base = reinterpret_cast<uintptr_t>(hMod);
            SYSTEM_INFO si;
            GetSystemInfo(&si);

            MEMORY_BASIC_INFORMATION mbi;
            uintptr_t addr = base;
            uintptr_t maxAddr = base + 0x10000000; // scan 256MB range

            while (addr < maxAddr &&
                   VirtualQuery(reinterpret_cast<void*>(addr), &mbi, sizeof(mbi))) {

                if (mbi.State == MEM_COMMIT &&
                    !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) &&
                    (mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE))) {

                    uintptr_t regionEnd = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
                    for (uintptr_t scan = addr; scan + 4 <= regionEnd; scan += 4) {
                        int val = 0;
                        if (SafeRead(scan, &val, 4) && val == targetValue) {
                            scanCandidates.push_back({scan, val});
                            if (scanCandidates.size() >= 100000) goto done;
                        }
                    }
                }

                addr = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
            }
            done:
            scanPhase = 1;
        }

        // Narrow scan: keep only candidates that now match a new value
        static void NarrowScan(int newValue) {
            std::vector<ScanCandidate> survivors;
            for (auto& c : scanCandidates) {
                int val = 0;
                if (SafeRead(c.address, &val, 4) && val == newValue) {
                    c.lastValue = val;
                    survivors.push_back(c);
                }
            }
            scanCandidates = survivors;

            // If only 1 candidate remains, lock it
            if (scanCandidates.size() == 1) {
                stats[scanTarget].address = scanCandidates[0].address;
                stats[scanTarget].found = true;
                scanPhase = 2;
            }
        }

        // Enforce frozen stats by writing back frozen values
        static void EnforceFrozenStats() {
            for (int i = 0; i < STAT_COUNT; i++) {
                if (stats[i].frozen && stats[i].found && stats[i].address) {
                    SafeWrite(stats[i].address, &stats[i].frozenValue, 4);
                }
            }
        }

        // Read stat values from discovered addresses
        static void ReadStatAddresses() {
            for (int i = 0; i < STAT_COUNT; i++) {
                if (stats[i].found && stats[i].address) {
                    int val = 0;
                    if (SafeRead(stats[i].address, &val, 4)) {
                        stats[i].lastValue = stats[i].currentValue;
                        stats[i].currentValue = val;
                    }
                }
            }
        }

    public:
        static bool Init() {
            active = true;
            return true;
        }

        static void Shutdown() { active = false; scanCandidates.clear(); }
        static bool IsActive() { return active; }

        // Parse stat values from FSCommands / wire protocol
        // This is the primary method - more reliable than memory scanning
        static void ProcessFSCommand(const std::string& cmd, const std::string& args) {
            if (!active) return;

            std::string lowerCmd = cmd;
            std::transform(lowerCmd.begin(), lowerCmd.end(), lowerCmd.begin(), ::tolower);

            // Health updates
            if (lowerCmd.find("health") != std::string::npos ||
                lowerCmd.find("hitpoint") != std::string::npos) {
                int cur = 0, mx = 0;
                if (sscanf(args.c_str(), "%d,%d", &cur, &mx) >= 1) {
                    stats[STAT_HEALTH].currentValue = cur;
                    if (mx > 0) stats[STAT_HEALTH].maxValue = mx;
                } else if (sscanf(args.c_str(), "%d", &cur) == 1) {
                    stats[STAT_HEALTH].currentValue = cur;
                }
            }

            // Mana updates
            if (lowerCmd.find("mana") != std::string::npos) {
                int cur = 0, mx = 0;
                if (sscanf(args.c_str(), "%d,%d", &cur, &mx) >= 1) {
                    stats[STAT_MANA].currentValue = cur;
                    if (mx > 0) stats[STAT_MANA].maxValue = mx;
                } else if (sscanf(args.c_str(), "%d", &cur) == 1) {
                    stats[STAT_MANA].currentValue = cur;
                }
            }

            // Gold
            if (lowerCmd.find("gold") != std::string::npos ||
                lowerCmd.find("coin") != std::string::npos) {
                int val = 0;
                if (sscanf(args.c_str(), "%d", &val) == 1) {
                    stats[STAT_GOLD].currentValue = val;
                }
            }

            // Pips
            if (lowerCmd.find("pip") != std::string::npos) {
                if (lowerCmd.find("power") != std::string::npos) {
                    int val = 0;
                    if (sscanf(args.c_str(), "%d", &val) == 1)
                        stats[STAT_POWER_PIPS].currentValue = val;
                } else {
                    int val = 0;
                    if (sscanf(args.c_str(), "%d", &val) == 1)
                        stats[STAT_PIPS].currentValue = val;
                }
            }

            // Energy
            if (lowerCmd.find("energy") != std::string::npos) {
                int cur = 0, mx = 0;
                if (sscanf(args.c_str(), "%d,%d", &cur, &mx) >= 1) {
                    stats[STAT_ENERGY].currentValue = cur;
                    if (mx > 0) stats[STAT_ENERGY].maxValue = mx;
                }
            }

            // Level / XP
            if (lowerCmd.find("level") != std::string::npos) {
                int val = 0;
                if (sscanf(args.c_str(), "%d", &val) == 1)
                    stats[STAT_LEVEL].currentValue = val;
            }
            if (lowerCmd.find("xp") != std::string::npos ||
                lowerCmd.find("experience") != std::string::npos) {
                int val = 0;
                if (sscanf(args.c_str(), "%d", &val) == 1)
                    stats[STAT_XP].currentValue = val;
            }

            // wireCommand often contains stat updates in serialized form
            if (lowerCmd == "wirecommand" || lowerCmd.find("wire") != std::string::npos) {
                // Wire commands are binary/serialized, but some contain readable stat updates
                // Parse common wire patterns for stat data
                // Format varies but may include: type|id|value or CSV of stats
                // This is a best-effort parse
            }
        }

        static void Update(float dt) {
            if (!active) return;

            DWORD now = GetTickCount();
            if (now - lastScanTime < scanInterval) return;
            lastScanTime = now;

            ReadStatAddresses();
            EnforceFrozenStats();
        }

        // Freeze a stat at its current value (writes back every frame)
        static void ToggleFreeze(StatID id) {
            if (id >= STAT_COUNT) return;
            stats[id].frozen = !stats[id].frozen;
            if (stats[id].frozen) {
                stats[id].frozenValue = stats[id].currentValue;
            }
        }

        // Start memory scan for a stat
        static void StartScan(StatID id, int currentValue) {
            scanTarget = id;
            InitialScan(currentValue);
        }

        static void NarrowScanStep(int newValue) {
            NarrowScan(newValue);
        }

        static const TrackedStat& GetStat(StatID id) { return stats[id]; }
        static size_t GetScanCandidates() { return scanCandidates.size(); }
        static int GetScanPhase() { return scanPhase; }

        // Manual address set (from external tools or prior discovery)
        static void SetStatAddress(StatID id, uintptr_t addr) {
            if (id >= STAT_COUNT) return;
            stats[id].address = addr;
            stats[id].found = true;
        }

        static bool HandleKey(unsigned short key, bool down) {
            if (!down || !active) return true;

            // Ctrl+H = toggle health freeze
            if (key == 'H' && (GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
                (GetAsyncKeyState(VK_SHIFT) & 0x8000)) {
                ToggleFreeze(STAT_HEALTH);
                return false;
            }

            // Ctrl+M = toggle mana freeze (already used by macros, use Ctrl+Shift+M)
            if (key == 'M' && (GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
                (GetAsyncKeyState(VK_SHIFT) & 0x8000)) {
                ToggleFreeze(STAT_MANA);
                return false;
            }

            return true;
        }

        static int RenderPanel(IDirect3DDevice9* dev, int x, int y) {
            int h = 30 + STAT_COUNT * 16 + 20;
            Overlay::DrawFilledRect(dev, x, y, 360, h, Overlay::BgPanel);
            Overlay::DrawRect(x, y, 360, h, D3DCOLOR_ARGB(255, 100, 200, 255), false);

            int ty = y + 4;
            Overlay::DrawText(x + 5, ty, D3DCOLOR_ARGB(255, 100, 200, 255),
                "STAT SCANNER"); ty += 16;

            for (int i = 0; i < STAT_COUNT; i++) {
                auto& s = stats[i];
                D3DCOLOR c = s.color;

                char line[128];
                if (s.maxValue > 0) {
                    float pct = (float)s.currentValue / s.maxValue * 100.0f;
                    snprintf(line, sizeof(line), "%-11s %d / %d (%.0f%%)%s%s",
                        s.name, s.currentValue, s.maxValue, pct,
                        s.frozen ? " [FROZEN]" : "",
                        s.found ? " [MEM]" : "");
                } else if (s.currentValue != 0) {
                    snprintf(line, sizeof(line), "%-11s %d%s%s",
                        s.name, s.currentValue,
                        s.frozen ? " [FROZEN]" : "",
                        s.found ? " [MEM]" : "");
                } else {
                    snprintf(line, sizeof(line), "%-11s ---", s.name);
                    c = Overlay::Gray;
                }

                Overlay::DrawText(x + 5, ty, c, "%s", line);

                // Draw health/mana bars
                if ((i == STAT_HEALTH || i == STAT_MANA || i == STAT_ENERGY) &&
                    s.maxValue > 0 && s.currentValue > 0) {
                    float ratio = std::clamp((float)s.currentValue / s.maxValue, 0.0f, 1.0f);
                    int barW = 80, barH = 4;
                    int barX = x + 280;
                    Overlay::DrawFilledRect(dev, barX, ty + 4, barW, barH,
                        D3DCOLOR_ARGB(100, 40, 40, 40));
                    Overlay::DrawFilledRect(dev, barX, ty + 4,
                        static_cast<int>(ratio * barW), barH, c);
                }

                ty += 16;
            }

            // Scan status
            if (scanPhase == 1 && !scanCandidates.empty()) {
                Overlay::DrawText(x + 5, ty, Overlay::Yellow,
                    "Scanning: %zu candidates for %s",
                    scanCandidates.size(), stats[scanTarget].name);
            } else {
                Overlay::DrawText(x + 5, ty, Overlay::Gray,
                    "Ctrl+Sh+H/M freeze | FSCmd + MemScan");
            }

            return y + h + 4;
        }
    };

} // namespace W101Hook

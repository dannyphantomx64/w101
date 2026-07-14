#pragma once
#include <Windows.h>
#include <d3d9.h>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include "../framework.h"
#include "../overlay.h"

namespace W101Hook {

    class WispCollector {
    public:
        enum WispType { WISP_HEALTH, WISP_MANA, WISP_GOLD, WISP_UNKNOWN };

        struct Wisp {
            void*     sprite;
            float     x, y;
            float     distance;
            WispType  type;
            bool      collected;
            DWORD     lastSeen;
        };

    private:
        static inline bool  active = false;
        static inline bool  autoCollect = false;
        static inline bool  scanning = false;
        static inline DWORD lastScanTime = 0;
        static inline DWORD scanInterval = 500;  // scan every 500ms
        static inline DWORD lastCollectTime = 0;
        static inline DWORD collectCooldown = 300;
        static inline float collectRadius = 5000.0f;
        static inline float playerX = 0, playerY = 0;
        static inline float savedX = 0, savedY = 0;
        static inline bool  hasSaved = false;
        static inline bool  returnAfterCollect = true;

        static inline std::vector<Wisp> wisps;
        static inline uint32_t totalCollected = 0;
        static inline uint32_t healthCollected = 0;
        static inline uint32_t manaCollected = 0;
        static inline int      currentTarget = -1;

        struct SWFMatrix {
            float a, b, c, d, tx, ty;
        };

        using GetWorldMatrixFn   = void*(*)(void* sprite);
        using SetMatrixFn        = void(*)(void* sprite, const void* matrix);
        using GetCharCountFn     = int(*)(void* sprite);
        using GetCharAtDepthFn   = void*(*)(void* sprite, int depth);
        using GetNameFn          = const char*(*)(void* sprite);

        static inline GetWorldMatrixFn   fnGetWorldMatrix = nullptr;
        static inline SetMatrixFn        fnSetMatrix = nullptr;
        static inline GetCharCountFn     fnGetCharCount = nullptr;
        static inline GetCharAtDepthFn   fnGetCharAtDepth = nullptr;
        static inline GetNameFn          fnGetName = nullptr;

        static bool ReadPos(void* sprite, float& ox, float& oy) {
            if (!fnGetWorldMatrix || !sprite) return false;
            void* mat = fnGetWorldMatrix(sprite);
            if (!mat) return false;
            auto* m = reinterpret_cast<SWFMatrix*>(mat);
            ox = m->tx;
            oy = m->ty;
            return true;
        }

        static bool TeleportPlayer(root* r, float x, float y) {
            if (!fnGetWorldMatrix || !fnSetMatrix || !r) return false;
            void* mat = fnGetWorldMatrix(static_cast<void*>(r));
            if (!mat) return false;
            SWFMatrix newMat = *reinterpret_cast<SWFMatrix*>(mat);
            newMat.tx = x;
            newMat.ty = y;
            fnSetMatrix(static_cast<void*>(r), &newMat);
            return true;
        }

        static const char* SafeGetName(void* sprite) {
            __try {
                return fnGetName(sprite);
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                return nullptr;
            }
        }

        static WispType ClassifyWisp(void* sprite) {
            if (!fnGetName || !sprite) return WISP_UNKNOWN;

            const char* rawName = SafeGetName(sprite);
            if (!rawName) return WISP_UNKNOWN;

            char lower[128];
            int i = 0;
            for (; rawName[i] && i < 126; i++)
                lower[i] = (char)tolower((unsigned char)rawName[i]);
            lower[i] = '\0';

            if (strstr(lower, "health") || strstr(lower, "wisph") ||
                strstr(lower, "redwis") || strstr(lower, "hp"))
                return WISP_HEALTH;

            if (strstr(lower, "mana") || strstr(lower, "wispm") ||
                strstr(lower, "bluewis") || strstr(lower, "mp"))
                return WISP_MANA;

            if (strstr(lower, "gold") || strstr(lower, "coin") ||
                strstr(lower, "goldwis"))
                return WISP_GOLD;

            if (strstr(lower, "wisp") || strstr(lower, "pickup") ||
                strstr(lower, "orb") || strstr(lower, "collectible"))
                return WISP_HEALTH;

            return WISP_UNKNOWN;
        }

        static int SafeGetChildCount(void* sprite) {
            __try {
                return fnGetCharCount(sprite);
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                return 0;
            }
        }

        static void* SafeGetChild(void* sprite, int idx) {
            __try {
                return fnGetCharAtDepth(sprite, idx);
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                return nullptr;
            }
        }

        static void ScanEntities(root* r, void* sprite, int depth) {
            if (!sprite || depth > 4) return;

            WispType type = ClassifyWisp(sprite);
            if (type != WISP_UNKNOWN) {
                float sx = 0, sy = 0;
                if (ReadPos(sprite, sx, sy)) {
                    float dx = sx - playerX;
                    float dy = sy - playerY;
                    float dist = sqrtf(dx * dx + dy * dy);

                    if (dist < collectRadius) {
                        bool found = false;
                        for (auto& w : wisps) {
                            if (w.sprite == sprite) {
                                w.x = sx;
                                w.y = sy;
                                w.distance = dist;
                                w.lastSeen = GetTickCount();
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            wisps.push_back({sprite, sx, sy, dist, type, false, GetTickCount()});
                        }
                    }
                }
            }

            if (fnGetCharCount && fnGetCharAtDepth) {
                int count = SafeGetChildCount(sprite);
                for (int i = 0; i < count && i < 200; i++) {
                    void* child = SafeGetChild(sprite, i);
                    if (child && child != sprite) {
                        ScanEntities(r, child, depth + 1);
                    }
                }
            }
        }

        static void PruneStaleWisps() {
            DWORD now = GetTickCount();
            wisps.erase(
                std::remove_if(wisps.begin(), wisps.end(),
                    [now](const Wisp& w) {
                        return w.collected || (now - w.lastSeen > 5000);
                    }),
                wisps.end()
            );
        }

        static void SortByDistance() {
            std::sort(wisps.begin(), wisps.end(),
                [](const Wisp& a, const Wisp& b) { return a.distance < b.distance; });
        }

    public:
        static bool Init() {
            fnGetWorldMatrix = reinterpret_cast<GetWorldMatrixFn>(
                Offsets::Resolve(W101::SpriteInstance::GetWorldMatrix));
            fnSetMatrix = reinterpret_cast<SetMatrixFn>(
                Offsets::Resolve(W101::SpriteInstance::SetMatrix));
            fnGetCharCount = reinterpret_cast<GetCharCountFn>(
                Offsets::Resolve(W101::SpriteInstance::GetCharacterCount));
            fnGetCharAtDepth = reinterpret_cast<GetCharAtDepthFn>(
                Offsets::Resolve(W101::SpriteInstance::GetCharacterAtDepth));
            fnGetName = reinterpret_cast<GetNameFn>(
                Offsets::Resolve(W101::SpriteInstance::GetName));

            active = (fnGetWorldMatrix && fnSetMatrix);
            return active;
        }

        static void Shutdown() { active = false; autoCollect = false; wisps.clear(); }
        static bool IsActive() { return active; }
        static bool IsAutoCollecting() { return autoCollect; }
        static size_t GetWispCount() { return wisps.size(); }
        static uint32_t GetTotalCollected() { return totalCollected; }

        static void Update(root* r, float dt) {
            if (!active || !r) return;

            ReadPos(static_cast<void*>(r), playerX, playerY);

            DWORD now = GetTickCount();

            // Periodic entity scan for wisps
            if (now - lastScanTime >= scanInterval) {
                lastScanTime = now;
                ScanEntities(r, static_cast<void*>(r), 0);
                PruneStaleWisps();
                SortByDistance();
            }

            // Auto-collect: teleport to nearest wisp, collect, return
            if (autoCollect && !wisps.empty()) {
                if (now - lastCollectTime >= collectCooldown) {
                    lastCollectTime = now;

                    // Find nearest uncollected wisp
                    int nearest = -1;
                    for (int i = 0; i < (int)wisps.size(); i++) {
                        if (!wisps[i].collected) { nearest = i; break; }
                    }

                    if (nearest >= 0) {
                        if (!hasSaved) {
                            savedX = playerX;
                            savedY = playerY;
                            hasSaved = true;
                        }

                        // Teleport to wisp
                        TeleportPlayer(r, wisps[nearest].x, wisps[nearest].y);
                        wisps[nearest].collected = true;
                        totalCollected++;

                        if (wisps[nearest].type == WISP_HEALTH) healthCollected++;
                        else if (wisps[nearest].type == WISP_MANA) manaCollected++;
                    } else if (hasSaved && returnAfterCollect) {
                        // All wisps collected, return to original position
                        TeleportPlayer(r, savedX, savedY);
                        hasSaved = false;
                    }
                }
            }
        }

        static void CollectNearest(root* r) {
            if (!active || wisps.empty() || !r) return;

            for (auto& w : wisps) {
                if (!w.collected) {
                    savedX = playerX;
                    savedY = playerY;
                    hasSaved = true;

                    TeleportPlayer(r, w.x, w.y);
                    w.collected = true;
                    totalCollected++;
                    if (w.type == WISP_HEALTH) healthCollected++;
                    else if (w.type == WISP_MANA) manaCollected++;
                    break;
                }
            }
        }

        static void ReturnToSaved(root* r) {
            if (hasSaved && r) {
                TeleportPlayer(r, savedX, savedY);
                hasSaved = false;
            }
        }

        static void ToggleAutoCollect() { autoCollect = !autoCollect; }
        static void SetRadius(float r) { collectRadius = std::clamp(r, 500.0f, 50000.0f); }
        static void SetReturnAfterCollect(bool v) { returnAfterCollect = v; }

        static const char* WispTypeName(WispType t) {
            switch (t) {
                case WISP_HEALTH: return "HP";
                case WISP_MANA:   return "MP";
                case WISP_GOLD:   return "GOLD";
                default:          return "???";
            }
        }

        static D3DCOLOR WispTypeColor(WispType t) {
            switch (t) {
                case WISP_HEALTH: return D3DCOLOR_ARGB(255, 255, 60, 60);
                case WISP_MANA:   return D3DCOLOR_ARGB(255, 60, 120, 255);
                case WISP_GOLD:   return D3DCOLOR_ARGB(255, 255, 215, 0);
                default:          return Overlay::Gray;
            }
        }

        static bool HandleKey(unsigned short key, bool down, root* r) {
            if (!down || !active) return true;

            // Ctrl+W = toggle auto collect
            if (key == 'W' && (GetAsyncKeyState(VK_CONTROL) & 0x8000)) {
                if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
                    // Ctrl+Shift+W = collect nearest single
                    CollectNearest(r);
                } else {
                    ToggleAutoCollect();
                }
                return false;
            }

            return true;
        }

        static int RenderPanel(IDirect3DDevice9* dev, int x, int y) {
            int wispShow = wisps.size() < 6 ? (int)wisps.size() : 6;
            int h = 70 + wispShow * 14;
            Overlay::DrawFilledRect(dev, x, y, 360, h, Overlay::BgPanel);
            Overlay::DrawRect(x, y, 360, h, D3DCOLOR_ARGB(255, 60, 255, 180), false);

            int ty = y + 4;
            D3DCOLOR headerColor = autoCollect ?
                D3DCOLOR_ARGB(255, 60, 255, 60) : D3DCOLOR_ARGB(255, 60, 255, 180);
            Overlay::DrawText(x + 5, ty, headerColor,
                "WISP COLLECTOR %s", autoCollect ? "[AUTO]" : ""); ty += 16;

            Overlay::DrawText(x + 5, ty, Overlay::White,
                "Wisps found: %d  |  Collected: %u",
                (int)wisps.size(), totalCollected); ty += 14;

            Overlay::DrawText(x + 5, ty, Overlay::White,
                "HP: %u  MP: %u  |  Radius: %.0f",
                healthCollected, manaCollected, collectRadius); ty += 14;

            Overlay::DrawText(x + 5, ty, Overlay::Gray,
                "Ctrl+W auto  Ctrl+Sh+W nearest"); ty += 16;

            // Show nearest wisps
            for (int i = 0; i < wispShow; i++) {
                auto& w = wisps[i];
                Overlay::DrawText(x + 5, ty, WispTypeColor(w.type),
                    "[%s] (%.0f, %.0f) dist: %.0f%s",
                    WispTypeName(w.type), w.x, w.y, w.distance,
                    w.collected ? " [GOT]" : "");
                ty += 14;
            }

            return y + h + 4;
        }
    };

} // namespace W101Hook

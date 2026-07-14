#pragma once
#include <Windows.h>
#include <d3d9.h>
#include <vector>
#include <string>
#include <mutex>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include "../framework.h"
#include "../overlay.h"

namespace W101Hook {

    class EntityRadar {
    public:
        struct Entity {
            uintptr_t   ptr;
            float       x, y;
            float       scaleX, scaleY;
            float       rotation;
            int         depth;
            int         frameCount;
            int         currentFrame;
            bool        visible;
            std::string name;
            uint32_t    hash;
        };

        enum RadarMode {
            RADAR_MINI,
            RADAR_FULL,
            RADAR_LIST
        };

    private:
        // gameswf sprite enumeration types
        using GetCharCountFn    = int(*)(void* sprite);
        using GetCharAtFn       = void*(*)(void* sprite, int index);
        using GetDepthFn        = int(*)(void* sprite);
        using GetNameFn         = const char*(*)(void* character);
        using GetFrameCountFn   = int(*)(void* sprite);
        using GetCurrentFrameFn = int(*)(void* sprite);
        using GetVisibleFn      = bool(*)(void* character);
        using GetWorldMatrixFn  = void*(*)(void* character);

        struct SWFMatrix {
            float a, b, c, d, tx, ty;
        };

        static inline GetCharCountFn    fnGetCharCount = nullptr;
        static inline GetCharAtFn       fnGetCharAt = nullptr;
        static inline GetNameFn         fnGetName = nullptr;
        static inline GetFrameCountFn   fnGetFrameCount = nullptr;
        static inline GetCurrentFrameFn fnGetCurrentFrame = nullptr;
        static inline GetVisibleFn      fnGetVisible = nullptr;
        static inline GetWorldMatrixFn  fnGetWorldMatrix = nullptr;

        static inline std::vector<Entity> entities;
        static inline std::mutex          entityMtx;
        static inline bool                active = false;
        static inline bool                scanning = true;
        static inline RadarMode           mode = RADAR_MINI;
        static inline int                 scanDepth = 3;
        static inline int                 maxEntities = 500;
        static inline float               radarRange = 2000.0f;
        static inline float               radarScale = 0.15f;
        static inline int                 radarSize = 200;

        // Player reference
        static inline float playerX = 0.0f;
        static inline float playerY = 0.0f;

        // Filters
        static inline bool  filterVisible = false;
        static inline bool  filterNamed = false;
        static inline float filterMinDist = 0.0f;
        static inline float filterMaxDist = 0.0f;
        static inline std::string filterName;

        // Stats
        static inline int   lastScanCount = 0;
        static inline DWORD lastScanTime = 0;
        static inline int   totalScanned = 0;

        static uint32_t HashEntity(void* ptr, float x, float y) {
            uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
            uint32_t h = static_cast<uint32_t>(addr ^ (addr >> 16));
            h ^= *reinterpret_cast<uint32_t*>(&x);
            h ^= *reinterpret_cast<uint32_t*>(&y) << 8;
            return h;
        }

        struct RawEntityData {
            uintptr_t ptr;
            float x, y, scaleX, scaleY, rotation;
            char  name[128];
            int   frameCount, currentFrame;
            bool  visible;
            bool  valid;
        };

        static RawEntityData SafeReadEntity(void* sprite) {
            RawEntityData d = {};
            __try {
                d.ptr = reinterpret_cast<uintptr_t>(sprite);
                if (fnGetWorldMatrix) {
                    void* matrix = fnGetWorldMatrix(sprite);
                    if (matrix) {
                        auto* m = reinterpret_cast<SWFMatrix*>(matrix);
                        d.x = m->tx; d.y = m->ty;
                        d.scaleX = sqrtf(m->a * m->a + m->b * m->b);
                        d.scaleY = sqrtf(m->c * m->c + m->d * m->d);
                        d.rotation = atan2f(m->b, m->a) * 57.2957795f;
                    }
                }
                if (fnGetName) {
                    const char* n = fnGetName(sprite);
                    if (n && n[0]) { strncpy(d.name, n, 127); d.name[127] = '\0'; }
                }
                if (fnGetFrameCount) d.frameCount = fnGetFrameCount(sprite);
                if (fnGetCurrentFrame) d.currentFrame = fnGetCurrentFrame(sprite);
                d.visible = fnGetVisible ? fnGetVisible(sprite) : true;
                d.valid = true;
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                d.valid = false;
            }
            return d;
        }

        static int SafeChildCount(void* sprite) {
            __try { return fnGetCharCount(sprite); }
            __except(EXCEPTION_EXECUTE_HANDLER) { return 0; }
        }

        static void* SafeChildAt(void* sprite, int i) {
            __try { return fnGetCharAt(sprite, i); }
            __except(EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
        }

        static void ScanSprite(void* sprite, int depth, std::vector<Entity>& found) {
            if (!sprite || depth <= 0) return;
            if (static_cast<int>(found.size()) >= maxEntities) return;

            RawEntityData d = SafeReadEntity(sprite);
            if (!d.valid) return;

            Entity ent = {};
            ent.ptr = d.ptr;
            ent.x = d.x; ent.y = d.y;
            ent.scaleX = d.scaleX; ent.scaleY = d.scaleY;
            ent.rotation = d.rotation;
            if (d.name[0]) ent.name = d.name;
            ent.frameCount = d.frameCount;
            ent.currentFrame = d.currentFrame;
            ent.visible = d.visible;
            ent.depth = depth;
            ent.hash = HashEntity(sprite, ent.x, ent.y);

            bool pass = true;
            if (filterVisible && !ent.visible) pass = false;
            if (filterNamed && ent.name.empty()) pass = false;
            if (!filterName.empty() && ent.name.find(filterName) == std::string::npos) pass = false;
            if (filterMaxDist > 0.0f) {
                float dx = ent.x - playerX;
                float dy = ent.y - playerY;
                float dist = sqrtf(dx * dx + dy * dy);
                if (dist > filterMaxDist || dist < filterMinDist) pass = false;
            }
            if (pass) found.push_back(ent);

            if (fnGetCharCount && fnGetCharAt) {
                int childCount = SafeChildCount(sprite);
                childCount = std::min(childCount, 200);
                for (int i = 0; i < childCount; i++) {
                    void* child = SafeChildAt(sprite, i);
                    if (child && child != sprite) {
                        ScanSprite(child, depth - 1, found);
                        if (static_cast<int>(found.size()) >= maxEntities) break;
                    }
                }
            }
        }

    public:
        static bool Init() {
            fnGetWorldMatrix = reinterpret_cast<GetWorldMatrixFn>(
                Offsets::Resolve(W101::SpriteInstance::GetWorldMatrix));
            fnGetCharCount = reinterpret_cast<GetCharCountFn>(
                Offsets::Resolve(W101::SpriteInstance::GetCharacterCount));
            fnGetCharAt = reinterpret_cast<GetCharAtFn>(
                Offsets::Resolve(W101::SpriteInstance::GetCharacterAtDepth));
            fnGetName = reinterpret_cast<GetNameFn>(
                Offsets::Resolve(W101::SpriteInstance::GetName));
            fnGetFrameCount = reinterpret_cast<GetFrameCountFn>(
                Offsets::Resolve(W101::Root::GetTotalFrames));
            fnGetCurrentFrame = reinterpret_cast<GetCurrentFrameFn>(
                Offsets::Resolve(W101::Root::GetCurrentFrame));

            active = (fnGetWorldMatrix != nullptr);
            return active;
        }

        static void Shutdown() { active = false; }
        static bool IsActive() { return active; }

        // --- Scanning (called from advance hook) ---

        static void Update(root* r, float dt) {
            if (!active || !scanning || !r) return;

            // Update player position
            if (fnGetWorldMatrix) {
                void* matrix = fnGetWorldMatrix(static_cast<void*>(r));
                if (matrix) {
                    auto* m = reinterpret_cast<SWFMatrix*>(matrix);
                    playerX = m->tx;
                    playerY = m->ty;
                }
            }

            // Scan entities from root
            DWORD startTick = GetTickCount();
            std::vector<Entity> found;
            found.reserve(128);

            ScanSprite(static_cast<void*>(r), scanDepth, found);

            lastScanTime = GetTickCount() - startTick;
            lastScanCount = static_cast<int>(found.size());
            totalScanned++;

            {
                std::lock_guard<std::mutex> lock(entityMtx);
                entities = std::move(found);
            }
        }

        // --- Controls ---
        static void ToggleScanning() { scanning = !scanning; }
        static bool IsScanning() { return scanning; }

        static void SetMode(RadarMode m) { mode = m; }
        static RadarMode GetMode() { return mode; }
        static void CycleMode() {
            mode = static_cast<RadarMode>((mode + 1) % 3);
        }

        static void SetScanDepth(int d) { scanDepth = std::clamp(d, 1, 8); }
        static int GetScanDepth() { return scanDepth; }

        static void SetRadarRange(float r) { radarRange = std::clamp(r, 100.0f, 20000.0f); }
        static float GetRadarRange() { return radarRange; }

        static void SetFilterVisible(bool v) { filterVisible = v; }
        static void SetFilterNamed(bool v) { filterNamed = v; }
        static void SetFilterName(const std::string& n) { filterName = n; }
        static void SetFilterDistance(float minD, float maxD) {
            filterMinDist = minD; filterMaxDist = maxD;
        }
        static void ClearFilters() {
            filterVisible = false; filterNamed = false;
            filterName.clear(); filterMinDist = 0; filterMaxDist = 0;
        }

        // --- Data ---
        static int GetEntityCount() {
            std::lock_guard<std::mutex> lock(entityMtx);
            return static_cast<int>(entities.size());
        }

        static std::vector<Entity> GetEntities() {
            std::lock_guard<std::mutex> lock(entityMtx);
            return entities;
        }

        static float GetPlayerX() { return playerX; }
        static float GetPlayerY() { return playerY; }
        static int GetLastScanTime() { return static_cast<int>(lastScanTime); }
        static int GetTotalScanned() { return totalScanned; }

        // --- Overlay ---

        static int RenderPanel(IDirect3DDevice9* dev, int x, int y) {
            std::lock_guard<std::mutex> lock(entityMtx);

            int entCount = static_cast<int>(entities.size());

            if (mode == RADAR_MINI || mode == RADAR_FULL) {
                return RenderRadarView(dev, x, y, entCount);
            } else {
                return RenderListView(dev, x, y, entCount);
            }
        }

    private:
        static int RenderRadarView(IDirect3DDevice9* dev, int x, int y, int entCount) {
            int size = (mode == RADAR_FULL) ? 300 : radarSize;
            int panelH = size + 40;

            Overlay::DrawFilledRect(dev, x, y, size + 20, panelH, Overlay::BgPanel);
            Overlay::DrawRect(x, y, size + 20, panelH,
                D3DCOLOR_ARGB(255, 100, 255, 200), false);

            int ty = y + 4;
            Overlay::DrawText(x + 5, ty, D3DCOLOR_ARGB(255, 100, 255, 200),
                "ENTITY RADAR [%s]  %d entities  %dms",
                mode == RADAR_FULL ? "FULL" : "MINI",
                entCount, lastScanTime); ty += 16;

            // Radar circle center
            int cx = x + 10 + size / 2;
            int cy = ty + size / 2;

            // Background circle (drawn as filled rect — approximation)
            Overlay::DrawFilledRect(dev, x + 10, ty, size, size,
                D3DCOLOR_ARGB(150, 10, 15, 20));

            // Crosshair
            int halfSize = size / 2;
            Overlay::DrawFilledRect(dev, cx - halfSize, cy, size, 1,
                D3DCOLOR_ARGB(60, 100, 255, 200));
            Overlay::DrawFilledRect(dev, cx, cy - halfSize, 1, size,
                D3DCOLOR_ARGB(60, 100, 255, 200));

            // Range rings
            int ring1 = static_cast<int>(halfSize * 0.33f);
            int ring2 = static_cast<int>(halfSize * 0.66f);
            Overlay::DrawRect(cx - ring1, cy - ring1, ring1 * 2, ring1 * 2,
                D3DCOLOR_ARGB(30, 100, 255, 200), false);
            Overlay::DrawRect(cx - ring2, cy - ring2, ring2 * 2, ring2 * 2,
                D3DCOLOR_ARGB(30, 100, 255, 200), false);

            // Player dot (center)
            Overlay::DrawFilledRect(dev, cx - 2, cy - 2, 5, 5,
                D3DCOLOR_ARGB(255, 60, 255, 60));

            // Entity dots
            for (auto& ent : entities) {
                float dx = ent.x - playerX;
                float dy = ent.y - playerY;
                float dist = sqrtf(dx * dx + dy * dy);

                if (dist > radarRange) continue;

                float scale = halfSize / radarRange;
                int dotX = cx + static_cast<int>(dx * scale);
                int dotY = cy + static_cast<int>(dy * scale);

                // Clamp to radar bounds
                dotX = std::clamp(dotX, x + 12, x + 8 + size);
                dotY = std::clamp(dotY, ty + 2, ty + size - 2);

                // Color based on entity properties
                D3DCOLOR dotColor;
                if (!ent.name.empty()) {
                    dotColor = D3DCOLOR_ARGB(255, 255, 200, 60); // named = yellow
                } else if (ent.frameCount > 1) {
                    dotColor = D3DCOLOR_ARGB(255, 255, 100, 60); // animated = orange
                } else if (!ent.visible) {
                    dotColor = D3DCOLOR_ARGB(120, 100, 100, 100); // hidden = dim gray
                } else {
                    dotColor = D3DCOLOR_ARGB(200, 200, 200, 200); // generic = white
                }

                Overlay::DrawFilledRect(dev, dotX - 1, dotY - 1, 3, 3, dotColor);

                // Draw name for nearby named entities
                if (!ent.name.empty() && dist < radarRange * 0.5f) {
                    std::string label = ent.name;
                    if (label.length() > 12) label = label.substr(0, 12);
                    Overlay::DrawText(dotX + 4, dotY - 5, dotColor, "%s", label.c_str());
                }
            }

            // Range label
            Overlay::DrawText(x + 5, ty + size + 2, Overlay::Gray,
                "Range: %.0f  |  Player: (%.0f, %.0f)",
                radarRange, playerX, playerY);

            return y + panelH + 4;
        }

        static int RenderListView(IDirect3DDevice9* dev, int x, int y, int entCount) {
            int showCount = std::min(entCount, 12);
            int h = 36 + showCount * 13;

            Overlay::DrawFilledRect(dev, x, y, 520, h, Overlay::BgPanel);
            Overlay::DrawRect(x, y, 520, h, D3DCOLOR_ARGB(255, 100, 255, 200), false);

            int ty = y + 4;
            Overlay::DrawText(x + 5, ty, D3DCOLOR_ARGB(255, 100, 255, 200),
                "ENTITY LIST  %d entities  Depth:%d  %dms",
                entCount, scanDepth, lastScanTime); ty += 16;

            Overlay::DrawText(x + 5, ty, Overlay::Gray,
                "Ptr              X         Y        Name"); ty += 14;

            // Sort by distance
            std::vector<std::pair<float, int>> sorted;
            for (int i = 0; i < entCount; i++) {
                float dx = entities[i].x - playerX;
                float dy = entities[i].y - playerY;
                sorted.push_back({ dx * dx + dy * dy, i });
            }
            std::sort(sorted.begin(), sorted.end());

            for (int s = 0; s < showCount; s++) {
                auto& ent = entities[sorted[s].second];
                float dist = sqrtf(sorted[s].first);

                D3DCOLOR c = ent.name.empty() ? Overlay::Gray : Overlay::White;
                std::string name = ent.name.empty() ? "-" : ent.name;
                if (name.length() > 20) name = name.substr(0, 20) + "..";

                Overlay::DrawText(x + 5, ty, c,
                    "0x%012llX  %7.0f  %7.0f  %s (%.0f)",
                    ent.ptr, ent.x, ent.y, name.c_str(), dist);
                ty += 13;
            }

            return y + h + 4;
        }
    };

} // namespace W101Hook

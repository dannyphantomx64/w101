#pragma once
#include <Windows.h>
#include <d3d9.h>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include "../framework.h"
#include "../overlay.h"

namespace W101Hook {

    class Teleporter {
    public:
        struct Position {
            float x, y;
        };

        struct Waypoint {
            std::string name;
            Position    pos;
            DWORD       timestamp;
        };

    private:
        // gameswf matrix layout: a=scaleX, b=skewY, c=skewX, d=scaleY, tx=translateX, ty=translateY
        // SWF matrix is a 2x3 affine: [a c tx; b d ty]
        // tx/ty at offsets +0x10 and +0x14 in the matrix struct (floats)
        struct SWFMatrix {
            float a, b, c, d, tx, ty;
        };

        // Function types from the dump
        using GetWorldMatrixFn  = void*(*)(void* spriteInstance);
        using SetMatrixFn       = void(*)(void* spriteInstance, const void* matrix);
        using GetCharacterFn    = void*(*)(void* root);

        static inline GetWorldMatrixFn  fnGetWorldMatrix = nullptr;
        static inline SetMatrixFn       fnSetMatrix = nullptr;
        static inline GetCharacterFn    fnGetCharacter = nullptr;

        static inline std::vector<Waypoint> waypoints;
        static inline Position              currentPos = { 0, 0 };
        static inline Position              lastSavedPos = { 0, 0 };
        static inline bool                  active = false;
        static inline bool                  tracking = true;
        static inline int                   selectedWaypoint = 0;

        // Teleport smoothing
        static inline bool      smoothTeleport = false;
        static inline Position  teleportTarget = { 0, 0 };
        static inline float     teleportSpeed = 500.0f; // units per second
        static inline bool      teleporting = false;

        // Distance tracking
        static inline float     totalDistance = 0.0f;
        static inline Position  prevTrackPos = { 0, 0 };
        static inline bool      hasFirstPos = false;

        // Noclip
        static inline bool      noclipEnabled = false;
        static inline float     noclipSpeed = 300.0f;

        static void* GetPlayerSprite(root* r) {
            if (!r) return nullptr;
            // root -> get current character -> sprite instance
            // Using the sprite instance chain from the dump
            // root itself is a movie_def_impl / sprite_instance
            return static_cast<void*>(r);
        }

        static bool ReadPosition(void* sprite, Position& out) {
            if (!sprite || !fnGetWorldMatrix) return false;

            void* matrix = fnGetWorldMatrix(sprite);
            if (!matrix) return false;

            // Matrix layout: tx at offset 0x10, ty at offset 0x14
            // Standard SWF matrix is 6 floats: a, b, c, d, tx, ty
            auto* m = reinterpret_cast<SWFMatrix*>(matrix);
            out.x = m->tx;
            out.y = m->ty;
            return true;
        }

        static bool WritePosition(void* sprite, const Position& pos) {
            if (!sprite || !fnGetWorldMatrix || !fnSetMatrix) return false;

            void* matrix = fnGetWorldMatrix(sprite);
            if (!matrix) return false;

            // Copy current matrix, modify translation
            SWFMatrix newMatrix = *reinterpret_cast<SWFMatrix*>(matrix);
            newMatrix.tx = pos.x;
            newMatrix.ty = pos.y;

            fnSetMatrix(sprite, &newMatrix);
            return true;
        }

    public:
        static bool Init() {
            // Resolve function pointers from dump offsets
            fnGetWorldMatrix = reinterpret_cast<GetWorldMatrixFn>(
                Offsets::Resolve(W101::SpriteInstance::GetWorldMatrix));
            fnSetMatrix = reinterpret_cast<SetMatrixFn>(
                Offsets::Resolve(W101::SpriteInstance::SetMatrix));

            if (!fnGetWorldMatrix || !fnSetMatrix) return false;

            active = true;
            return true;
        }

        static void Shutdown() {
            active = false;
            teleporting = false;
            noclipEnabled = false;
        }

        static bool IsActive() { return active; }

        // --- Position Tracking (called every frame from advance hook) ---

        static void Update(root* r, float dt) {
            if (!active || !tracking || !r) return;

            void* sprite = GetPlayerSprite(r);
            if (!sprite) return;

            Position newPos;
            if (!ReadPosition(sprite, newPos)) return;

            // Distance tracking
            if (hasFirstPos) {
                float dx = newPos.x - prevTrackPos.x;
                float dy = newPos.y - prevTrackPos.y;
                float dist = sqrtf(dx * dx + dy * dy);
                if (dist < 10000.0f) { // sanity check
                    totalDistance += dist;
                }
            }
            prevTrackPos = newPos;
            hasFirstPos = true;
            currentPos = newPos;

            // Smooth teleport interpolation
            if (teleporting) {
                float dx = teleportTarget.x - currentPos.x;
                float dy = teleportTarget.y - currentPos.y;
                float dist = sqrtf(dx * dx + dy * dy);

                if (dist < 2.0f) {
                    WritePosition(sprite, teleportTarget);
                    teleporting = false;
                } else {
                    float step = teleportSpeed * dt;
                    if (step >= dist) {
                        WritePosition(sprite, teleportTarget);
                        teleporting = false;
                    } else {
                        float ratio = step / dist;
                        Position interp;
                        interp.x = currentPos.x + dx * ratio;
                        interp.y = currentPos.y + dy * ratio;
                        WritePosition(sprite, interp);
                    }
                }
            }

            // Noclip movement
            if (noclipEnabled) {
                Position movePos = currentPos;
                bool moved = false;

                if (GetAsyncKeyState('W') & 0x8000) { movePos.y -= noclipSpeed * dt; moved = true; }
                if (GetAsyncKeyState('S') & 0x8000) { movePos.y += noclipSpeed * dt; moved = true; }
                if (GetAsyncKeyState('A') & 0x8000) { movePos.x -= noclipSpeed * dt; moved = true; }
                if (GetAsyncKeyState('D') & 0x8000) { movePos.x += noclipSpeed * dt; moved = true; }

                // Shift = boost
                if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
                    float boost = 3.0f;
                    if (moved) {
                        movePos.x = currentPos.x + (movePos.x - currentPos.x) * boost;
                        movePos.y = currentPos.y + (movePos.y - currentPos.y) * boost;
                    }
                }

                if (moved) {
                    WritePosition(sprite, movePos);
                }
            }
        }

        // --- Waypoints ---

        static void SaveWaypoint(const std::string& name = "") {
            Waypoint wp;
            if (name.empty()) {
                char buf[32];
                snprintf(buf, sizeof(buf), "WP_%d", static_cast<int>(waypoints.size()) + 1);
                wp.name = buf;
            } else {
                wp.name = name;
            }
            wp.pos = currentPos;
            wp.timestamp = GetTickCount();

            waypoints.push_back(wp);
            if (waypoints.size() > 64) waypoints.erase(waypoints.begin());
        }

        static bool TeleportToWaypoint(int index, root* r, bool smooth = false) {
            if (index < 0 || index >= static_cast<int>(waypoints.size())) return false;

            if (smooth) {
                teleportTarget = waypoints[index].pos;
                teleporting = true;
                return true;
            }

            void* sprite = GetPlayerSprite(r);
            if (!sprite) return false;
            return WritePosition(sprite, waypoints[index].pos);
        }

        static bool TeleportToPosition(float x, float y, root* r, bool smooth = false) {
            if (smooth) {
                teleportTarget = { x, y };
                teleporting = true;
                return true;
            }

            void* sprite = GetPlayerSprite(r);
            if (!sprite) return false;
            return WritePosition(sprite, { x, y });
        }

        static void DeleteWaypoint(int index) {
            if (index >= 0 && index < static_cast<int>(waypoints.size())) {
                waypoints.erase(waypoints.begin() + index);
                if (selectedWaypoint >= static_cast<int>(waypoints.size())) {
                    selectedWaypoint = std::max(0, static_cast<int>(waypoints.size()) - 1);
                }
            }
        }

        static void ClearWaypoints() { waypoints.clear(); selectedWaypoint = 0; }

        // --- Noclip ---
        static void ToggleNoclip() { noclipEnabled = !noclipEnabled; }
        static bool IsNoclip() { return noclipEnabled; }
        static void SetNoclipSpeed(float s) { noclipSpeed = std::clamp(s, 50.0f, 5000.0f); }
        static float GetNoclipSpeed() { return noclipSpeed; }

        // --- Getters ---
        static Position GetCurrentPos() { return currentPos; }
        static float GetTotalDistance() { return totalDistance; }
        static void ResetDistance() { totalDistance = 0.0f; }
        static bool IsTeleporting() { return teleporting; }
        static int GetWaypointCount() { return static_cast<int>(waypoints.size()); }
        static int GetSelectedWaypoint() { return selectedWaypoint; }
        static void SetSelectedWaypoint(int i) {
            selectedWaypoint = std::clamp(i, 0, std::max(0, static_cast<int>(waypoints.size()) - 1));
        }

        static const std::vector<Waypoint>& GetWaypoints() { return waypoints; }

        static float DistanceTo(const Position& target) {
            float dx = target.x - currentPos.x;
            float dy = target.y - currentPos.y;
            return sqrtf(dx * dx + dy * dy);
        }

        static bool HandleKey(unsigned short key, bool down, root* r) {
            if (!down || !active) return true;

            switch (key) {
                case VK_INSERT:
                    SaveWaypoint();
                    return false;

                case VK_HOME:
                    if (!waypoints.empty()) {
                        TeleportToWaypoint(selectedWaypoint, r, smoothTeleport);
                    }
                    return false;

                case VK_PRIOR: // Page Up — select prev waypoint
                    if (selectedWaypoint > 0) selectedWaypoint--;
                    return false;

                case VK_NEXT: // Page Down — select next waypoint
                    if (selectedWaypoint < static_cast<int>(waypoints.size()) - 1)
                        selectedWaypoint++;
                    return false;

                case VK_DELETE:
                    if (!waypoints.empty()) DeleteWaypoint(selectedWaypoint);
                    return false;

                case 'N':
                    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
                        ToggleNoclip();
                        return false;
                    }
                    break;

                case 'T':
                    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
                        smoothTeleport = !smoothTeleport;
                        return false;
                    }
                    break;
            }

            return true;
        }

        // --- Overlay ---

        static int RenderPanel(IDirect3DDevice9* dev, int x, int y) {
            int wpCount = static_cast<int>(waypoints.size());
            int showWp = std::min(wpCount, 6);
            int h = 82 + showWp * 13;

            Overlay::DrawFilledRect(dev, x, y, 360, h, Overlay::BgPanel);
            Overlay::DrawRect(x, y, 360, h, D3DCOLOR_ARGB(255, 255, 215, 0), false);

            int ty = y + 4;
            Overlay::DrawText(x + 5, ty, D3DCOLOR_ARGB(255, 255, 215, 0), "TELEPORTER"); ty += 16;

            Overlay::DrawText(x + 5, ty, Overlay::White,
                "Pos: (%.1f, %.1f)", currentPos.x, currentPos.y); ty += 14;

            D3DCOLOR noclipColor = noclipEnabled ?
                D3DCOLOR_ARGB(255, 60, 255, 60) : D3DCOLOR_ARGB(255, 160, 160, 160);
            Overlay::DrawText(x + 5, ty, noclipColor,
                "Noclip: %s (Ctrl+N)  |  Smooth: %s (Ctrl+T)",
                noclipEnabled ? "ON" : "OFF",
                smoothTeleport ? "ON" : "OFF"); ty += 14;

            Overlay::DrawText(x + 5, ty, Overlay::Gray,
                "Distance: %.0f  |  Speed: %.0f",
                totalDistance, noclipSpeed); ty += 14;

            if (teleporting) {
                float dist = DistanceTo(teleportTarget);
                Overlay::DrawText(x + 5, ty, Overlay::Yellow,
                    "Teleporting... %.0f units remaining", dist); ty += 14;
            }

            // Waypoint list
            Overlay::DrawText(x + 5, ty, Overlay::Yellow,
                "--- Waypoints (%d) INS:Save HOME:Go PgUp/Dn:Select ---", wpCount); ty += 14;

            int startIdx = std::max(0, selectedWaypoint - 3);
            for (int i = startIdx; i < startIdx + showWp && i < wpCount; i++) {
                D3DCOLOR c = (i == selectedWaypoint) ?
                    D3DCOLOR_ARGB(255, 255, 255, 60) : Overlay::Gray;
                float dist = DistanceTo(waypoints[i].pos);
                Overlay::DrawText(x + 5, ty, c,
                    "%s%s (%.0f, %.0f) [%.0f away]",
                    (i == selectedWaypoint) ? "> " : "  ",
                    waypoints[i].name.c_str(),
                    waypoints[i].pos.x, waypoints[i].pos.y, dist);
                ty += 13;
            }

            return y + h + 4;
        }
    };

} // namespace W101Hook

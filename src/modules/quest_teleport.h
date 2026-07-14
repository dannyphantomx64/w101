#pragma once
#include <Windows.h>
#include <d3d9.h>
#include <string>
#include <vector>
#include <cmath>
#include "../framework.h"
#include "../overlay.h"

namespace W101Hook {

    class QuestTeleport {
    public:
        struct QuestMarker {
            float x, y, z;
            bool  valid;
        };

    private:
        // The quest arrow XYZ is stored in client memory
        // We find it by scanning for the quest navigation structure
        // The game renders a yellow arrow pointing to quest objectives
        static inline QuestMarker currentQuest = { 0, 0, 0, false };
        static inline float       playerX = 0, playerY = 0;
        static inline bool        active = false;
        static inline bool        autoTeleport = false;
        static inline DWORD       lastTeleportTime = 0;
        static inline DWORD       teleportCooldown = 2000; // 2 sec between auto-TPs
        static inline uint32_t    totalTeleports = 0;

        // Quest arrow memory scanning
        static inline uintptr_t   questArrowBase = 0;
        static inline bool        questScanned = false;

        // Position save for return-after-teleport
        static inline float       savedX = 0, savedY = 0;
        static inline bool        hasSavedPos = false;

        struct SWFMatrix {
            float a, b, c, d, tx, ty;
        };

        using GetWorldMatrixFn = void*(*)(void* sprite);
        using SetMatrixFn      = void(*)(void* sprite, const void* matrix);

        static inline GetWorldMatrixFn fnGetWorldMatrix = nullptr;
        static inline SetMatrixFn      fnSetMatrix = nullptr;

        static bool ReadPlayerPos(root* r) {
            if (!fnGetWorldMatrix || !r) return false;
            void* matrix = fnGetWorldMatrix(static_cast<void*>(r));
            if (!matrix) return false;
            auto* m = reinterpret_cast<SWFMatrix*>(matrix);
            playerX = m->tx;
            playerY = m->ty;
            return true;
        }

        static bool TeleportTo(root* r, float x, float y) {
            if (!fnGetWorldMatrix || !fnSetMatrix || !r) return false;
            void* sprite = static_cast<void*>(r);
            void* matrix = fnGetWorldMatrix(sprite);
            if (!matrix) return false;

            SWFMatrix newMatrix = *reinterpret_cast<SWFMatrix*>(matrix);
            newMatrix.tx = x;
            newMatrix.ty = y;
            fnSetMatrix(sprite, &newMatrix);
            return true;
        }

        // Scan memory for quest navigation structure
        // The quest arrow coords are typically near the navigation subsystem
        // Pattern: look for float triplets that change when quest objectives change
        static bool ScanForQuestArrow() {
            if (questScanned && questArrowBase) return true;

            HMODULE hMod = GetModuleHandleA("WizardGraphicalClient.exe");
            if (!hMod) return false;

            uintptr_t base = reinterpret_cast<uintptr_t>(hMod);
            PIMAGE_DOS_HEADER dos = reinterpret_cast<PIMAGE_DOS_HEADER>(hMod);
            PIMAGE_NT_HEADERS nt = reinterpret_cast<PIMAGE_NT_HEADERS>(base + dos->e_lfanew);
            uintptr_t end = base + nt->OptionalHeader.SizeOfImage;

            // Scan .data and .rdata sections for potential quest coordinate pointers
            // The quest system typically stores navigation coords as 3 consecutive floats
            // We look for a known pattern near the navigation manager

            // For now, mark as needing runtime discovery via FSCommand interception
            // The quest system communicates via FSCommands like "wireCommand" with quest data
            questScanned = true;
            return false;
        }

    public:
        static bool Init() {
            fnGetWorldMatrix = reinterpret_cast<GetWorldMatrixFn>(
                Offsets::Resolve(W101::SpriteInstance::GetWorldMatrix));
            fnSetMatrix = reinterpret_cast<SetMatrixFn>(
                Offsets::Resolve(W101::SpriteInstance::SetMatrix));

            active = (fnGetWorldMatrix != nullptr && fnSetMatrix != nullptr);
            return active;
        }

        static void Shutdown() { active = false; autoTeleport = false; }
        static bool IsActive() { return active; }

        // Called from FSCommand processor to capture quest coordinates
        // W101 sends quest objective locations via wire protocol
        static void ProcessFSCommand(const std::string& cmd, const std::string& args) {
            if (!active) return;

            // Quest-related FSCommands contain navigation data
            // wireCommand with quest payload includes target coordinates
            if (cmd.find("quest") != std::string::npos ||
                cmd.find("Quest") != std::string::npos ||
                cmd.find("navigate") != std::string::npos ||
                cmd.find("Navigate") != std::string::npos ||
                cmd.find("arrow") != std::string::npos ||
                cmd.find("objective") != std::string::npos) {

                // Try to parse coordinates from args
                // Format varies but often contains comma-separated values
                float x = 0, y = 0, z = 0;
                if (sscanf(args.c_str(), "%f,%f,%f", &x, &y, &z) >= 2 ||
                    sscanf(args.c_str(), "%f %f %f", &x, &y, &z) >= 2) {
                    currentQuest.x = x;
                    currentQuest.y = y;
                    currentQuest.z = z;
                    currentQuest.valid = true;
                }
            }
        }

        // Manual quest coordinate input (from memory scanner results)
        static void SetQuestTarget(float x, float y, float z = 0) {
            currentQuest.x = x;
            currentQuest.y = y;
            currentQuest.z = z;
            currentQuest.valid = true;
        }

        static void Update(root* r, float dt) {
            if (!active || !r) return;
            ReadPlayerPos(r);
            ScanForQuestArrow();

            // Auto teleport to quest objective
            if (autoTeleport && currentQuest.valid) {
                DWORD now = GetTickCount();
                if (now - lastTeleportTime >= teleportCooldown) {
                    float dx = currentQuest.x - playerX;
                    float dy = currentQuest.y - playerY;
                    float dist = sqrtf(dx * dx + dy * dy);

                    if (dist > 50.0f) { // only teleport if far enough
                        TeleportTo(r, currentQuest.x, currentQuest.y);
                        lastTeleportTime = now;
                        totalTeleports++;
                    }
                }
            }
        }

        // --- Controls ---

        static void TeleportToQuest(root* r) {
            if (!currentQuest.valid || !r) return;

            // Save current position for return
            savedX = playerX;
            savedY = playerY;
            hasSavedPos = true;

            TeleportTo(r, currentQuest.x, currentQuest.y);
            totalTeleports++;
            lastTeleportTime = GetTickCount();
        }

        static void ReturnToSaved(root* r) {
            if (!hasSavedPos || !r) return;
            TeleportTo(r, savedX, savedY);
            hasSavedPos = false;
        }

        static void ToggleAutoTeleport() { autoTeleport = !autoTeleport; }
        static bool IsAutoTeleport() { return autoTeleport; }

        static void SetCooldown(DWORD ms) {
            teleportCooldown = std::clamp(ms, (DWORD)500, (DWORD)30000);
        }

        static float GetDistanceToQuest() {
            if (!currentQuest.valid) return -1.0f;
            float dx = currentQuest.x - playerX;
            float dy = currentQuest.y - playerY;
            return sqrtf(dx * dx + dy * dy);
        }

        static const QuestMarker& GetQuestMarker() { return currentQuest; }
        static uint32_t GetTotalTeleports() { return totalTeleports; }
        static bool HasSavedPosition() { return hasSavedPos; }

        static bool HandleKey(unsigned short key, bool down, root* r) {
            if (!down || !active) return true;

            // Ctrl+Q = teleport to quest
            if (key == 'Q' && (GetAsyncKeyState(VK_CONTROL) & 0x8000)) {
                if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
                    ToggleAutoTeleport();
                } else {
                    TeleportToQuest(r);
                }
                return false;
            }

            // Ctrl+B = return to saved position
            if (key == 'B' && (GetAsyncKeyState(VK_CONTROL) & 0x8000)) {
                ReturnToSaved(r);
                return false;
            }

            return true;
        }

        // --- Overlay ---
        static int RenderPanel(IDirect3DDevice9* dev, int x, int y) {
            int h = 68;
            Overlay::DrawFilledRect(dev, x, y, 360, h, Overlay::BgPanel);
            Overlay::DrawRect(x, y, 360, h, D3DCOLOR_ARGB(255, 255, 255, 100), false);

            int ty = y + 4;
            D3DCOLOR autoColor = autoTeleport ?
                D3DCOLOR_ARGB(255, 60, 255, 60) : D3DCOLOR_ARGB(255, 255, 255, 100);
            Overlay::DrawText(x + 5, ty, autoColor,
                "QUEST TELEPORT %s", autoTeleport ? "[AUTO]" : ""); ty += 16;

            if (currentQuest.valid) {
                float dist = GetDistanceToQuest();
                Overlay::DrawText(x + 5, ty, Overlay::White,
                    "Quest: (%.0f, %.0f)  Dist: %.0f",
                    currentQuest.x, currentQuest.y, dist); ty += 14;
            } else {
                Overlay::DrawText(x + 5, ty, Overlay::Gray,
                    "No quest target detected"); ty += 14;
            }

            Overlay::DrawText(x + 5, ty, Overlay::White,
                "Player: (%.0f, %.0f)  TPs: %u",
                playerX, playerY, totalTeleports); ty += 14;

            Overlay::DrawText(x + 5, ty, Overlay::Gray,
                "Ctrl+Q tp  Ctrl+Sh+Q auto  Ctrl+B return");

            return y + h + 4;
        }
    };

} // namespace W101Hook

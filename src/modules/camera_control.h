#pragma once
#include <Windows.h>
#include <d3d9.h>
#include <cmath>
#include <algorithm>
#include "../framework.h"
#include "../overlay.h"

namespace W101Hook {

    class CameraControl {
    public:
        struct ViewportState {
            int x, y, w, h;
            float zoom;
            float panX, panY;
            bool freeCam;
        };

    private:
        static inline ViewportState current = { 0, 0, 800, 600, 1.0f, 0.0f, 0.0f, false };
        static inline ViewportState original = {};
        static inline bool          active = false;
        static inline bool          captured = false;

        static inline float zoomMin = 0.1f;
        static inline float zoomMax = 10.0f;
        static inline float zoomStep = 0.1f;
        static inline float panSpeed = 200.0f;
        static inline float smoothFactor = 0.15f;

        // Smooth interpolation targets
        static inline float targetZoom = 1.0f;
        static inline float targetPanX = 0.0f;
        static inline float targetPanY = 0.0f;
        static inline float currentZoom = 1.0f;
        static inline float currentPanX = 0.0f;
        static inline float currentPanY = 0.0f;

        // Viewport presets
        struct ViewPreset {
            const char* name;
            float zoom;
            float panX, panY;
        };

        static constexpr ViewPreset presets[] = {
            { "Default",   1.0f,  0.0f,   0.0f },
            { "Zoomed",    2.0f,  0.0f,   0.0f },
            { "Wide",      0.5f,  0.0f,   0.0f },
            { "Ultra Wide",0.25f, 0.0f,   0.0f },
            { "Top Down",  1.5f,  0.0f, -200.0f },
            { "Cinematic", 0.8f,  0.0f,  50.0f },
        };
        static constexpr int presetCount = sizeof(presets) / sizeof(ViewPreset);
        static inline int currentPreset = 0;

        static float Lerp(float a, float b, float t) {
            return a + (b - a) * t;
        }

    public:
        static bool Init() {
            active = true;
            return true;
        }

        static void Shutdown() {
            if (captured) RestoreViewport();
            active = false;
        }

        static bool IsActive() { return active; }
        static bool IsFreeCam() { return current.freeCam; }

        static void CaptureOriginal(root* r) {
            if (captured || !r) return;
            // Store whatever viewport the game is currently using
            // We'll override it via set_display_viewport
            original = current;
            captured = true;
        }

        static void RestoreViewport() {
            if (!captured) return;
            targetZoom = 1.0f;
            targetPanX = 0.0f;
            targetPanY = 0.0f;
            current.freeCam = false;
            captured = false;
        }

        // Called every frame from advance hook
        static void Update(root* r, float dt) {
            if (!active || !r) return;

            if (!captured) CaptureOriginal(r);

            // Smooth interpolation
            float t = std::min(smoothFactor * dt * 60.0f, 1.0f);
            currentZoom = Lerp(currentZoom, targetZoom, t);
            currentPanX = Lerp(currentPanX, targetPanX, t);
            currentPanY = Lerp(currentPanY, targetPanY, t);

            // Free camera WASD (using arrow keys to avoid conflict with noclip)
            if (current.freeCam) {
                float speed = panSpeed * dt / currentZoom;

                if (GetAsyncKeyState(VK_LEFT) & 0x8000)  targetPanX -= speed;
                if (GetAsyncKeyState(VK_RIGHT) & 0x8000) targetPanX += speed;
                if (GetAsyncKeyState(VK_UP) & 0x8000)    targetPanY -= speed;
                if (GetAsyncKeyState(VK_DOWN) & 0x8000)  targetPanY += speed;

                if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
                    // already moved, apply boost
                    targetPanX += (targetPanX - currentPanX) * 2.0f;
                    targetPanY += (targetPanY - currentPanY) * 2.0f;
                }
            }

            // Apply viewport via framework
            if (std::abs(currentZoom - 1.0f) > 0.001f ||
                std::abs(currentPanX) > 0.1f ||
                std::abs(currentPanY) > 0.1f) {

                // Calculate zoomed viewport
                D3DVIEWPORT9 vp;
                IDirect3DDevice9* dev = D3D9Hook::pDevice;
                if (dev) {
                    dev->GetViewport(&vp);
                    current.w = vp.Width;
                    current.h = vp.Height;
                }

                int zoomedW = static_cast<int>(current.w / currentZoom);
                int zoomedH = static_cast<int>(current.h / currentZoom);
                int offsetX = (current.w - zoomedW) / 2 + static_cast<int>(currentPanX);
                int offsetY = (current.h - zoomedH) / 2 + static_cast<int>(currentPanY);

                Framework::SetViewport(r, offsetX, offsetY, zoomedW, zoomedH);
            }

            current.zoom = currentZoom;
            current.panX = currentPanX;
            current.panY = currentPanY;
        }

        // --- Controls ---

        static void ZoomIn() {
            targetZoom = std::min(targetZoom + zoomStep * targetZoom, zoomMax);
        }

        static void ZoomOut() {
            targetZoom = std::max(targetZoom - zoomStep * targetZoom, zoomMin);
        }

        static void SetZoom(float z) {
            targetZoom = std::clamp(z, zoomMin, zoomMax);
        }

        static void ResetView() {
            targetZoom = 1.0f;
            targetPanX = 0.0f;
            targetPanY = 0.0f;
            currentPreset = 0;
        }

        static void ToggleFreeCam() {
            current.freeCam = !current.freeCam;
            if (!current.freeCam) {
                targetPanX = 0.0f;
                targetPanY = 0.0f;
            }
        }

        static void ApplyPreset(int index) {
            if (index < 0 || index >= presetCount) return;
            currentPreset = index;
            targetZoom = presets[index].zoom;
            targetPanX = presets[index].panX;
            targetPanY = presets[index].panY;
        }

        static void NextPreset() {
            currentPreset = (currentPreset + 1) % presetCount;
            ApplyPreset(currentPreset);
        }

        static bool HandleKey(unsigned short key, bool down) {
            if (!down || !active) return true;

            // Ctrl+Z = zoom in, Ctrl+X = zoom out
            if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
                if (key == 'Z') { ZoomIn(); return false; }
                if (key == 'X') { ZoomOut(); return false; }
                if (key == 'C') { ResetView(); return false; }
                if (key == 'F') { ToggleFreeCam(); return false; }
                if (key == 'V') { NextPreset(); return false; }
            }

            // Mouse wheel zoom (handled separately in mouse callback)
            return true;
        }

        static void ProcessWheel(int delta) {
            if (!active) return;
            if (delta > 0) ZoomIn();
            else if (delta < 0) ZoomOut();
        }

        // --- Getters ---
        static float GetZoom() { return currentZoom; }
        static float GetPanX() { return currentPanX; }
        static float GetPanY() { return currentPanY; }
        static const char* GetPresetName() { return presets[currentPreset].name; }
        static int GetPresetIndex() { return currentPreset; }

        // --- Overlay ---
        static int RenderPanel(IDirect3DDevice9* dev, int x, int y) {
            int h = 68;
            Overlay::DrawFilledRect(dev, x, y, 300, h, Overlay::BgPanel);
            Overlay::DrawRect(x, y, 300, h, D3DCOLOR_ARGB(255, 100, 200, 255), false);

            int ty = y + 4;
            Overlay::DrawText(x + 5, ty, D3DCOLOR_ARGB(255, 100, 200, 255),
                "CAMERA CONTROL"); ty += 16;

            D3DCOLOR freeCamColor = current.freeCam ?
                D3DCOLOR_ARGB(255, 60, 255, 60) : Overlay::Gray;
            Overlay::DrawText(x + 5, ty, Overlay::White,
                "Zoom: %.2fx  |  Preset: %s", currentZoom, presets[currentPreset].name); ty += 14;

            Overlay::DrawText(x + 5, ty, freeCamColor,
                "FreeCam: %s  |  Pan: (%.0f, %.0f)",
                current.freeCam ? "ON" : "OFF", currentPanX, currentPanY); ty += 14;

            Overlay::DrawText(x + 5, ty, Overlay::Gray,
                "Ctrl+Z/X zoom  Ctrl+C reset  Ctrl+F freecam  Ctrl+V preset");

            return y + h + 4;
        }
    };

} // namespace W101Hook

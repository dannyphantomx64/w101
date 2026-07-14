#pragma once
#include <d3d9.h>
#include <string>
#include <vector>
#include <functional>

#pragma comment(lib, "d3d9.lib")

namespace W101Hook {

    class Overlay {
    public:
        struct TextEntry {
            std::string text;
            int x, y;
            D3DCOLOR color;
        };

        struct Line {
            int x1, y1, x2, y2;
            D3DCOLOR color;
            float width;
        };

        struct Rect {
            int x, y, w, h;
            D3DCOLOR color;
            bool filled;
        };

    private:
        static inline ID3DXFont*   pFont      = nullptr;
        static inline ID3DXLine*   pLine      = nullptr;
        static inline bool         visible    = true;

        static inline std::vector<TextEntry> textQueue;
        static inline std::vector<Line>      lineQueue;
        static inline std::vector<Rect>      rectQueue;

    public:
        static bool Init(IDirect3DDevice9* dev) {
            if (pFont) return true;

            D3DXCreateFontA(dev, 14, 0, FW_NORMAL, 1, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE, "Consolas", &pFont);

            D3DXCreateLine(dev, &pLine);
            if (pLine) pLine->SetAntialias(TRUE);

            return pFont != nullptr;
        }

        static void OnReset() {
            if (pFont) { pFont->OnLostDevice(); }
            if (pLine) { pLine->OnLostDevice(); }
        }

        static void OnResetPost() {
            if (pFont) { pFont->OnResetDevice(); }
            if (pLine) { pLine->OnResetDevice(); }
        }

        static void Release() {
            if (pFont) { pFont->Release(); pFont = nullptr; }
            if (pLine) { pLine->Release(); pLine = nullptr; }
        }

        static void Toggle() { visible = !visible; }
        static bool IsVisible() { return visible; }

        // --- Drawing API ---

        static void DrawText(int x, int y, D3DCOLOR color, const char* fmt, ...) {
            if (!visible || !pFont) return;
            char buf[512];
            va_list args;
            va_start(args, fmt);
            vsnprintf(buf, sizeof(buf), fmt, args);
            va_end(args);
            textQueue.push_back({ buf, x, y, color });
        }

        static void DrawLine(int x1, int y1, int x2, int y2, D3DCOLOR color, float width = 1.0f) {
            if (!visible) return;
            lineQueue.push_back({ x1, y1, x2, y2, color, width });
        }

        static void DrawRect(int x, int y, int w, int h, D3DCOLOR color, bool filled = false) {
            if (!visible) return;
            rectQueue.push_back({ x, y, w, h, color, filled });
        }

        static void DrawFilledRect(IDirect3DDevice9* dev, int x, int y, int w, int h, D3DCOLOR color) {
            if (!dev) return;
            D3DRECT rect = { x, y, x + w, y + h };
            dev->Clear(1, &rect, D3DCLEAR_TARGET, color, 0, 0);
        }

        static void Render(IDirect3DDevice9* dev) {
            if (!visible) return;

            for (auto& r : rectQueue) {
                if (r.filled) {
                    DrawFilledRect(dev, r.x, r.y, r.w, r.h, r.color);
                } else {
                    DrawFilledRect(dev, r.x, r.y, r.w, 1, r.color);
                    DrawFilledRect(dev, r.x, r.y + r.h, r.w, 1, r.color);
                    DrawFilledRect(dev, r.x, r.y, 1, r.h, r.color);
                    DrawFilledRect(dev, r.x + r.w, r.y, 1, r.h + 1, r.color);
                }
            }

            if (pLine) {
                for (auto& l : lineQueue) {
                    D3DXVECTOR2 pts[2] = {
                        { static_cast<float>(l.x1), static_cast<float>(l.y1) },
                        { static_cast<float>(l.x2), static_cast<float>(l.y2) }
                    };
                    pLine->SetWidth(l.width);
                    pLine->Begin();
                    pLine->Draw(pts, 2, l.color);
                    pLine->End();
                }
            }

            if (pFont) {
                for (auto& t : textQueue) {
                    RECT rc = { t.x + 1, t.y + 1, 0, 0 };
                    pFont->DrawTextA(nullptr, t.text.c_str(), -1, &rc,
                        DT_NOCLIP, D3DCOLOR_ARGB(180, 0, 0, 0));

                    rc = { t.x, t.y, 0, 0 };
                    pFont->DrawTextA(nullptr, t.text.c_str(), -1, &rc,
                        DT_NOCLIP, t.color);
                }
            }

            textQueue.clear();
            lineQueue.clear();
            rectQueue.clear();
        }

        // --- Utility Colors ---
        static constexpr D3DCOLOR White   = D3DCOLOR_ARGB(255, 255, 255, 255);
        static constexpr D3DCOLOR Red     = D3DCOLOR_ARGB(255, 255, 60,  60);
        static constexpr D3DCOLOR Green   = D3DCOLOR_ARGB(255, 60,  255, 60);
        static constexpr D3DCOLOR Blue    = D3DCOLOR_ARGB(255, 60,  120, 255);
        static constexpr D3DCOLOR Yellow  = D3DCOLOR_ARGB(255, 255, 255, 60);
        static constexpr D3DCOLOR Cyan    = D3DCOLOR_ARGB(255, 60,  255, 255);
        static constexpr D3DCOLOR Magenta = D3DCOLOR_ARGB(255, 255, 60,  255);
        static constexpr D3DCOLOR Orange  = D3DCOLOR_ARGB(255, 255, 165, 0);
        static constexpr D3DCOLOR Gray    = D3DCOLOR_ARGB(255, 160, 160, 160);
        static constexpr D3DCOLOR BgDark  = D3DCOLOR_ARGB(200, 20,  20,  25);
        static constexpr D3DCOLOR BgPanel = D3DCOLOR_ARGB(220, 30,  30,  40);
    };

} // namespace W101Hook

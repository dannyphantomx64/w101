#pragma once
#include <d3d9.h>
#include <string>
#include <vector>
#include <functional>
#include <cstdarg>
#include <cstdio>

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
        static inline bool         visible    = true;
        static inline bool         initialized = false;

        static inline std::vector<TextEntry> textQueue;
        static inline std::vector<Line>      lineQueue;
        static inline std::vector<Rect>      rectQueue;

        // GDI font for text rendering (no D3DX dependency)
        static inline HFONT        hFont      = nullptr;
        static inline HDC          hdc        = nullptr;
        static inline HBITMAP      hBitmap    = nullptr;
        static inline HBITMAP      hOldBitmap = nullptr;
        static inline uint32_t*    bitmapBits = nullptr;
        static inline int          bmpWidth   = 0;
        static inline int          bmpHeight  = 0;

        static inline IDirect3DTexture9* pFontTexture = nullptr;

        // Vertex for colored rects
        struct Vertex2D {
            float x, y, z, rhw;
            D3DCOLOR color;
        };
        static constexpr DWORD FVF_2D = D3DFVF_XYZRHW | D3DFVF_DIFFUSE;

        static void CreateFontResources(IDirect3DDevice9* dev) {
            if (hFont) return;

            hFont = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Consolas");

            bmpWidth = 1024;
            bmpHeight = 512;

            hdc = CreateCompatibleDC(nullptr);

            BITMAPINFO bmi = {};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = bmpWidth;
            bmi.bmiHeader.biHeight = -bmpHeight; // top-down
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;

            hBitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS,
                reinterpret_cast<void**>(&bitmapBits), nullptr, 0);
            hOldBitmap = static_cast<HBITMAP>(SelectObject(hdc, hBitmap));
            SelectObject(hdc, hFont);
            SetBkMode(hdc, TRANSPARENT);
        }

        static void DestroyFontResources() {
            if (pFontTexture) { pFontTexture->Release(); pFontTexture = nullptr; }
            if (hdc && hOldBitmap) SelectObject(hdc, hOldBitmap);
            if (hBitmap) DeleteObject(hBitmap);
            if (hFont) DeleteObject(hFont);
            if (hdc) DeleteDC(hdc);
            hFont = nullptr; hdc = nullptr; hBitmap = nullptr;
            hOldBitmap = nullptr; bitmapBits = nullptr;
        }

        static void RenderTextGDI(IDirect3DDevice9* dev, int x, int y,
            D3DCOLOR color, const char* text) {
            if (!hdc || !bitmapBits || !text || !text[0]) return;

            // Measure text
            SIZE textSize;
            int len = static_cast<int>(strlen(text));
            GetTextExtentPoint32A(hdc, text, len, &textSize);

            int tw = (std::min)(static_cast<int>(textSize.cx) + 4, bmpWidth);
            int th = (std::min)(static_cast<int>(textSize.cy) + 2, bmpHeight);

            // Clear the region
            memset(bitmapBits, 0, bmpWidth * th * 4);

            // Draw shadow
            RECT rc = { 1, 1, tw, th };
            SetTextColor(hdc, RGB(0, 0, 0));
            ::DrawTextA(hdc, text, len, &rc, DT_LEFT | DT_TOP | DT_NOCLIP);

            // Draw text
            rc = { 0, 0, tw, th };
            uint8_t r = (color >> 16) & 0xFF;
            uint8_t g = (color >> 8) & 0xFF;
            uint8_t b = color & 0xFF;
            SetTextColor(hdc, RGB(r, g, b));
            ::DrawTextA(hdc, text, len, &rc, DT_LEFT | DT_TOP | DT_NOCLIP);

            // Create/update texture
            if (!pFontTexture || true) {
                if (pFontTexture) pFontTexture->Release();
                dev->CreateTexture(tw, th, 1, 0, D3DFMT_A8R8G8B8,
                    D3DPOOL_MANAGED, &pFontTexture, nullptr);
            }

            if (pFontTexture) {
                D3DLOCKED_RECT locked;
                if (SUCCEEDED(pFontTexture->LockRect(0, &locked, nullptr, 0))) {
                    uint8_t textAlpha = (color >> 24) & 0xFF;
                    for (int row = 0; row < th; row++) {
                        uint32_t* src = bitmapBits + row * bmpWidth;
                        uint32_t* dst = reinterpret_cast<uint32_t*>(
                            static_cast<uint8_t*>(locked.pBits) + row * locked.Pitch);
                        for (int col = 0; col < tw; col++) {
                            uint32_t pixel = src[col];
                            if (pixel & 0x00FFFFFF) {
                                uint8_t pr = (pixel >> 16) & 0xFF;
                                uint8_t pg = (pixel >> 8) & 0xFF;
                                uint8_t pb = pixel & 0xFF;
                                uint8_t brightness = (std::max)({ pr, pg, pb });
                                uint8_t alpha = static_cast<uint8_t>(
                                    (brightness * textAlpha) / 255);
                                dst[col] = (alpha << 24) | (pixel & 0x00FFFFFF);
                            } else {
                                dst[col] = 0;
                            }
                        }
                    }
                    pFontTexture->UnlockRect(0);

                    // Draw textured quad
                    dev->SetTexture(0, pFontTexture);
                    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
                    dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
                    dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
                    dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
                    dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                    dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

                    struct TexVertex {
                        float x, y, z, rhw;
                        D3DCOLOR color;
                        float u, v;
                    };

                    TexVertex quad[4] = {
                        { static_cast<float>(x),      static_cast<float>(y),      0, 1, 0xFFFFFFFF, 0, 0 },
                        { static_cast<float>(x + tw),  static_cast<float>(y),      0, 1, 0xFFFFFFFF, 1, 0 },
                        { static_cast<float>(x),      static_cast<float>(y + th),  0, 1, 0xFFFFFFFF, 0, 1 },
                        { static_cast<float>(x + tw),  static_cast<float>(y + th),  0, 1, 0xFFFFFFFF, 1, 1 },
                    };

                    dev->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
                    dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(TexVertex));
                    dev->SetTexture(0, nullptr);
                }
            }
        }

    public:
        static bool Init(IDirect3DDevice9* dev) {
            if (initialized) return true;
            CreateFontResources(dev);
            initialized = true;
            return true;
        }

        static void OnReset() {
            if (pFontTexture) { pFontTexture->Release(); pFontTexture = nullptr; }
        }

        static void OnResetPost() {
            // texture recreated on next draw
        }

        static void Release() {
            DestroyFontResources();
            initialized = false;
        }

        static void Toggle() { visible = !visible; }
        static bool IsVisible() { return visible; }

        // --- Drawing API ---

        static void DrawText(int x, int y, D3DCOLOR color, const char* fmt, ...) {
            if (!visible) return;
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
            if (!visible || !dev) return;

            // Save render state
            DWORD prevFVF, prevAlpha, prevSrc, prevDst, prevCull, prevZEnable, prevLighting;
            dev->GetFVF(&prevFVF);
            dev->GetRenderState(D3DRS_ALPHABLENDENABLE, &prevAlpha);
            dev->GetRenderState(D3DRS_SRCBLEND, &prevSrc);
            dev->GetRenderState(D3DRS_DESTBLEND, &prevDst);
            dev->GetRenderState(D3DRS_CULLMODE, &prevCull);
            dev->GetRenderState(D3DRS_ZENABLE, &prevZEnable);
            dev->GetRenderState(D3DRS_LIGHTING, &prevLighting);

            dev->SetRenderState(D3DRS_ZENABLE, FALSE);
            dev->SetRenderState(D3DRS_LIGHTING, FALSE);
            dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
            dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
            dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
            dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

            // Draw filled rects via Clear
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

            // Draw lines as thin rects (no D3DXLine dependency)
            dev->SetFVF(FVF_2D);
            for (auto& l : lineQueue) {
                Vertex2D verts[2] = {
                    { static_cast<float>(l.x1), static_cast<float>(l.y1), 0, 1, l.color },
                    { static_cast<float>(l.x2), static_cast<float>(l.y2), 0, 1, l.color },
                };
                dev->DrawPrimitiveUP(D3DPT_LINELIST, 1, verts, sizeof(Vertex2D));
            }

            // Draw text via GDI -> texture blit
            for (auto& t : textQueue) {
                RenderTextGDI(dev, t.x, t.y, t.color, t.text.c_str());
            }

            // Restore render state
            dev->SetFVF(prevFVF);
            dev->SetRenderState(D3DRS_ALPHABLENDENABLE, prevAlpha);
            dev->SetRenderState(D3DRS_SRCBLEND, prevSrc);
            dev->SetRenderState(D3DRS_DESTBLEND, prevDst);
            dev->SetRenderState(D3DRS_CULLMODE, prevCull);
            dev->SetRenderState(D3DRS_ZENABLE, prevZEnable);
            dev->SetRenderState(D3DRS_LIGHTING, prevLighting);

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

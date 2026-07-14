#pragma once
#include <Windows.h>
#include <vector>
#include <string>
#include <mutex>
#include <algorithm>
#include <cstdint>

namespace W101Hook {

    class MemoryScanner {
    public:
        struct ScanResult {
            uintptr_t   address;
            uint8_t     bytes[64];
            int         matchLen;
            std::string label;
        };

        struct Region {
            uintptr_t base;
            size_t    size;
            DWORD     protect;
        };

    private:
        static inline std::vector<ScanResult> results;
        static inline std::vector<Region>     regions;
        static inline std::mutex              scanMtx;
        static inline bool                    scanning = false;
        static inline bool                    active = false;
        static inline int                     lastScanTime = 0;
        static inline int                     lastRegionCount = 0;
        static inline size_t                  lastScanBytes = 0;
        static inline int                     selectedResult = 0;

        // Value scan state
        static inline std::vector<uintptr_t>  valueScanAddresses;
        static inline int                     valueScanSize = 4;
        static inline uint64_t                valueScanTarget = 0;
        static inline int                     valueScanCount = 0;

        static bool IsReadable(DWORD protect) {
            return (protect & PAGE_READONLY) ||
                   (protect & PAGE_READWRITE) ||
                   (protect & PAGE_EXECUTE_READ) ||
                   (protect & PAGE_EXECUTE_READWRITE) ||
                   (protect & PAGE_WRITECOPY) ||
                   (protect & PAGE_EXECUTE_WRITECOPY);
        }

        static bool IsWritable(DWORD protect) {
            return (protect & PAGE_READWRITE) ||
                   (protect & PAGE_EXECUTE_READWRITE) ||
                   (protect & PAGE_WRITECOPY) ||
                   (protect & PAGE_EXECUTE_WRITECOPY);
        }

        static void EnumerateRegions(uintptr_t start, uintptr_t end) {
            regions.clear();
            MEMORY_BASIC_INFORMATION mbi;
            uintptr_t addr = start;

            while (addr < end && VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi))) {
                if (mbi.State == MEM_COMMIT && IsReadable(mbi.Protect) &&
                    !(mbi.Protect & PAGE_GUARD)) {
                    Region r;
                    r.base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
                    r.size = mbi.RegionSize;
                    r.protect = mbi.Protect;
                    regions.push_back(r);
                }
                addr = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
            }
            lastRegionCount = static_cast<int>(regions.size());
        }

        static bool PatternMatch(const uint8_t* data, const uint8_t* pattern,
            const uint8_t* mask, int len) {
            for (int i = 0; i < len; i++) {
                if (mask[i] == 'x' && data[i] != pattern[i]) return false;
            }
            return true;
        }

    public:
        static bool Init() {
            active = true;
            return true;
        }

        static void Shutdown() { active = false; }
        static bool IsActive() { return active; }
        static bool IsScanning() { return scanning; }

        // --- Pattern Scan (AOB / Signature) ---
        // Pattern: "48 8B 05 ?? ?? ?? ?? 48 85 C0"
        // ?? = wildcard

        static bool ParsePattern(const std::string& patternStr,
            std::vector<uint8_t>& pattern, std::vector<uint8_t>& mask) {

            pattern.clear();
            mask.clear();

            std::string token;
            for (size_t i = 0; i <= patternStr.size(); i++) {
                if (i == patternStr.size() || patternStr[i] == ' ') {
                    if (!token.empty()) {
                        if (token == "??" || token == "?") {
                            pattern.push_back(0);
                            mask.push_back('?');
                        } else {
                            uint8_t byte = static_cast<uint8_t>(
                                strtoul(token.c_str(), nullptr, 16));
                            pattern.push_back(byte);
                            mask.push_back('x');
                        }
                        token.clear();
                    }
                } else {
                    token += patternStr[i];
                }
            }

            return !pattern.empty();
        }

        static int PatternScan(const std::string& patternStr,
            const std::string& label = "", uintptr_t rangeStart = 0,
            uintptr_t rangeEnd = 0, int maxResults = 100) {

            if (scanning) return -1;
            scanning = true;

            std::vector<uint8_t> pattern, mask;
            if (!ParsePattern(patternStr, pattern, mask)) {
                scanning = false;
                return 0;
            }

            DWORD startTick = GetTickCount();

            if (rangeStart == 0) {
                HMODULE hMod = GetModuleHandleA("WizardGraphicalClient.exe");
                if (hMod) {
                    rangeStart = reinterpret_cast<uintptr_t>(hMod);
                    PIMAGE_DOS_HEADER dos = reinterpret_cast<PIMAGE_DOS_HEADER>(hMod);
                    PIMAGE_NT_HEADERS nt = reinterpret_cast<PIMAGE_NT_HEADERS>(
                        rangeStart + dos->e_lfanew);
                    rangeEnd = rangeStart + nt->OptionalHeader.SizeOfImage;
                }
            }

            if (rangeEnd == 0) rangeEnd = rangeStart + 0x10000000;

            EnumerateRegions(rangeStart, rangeEnd);

            std::vector<ScanResult> found;
            size_t bytesScanned = 0;
            int patLen = static_cast<int>(pattern.size());

            for (auto& region : regions) {
                if (region.base < rangeStart) continue;
                if (region.base >= rangeEnd) break;

                uintptr_t scanStart = std::max(region.base, rangeStart);
                uintptr_t scanEnd = std::min(region.base + region.size, rangeEnd);

                if (scanEnd - scanStart < static_cast<size_t>(patLen)) continue;

                __try {
                    const uint8_t* mem = reinterpret_cast<const uint8_t*>(scanStart);
                    size_t scanLen = scanEnd - scanStart - patLen;
                    bytesScanned += scanLen;

                    for (size_t i = 0; i <= scanLen; i++) {
                        if (PatternMatch(mem + i, pattern.data(), mask.data(), patLen)) {
                            ScanResult res;
                            res.address = scanStart + i;
                            res.matchLen = patLen;
                            res.label = label.empty() ? patternStr : label;
                            int copyLen = std::min(64, patLen);
                            memcpy(res.bytes, mem + i, copyLen);
                            found.push_back(res);

                            if (static_cast<int>(found.size()) >= maxResults) goto done;
                        }
                    }
                }
                __except (EXCEPTION_EXECUTE_HANDLER) {
                    continue;
                }
            }

        done:
            lastScanTime = GetTickCount() - startTick;
            lastScanBytes = bytesScanned;

            {
                std::lock_guard<std::mutex> lock(scanMtx);
                results = std::move(found);
                selectedResult = 0;
            }

            scanning = false;
            return static_cast<int>(results.size());
        }

        // --- Value Scan ---

        static int ValueScanFirst(uint64_t value, int size = 4,
            uintptr_t rangeStart = 0, uintptr_t rangeEnd = 0) {

            if (scanning) return -1;
            scanning = true;

            DWORD startTick = GetTickCount();
            valueScanTarget = value;
            valueScanSize = size;

            if (rangeStart == 0) {
                HMODULE hMod = GetModuleHandleA("WizardGraphicalClient.exe");
                if (hMod) {
                    rangeStart = reinterpret_cast<uintptr_t>(hMod);
                    PIMAGE_DOS_HEADER dos = reinterpret_cast<PIMAGE_DOS_HEADER>(hMod);
                    PIMAGE_NT_HEADERS nt = reinterpret_cast<PIMAGE_NT_HEADERS>(
                        rangeStart + dos->e_lfanew);
                    rangeEnd = rangeStart + nt->OptionalHeader.SizeOfImage;
                }
            }

            if (rangeEnd == 0) rangeEnd = rangeStart + 0x10000000;
            EnumerateRegions(rangeStart, rangeEnd);

            valueScanAddresses.clear();
            size_t bytesScanned = 0;

            for (auto& region : regions) {
                if (region.base < rangeStart) continue;
                if (region.base >= rangeEnd) break;

                uintptr_t scanStart = std::max(region.base, rangeStart);
                uintptr_t scanEnd = std::min(region.base + region.size, rangeEnd);
                if (scanEnd - scanStart < static_cast<size_t>(size)) continue;

                __try {
                    const uint8_t* mem = reinterpret_cast<const uint8_t*>(scanStart);
                    size_t scanLen = scanEnd - scanStart - size;
                    bytesScanned += scanLen;

                    for (size_t i = 0; i <= scanLen; i += static_cast<size_t>(size)) {
                        bool match = false;
                        switch (size) {
                            case 1: match = (mem[i] == static_cast<uint8_t>(value)); break;
                            case 2: match = (*reinterpret_cast<const uint16_t*>(mem + i) == static_cast<uint16_t>(value)); break;
                            case 4: match = (*reinterpret_cast<const uint32_t*>(mem + i) == static_cast<uint32_t>(value)); break;
                            case 8: match = (*reinterpret_cast<const uint64_t*>(mem + i) == value); break;
                        }
                        if (match) {
                            valueScanAddresses.push_back(scanStart + i);
                            if (valueScanAddresses.size() > 500000) goto vdone;
                        }
                    }
                }
                __except (EXCEPTION_EXECUTE_HANDLER) {
                    continue;
                }
            }

        vdone:
            lastScanTime = GetTickCount() - startTick;
            lastScanBytes = bytesScanned;
            valueScanCount = static_cast<int>(valueScanAddresses.size());

            scanning = false;
            return valueScanCount;
        }

        static int ValueScanNext(uint64_t newValue) {
            if (scanning) return -1;
            scanning = true;

            DWORD startTick = GetTickCount();
            valueScanTarget = newValue;

            std::vector<uintptr_t> narrowed;

            for (auto addr : valueScanAddresses) {
                __try {
                    bool match = false;
                    switch (valueScanSize) {
                        case 1: match = (*reinterpret_cast<uint8_t*>(addr) == static_cast<uint8_t>(newValue)); break;
                        case 2: match = (*reinterpret_cast<uint16_t*>(addr) == static_cast<uint16_t>(newValue)); break;
                        case 4: match = (*reinterpret_cast<uint32_t*>(addr) == static_cast<uint32_t>(newValue)); break;
                        case 8: match = (*reinterpret_cast<uint64_t*>(addr) == newValue); break;
                    }
                    if (match) narrowed.push_back(addr);
                }
                __except (EXCEPTION_EXECUTE_HANDLER) {
                    continue;
                }
            }

            valueScanAddresses = std::move(narrowed);
            valueScanCount = static_cast<int>(valueScanAddresses.size());
            lastScanTime = GetTickCount() - startTick;

            // Convert to results for display
            {
                std::lock_guard<std::mutex> lock(scanMtx);
                results.clear();
                int showCount = std::min(valueScanCount, 50);
                for (int i = 0; i < showCount; i++) {
                    ScanResult res;
                    res.address = valueScanAddresses[i];
                    res.matchLen = valueScanSize;
                    res.label = "value_scan";
                    __try {
                        memcpy(res.bytes, reinterpret_cast<void*>(res.address),
                            std::min(64, valueScanSize + 16));
                    }
                    __except (EXCEPTION_EXECUTE_HANDLER) {
                        memset(res.bytes, 0, 64);
                    }
                    results.push_back(res);
                }
                selectedResult = 0;
            }

            scanning = false;
            return valueScanCount;
        }

        // --- Write Memory ---

        static bool WriteValue(uintptr_t addr, uint64_t value, int size) {
            DWORD oldProt;
            if (!VirtualProtect(reinterpret_cast<LPVOID>(addr), size,
                PAGE_EXECUTE_READWRITE, &oldProt)) return false;

            __try {
                switch (size) {
                    case 1: *reinterpret_cast<uint8_t*>(addr) = static_cast<uint8_t>(value); break;
                    case 2: *reinterpret_cast<uint16_t*>(addr) = static_cast<uint16_t>(value); break;
                    case 4: *reinterpret_cast<uint32_t*>(addr) = static_cast<uint32_t>(value); break;
                    case 8: *reinterpret_cast<uint64_t*>(addr) = value; break;
                    default: return false;
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                VirtualProtect(reinterpret_cast<LPVOID>(addr), size, oldProt, &oldProt);
                return false;
            }

            VirtualProtect(reinterpret_cast<LPVOID>(addr), size, oldProt, &oldProt);
            return true;
        }

        static bool WriteFloat(uintptr_t addr, float value) {
            DWORD oldProt;
            if (!VirtualProtect(reinterpret_cast<LPVOID>(addr), 4,
                PAGE_EXECUTE_READWRITE, &oldProt)) return false;

            __try {
                *reinterpret_cast<float*>(addr) = value;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                VirtualProtect(reinterpret_cast<LPVOID>(addr), 4, oldProt, &oldProt);
                return false;
            }

            VirtualProtect(reinterpret_cast<LPVOID>(addr), 4, oldProt, &oldProt);
            return true;
        }

        // --- NOP Patch ---

        static bool NopBytes(uintptr_t addr, int count) {
            DWORD oldProt;
            if (!VirtualProtect(reinterpret_cast<LPVOID>(addr), count,
                PAGE_EXECUTE_READWRITE, &oldProt)) return false;

            memset(reinterpret_cast<void*>(addr), 0x90, count);
            VirtualProtect(reinterpret_cast<LPVOID>(addr), count, oldProt, &oldProt);
            return true;
        }

        // --- Getters ---

        static std::vector<ScanResult> GetResults() {
            std::lock_guard<std::mutex> lock(scanMtx);
            return results;
        }

        static int GetResultCount() {
            std::lock_guard<std::mutex> lock(scanMtx);
            return static_cast<int>(results.size());
        }

        static int GetValueScanCount() { return valueScanCount; }
        static int GetLastScanTime() { return lastScanTime; }
        static size_t GetLastScanBytes() { return lastScanBytes; }
        static int GetRegionCount() { return lastRegionCount; }
        static int GetSelectedResult() { return selectedResult; }

        static void SetSelectedResult(int i) {
            std::lock_guard<std::mutex> lock(scanMtx);
            selectedResult = std::clamp(i, 0,
                std::max(0, static_cast<int>(results.size()) - 1));
        }

        static void ClearResults() {
            std::lock_guard<std::mutex> lock(scanMtx);
            results.clear();
            valueScanAddresses.clear();
            valueScanCount = 0;
            selectedResult = 0;
        }

        static std::string FormatBytes(size_t bytes) {
            char buf[64];
            if (bytes < 1024)
                snprintf(buf, sizeof(buf), "%zuB", bytes);
            else if (bytes < 1048576)
                snprintf(buf, sizeof(buf), "%.1fKB", bytes / 1024.0);
            else
                snprintf(buf, sizeof(buf), "%.1fMB", bytes / 1048576.0);
            return buf;
        }

        static std::string HexDumpResult(const ScanResult& res, int maxBytes = 16) {
            std::string out;
            char hex[4];
            int len = std::min(res.matchLen, maxBytes);
            for (int i = 0; i < len; i++) {
                snprintf(hex, sizeof(hex), "%02X ", res.bytes[i]);
                out += hex;
            }
            return out;
        }

        // --- Overlay ---

        static int RenderPanel(IDirect3DDevice9* dev, int x, int y) {
            std::lock_guard<std::mutex> lock(scanMtx);

            int resCount = static_cast<int>(results.size());
            int showRes = std::min(resCount, 8);
            int h = 52 + showRes * 13;

            Overlay::DrawFilledRect(dev, x, y, 520, h, Overlay::BgPanel);
            Overlay::DrawRect(x, y, 520, h, D3DCOLOR_ARGB(255, 255, 100, 100), false);

            int ty = y + 4;
            Overlay::DrawText(x + 5, ty, D3DCOLOR_ARGB(255, 255, 100, 100),
                "MEMORY SCANNER %s", scanning ? "[SCANNING...]" : ""); ty += 16;

            Overlay::DrawText(x + 5, ty, Overlay::White,
                "Results: %d  |  Scanned: %s in %dms  |  Regions: %d",
                valueScanCount > 0 ? valueScanCount : resCount,
                FormatBytes(lastScanBytes).c_str(),
                lastScanTime, lastRegionCount); ty += 14;

            Overlay::DrawText(x + 5, ty, Overlay::Gray,
                "Value narrowing: %d candidates",
                static_cast<int>(valueScanAddresses.size())); ty += 14;

            for (int i = 0; i < showRes; i++) {
                D3DCOLOR c = (i == selectedResult) ?
                    D3DCOLOR_ARGB(255, 255, 255, 60) : Overlay::White;
                std::string hex = HexDumpResult(results[i]);
                Overlay::DrawText(x + 5, ty, c,
                    "%s0x%llX: %s",
                    (i == selectedResult) ? "> " : "  ",
                    results[i].address, hex.c_str());
                ty += 13;
            }

            return y + h + 4;
        }
    };

} // namespace W101Hook

#pragma once
#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <vector>
#include <mutex>
#include <string>
#include <cstdint>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")

namespace W101Hook {

    class PacketSniffer {
    public:
        enum Direction { OUTBOUND, INBOUND };

        struct Packet {
            Direction   dir;
            DWORD       timestamp;
            SOCKET      sock;
            uint16_t    size;
            uint8_t     header[64]; // first 64 bytes snapshot
            uint8_t     headerLen;
        };

        struct Stats {
            uint64_t totalSent;
            uint64_t totalRecv;
            uint32_t packetsSent;
            uint32_t packetsRecv;
            DWORD    startTime;
            float    sendRate;
            float    recvRate;
        };

    private:
        using SendFn    = int(WINAPI*)(SOCKET, const char*, int, int);
        using RecvFn    = int(WINAPI*)(SOCKET, char*, int, int);
        using WSASendFn = int(WINAPI*)(SOCKET, LPWSABUF, DWORD, LPDWORD, DWORD, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);
        using WSARecvFn = int(WINAPI*)(SOCKET, LPWSABUF, DWORD, LPDWORD, LPDWORD, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);

        static inline SendFn    oSend    = nullptr;
        static inline RecvFn    oRecv    = nullptr;
        static inline WSASendFn oWSASend = nullptr;
        static inline WSARecvFn oWSARecv = nullptr;

        static inline std::vector<Packet> packets;
        static inline std::mutex          packetMtx;
        static inline Stats               stats = {};
        static inline bool                active = false;
        static inline bool                logging = true;
        static inline bool                filterOutbound = true;
        static inline bool                filterInbound = true;
        static inline size_t              maxPackets = 8000;

        // Rate tracking
        static inline uint64_t rateSentAccum = 0;
        static inline uint64_t rateRecvAccum = 0;
        static inline DWORD    rateLastTick  = 0;

        static void RecordPacket(Direction dir, SOCKET s, const void* data, int len) {
            if (!logging) return;
            if (dir == OUTBOUND && !filterOutbound) return;
            if (dir == INBOUND && !filterInbound) return;
            if (len <= 0 || !data) return;

            Packet pkt = {};
            pkt.dir = dir;
            pkt.timestamp = GetTickCount();
            pkt.sock = s;
            pkt.size = static_cast<uint16_t>(std::min(len, 65535));
            pkt.headerLen = static_cast<uint8_t>(std::min(len, 64));
            memcpy(pkt.header, data, pkt.headerLen);

            if (dir == OUTBOUND) {
                stats.totalSent += len;
                stats.packetsSent++;
                rateSentAccum += len;
            } else {
                stats.totalRecv += len;
                stats.packetsRecv++;
                rateRecvAccum += len;
            }

            // Rate calculation every second
            DWORD now = GetTickCount();
            if (now - rateLastTick >= 1000) {
                float elapsed = (now - rateLastTick) / 1000.0f;
                stats.sendRate = rateSentAccum / elapsed;
                stats.recvRate = rateRecvAccum / elapsed;
                rateSentAccum = 0;
                rateRecvAccum = 0;
                rateLastTick = now;
            }

            std::lock_guard<std::mutex> lock(packetMtx);
            packets.push_back(pkt);
            if (packets.size() > maxPackets) {
                packets.erase(packets.begin(), packets.begin() + (maxPackets / 2));
            }
        }

        // --- IAT Hook Detours ---

        static int WINAPI hkSend(SOCKET s, const char* buf, int len, int flags) {
            RecordPacket(OUTBOUND, s, buf, len);
            return oSend(s, buf, len, flags);
        }

        static int WINAPI hkRecv(SOCKET s, char* buf, int len, int flags) {
            int result = oRecv(s, buf, len, flags);
            if (result > 0) RecordPacket(INBOUND, s, buf, result);
            return result;
        }

        static int WINAPI hkWSASend(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount,
            LPDWORD lpNumberOfBytesSent, DWORD dwFlags,
            LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine) {
            for (DWORD i = 0; i < dwBufferCount; i++) {
                if (lpBuffers[i].buf && lpBuffers[i].len > 0) {
                    RecordPacket(OUTBOUND, s, lpBuffers[i].buf, lpBuffers[i].len);
                }
            }
            return oWSASend(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent,
                dwFlags, lpOverlapped, lpCompletionRoutine);
        }

        static int WINAPI hkWSARecv(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount,
            LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags,
            LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine) {
            int result = oWSARecv(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd,
                lpFlags, lpOverlapped, lpCompletionRoutine);
            if (result == 0 && lpNumberOfBytesRecvd && *lpNumberOfBytesRecvd > 0) {
                for (DWORD i = 0; i < dwBufferCount; i++) {
                    if (lpBuffers[i].buf && lpBuffers[i].len > 0) {
                        RecordPacket(INBOUND, s, lpBuffers[i].buf,
                            std::min((DWORD)lpBuffers[i].len, *lpNumberOfBytesRecvd));
                    }
                }
            }
            return result;
        }

        // --- IAT Patching ---

        static bool PatchIAT(uintptr_t moduleBase, const char* dllName,
            const char* funcName, void* hookFunc, void** origFunc) {

            PIMAGE_DOS_HEADER dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(moduleBase);
            if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) return false;

            PIMAGE_NT_HEADERS ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(
                moduleBase + dosHeader->e_lfanew);
            if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) return false;

            auto& importDir = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
            if (importDir.VirtualAddress == 0) return false;

            PIMAGE_IMPORT_DESCRIPTOR importDesc = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(
                moduleBase + importDir.VirtualAddress);

            while (importDesc->Name) {
                const char* currentDll = reinterpret_cast<const char*>(
                    moduleBase + importDesc->Name);

                if (_stricmp(currentDll, dllName) == 0) {
                    PIMAGE_THUNK_DATA origThunk = reinterpret_cast<PIMAGE_THUNK_DATA>(
                        moduleBase + importDesc->OriginalFirstThunk);
                    PIMAGE_THUNK_DATA thunk = reinterpret_cast<PIMAGE_THUNK_DATA>(
                        moduleBase + importDesc->FirstThunk);

                    while (origThunk->u1.AddressOfData) {
                        if (!(origThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG)) {
                            PIMAGE_IMPORT_BY_NAME importByName =
                                reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(
                                    moduleBase + origThunk->u1.AddressOfData);

                            if (strcmp(importByName->Name, funcName) == 0) {
                                DWORD oldProt;
                                VirtualProtect(&thunk->u1.Function, sizeof(uintptr_t),
                                    PAGE_EXECUTE_READWRITE, &oldProt);

                                *origFunc = reinterpret_cast<void*>(thunk->u1.Function);
                                thunk->u1.Function = reinterpret_cast<uintptr_t>(hookFunc);

                                VirtualProtect(&thunk->u1.Function, sizeof(uintptr_t),
                                    oldProt, &oldProt);
                                return true;
                            }
                        }
                        origThunk++;
                        thunk++;
                    }
                }
                importDesc++;
            }
            return false;
        }

    public:
        static bool Init() {
            if (active) return true;

            uintptr_t gameBase = reinterpret_cast<uintptr_t>(
                GetModuleHandleA("WizardGraphicalClient.exe"));
            if (!gameBase) return false;

            stats.startTime = GetTickCount();
            rateLastTick = GetTickCount();

            bool hooked = false;

            hooked |= PatchIAT(gameBase, "WS2_32.dll", "send",
                reinterpret_cast<void*>(hkSend), reinterpret_cast<void**>(&oSend));
            hooked |= PatchIAT(gameBase, "WS2_32.dll", "recv",
                reinterpret_cast<void*>(hkRecv), reinterpret_cast<void**>(&oRecv));
            hooked |= PatchIAT(gameBase, "WS2_32.dll", "WSASend",
                reinterpret_cast<void*>(hkWSASend), reinterpret_cast<void**>(&oWSASend));
            hooked |= PatchIAT(gameBase, "WS2_32.dll", "WSARecv",
                reinterpret_cast<void*>(hkWSARecv), reinterpret_cast<void**>(&oWSARecv));

            // Also try lowercase (some linkers use different casing)
            if (!oSend) {
                hooked |= PatchIAT(gameBase, "ws2_32.dll", "send",
                    reinterpret_cast<void*>(hkSend), reinterpret_cast<void**>(&oSend));
            }
            if (!oRecv) {
                hooked |= PatchIAT(gameBase, "ws2_32.dll", "recv",
                    reinterpret_cast<void*>(hkRecv), reinterpret_cast<void**>(&oRecv));
            }
            if (!oWSASend) {
                hooked |= PatchIAT(gameBase, "ws2_32.dll", "WSASend",
                    reinterpret_cast<void*>(hkWSASend), reinterpret_cast<void**>(&oWSASend));
            }
            if (!oWSARecv) {
                hooked |= PatchIAT(gameBase, "ws2_32.dll", "WSARecv",
                    reinterpret_cast<void*>(hkWSARecv), reinterpret_cast<void**>(&oWSARecv));
            }

            active = hooked;
            return hooked;
        }

        static void Shutdown() {
            // restore original IAT entries would go here
            // for now just stop recording
            active = false;
            logging = false;
        }

        static void ToggleLogging() { logging = !logging; }
        static bool IsLogging() { return logging; }
        static bool IsActive() { return active; }
        static void SetFilterOutbound(bool v) { filterOutbound = v; }
        static void SetFilterInbound(bool v) { filterInbound = v; }
        static bool GetFilterOutbound() { return filterOutbound; }
        static bool GetFilterInbound() { return filterInbound; }

        static const Stats& GetStats() { return stats; }

        static std::vector<Packet> GetRecentPackets(int count = 20) {
            std::lock_guard<std::mutex> lock(packetMtx);
            int total = static_cast<int>(packets.size());
            int start = std::max(0, total - count);
            return std::vector<Packet>(packets.begin() + start, packets.end());
        }

        static void ClearPackets() {
            std::lock_guard<std::mutex> lock(packetMtx);
            packets.clear();
        }

        static int GetPacketCount() {
            std::lock_guard<std::mutex> lock(packetMtx);
            return static_cast<int>(packets.size());
        }

        // Hex dump a packet header to string
        static std::string HexDump(const Packet& pkt, int maxBytes = 32) {
            std::string result;
            int len = std::min(static_cast<int>(pkt.headerLen), maxBytes);
            char hex[4];
            for (int i = 0; i < len; i++) {
                snprintf(hex, sizeof(hex), "%02X ", pkt.header[i]);
                result += hex;
            }
            if (pkt.size > maxBytes) result += "...";
            return result;
        }

        // ASCII dump (printable chars only)
        static std::string AsciiDump(const Packet& pkt, int maxBytes = 32) {
            std::string result;
            int len = std::min(static_cast<int>(pkt.headerLen), maxBytes);
            for (int i = 0; i < len; i++) {
                char c = static_cast<char>(pkt.header[i]);
                result += (c >= 32 && c < 127) ? c : '.';
            }
            return result;
        }

        static std::string FormatSize(uint64_t bytes) {
            char buf[64];
            if (bytes < 1024)
                snprintf(buf, sizeof(buf), "%lluB", bytes);
            else if (bytes < 1048576)
                snprintf(buf, sizeof(buf), "%.1fKB", bytes / 1024.0);
            else
                snprintf(buf, sizeof(buf), "%.2fMB", bytes / 1048576.0);
            return buf;
        }

        static std::string FormatRate(float bytesPerSec) {
            char buf[64];
            if (bytesPerSec < 1024)
                snprintf(buf, sizeof(buf), "%.0f B/s", bytesPerSec);
            else
                snprintf(buf, sizeof(buf), "%.1f KB/s", bytesPerSec / 1024.0f);
            return buf;
        }
    };

} // namespace W101Hook

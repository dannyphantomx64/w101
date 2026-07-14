#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <algorithm>
#include "../framework.h"

namespace W101Hook {

    class ScriptMonitor {
    public:
        struct MethodCall {
            DWORD       timestamp;
            std::string methodName;
            std::string objectType;
            int         argCount;
            uintptr_t   callerAddr;
            uint32_t    callIndex;
        };

        struct FSCommandEntry {
            DWORD       timestamp;
            std::string command;
            std::string args;
            std::string category;
        };

        struct MethodStats {
            std::string name;
            uint32_t    callCount;
            DWORD       firstSeen;
            DWORD       lastSeen;
        };

    private:
        // Call method trampoline types (matching the dump offsets)
        using CallMethodFn = bool(*)(
            const void* method,     // fn_call structure or similar
            const char* methodName,
            void*       thisPtr,
            void*       env,
            int         nargs,
            int         firstArgBottomIndex
        );

        using CallMethod1Fn = bool(*)(
            const char* methodName,
            void*       result,
            void*       obj,
            void*       env,
            void*       arg0
        );

        static inline Trampoline          callMethodHook;
        static inline Trampoline          callMethod1Hook;
        static inline CallMethodFn        oCallMethod = nullptr;
        static inline CallMethod1Fn       oCallMethod1 = nullptr;

        static inline std::vector<MethodCall>    methodCalls;
        static inline std::vector<FSCommandEntry> fsCmds;
        static inline std::mutex                  callMtx;
        static inline std::mutex                  fsMtx;

        static inline std::unordered_map<std::string, MethodStats> methodStatsMap;

        static inline bool     active = false;
        static inline bool     logging = true;
        static inline bool     logFsCommands = true;
        static inline uint32_t totalCalls = 0;
        static inline uint32_t totalFsCmds = 0;
        static inline size_t   maxCallEntries = 5000;
        static inline size_t   maxFsEntries = 2000;

        // Filters
        static inline std::string filterPattern;
        static inline bool        filterExclude = false;

        // FSCommand categories based on W101's known protocol
        static std::string CategorizeFSCommand(const std::string& cmd) {
            if (cmd.find("wireCommand") != std::string::npos) return "NETWORK";
            if (cmd.find("cycleChat") != std::string::npos) return "CHAT";
            if (cmd.find("chatInput") != std::string::npos) return "CHAT";
            if (cmd.find("sendChat") != std::string::npos) return "CHAT";
            if (cmd.find("Sound") != std::string::npos) return "AUDIO";
            if (cmd.find("sound") != std::string::npos) return "AUDIO";
            if (cmd.find("music") != std::string::npos) return "AUDIO";
            if (cmd.find("navigate") != std::string::npos) return "NAV";
            if (cmd.find("zone") != std::string::npos) return "ZONE";
            if (cmd.find("spell") != std::string::npos) return "COMBAT";
            if (cmd.find("duel") != std::string::npos) return "COMBAT";
            if (cmd.find("combat") != std::string::npos) return "COMBAT";
            if (cmd.find("quest") != std::string::npos) return "QUEST";
            if (cmd.find("login") != std::string::npos) return "AUTH";
            if (cmd.find("Login") != std::string::npos) return "AUTH";
            if (cmd.find("character") != std::string::npos) return "CHAR";
            if (cmd.find("window") != std::string::npos) return "UI";
            if (cmd.find("ui") != std::string::npos) return "UI";
            if (cmd.find("UI") != std::string::npos) return "UI";
            if (cmd.find("close") != std::string::npos) return "UI";
            if (cmd.find("open") != std::string::npos) return "UI";
            if (cmd.find("inventory") != std::string::npos) return "ITEM";
            if (cmd.find("item") != std::string::npos) return "ITEM";
            if (cmd.find("equip") != std::string::npos) return "ITEM";
            if (cmd.find("pet") != std::string::npos) return "PET";
            if (cmd.find("mount") != std::string::npos) return "MOUNT";
            if (cmd.find("friend") != std::string::npos) return "SOCIAL";
            if (cmd.find("trade") != std::string::npos) return "TRADE";
            if (cmd.find("bazaar") != std::string::npos) return "TRADE";
            if (cmd.find("crowns") != std::string::npos) return "SHOP";
            if (cmd.find("shop") != std::string::npos) return "SHOP";
            if (cmd.find("house") != std::string::npos) return "HOUSING";
            if (cmd.find("garden") != std::string::npos) return "HOUSING";
            return "OTHER";
        }

        static bool MatchesFilter(const std::string& name) {
            if (filterPattern.empty()) return true;

            bool found = name.find(filterPattern) != std::string::npos;
            return filterExclude ? !found : found;
        }

        // --- Trampoline Detours ---

        static bool __fastcall hkCallMethod(
            const void* method, const char* methodName,
            void* thisPtr, void* env, int nargs, int firstArgBottomIndex) {

            if (logging && methodName) {
                std::string name(methodName);
                if (MatchesFilter(name)) {
                    MethodCall entry = {};
                    entry.timestamp = GetTickCount();
                    entry.methodName = name;
                    entry.argCount = nargs;
                    entry.callIndex = totalCalls;
                    entry.callerAddr = reinterpret_cast<uintptr_t>(_ReturnAddress());

                    totalCalls++;

                    // Update stats
                    auto& stat = methodStatsMap[name];
                    if (stat.callCount == 0) {
                        stat.name = name;
                        stat.firstSeen = entry.timestamp;
                    }
                    stat.callCount++;
                    stat.lastSeen = entry.timestamp;

                    std::lock_guard<std::mutex> lock(callMtx);
                    methodCalls.push_back(entry);
                    if (methodCalls.size() > maxCallEntries) {
                        methodCalls.erase(methodCalls.begin(),
                            methodCalls.begin() + (maxCallEntries / 2));
                    }
                }
            }

            return oCallMethod(method, methodName, thisPtr, env, nargs, firstArgBottomIndex);
        }

        static bool __fastcall hkCallMethod1(
            const char* methodName, void* result, void* obj, void* env, void* arg0) {

            if (logging && methodName) {
                std::string name(methodName);
                if (MatchesFilter(name)) {
                    MethodCall entry = {};
                    entry.timestamp = GetTickCount();
                    entry.methodName = name;
                    entry.argCount = 1;
                    entry.callIndex = totalCalls;
                    entry.callerAddr = reinterpret_cast<uintptr_t>(_ReturnAddress());

                    totalCalls++;

                    auto& stat = methodStatsMap[name];
                    if (stat.callCount == 0) {
                        stat.name = name;
                        stat.firstSeen = entry.timestamp;
                    }
                    stat.callCount++;
                    stat.lastSeen = entry.timestamp;

                    std::lock_guard<std::mutex> lock(callMtx);
                    methodCalls.push_back(entry);
                    if (methodCalls.size() > maxCallEntries) {
                        methodCalls.erase(methodCalls.begin(),
                            methodCalls.begin() + (maxCallEntries / 2));
                    }
                }
            }

            return oCallMethod1(methodName, result, obj, env, arg0);
        }

    public:
        static bool Init() {
            if (active) return true;

            uintptr_t callMethodAddr = Offsets::Resolve(W101::CallMethod::CallMethod);
            uintptr_t callMethod1Addr = Offsets::Resolve(W101::CallMethod::CallMethod1Arg);

            if (callMethodAddr) {
                oCallMethod = reinterpret_cast<CallMethodFn>(
                    callMethodHook.Install(callMethodAddr,
                        reinterpret_cast<uintptr_t>(hkCallMethod)));
                if (oCallMethod) active = true;
            }

            if (callMethod1Addr) {
                oCallMethod1 = reinterpret_cast<CallMethod1Fn>(
                    callMethod1Hook.Install(callMethod1Addr,
                        reinterpret_cast<uintptr_t>(hkCallMethod1)));
            }

            return active;
        }

        static void Shutdown() {
            callMethodHook.Remove();
            callMethod1Hook.Remove();
            active = false;
        }

        static void ProcessFSCommand(const std::string& cmd, const std::string& args) {
            if (!logFsCommands) return;

            FSCommandEntry entry = {};
            entry.timestamp = GetTickCount();
            entry.command = cmd;
            entry.args = args;
            entry.category = CategorizeFSCommand(cmd);
            totalFsCmds++;

            std::lock_guard<std::mutex> lock(fsMtx);
            fsCmds.push_back(entry);
            if (fsCmds.size() > maxFsEntries) {
                fsCmds.erase(fsCmds.begin(), fsCmds.begin() + (maxFsEntries / 2));
            }
        }

        // --- Controls ---
        static void ToggleLogging() { logging = !logging; }
        static void ToggleFsLogging() { logFsCommands = !logFsCommands; }
        static bool IsLogging() { return logging; }
        static bool IsFsLogging() { return logFsCommands; }
        static bool IsActive() { return active; }
        static uint32_t GetTotalCalls() { return totalCalls; }
        static uint32_t GetTotalFsCmds() { return totalFsCmds; }

        static void SetFilter(const std::string& pattern, bool exclude = false) {
            filterPattern = pattern;
            filterExclude = exclude;
        }
        static void ClearFilter() { filterPattern.clear(); filterExclude = false; }
        static const std::string& GetFilter() { return filterPattern; }

        // --- Data Access ---

        static std::vector<MethodCall> GetRecentCalls(int count = 15) {
            std::lock_guard<std::mutex> lock(callMtx);
            int total = static_cast<int>(methodCalls.size());
            int start = std::max(0, total - count);
            return std::vector<MethodCall>(methodCalls.begin() + start, methodCalls.end());
        }

        static std::vector<FSCommandEntry> GetRecentFsCommands(int count = 15) {
            std::lock_guard<std::mutex> lock(fsMtx);
            int total = static_cast<int>(fsCmds.size());
            int start = std::max(0, total - count);
            return std::vector<FSCommandEntry>(fsCmds.begin() + start, fsCmds.end());
        }

        static std::vector<MethodStats> GetTopMethods(int count = 20) {
            std::vector<MethodStats> result;
            result.reserve(methodStatsMap.size());
            for (auto& p : methodStatsMap) {
                result.push_back(p.second);
            }
            std::sort(result.begin(), result.end(),
                [](const MethodStats& a, const MethodStats& b) {
                    return a.callCount > b.callCount;
                });
            if (static_cast<int>(result.size()) > count) result.resize(count);
            return result;
        }

        static int GetUniqueMethodCount() {
            return static_cast<int>(methodStatsMap.size());
        }

        static void ClearCalls() {
            std::lock_guard<std::mutex> lock(callMtx);
            methodCalls.clear();
            totalCalls = 0;
            methodStatsMap.clear();
        }

        static void ClearFsCommands() {
            std::lock_guard<std::mutex> lock(fsMtx);
            fsCmds.clear();
            totalFsCmds = 0;
        }

        static D3DCOLOR GetCategoryColor(const std::string& cat) {
            if (cat == "NETWORK") return D3DCOLOR_ARGB(255, 255, 100, 50);
            if (cat == "CHAT")    return D3DCOLOR_ARGB(255, 100, 255, 100);
            if (cat == "COMBAT")  return D3DCOLOR_ARGB(255, 255, 60,  60);
            if (cat == "QUEST")   return D3DCOLOR_ARGB(255, 255, 255, 60);
            if (cat == "AUTH")    return D3DCOLOR_ARGB(255, 255, 60,  255);
            if (cat == "UI")      return D3DCOLOR_ARGB(255, 60,  200, 255);
            if (cat == "ZONE")    return D3DCOLOR_ARGB(255, 60,  255, 200);
            if (cat == "AUDIO")   return D3DCOLOR_ARGB(255, 200, 200, 200);
            if (cat == "ITEM")    return D3DCOLOR_ARGB(255, 255, 200, 60);
            if (cat == "PET")     return D3DCOLOR_ARGB(255, 180, 255, 180);
            if (cat == "SOCIAL")  return D3DCOLOR_ARGB(255, 180, 180, 255);
            if (cat == "TRADE")   return D3DCOLOR_ARGB(255, 255, 180, 100);
            if (cat == "SHOP")    return D3DCOLOR_ARGB(255, 255, 215, 0);
            if (cat == "HOUSING") return D3DCOLOR_ARGB(255, 160, 120, 255);
            if (cat == "MOUNT")   return D3DCOLOR_ARGB(255, 120, 255, 160);
            if (cat == "CHAR")    return D3DCOLOR_ARGB(255, 200, 150, 255);
            if (cat == "NAV")     return D3DCOLOR_ARGB(255, 150, 200, 255);
            return D3DCOLOR_ARGB(255, 160, 160, 160);
        }
    };

} // namespace W101Hook

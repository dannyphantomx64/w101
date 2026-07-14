#pragma once
#include <Windows.h>
#include <d3d9.h>
#include <string>
#include <vector>
#include <mutex>
#include <functional>
#include "../framework.h"
#include "../overlay.h"

namespace W101Hook {

    class ScriptExecutor {
    public:
        struct ExecutionResult {
            DWORD       timestamp;
            std::string method;
            std::string args;
            bool        success;
            std::string returnValue;
        };

        struct QuickCommand {
            std::string name;
            std::string method;
            std::string description;
            unsigned short hotkey;
        };

    private:
        // gameswf call_method function types from the dump
        using CallMethodFn = bool(*)(
            const void* method, const char* methodName,
            void* thisPtr, void* env, int nargs, int firstArgBottomIndex);

        using CallMethod0Fn = bool(*)(
            const char* methodName, void* result, void* obj, void* env);

        using CallMethod1Fn = bool(*)(
            const char* methodName, void* result, void* obj, void* env, void* arg0);

        using CallMethod2Fn = bool(*)(
            const char* methodName, void* result, void* obj, void* env,
            void* arg0, void* arg1);

        // as_value manipulation
        using AsValueSetStringFn = void(*)(void* asValue, const char* str);
        using AsValueSetNumberFn = void(*)(void* asValue, double num);
        using AsValueSetBoolFn   = void(*)(void* asValue, bool val);
        using AsValueToStringFn  = const char*(*)(void* asValue);
        using AsValueToNumberFn  = double(*)(void* asValue);

        // FSCommand — direct invocation
        using FSCommandFn = void(*)(void* interface_handler, const char* cmd, const char* args);

        static inline CallMethod0Fn    fnCallMethod0 = nullptr;
        static inline CallMethod1Fn    fnCallMethod1 = nullptr;
        static inline CallMethod2Fn    fnCallMethod2 = nullptr;
        static inline AsValueSetStringFn fnSetString = nullptr;
        static inline AsValueSetNumberFn fnSetNumber = nullptr;
        static inline AsValueToStringFn  fnToString = nullptr;
        static inline AsValueToNumberFn  fnToNumber = nullptr;

        static inline std::vector<ExecutionResult> history;
        static inline std::vector<QuickCommand>    quickCommands;
        static inline std::mutex                   histMtx;
        static inline bool                         active = false;

        // Pending execution queue (thread-safe — executes on game thread via advance hook)
        struct PendingExec {
            std::string method;
            std::vector<std::string> args;
            bool isFsCommand;
        };

        static inline std::vector<PendingExec> pendingQueue;
        static inline std::mutex               queueMtx;

        static void RecordResult(const std::string& method, const std::string& args,
            bool success, const std::string& retVal = "") {
            ExecutionResult res;
            res.timestamp = GetTickCount();
            res.method = method;
            res.args = args;
            res.success = success;
            res.returnValue = retVal;

            std::lock_guard<std::mutex> lock(histMtx);
            history.push_back(res);
            if (history.size() > 500) {
                history.erase(history.begin(), history.begin() + 250);
            }
        }

    public:
        static bool Init() {
            // Resolve AS value manipulation functions from dump
            fnCallMethod0 = reinterpret_cast<CallMethod0Fn>(
                Offsets::Resolve(W101::CallMethod::CallMethod0Arg));
            fnCallMethod1 = reinterpret_cast<CallMethod1Fn>(
                Offsets::Resolve(W101::CallMethod::CallMethod1Arg));
            fnCallMethod2 = reinterpret_cast<CallMethod2Fn>(
                Offsets::Resolve(W101::CallMethod::CallMethod2Arg));

            fnSetString = reinterpret_cast<AsValueSetStringFn>(
                Offsets::Resolve(W101::AsValue::SetString));
            fnSetNumber = reinterpret_cast<AsValueSetNumberFn>(
                Offsets::Resolve(W101::AsValue::SetDouble));
            fnToString = reinterpret_cast<AsValueToStringFn>(
                Offsets::Resolve(W101::AsValue::ToString));
            fnToNumber = reinterpret_cast<AsValueToNumberFn>(
                Offsets::Resolve(W101::AsValue::ToNumber));

            // Pre-built quick commands for W101
            RegisterQuickCommand("gotoFrame", "gotoFrame",
                "Jump to a specific SWF frame", 0);
            RegisterQuickCommand("cycleChat", "cycleChat",
                "Cycle through chat channels", 0);
            RegisterQuickCommand("wireCommand", "wireCommand",
                "Send raw wire protocol command", 0);
            RegisterQuickCommand("toggleUI", "toggleUI",
                "Show/hide UI element", 0);

            active = true;
            return true;
        }

        static void Shutdown() {
            active = false;
            std::lock_guard<std::mutex> lock(queueMtx);
            pendingQueue.clear();
        }

        static bool IsActive() { return active; }

        // --- Quick Command Registration ---

        static void RegisterQuickCommand(const std::string& name, const std::string& method,
            const std::string& desc, unsigned short hotkey = 0) {
            QuickCommand qc;
            qc.name = name;
            qc.method = method;
            qc.description = desc;
            qc.hotkey = hotkey;
            quickCommands.push_back(qc);
        }

        // --- Execution (queues for game thread) ---

        static void QueueMethodCall(const std::string& methodName,
            const std::vector<std::string>& args = {}) {
            PendingExec pe;
            pe.method = methodName;
            pe.args = args;
            pe.isFsCommand = false;

            std::lock_guard<std::mutex> lock(queueMtx);
            pendingQueue.push_back(pe);
        }

        static void QueueFSCommand(const std::string& command, const std::string& args = "") {
            PendingExec pe;
            pe.method = command;
            pe.args = { args };
            pe.isFsCommand = true;

            std::lock_guard<std::mutex> lock(queueMtx);
            pendingQueue.push_back(pe);
        }

        // Called from advance hook — processes pending executions on the game thread
        static void ProcessQueue(root* r) {
            if (!active) return;

            std::vector<PendingExec> batch;
            {
                std::lock_guard<std::mutex> lock(queueMtx);
                if (pendingQueue.empty()) return;
                batch.swap(pendingQueue);
            }

            for (auto& pe : batch) {
                if (pe.isFsCommand) {
                    ExecuteFSCommand(r, pe.method, pe.args.empty() ? "" : pe.args[0]);
                } else {
                    ExecuteMethodCall(r, pe.method, pe.args);
                }
            }
        }

    private:
        static void ExecuteMethodCall(root* r, const std::string& method,
            const std::vector<std::string>& args) {
            if (!r) {
                RecordResult(method, "", false, "no root");
                return;
            }

            // as_value result buffer (typically 16-32 bytes on x64)
            uint8_t resultBuf[64] = {};
            void* result = resultBuf;
            void* obj = static_cast<void*>(r);
            void* env = nullptr; // null env = use root environment

            struct CallResult {
                bool success;
                bool exception;
                char argStr[256];
            };

            auto doCall = [](const char* methodName, int argCount,
                const char* arg0, const char* arg1,
                void* result, void* obj, void* env,
                auto fn0, auto fn1, auto fn2, auto fnSet) -> CallResult {
                CallResult cr = { false, false, "" };
                __try {
                    if (argCount == 0 && fn0) {
                        cr.success = fn0(methodName, result, obj, env);
                        strncpy(cr.argStr, "(no args)", 255);
                    } else if (argCount == 1 && fn1) {
                        uint8_t a0[64] = {};
                        if (fnSet) fnSet(a0, arg0);
                        cr.success = fn1(methodName, result, obj, env, a0);
                        strncpy(cr.argStr, arg0 ? arg0 : "", 255);
                    } else if (argCount == 2 && fn2) {
                        uint8_t a0[64] = {}, a1[64] = {};
                        if (fnSet) { fnSet(a0, arg0); fnSet(a1, arg1); }
                        cr.success = fn2(methodName, result, obj, env, a0, a1);
                        snprintf(cr.argStr, 255, "%s, %s", arg0 ? arg0 : "", arg1 ? arg1 : "");
                    }
                } __except(EXCEPTION_EXECUTE_HANDLER) {
                    cr.exception = true;
                }
                return cr;
            };

            if ((int)args.size() > 2) {
                RecordResult(method, "too many args", false, "max 2 args supported");
                return;
            }

            const char* a0 = args.size() > 0 ? args[0].c_str() : nullptr;
            const char* a1 = args.size() > 1 ? args[1].c_str() : nullptr;

            auto cr = doCall(method.c_str(), (int)args.size(), a0, a1,
                result, obj, env, fnCallMethod0, fnCallMethod1, fnCallMethod2, fnSetString);

            if (cr.exception) {
                RecordResult(method, cr.argStr, false, "EXCEPTION");
                return;
            }

            std::string retStr = "void";
            if (cr.success && fnToString) {
                const char* s = SafeToString(result);
                if (s) retStr = s;
            }
            RecordResult(method, cr.argStr, cr.success, retStr);
        }

        static const char* SafeToString(void* result) {
            __try {
                return fnToString(result);
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                return nullptr;
            }
        }

        static bool SafeGameLogMsg(const char* msg) {
            __try {
                Framework::GameLogMsg("%s", msg);
                return true;
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
        }

        static void ExecuteFSCommand(root* r, const std::string& cmd, const std::string& args) {
            if (!r) {
                RecordResult("FS:" + cmd, args, false, "no root");
                return;
            }

            char buf[512];
            snprintf(buf, sizeof(buf), "ScriptExecutor: FS %s(%s)", cmd.c_str(), args.c_str());
            if (SafeGameLogMsg(buf)) {
                RecordResult("FS:" + cmd, args, true, "dispatched");
            } else {
                RecordResult("FS:" + cmd, args, false, "EXCEPTION");
            }
        }

    public:
        // --- GotoFrame shortcut ---

        static void GotoFrame(root* r, int frame) {
            if (!r) return;
            Framework::GotoFrame(r, frame);
            RecordResult("gotoFrame", std::to_string(frame), true, "ok");
        }

        // --- Data Access ---

        static std::vector<ExecutionResult> GetHistory(int count = 15) {
            std::lock_guard<std::mutex> lock(histMtx);
            int total = static_cast<int>(history.size());
            int start = std::max(0, total - count);
            return std::vector<ExecutionResult>(history.begin() + start, history.end());
        }

        static int GetHistoryCount() {
            std::lock_guard<std::mutex> lock(histMtx);
            return static_cast<int>(history.size());
        }

        static void ClearHistory() {
            std::lock_guard<std::mutex> lock(histMtx);
            history.clear();
        }

        static int GetPendingCount() {
            std::lock_guard<std::mutex> lock(queueMtx);
            return static_cast<int>(pendingQueue.size());
        }

        static const std::vector<QuickCommand>& GetQuickCommands() { return quickCommands; }

        // --- Overlay ---

        static int RenderPanel(IDirect3DDevice9* dev, int x, int y) {
            auto hist = GetHistory(8);
            int histCount = static_cast<int>(hist.size());
            int h = 50 + histCount * 13;

            Overlay::DrawFilledRect(dev, x, y, 520, h, Overlay::BgPanel);
            Overlay::DrawRect(x, y, 520, h, D3DCOLOR_ARGB(255, 180, 120, 255), false);

            int ty = y + 4;
            Overlay::DrawText(x + 5, ty, D3DCOLOR_ARGB(255, 180, 120, 255),
                "SCRIPT EXECUTOR"); ty += 16;

            int pending = GetPendingCount();
            Overlay::DrawText(x + 5, ty, Overlay::White,
                "History: %d  |  Quick cmds: %d  |  Pending: %d",
                GetHistoryCount(), static_cast<int>(quickCommands.size()), pending); ty += 14;

            Overlay::DrawText(x + 5, ty, Overlay::Yellow,
                "--- Execution Log ---"); ty += 14;

            for (auto& res : hist) {
                D3DCOLOR c = res.success ?
                    D3DCOLOR_ARGB(255, 60, 255, 60) :
                    D3DCOLOR_ARGB(255, 255, 60, 60);
                std::string line = res.method;
                if (!res.args.empty()) line += "(" + res.args + ")";
                if (!res.returnValue.empty()) line += " -> " + res.returnValue;
                if (line.length() > 65) line = line.substr(0, 65) + "...";
                Overlay::DrawText(x + 5, ty, c, "%s %s",
                    res.success ? "OK" : "ERR", line.c_str());
                ty += 13;
            }

            return y + h + 4;
        }
    };

} // namespace W101Hook

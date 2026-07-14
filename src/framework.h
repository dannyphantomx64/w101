#pragma once
#include <Windows.h>
#include <d3d9.h>
#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <functional>
#include <unordered_map>
#include "../WizardGraphicalClient_Dump.h"

#pragma comment(lib, "d3d9.lib")

namespace W101Hook {

    // ========================================================================
    // RUNTIME OFFSET RESOLVER
    // ========================================================================
    class Offsets {
    public:
        static uintptr_t Base() {
            static uintptr_t base = reinterpret_cast<uintptr_t>(
                GetModuleHandleA("WizardGraphicalClient.exe")
            );
            return base;
        }

        static uintptr_t Resolve(uintptr_t staticAddr) {
            return staticAddr - W101::BASE + Base();
        }
    };

    // ========================================================================
    // MINIMAL TRAMPOLINE ENGINE
    // ========================================================================
    class Trampoline {
    public:
        static constexpr size_t STOLEN_BYTES = 14;

        struct Hook {
            uintptr_t target;
            uintptr_t detour;
            uintptr_t trampoline;
            uint8_t   original[STOLEN_BYTES];
            bool      active;
        };

        static bool Install(Hook& hook) {
            hook.trampoline = reinterpret_cast<uintptr_t>(
                VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE)
            );
            if (!hook.trampoline) return false;

            memcpy(hook.original, reinterpret_cast<void*>(hook.target), STOLEN_BYTES);
            memcpy(reinterpret_cast<void*>(hook.trampoline), hook.original, STOLEN_BYTES);

            uint8_t* trampJmp = reinterpret_cast<uint8_t*>(hook.trampoline + STOLEN_BYTES);
            trampJmp[0] = 0xFF;
            trampJmp[1] = 0x25;
            *reinterpret_cast<uint32_t*>(trampJmp + 2) = 0;
            *reinterpret_cast<uintptr_t*>(trampJmp + 6) = hook.target + STOLEN_BYTES;

            DWORD oldProt;
            VirtualProtect(reinterpret_cast<void*>(hook.target), STOLEN_BYTES, PAGE_EXECUTE_READWRITE, &oldProt);

            uint8_t* targetBytes = reinterpret_cast<uint8_t*>(hook.target);
            targetBytes[0] = 0xFF;
            targetBytes[1] = 0x25;
            *reinterpret_cast<uint32_t*>(targetBytes + 2) = 0;
            *reinterpret_cast<uintptr_t*>(targetBytes + 6) = hook.detour;

            VirtualProtect(reinterpret_cast<void*>(hook.target), STOLEN_BYTES, oldProt, &oldProt);
            FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(hook.target), STOLEN_BYTES);

            hook.active = true;
            return true;
        }

        static bool Remove(Hook& hook) {
            if (!hook.active) return false;

            DWORD oldProt;
            VirtualProtect(reinterpret_cast<void*>(hook.target), STOLEN_BYTES, PAGE_EXECUTE_READWRITE, &oldProt);
            memcpy(reinterpret_cast<void*>(hook.target), hook.original, STOLEN_BYTES);
            VirtualProtect(reinterpret_cast<void*>(hook.target), STOLEN_BYTES, oldProt, &oldProt);
            FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(hook.target), STOLEN_BYTES);

            VirtualFree(reinterpret_cast<void*>(hook.trampoline), 0, MEM_RELEASE);
            hook.active = false;
            return true;
        }

        // Instance interface for per-module use
        Hook m_hook = {};

        uintptr_t Install(uintptr_t target, uintptr_t detour) {
            m_hook.target = target;
            m_hook.detour = detour;
            if (!Install(m_hook)) return 0;
            return m_hook.trampoline;
        }

        bool Remove() {
            return Remove(m_hook);
        }
    };

    // ========================================================================
    // D3D9 VTABLE HOOK
    // ========================================================================
    class D3D9Hook {
    public:
        using EndSceneFn = HRESULT(__stdcall*)(IDirect3DDevice9*);
        using ResetFn    = HRESULT(__stdcall*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);

        static inline EndSceneFn oEndScene = nullptr;
        static inline ResetFn    oReset    = nullptr;
        static inline IDirect3DDevice9* pDevice = nullptr;
        static inline bool initialized = false;

        static inline std::function<void(IDirect3DDevice9*)> onEndScene = nullptr;
        static inline std::function<void(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*)> onReset = nullptr;

        static HRESULT __stdcall hkEndScene(IDirect3DDevice9* dev) {
            pDevice = dev;
            if (onEndScene) onEndScene(dev);
            return oEndScene(dev);
        }

        static HRESULT __stdcall hkReset(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* params) {
            if (onReset) onReset(dev, params);
            return oReset(dev, params);
        }

        static bool HookFromDevice(IDirect3DDevice9* device) {
            if (initialized) return true;
            pDevice = device;

            void** vtable = *reinterpret_cast<void***>(device);

            DWORD oldProt;
            VirtualProtect(&vtable[16], sizeof(void*) * 2, PAGE_EXECUTE_READWRITE, &oldProt);
            VirtualProtect(&vtable[42], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt);

            oReset = reinterpret_cast<ResetFn>(vtable[16]);
            oEndScene = reinterpret_cast<EndSceneFn>(vtable[42]);

            vtable[16] = reinterpret_cast<void*>(&hkReset);
            vtable[42] = reinterpret_cast<void*>(&hkEndScene);

            initialized = true;
            return true;
        }

        static bool HookFromDummy(HWND gameWindow) {
            IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
            if (!d3d) return false;

            D3DPRESENT_PARAMETERS pp = {};
            pp.Windowed = TRUE;
            pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
            pp.hDeviceWindow = gameWindow;
            pp.BackBufferFormat = D3DFMT_UNKNOWN;

            IDirect3DDevice9* dummy = nullptr;
            HRESULT hr = d3d->CreateDevice(
                D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, gameWindow,
                D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &dummy
            );

            if (FAILED(hr)) {
                d3d->Release();
                return false;
            }

            void** vtable = *reinterpret_cast<void***>(dummy);
            oReset = reinterpret_cast<ResetFn>(vtable[16]);
            oEndScene = reinterpret_cast<EndSceneFn>(vtable[42]);

            dummy->Release();
            d3d->Release();

            void** realVtable = *reinterpret_cast<void***>(dummy);
            DWORD oldProt;
            VirtualProtect(&realVtable[16], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt);
            realVtable[16] = reinterpret_cast<void*>(&hkReset);
            VirtualProtect(&realVtable[16], sizeof(void*), oldProt, &oldProt);

            VirtualProtect(&realVtable[42], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt);
            realVtable[42] = reinterpret_cast<void*>(&hkEndScene);
            VirtualProtect(&realVtable[42], sizeof(void*), oldProt, &oldProt);

            initialized = true;
            return true;
        }
    };

    // ========================================================================
    // GAMESWF TYPE DEFINITIONS
    // ========================================================================
    struct as_value;
    struct as_object;
    struct as_environment;
    struct as_function;
    struct root;
    struct player;
    struct character;
    struct tu_string;
    struct tu_stringi;
    struct render_handler;
    struct event_id;

    struct fn_call {
        as_value*       result;
        as_object*      this_ptr;
        as_environment* env;
        int             nargs;
        int             first_arg_bottom_index;
    };

    // ========================================================================
    // FUNCTION TYPEDEFS FROM EXPORTS
    // ========================================================================

    // Root
    using RootAdvanceFn         = void(__fastcall*)(root*, float);
    using RootDisplayFn         = void(__fastcall*)(root*);
    using RootNotifyMouseFn     = void(__fastcall*)(root*, int, int, int, int);
    using RootGotoFrameFn       = void(__fastcall*)(root*, int);
    using RootGetCurrentFrameFn = int(__fastcall*)(root*);
    using RootSetViewportFn     = void(__fastcall*)(root*, int, int, int, int);

    // Player
    using PlayerNotifyKeyFn     = void(__fastcall*)(player*, unsigned short, bool);
    using PlayerGetRootFn       = root*(__fastcall*)(player*);
    using PlayerLoadFileFn      = void*(__fastcall*)(player*, const char*);
    using PlayerCreateMovieFn   = void*(__fastcall*)(player*, const char*);

    // as_object
    using GetMemberFn           = bool(__fastcall*)(as_object*, const tu_stringi&, as_value*);
    using SetMemberFn           = bool(__fastcall*)(as_object*, const tu_stringi&, const as_value&);
    using OnEventFn             = bool(__fastcall*)(as_object*, const event_id&);
    using EnumerateFn           = void(__fastcall*)(as_object*, as_environment*);

    // Method calls
    using CallMethodValueFn     = void*(__fastcall*)(const as_value&, as_environment*, const as_value&, int, int);
    using CallMethodStringFn    = void*(__fastcall*)(as_environment*, as_object*, const char*, const as_value*, int);

    // Callbacks
    using LogCallbackFn         = void(*)(bool, const char*);
    using FileOpenerFn          = void*(*)(const char*);
    using FsCommandFn           = void(*)(character*, const char*, const char*);
    using RegisterLogFn         = void(*)(LogCallbackFn);
    using RegisterFileOpenerFn  = void(*)(FileOpenerFn);
    using RegisterFsCommandFn   = void(*)(FsCommandFn);

    // D3D9
    using CreateRenderD3DFn     = render_handler*(*)(IDirect3DDevice9*);
    using GetRenderHandlerFn    = render_handler*(*)();

    // Logging
    using LogMsgFn              = void(*)(const char*, ...);
    using LogErrorFn            = void(*)(const char*, ...);

    // ========================================================================
    // CORE FRAMEWORK
    // ========================================================================
    class Framework {
    public:
        struct LogEntry {
            bool        isError;
            std::string message;
            DWORD       timestamp;
        };

        struct FsCommandEntry {
            std::string command;
            std::string args;
            DWORD       timestamp;
        };

    private:
        static inline Trampoline::Hook advanceHook = {};
        static inline Trampoline::Hook displayHook = {};
        static inline Trampoline::Hook mouseHook   = {};
        static inline Trampoline::Hook keyHook     = {};

        static inline RootAdvanceFn       oAdvance  = nullptr;
        static inline RootDisplayFn       oDisplay  = nullptr;
        static inline RootNotifyMouseFn   oMouse    = nullptr;
        static inline PlayerNotifyKeyFn   oKey      = nullptr;

        static inline std::vector<LogEntry>       logBuffer;
        static inline std::vector<FsCommandEntry> fsCommandBuffer;
        static inline std::mutex                  logMtx;
        static inline std::mutex                  fsCmdMtx;

        static inline std::function<void(root*, float)>               onAdvance  = nullptr;
        static inline std::function<void(root*)>                      onDisplay  = nullptr;
        static inline std::function<bool(root*, int&, int&, int&, int&)> onMouse = nullptr;
        static inline std::function<bool(player*, unsigned short&, bool&)> onKey  = nullptr;
        static inline std::function<void(const char*, const char*)>   onFsCommand = nullptr;

        static inline bool running = false;
        static inline HMODULE selfModule = nullptr;

        // --- Hook callbacks ---

        static void __fastcall hkAdvance(root* self, float dt) {
            if (onAdvance) onAdvance(self, dt);
            oAdvance(self, dt);
        }

        static void __fastcall hkDisplay(root* self) {
            oDisplay(self);
            if (onDisplay) onDisplay(self);
        }

        static void __fastcall hkMouse(root* self, int x, int y, int buttons, int wheel) {
            if (onMouse) {
                if (!onMouse(self, x, y, buttons, wheel)) return;
            }
            oMouse(self, x, y, buttons, wheel);
        }

        static void __fastcall hkKey(player* self, unsigned short key, bool down) {
            if (onKey) {
                if (!onKey(self, key, down)) return;
            }
            oKey(self, key, down);
        }

        static void logCallback(bool isError, const char* msg) {
            std::lock_guard<std::mutex> lock(logMtx);
            logBuffer.push_back({ isError, msg ? msg : "", GetTickCount() });
            if (logBuffer.size() > 10000) {
                logBuffer.erase(logBuffer.begin(), logBuffer.begin() + 5000);
            }
        }

        static void fsCommandCallback(character* ch, const char* cmd, const char* args) {
            std::lock_guard<std::mutex> lock(fsCmdMtx);
            fsCommandBuffer.push_back({ cmd ? cmd : "", args ? args : "", GetTickCount() });
            if (fsCommandBuffer.size() > 5000) {
                fsCommandBuffer.erase(fsCommandBuffer.begin(), fsCommandBuffer.begin() + 2500);
            }
            if (onFsCommand) onFsCommand(cmd, args);
        }

    public:
        static void SetAdvanceCallback(std::function<void(root*, float)> cb) { onAdvance = cb; }
        static void SetDisplayCallback(std::function<void(root*)> cb) { onDisplay = cb; }
        static void SetMouseCallback(std::function<bool(root*, int&, int&, int&, int&)> cb) { onMouse = cb; }
        static void SetKeyCallback(std::function<bool(player*, unsigned short&, bool&)> cb) { onKey = cb; }
        static void SetFsCommandCallback(std::function<void(const char*, const char*)> cb) { onFsCommand = cb; }

        static const std::vector<LogEntry>& GetLogBuffer() { return logBuffer; }
        static const std::vector<FsCommandEntry>& GetFsCommandBuffer() { return fsCommandBuffer; }

        static bool Init(HMODULE hModule) {
            selfModule = hModule;

            // Register native callbacks (no trampoline needed)
            auto regLog = reinterpret_cast<RegisterLogFn>(
                Offsets::Resolve(W101::Logging::RegisterLogCallback)
            );
            auto regFs = reinterpret_cast<RegisterFsCommandFn>(
                Offsets::Resolve(W101::Logging::RegisterFsCommand)
            );

            regLog(logCallback);
            regFs(fsCommandCallback);

            // Trampoline: root::advance
            advanceHook.target = Offsets::Resolve(W101::Root::Advance);
            advanceHook.detour = reinterpret_cast<uintptr_t>(&hkAdvance);
            if (!Trampoline::Install(advanceHook)) return false;
            oAdvance = reinterpret_cast<RootAdvanceFn>(advanceHook.trampoline);

            // Trampoline: root::display
            displayHook.target = Offsets::Resolve(W101::Root::Display);
            displayHook.detour = reinterpret_cast<uintptr_t>(&hkDisplay);
            if (!Trampoline::Install(displayHook)) return false;
            oDisplay = reinterpret_cast<RootDisplayFn>(displayHook.trampoline);

            // Trampoline: root::notify_mouse_state
            mouseHook.target = Offsets::Resolve(W101::Root::NotifyMouseState);
            mouseHook.detour = reinterpret_cast<uintptr_t>(&hkMouse);
            if (!Trampoline::Install(mouseHook)) return false;
            oMouse = reinterpret_cast<RootNotifyMouseFn>(mouseHook.trampoline);

            // Trampoline: player::notify_key_event
            keyHook.target = Offsets::Resolve(W101::Player::NotifyKeyEvent);
            keyHook.detour = reinterpret_cast<uintptr_t>(&hkKey);
            if (!Trampoline::Install(keyHook)) return false;
            oKey = reinterpret_cast<PlayerNotifyKeyFn>(keyHook.trampoline);

            running = true;
            return true;
        }

        static void Shutdown() {
            running = false;
            Trampoline::Remove(advanceHook);
            Trampoline::Remove(displayHook);
            Trampoline::Remove(mouseHook);
            Trampoline::Remove(keyHook);
        }

        static bool IsRunning() { return running; }
        static HMODULE GetModule() { return selfModule; }

        // Direct function callers
        static void GameLogMsg(const char* fmt, ...) {
            auto fn = reinterpret_cast<LogMsgFn>(Offsets::Resolve(W101::Logging::LogMsg));
            va_list args;
            va_start(args, fmt);
            char buf[1024];
            vsnprintf(buf, sizeof(buf), fmt, args);
            va_end(args);
            fn("%s", buf);
        }

        static int GetCurrentFrame(root* r) {
            auto fn = reinterpret_cast<RootGetCurrentFrameFn>(
                Offsets::Resolve(W101::Root::GetCurrentFrame)
            );
            return fn(r);
        }

        static void GotoFrame(root* r, int frame) {
            auto fn = reinterpret_cast<RootGotoFrameFn>(
                Offsets::Resolve(W101::Root::GotoFrame)
            );
            fn(r, frame);
        }

        static void SetViewport(root* r, int x, int y, int w, int h) {
            auto fn = reinterpret_cast<RootSetViewportFn>(
                Offsets::Resolve(W101::Root::SetDisplayViewport)
            );
            fn(r, x, y, w, h);
        }
    };

} // namespace W101Hook

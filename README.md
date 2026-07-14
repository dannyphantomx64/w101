# Wizard101 — WizardGraphicalClient.exe Offset Dump + Intelligence Suite

**Date:** 7/13/26

## Binary Info

| Field | Value |
|-------|-------|
| **Module** | `WizardGraphicalClient.exe` |
| **Architecture** | x86-64 (PE64) |
| **Base Address** | `0x140000000` |
| **Size** | `0x370C000` (~55MB) |
| **Engine** | gameswf (Flash/SWF) + Direct3D 9 |
| **MD5** | `dd71fe8a7d16be3da63da21c1a4fbcb5` |
| **SHA256** | `e73e3738ab2c15fc3fd613fdf9a1fda0ab90340098073d23227c10afb205f373` |

## What's Included

- **`WizardGraphicalClient_Dump.h`** — Full C++ header with all 343+ exported offsets organized by subsystem, protection analysis, and 9 documented hook/trampoline strategies.
- **`src/`** — Complete Intelligence Suite v2 — injectable DLL framework with 8 modules, D3D9 overlay, and debug console.

## Offset Categories

- Player (session controller)
- Root (scene graph, frame advance, display)
- Sprite Instance (display objects)
- D3D9 Rendering (IDirect3DDevice9 handler)
- as_object (base game object — virtual, hookable)
- as_value (ActionScript variant type)
- as_environment / as_array
- Method Calling (ActionScript dispatch)
- Listener / Event System
- Extended Types (point, rectangle, transform, color transform, bitmap)
- File I/O
- Threading & Synchronization
- Matrix / Color Transform math
- Timer / DateTime
- Random
- Sound
- Logging & Callbacks
- Dynamic Library Loading
- String Utilities
- Memory Buffer (membuf)
- VM Stack
- URL Handling
- Global Settings / Config

## Protection Status

**Minimal.** No anti-cheat engine, NOP mutex stubs, no integrity checks, no anti-debug. 343+ gameswf functions exported by ordinal. Three callback registration functions require zero trampolining.

## Hook Strategies Documented

1. D3D9 vtable hook (EndScene/Reset)
2. Frame tick trampoline (`root::advance`)
3. Input intercept (mouse + keyboard)
4. ActionScript property intercept (vtable swap)
5. Script method call intercept
6. Logging hook (callback registration)
7. File I/O intercept (asset replacement)
8. FSCommand intercept (game command channel)
9. IAT hook (network/API redirection)

---

## Intelligence Suite v2

Injectable DLL framework built on top of the offset dump. 8 modules with a dual-column D3D9 overlay, debug console, and full keybind system.

### Architecture

```
src/
├── dllmain.cpp          — DLL entry, hook callbacks, overlay rendering, main loop
├── framework.h          — Offsets class, Trampoline engine, D3D9Hook, Framework manager
├── console.h            — Debug console with ANSI color output
├── overlay.h            — D3DXFont/D3DXLine overlay renderer with draw queue
└── modules/
    ├── speed_control.h  — Delta-time manipulation
    ├── packet_sniffer.h — WinSock IAT hook network monitor
    ├── script_monitor.h — ActionScript call_method logger
    ├── macro_engine.h   — Input record/replay system
    ├── teleporter.h     — Position read/write + noclip
    ├── memory_scanner.h — AOB pattern + value scanner
    ├── script_executor.h— ActionScript method invocation
    ├── entity_radar.h   — Sprite tree entity radar
    └── suite.h          — Master integrator for all modules
```

### Module Breakdown

#### 1. Speed Control (`speed_control.h`)
Delta-time manipulation engine. Intercepts `root::advance` dt parameter to speed up, slow down, or freeze all time-dependent game systems.

- 13 speed presets: 0.1x, 0.25x, 0.5x, 0.75x, 1.0x, 1.5x, 2.0x, 3.0x, 4.0x, 6.0x, 8.0x, 12.0x, 16.0x
- Freeze mode (dt=0, halts all animations/movement/cooldowns)
- Clamps modified dt at 0.5s to prevent physics integration explosions
- Accumulative drift tracking
- Visual speed bar in overlay

| Hotkey | Action |
|--------|--------|
| `Num+` | Speed up one preset |
| `Num-` | Slow down one preset |
| `Num*` | Reset to 1.0x |
| `Num/` | Toggle freeze |
| `Num0` | Toggle speed control on/off |
| `Num1-7` | Direct preset selection |

#### 2. Packet Sniffer (`packet_sniffer.h`)
Network traffic monitor via IAT (Import Address Table) patching on WinSock functions.

- Hooks `send`, `recv`, `WSASend`, `WSARecv` in the game's import table
- Captures first 64 bytes of each packet header
- Direction tracking (TX outbound / RX inbound)
- Per-second send/receive rate calculation
- Hex dump and ASCII dump formatters
- Total bytes sent/received with auto-scaling units (B/KB/MB)
- 8000 packet circular buffer
- Direction filtering (outbound only, inbound only, or both)

#### 3. Script Monitor (`script_monitor.h`)
ActionScript method call logger via 14-byte x64 trampoline on `call_method`.

- Trampolines `call_method` and `call_method_1arg` to intercept all AS method calls
- Logs method name, arg count, call index, and caller return address
- Per-method statistics (call count, first seen, last seen)
- Top-N most called methods ranking
- FSCommand categorizer with 17 categories: NETWORK, CHAT, AUDIO, NAV, ZONE, COMBAT, QUEST, AUTH, CHAR, UI, ITEM, PET, MOUNT, SOCIAL, TRADE, SHOP, HOUSING
- Substring filter with include/exclude modes
- Color-coded category display in overlay

#### 4. Macro Engine (`macro_engine.h`)
Input record and replay system with timing preservation.

- Records keyboard (down/up), mouse (move/click/wheel) events
- Inter-event delays stored as explicit entries for speed-independent scaling
- Mouse move throttling at ~60Hz to prevent buffer bloat
- Playback speed adjustment (0.1x to 10.0x)
- Single play, N-loop, and infinite loop modes
- Pause/resume during playback
- Up to 32 saved macros in memory
- 50,000 event cap per macro

| Hotkey | Action |
|--------|--------|
| `F9` | Start/stop recording |
| `F10` | Play last recorded macro |
| `F11` | Loop playback / pause |
| `F12` | Stop playback |

#### 5. Teleporter (`teleporter.h`)
Position read/write via SWF affine matrix manipulation.

- Reads player position from `sprite_instance::get_world_matrix` (tx/ty translation)
- Writes position via `sprite_instance::set_matrix` with modified translation
- Waypoint system: save, select, teleport, delete (up to 64 waypoints)
- Instant teleport or smooth interpolation mode (configurable speed)
- Noclip mode: WASD movement with Shift boost, bypasses collision
- Distance odometer tracking
- Distance-to-waypoint display

| Hotkey | Action |
|--------|--------|
| `INSERT` | Save current position as waypoint |
| `HOME` | Teleport to selected waypoint |
| `Page Up` | Select previous waypoint |
| `Page Down` | Select next waypoint |
| `DELETE` | Delete selected waypoint |
| `Ctrl+N` | Toggle noclip mode |
| `Ctrl+T` | Toggle smooth teleport |

#### 6. Memory Scanner (`memory_scanner.h`)
Runtime memory scanner with AOB pattern matching and value narrowing.

- **AOB (Array of Bytes) scan**: pattern string format `"48 8B 05 ?? ?? ?? ?? 48 85 C0"` with `??` wildcards
- **Value scan**: first scan + iterative narrowing for finding dynamic addresses
- Supports 1/2/4/8 byte integer scans
- SEH-protected memory reads (survives guard pages, decommitted regions)
- VirtualQuery-based region enumeration with protection filtering
- Memory write with automatic VirtualProtect
- NOP patching for instruction removal
- Scan statistics: time elapsed, bytes scanned, region count
- Up to 500,000 candidate addresses in value scan mode

#### 7. Script Executor (`script_executor.h`)
ActionScript method invocation engine — call arbitrary AS methods through the game's own VM.

- Calls `call_method` variants (0-arg, 1-arg, 2-arg) on the game thread
- Thread-safe execution queue (pushed from any thread, processed in advance hook)
- `as_value` marshaling: string and number argument construction
- Return value extraction via `as_value::to_string`
- FSCommand dispatch
- GotoFrame shortcut via `Framework::GotoFrame`
- Pre-registered quick commands for common W101 operations
- Execution history with success/failure tracking
- SEH-wrapped calls to prevent VM crashes from propagating

#### 8. Entity Radar (`entity_radar.h`)
Sprite tree entity enumeration with visual radar overlay.

- Recursive display list traversal via `GetCharacterCount`/`GetCharacterAtDepth`
- Configurable scan depth (1-8 levels, default 3)
- Extracts: world position (tx/ty), scale, rotation, name, frame count, visibility
- Three display modes:
  - **Mini radar**: compact square with entity dots, crosshair, range rings
  - **Full radar**: larger view with entity name labels
  - **List view**: distance-sorted table with pointer, position, name
- Entity color coding: yellow=named, orange=animated, gray=hidden, white=generic
- Distance-based filtering (min/max range)
- Name substring filter
- Player position tracking

| Hotkey | Action |
|--------|--------|
| `Ctrl+R` | Cycle radar mode (mini/full/list) |

### Core Framework

#### Trampoline Engine (`framework.h`)
14-byte x64 absolute jump detour: `FF 25 00 00 00 00` + 8-byte target address. Allocates executable trampoline memory within 2GB of the hook site, saves original bytes for clean removal.

#### D3D9 Hook (`framework.h`)
EndScene (vtable index 42) and Reset (vtable index 16) hooks via dummy device creation. Falls back to runtime capture if dummy device fails.

#### Overlay (`overlay.h`)
D3DXFont + D3DXLine based rendering with draw queue. Text shadow for readability. Device lost/reset handling. 11 predefined colors.

#### Console (`console.h`)
AllocConsole-based debug output with ANSI escape color codes. Info/Success/Warn/Error levels with color prefixes.

### Panel Toggle Keybinds

| Hotkey | Panel |
|--------|-------|
| `F1` | Toggle entire overlay |
| `F2` | Info panel (FPS, dt, frame, mouse, base addr) |
| `F3` | Game log monitor |
| `F4` | FSCommand monitor |
| `F5` | Speed control panel |
| `F6` | Network panel |
| `F7` | Script monitor panel |
| `F8` | Macro engine panel |
| `Ctrl+1-8` | Toggle panels by index |
| `END` | Eject DLL |

### Build

Compile as x64 DLL targeting Windows. Requires:
- Windows SDK (WinSock2, D3D9)
- DirectX SDK (D3DX9 — `d3dx9.lib` for font/line rendering)
- C++17 or later (`inline` static members)
- MSVC recommended (`__try/__except` SEH, `_ReturnAddress()`)

Inject into `WizardGraphicalClient.exe` via any standard DLL injector.

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
| **SHA256** | `e73e3738ab2c15fc3fd613fdf9a1fda0ab2c15fc3fd613fdf9a1fda0ab90340098073d23227c10afb205f373` |

## What's Included

- **`WizardGraphicalClient_Dump.h`** — Full C++ header with all 343+ exported offsets organized by subsystem, protection analysis, and 9 documented hook/trampoline strategies.
- **`src/`** — Intelligence Suite v4 — injectable DLL framework with 16 modules, D3D9 overlay, exploitation layer, and debug console.

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

## Server vs Client Side

| System | Side | Exploitable? |
|--------|------|-------------|
| Position XYZ | Client | Yes — matrix read/write |
| Speed / dt | Client | Yes — dt manipulation |
| Camera / Viewport | Client | Yes — viewport override |
| Entity positions | Client | Yes — sprite tree enum |
| Wisp collection | Client | Yes — teleport to wisp entity |
| Combat input | Client | Yes — automated spell selection |
| Dialogue / NPC | Client | Yes — automated skip |
| Health / Mana | Server | Read only — FSCommand intercept |
| Gold / Crowns | Server | Read only — display value |
| Level / XP | Server | Read only — wire protocol |
| Membership zones | Server | No — server gate check |
| Inventory / Items | Server | No — server validated |

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

## Intelligence Suite v4

Injectable DLL framework built on top of the offset dump. **16 modules** across two layers: intelligence (monitoring/analysis) and exploitation (game manipulation). Dual-column D3D9 overlay, debug console, persistent config, and full keybind system.

### Architecture

```
src/
├── dllmain.cpp             — DLL entry, hook callbacks, overlay rendering
├── framework.h             — Offsets, Trampoline engine, D3D9Hook, Framework
├── console.h               — Debug console with ANSI color
├── overlay.h               — Raw D3D9 + GDI text overlay renderer
└── modules/
    ├── speed_control.h     — Delta-time manipulation
    ├── packet_sniffer.h    — WinSock IAT hook network monitor
    ├── script_monitor.h    — ActionScript call_method logger
    ├── macro_engine.h      — Input record/replay system
    ├── teleporter.h        — Position read/write + noclip
    ├── memory_scanner.h    — AOB pattern + value scanner
    ├── script_executor.h   — ActionScript method invocation
    ├── entity_radar.h      — Sprite tree entity radar
    ├── camera_control.h    — Viewport zoom/pan/freecam
    ├── anti_afk.h          — Idle prevention input sim
    ├── chat_logger.h       — Chat channel capture + file log
    ├── config_system.h     — INI-style persistent config
    ├── quest_teleport.h    — Quest objective auto-teleport
    ├── wisp_collector.h    — Health/mana wisp auto-collector
    ├── auto_combat.h       — Turn-based combat automation
    ├── auto_dialogue.h     — NPC dialogue auto-skip
    ├── stat_scanner.h      — Stat display + memory freeze
    └── suite.h             — Master integrator (16 modules)
```

---

### Intelligence Layer (Modules 1-12)

#### 1. Speed Control (`speed_control.h`)
Delta-time manipulation engine. Intercepts `root::advance` dt parameter.

- 13 speed presets: 0.1x → 16.0x
- Freeze mode (dt=0), drift tracking, visual speed bar

| Hotkey | Action |
|--------|--------|
| `Num+/-` | Speed up/down |
| `Num*` | Reset to 1.0x |
| `Num/` | Toggle freeze |
| `Num0` | Toggle on/off |
| `Num1-7` | Direct presets |

#### 2. Packet Sniffer (`packet_sniffer.h`)
IAT hooks on `send/recv/WSASend/WSARecv`. Captures headers, tracks rates, hex/ASCII dump, 8000-packet circular buffer.

#### 3. Script Monitor (`script_monitor.h`)
Trampolines `call_method` to log all ActionScript calls. Per-method stats, 17 FSCommand categories, substring filter.

#### 4. Macro Engine (`macro_engine.h`)
Input record/replay with timing preservation. Speed scaling 0.1x-10x, infinite loop, 32 saved macros.

| Hotkey | Action |
|--------|--------|
| `F9` | Record |
| `F10` | Play |
| `F11` | Loop/pause |
| `F12` | Stop |

#### 5. Teleporter (`teleporter.h`)
SWF matrix position read/write. 64 waypoints, smooth interpolation, noclip WASD+Shift, distance odometer.

| Hotkey | Action |
|--------|--------|
| `INSERT` | Save waypoint |
| `HOME` | Teleport to waypoint |
| `PgUp/Dn` | Select waypoint |
| `DELETE` | Delete waypoint |
| `Ctrl+N` | Toggle noclip |
| `Ctrl+T` | Toggle smooth teleport |

#### 6. Memory Scanner (`memory_scanner.h`)
AOB pattern scan with `??` wildcards, iterative value narrowing, SEH-protected reads, VirtualQuery enumeration. NOP patching, WriteValue, 500K candidate cap.

#### 7. Script Executor (`script_executor.h`)
Thread-safe ActionScript method invocation via execution queue processed on game thread. as_value marshaling, FSCommand dispatch, pre-registered quick commands.

#### 8. Entity Radar (`entity_radar.h`)
Recursive sprite tree walk. Mini radar, full radar, list view. Color-coded entities, depth 1-8, distance/name filtering.

| Hotkey | Action |
|--------|--------|
| `Ctrl+R` | Cycle radar mode |

#### 9. Camera Control (`camera_control.h`)
Viewport zoom/pan via `Framework::SetViewport`. Free camera mode, 6 presets (Default, Zoomed, Wide, Ultra Wide, Top Down, Cinematic).

| Hotkey | Action |
|--------|--------|
| `Ctrl+Z/X` | Zoom in/out |
| `Ctrl+F` | Toggle free camera |
| `Ctrl+V` | Cycle presets |
| `Ctrl+C` | Reset camera |

#### 10. Anti-AFK (`anti_afk.h`)
Periodic input simulation cycling through mouse jiggle, shift press, ctrl press, mouse move. 30s-10min interval.

| Hotkey | Action |
|--------|--------|
| `Ctrl+Shift+A` | Toggle anti-AFK |

#### 11. Chat Logger (`chat_logger.h`)
FSCommand chat capture with channel detection (CHAT, WHISPER, TEAM, GUILD, TRADE, etc). File logging with timestamps, channel stats.

#### 12. Config System (`config_system.h`)
INI-style persistent config (`w101_suite.cfg`). Auto-save on ejection. Section.Key format, String/Int/Float/Bool types.

---

### Exploitation Layer (Modules 13-16)

#### 13. Quest Teleport (`quest_teleport.h`)
Intercepts quest-related FSCommands to extract quest objective coordinates. Auto-teleports player to quest objectives with configurable cooldown. Saves position for return trip.

| Hotkey | Action |
|--------|--------|
| `Ctrl+Q` | Teleport to quest objective |
| `Ctrl+Shift+Q` | Toggle auto quest teleport |
| `Ctrl+B` | Return to saved position |

#### 14. Wisp Collector (`wisp_collector.h`)
Entity scanner specialized for health/mana/gold wisps. Recursively walks the sprite tree, classifies entities by name pattern (health/mana/gold), tracks position and distance. Auto-collect mode teleports player to nearest wisp, collects, and returns to original position.

- Classifies wisps: HP (red), MP (blue), Gold (yellow)
- Configurable collection radius (500-50000 units)
- Auto-return to saved position after collection
- Stale wisp pruning (5s timeout)
- SEH-protected entity scanning

| Hotkey | Action |
|--------|--------|
| `Ctrl+W` | Toggle auto wisp collection |
| `Ctrl+Shift+W` | Collect nearest wisp |

#### 15. Auto Combat (`auto_combat.h`)
Turn-based combat automation via simulated input. Detects combat state through FSCommand interception (duel/combat start/end/turn). Selects spells and targets automatically.

- 5 combat strategies:
  - **STRONGEST**: Highest-rank spell first
  - **AOE**: Prioritize area-of-effect spells
  - **HEAL FIRST**: Heal when HP < 33%, otherwise hit
  - **BLADE+HIT**: Stack 2 blades then cast biggest spell
  - **PASS**: Pass every turn (pip building)
- Auto-flee at configurable HP threshold (default 15%)
- Combat statistics: battles, rounds, spells cast, wins, flee count
- Health/pip tracking from FSCommand data
- Combat action log

| Hotkey | Action |
|--------|--------|
| `Ctrl+G` | Toggle auto combat |
| `Ctrl+Shift+G` | Cycle combat strategy |

#### 16. Auto Dialogue (`auto_dialogue.h`)
Automatic NPC dialogue skipper. Detects dialogue open/close via FSCommand interception. Cycles through skip methods (SPACE press, click dialogue box, X key) for reliability.

- Configurable skip delay (100ms-2000ms, default 200ms)
- Stuck dialogue detection with ESC force-close (30s timeout)
- Quest accept/decline detection
- Popup/notification auto-dismiss
- Dialogue history log

| Hotkey | Action |
|--------|--------|
| `Ctrl+D` | Toggle auto dialogue skip |

#### 17. Stat Scanner (`stat_scanner.h`)
Real-time stat display via FSCommand/wire protocol interception. Tracks Health, Mana, Pips, Power Pips, Gold, Energy, Level, XP. Memory scan mode for discovering stat addresses. Value freeze by writing back frozen values every frame.

- Dual mode: FSCommand parsing (primary) + memory scanning (manual)
- Health/Mana/Energy visual bars
- Stat freeze: locks client-side value by continuously writing to memory address
- Memory scan: initial value scan → iterative narrowing → address lock
- SEH-protected memory read/write with VirtualProtect

| Hotkey | Action |
|--------|--------|
| `Ctrl+Shift+H` | Freeze/unfreeze health |
| `Ctrl+Shift+M` | Freeze/unfreeze mana |

---

### Panel System

| Key | Panel |
|-----|-------|
| `F1` | Toggle entire overlay |
| `F2` | Info panel |
| `F3` | Log monitor |
| `F4` | FSCommand monitor |
| `F5-F8` | Speed/Network/Script/Macro |
| `Ctrl+1-9,0` | Panels 1-10 by index |
| `Ctrl+Shift+1-6` | Panels 11-16 by index |
| `END` | Eject DLL |

### Build

**Requirements:**
- Visual Studio 2022 with C++ workload
- Windows SDK (included with VS)
- No legacy DirectX SDK needed (pure D3D9 + GDI rendering)

**Build steps:**
```bash
# Option 1: Use build script
build.bat

# Option 2: Manual CMake
mkdir build && cd build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release --parallel
```

**Output:** `w101suite.dll` — inject into `WizardGraphicalClient.exe` via any standard DLL injector.

**Linked libraries:** d3d9, ws2_32, gdi32, user32, kernel32

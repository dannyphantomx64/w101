# Wizard101 — WizardGraphicalClient.exe Offset Dump

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

- **`WizardGraphicalClient_Dump.h`** — Full C++ header with all exported offsets organized by subsystem, protection analysis, and 9 documented hook/trampoline strategies.

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

1. D3D9 vtable hook
2. Frame tick trampoline (`root::advance`)
3. Input intercept (mouse + keyboard)
4. ActionScript property intercept (vtable swap)
5. Script method call intercept
6. Logging hook (callback registration)
7. File I/O intercept (asset replacement)
8. FSCommand intercept (game command channel)
9. IAT hook (network/API redirection)

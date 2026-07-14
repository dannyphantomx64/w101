// ============================================================================
// WizardGraphicalClient.exe — Full Offset Dump & Hook Analysis
// Binary: x64 PE | Base: 0x140000000 | Size: 0x370C000 (~55MB)
// MD5:    dd71fe8a7d16be3da63da21c1a4fbcb5
// SHA256: e73e3738ab2c15fc3fd613fdf9a1fda0ab90340098073d23227c10afb205f373
// Engine: gameswf (Flash/SWF) + Direct3D 9
// ============================================================================
#pragma once
#include <cstdint>

namespace W101 {

    // Base address — rebase at runtime with GetModuleHandle
    constexpr uintptr_t BASE = 0x140000000;

    // ========================================================================
    // SEGMENT MAP
    // ========================================================================
    namespace Segments {
        constexpr uintptr_t TEXT_START   = 0x140001000;  // .text  (rx)  ~41MB code
        constexpr uintptr_t TEXT_END     = 0x142952000;
        constexpr uintptr_t IDATA_START  = 0x142952000;  // .idata (r)   IAT imports
        constexpr uintptr_t IDATA_END    = 0x1429538C0;
        constexpr uintptr_t RDATA_START  = 0x1429538C0;  // .rdata (r)   vtables, strings, constants
        constexpr uintptr_t RDATA_END    = 0x14327E000;
        constexpr uintptr_t DATA_START   = 0x14327E000;  // .data  (rw)  globals, statics
        constexpr uintptr_t DATA_END     = 0x14345B000;
        constexpr uintptr_t PDATA_START  = 0x14345B000;  // .pdata (r)   exception unwind tables
        constexpr uintptr_t PDATA_END    = 0x143689000;
    }

    // ========================================================================
    // ENTRY POINT
    // ========================================================================
    constexpr uintptr_t EntryPoint = 0x1424565FC;  // CRT start -> WinMain

    // ========================================================================
    // PLAYER — Core game session controller
    // ========================================================================
    namespace Player {
        constexpr uintptr_t Ctor                     = 0x141912310;
        constexpr uintptr_t Dtor                     = 0x141912460;
        constexpr uintptr_t ActionInit               = 0x1419132D0;
        constexpr uintptr_t ActionClear              = 0x141914BE0;
        constexpr uintptr_t CreateMovie              = 0x141912C80;
        constexpr uintptr_t LoadFile                 = 0x1419131B0;
        constexpr uintptr_t GetRoot                  = 0x1419129A0;
        constexpr uintptr_t SetRoot                  = 0x1419129F0;
        constexpr uintptr_t GetRootMovie             = 0x141912AB0;
        constexpr uintptr_t NotifyKeyEvent           = 0x141912B20;
        constexpr uintptr_t SetWorkdir               = 0x141912BF0;
        constexpr uintptr_t GetWorkdir               = 0x141912BD0;
        constexpr uintptr_t SetSeparateThread        = 0x141912C70;
        constexpr uintptr_t UseSeparateThread        = 0x141912C60;
        constexpr uintptr_t SetForceRealtimeFPS      = 0x141915080;
        constexpr uintptr_t GetForceRealtimeFPS      = 0x141915070;
        constexpr uintptr_t SetFlashVars             = 0x141912BC0;
        constexpr uintptr_t VerboseAction            = 0x141912BA0;
        constexpr uintptr_t VerboseParse             = 0x141912BB0;
        constexpr uintptr_t SetLogBitmapInfo         = 0x140490F90;
        constexpr uintptr_t GetLogBitmapInfo         = 0x140490E80;
    }

    // ========================================================================
    // ROOT — Scene graph root, frame advance & display
    // ========================================================================
    namespace Root {
        constexpr uintptr_t Advance                  = 0x141871C00;  // ** PRIME HOOK: called every frame tick
        constexpr uintptr_t Display                  = 0x141872090;  // ** PRIME HOOK: rendering entry
        constexpr uintptr_t GotoFrame                = 0x141872050;
        constexpr uintptr_t NotifyMouseState         = 0x141871450;  // ** HOOK: mouse input intercept
        constexpr uintptr_t SetDisplayViewport       = 0x1418713E0;
        constexpr uintptr_t SetBackgroundColor       = 0x141871B90;
        constexpr uintptr_t SetBackgroundAlpha       = 0x141871BA0;
        constexpr uintptr_t GetCurrentFrame          = 0x141871AE0;
        constexpr uintptr_t GetFrameCount            = 0x140490E60;
        constexpr uintptr_t GetRootMovie             = 0x141871740;
    }

    // ========================================================================
    // SPRITE INSTANCE — Individual display objects (characters, UI, etc.)
    // ========================================================================
    namespace SpriteInstance {
        constexpr uintptr_t GotoFrameInt             = 0x1419569E0;
        constexpr uintptr_t GotoFrameStr             = 0x141956960;
        constexpr uintptr_t GotoLabeledFrame         = 0x141956B70;
        constexpr uintptr_t SetPlayState             = 0x141955790;
        constexpr uintptr_t GetRootMovie             = 0x1418731A0;
    }

    // ========================================================================
    // D3D9 RENDERING — Direct3D 9 handler (IDirect3DDevice9)
    // ========================================================================
    namespace D3D9 {
        constexpr uintptr_t CreateRenderHandler      = 0x1419364F0;  // takes IDirect3DDevice9*
        constexpr uintptr_t ReleaseRenderHandler     = 0x141936550;
        constexpr uintptr_t ResetRenderHandler       = 0x141936530;  // called on device reset
        constexpr uintptr_t SetRenderHandler         = 0x14192F140;
        constexpr uintptr_t GetRenderHandler         = 0x14192F150;  // ** returns render_handler*
    }

    // ========================================================================
    // AS_OBJECT — Base class for ALL game objects (virtual, hookable vtable)
    // ========================================================================
    namespace AsObject {
        constexpr uintptr_t Ctor                     = 0x14190E0D0;
        constexpr uintptr_t Dtor                     = 0x14190E1C0;  // virtual
        constexpr uintptr_t GetMember                = 0x14190E7B0;  // virtual — property read
        constexpr uintptr_t SetMember                = 0x14190E550;  // virtual — property write
        constexpr uintptr_t FindProperty             = 0x14190E860;  // virtual
        constexpr uintptr_t Enumerate                = 0x14190EDE0;  // virtual — iterate properties
        constexpr uintptr_t OnEvent                  = 0x14190EAF0;  // virtual — event dispatch
        constexpr uintptr_t CopyTo                   = 0x14190F7B0;  // virtual
        constexpr uintptr_t ClearRefs                = 0x14190F3F0;  // virtual
        constexpr uintptr_t Dump                     = 0x14190FCC0;  // virtual — debug dump
        constexpr uintptr_t DumpStr                  = 0x14190F8C0;  // virtual
        constexpr uintptr_t GetRoot                  = 0x141910020;  // virtual
        constexpr uintptr_t GetProto                 = 0x14190F0B0;  // virtual — prototype chain
        constexpr uintptr_t FindTarget               = 0x14190FD20;
        constexpr uintptr_t Watch                    = 0x14190F0C0;  // virtual — property watchers
        constexpr uintptr_t Unwatch                  = 0x14190F1F0;  // virtual
        constexpr uintptr_t CallWatcher              = 0x14190E370;
        constexpr uintptr_t BuiltinMember            = 0x14190E2F0;
        constexpr uintptr_t ThisAlive                = 0x14190F640;  // virtual — GC alive check
        constexpr uintptr_t ToNumber                 = 0x14190E2C0;  // virtual
        constexpr uintptr_t ToString                 = 0x140491070;  // virtual
        constexpr uintptr_t ToBool                   = 0x140242BE0;  // virtual
        constexpr uintptr_t Is                       = 0x140490F10;  // virtual — type check
        constexpr uintptr_t GetEnvironment           = 0x140242BF0;  // virtual
        constexpr uintptr_t Advance                  = 0x140242C00;  // virtual — per-frame update
        constexpr uintptr_t Alive                    = 0x140242C00;  // virtual
    }

    // ========================================================================
    // AS_VALUE — Variant type for all ActionScript values
    // ========================================================================
    namespace AsValue {
        constexpr uintptr_t CtorDefault              = 0x14186DE10;
        constexpr uintptr_t CtorCopy                 = 0x14186DE40;
        constexpr uintptr_t CtorInt                  = 0x14186DF90;
        constexpr uintptr_t CtorFloat                = 0x14186E030;
        constexpr uintptr_t CtorDouble               = 0x14186E0D0;
        constexpr uintptr_t CtorBool                 = 0x14186DF60;
        constexpr uintptr_t CtorString               = 0x14186DE80;
        constexpr uintptr_t CtorWString              = 0x14186DF10;
        constexpr uintptr_t CtorObject               = 0x14186E100;
        constexpr uintptr_t CtorCFunction            = 0x14186E170;
        constexpr uintptr_t CtorSFunction            = 0x14186E1B0;
        constexpr uintptr_t SetString                = 0x14186F140;
        constexpr uintptr_t SetTuString              = 0x14186EFD0;
        constexpr uintptr_t SetTuStringNoHtml        = 0x14186F010;
        constexpr uintptr_t SetDouble                = 0x14186F1C0;
        constexpr uintptr_t SetInt                   = 0x140490F80;
        constexpr uintptr_t SetBool                  = 0x14186F200;
        constexpr uintptr_t SetAsObject              = 0x14186F230;
        constexpr uintptr_t SetAsCFunction           = 0x14186F2D0;
        constexpr uintptr_t SetNull                  = 0x140490FB0;
        constexpr uintptr_t SetUndefined             = 0x140490FC0;
        constexpr uintptr_t SetNaN                   = 0x140490FA0;
        constexpr uintptr_t ToStringPtr              = 0x14186E470;
        constexpr uintptr_t ToTuString               = 0x14186E490;
        constexpr uintptr_t ToNumber                 = 0x14186E840;
        constexpr uintptr_t ToBool                   = 0x14186EA60;
        constexpr uintptr_t ToInt                    = 0x140491050;
        constexpr uintptr_t ToFloat                  = 0x140491030;
        constexpr uintptr_t ToObject                 = 0x14186EE20;
        constexpr uintptr_t ToFunction               = 0x14186EC70;
        constexpr uintptr_t ToProperty               = 0x14186EFB0;
        constexpr uintptr_t DropRefs                 = 0x14186E3A0;
        constexpr uintptr_t OperatorAssign           = 0x14186F4D0;
        constexpr uintptr_t OperatorEqual            = 0x14186F6B0;
        constexpr uintptr_t OperatorNotEqual         = 0x14186F900;
    }

    // ========================================================================
    // AS_ENVIRONMENT — Script execution context
    // ========================================================================
    namespace AsEnvironment {
        constexpr uintptr_t Ctor                     = 0x14190AF30;
        constexpr uintptr_t Dtor                     = 0x14190B1E0;
    }

    // ========================================================================
    // AS_ARRAY — Dynamic array for ActionScript
    // ========================================================================
    namespace AsArray {
        constexpr uintptr_t Ctor                     = 0x14195FCB0;
        constexpr uintptr_t Push                     = 0x141960E30;
        constexpr uintptr_t Pop                      = 0x141961C70;
        constexpr uintptr_t Insert                   = 0x141961620;
        constexpr uintptr_t Remove                   = 0x141960FE0;
        constexpr uintptr_t Clear                    = 0x141961D90;
        constexpr uintptr_t Size                     = 0x141962A30;
        constexpr uintptr_t Sort                     = 0x141961FD0;
        constexpr uintptr_t CopyTo                   = 0x141962920;
        constexpr uintptr_t ToString                 = 0x141960860;
        constexpr uintptr_t Erase                    = 0x141961D80;
    }

    // ========================================================================
    // METHOD CALLING — ActionScript method dispatch
    // ========================================================================
    namespace CallMethod {
        constexpr uintptr_t ByValue                  = 0x141920420;
        constexpr uintptr_t ByFunction               = 0x1419204F0;
        constexpr uintptr_t ByString                 = 0x14191FF90;
        constexpr uintptr_t Parsed                   = 0x14191F8C0;
    }

    // ========================================================================
    // LISTENER — Event system
    // ========================================================================
    namespace Listener {
        constexpr uintptr_t Add                      = 0x141927070;
        constexpr uintptr_t Remove                   = 0x1419272A0;
        constexpr uintptr_t NotifyEventId            = 0x141927380;
        constexpr uintptr_t NotifyString             = 0x141927460;
        constexpr uintptr_t Advance                  = 0x1419278A0;
        constexpr uintptr_t Enumerate                = 0x141927B50;
        constexpr uintptr_t Size                     = 0x141927AB0;
        constexpr uintptr_t Clear                    = 0x140490D10;
    }

    // ========================================================================
    // EXTENDED TYPES (as_point, as_rectangle, as_transform, etc.)
    // ========================================================================
    namespace AsPoint {
        constexpr uintptr_t GetMember                = 0x1419861B0;
        constexpr uintptr_t SetMember                = 0x1419860E0;
    }
    namespace AsRectangle {
        constexpr uintptr_t GetMember                = 0x1419876B0;
        constexpr uintptr_t SetMember                = 0x1419873A0;
    }
    namespace AsTransform {
        constexpr uintptr_t GetMember                = 0x14198A920;
        constexpr uintptr_t SetMember                = 0x14198A800;
    }
    namespace AsColorTransform {
        constexpr uintptr_t GetMember                = 0x141989EC0;
        constexpr uintptr_t SetMember                = 0x141989B90;
    }
    namespace AsBitmapData {
        constexpr uintptr_t GetMember                = 0x14198B830;
    }
    namespace AsClass {
        constexpr uintptr_t FindProperty             = 0x141982150;
    }

    // ========================================================================
    // FILE I/O
    // ========================================================================
    namespace FileIO {
        constexpr uintptr_t TuFileCtor_FILE          = 0x14193B5C0;
        constexpr uintptr_t TuFileCtor_Path          = 0x14193B630;
        constexpr uintptr_t TuFileCtor_MemBuf        = 0x14193B6E0;
        constexpr uintptr_t TuFileDtor               = 0x14193B810;
        constexpr uintptr_t ReadByte                 = 0x141903370;
        constexpr uintptr_t ReadBytes                = 0x1419033A0;
        constexpr uintptr_t ReadFloat32              = 0x1419033F0;
        constexpr uintptr_t ReadDouble64             = 0x1419033C0;
        constexpr uintptr_t ReadString               = 0x14193BA50;
        constexpr uintptr_t WriteByte                = 0x1419035F0;
        constexpr uintptr_t WriteBytes               = 0x141903610;
        constexpr uintptr_t WriteFloat32             = 0x141903660;
        constexpr uintptr_t WriteDouble64            = 0x141903630;
        constexpr uintptr_t WriteString              = 0x14193B9F0;
        constexpr uintptr_t GetPosition              = 0x141902FF0;
        constexpr uintptr_t SetPosition              = 0x141903420;
        constexpr uintptr_t GetSize                  = 0x141903490;
        constexpr uintptr_t GetEOF                   = 0x141902FD0;
        constexpr uintptr_t GetError                 = 0x141902FE0;
        constexpr uintptr_t GoToEnd                  = 0x141903030;
        constexpr uintptr_t CopyFrom                 = 0x14193B850;
        constexpr uintptr_t CopyToMembuf             = 0x14193B8C0;
        constexpr uintptr_t CopyBytes                = 0x14193B950;
        constexpr uintptr_t ReadJPEG_File            = 0x14193C930;
        constexpr uintptr_t ReadJPEG_Path            = 0x14193C8E0;
    }

    // ========================================================================
    // THREADING & SYNCHRONIZATION
    // ========================================================================
    namespace Threading {
        // NOTE: lock/unlock/signal/wait all resolve to 0x140242C00 (ret; NOP stub)
        // The gameswf layer has MINIMAL thread sync — easy to hook without races
        constexpr uintptr_t NOP_STUB                 = 0x140242C00;
        constexpr uintptr_t MutexCtor                = 0x1402972C0;
        constexpr uintptr_t MutexLock                = 0x140242C00;  // NOP
        constexpr uintptr_t MutexUnlock              = 0x140242C00;  // NOP
        constexpr uintptr_t ConditionCtor            = 0x1402972C0;
        constexpr uintptr_t ConditionSignal          = 0x140242C00;  // NOP
        constexpr uintptr_t ConditionWait            = 0x140242C00;  // NOP
        constexpr uintptr_t ThreadCtor               = 0x141870400;
        constexpr uintptr_t ThreadDtor               = 0x141870590;
        constexpr uintptr_t AutolockCtor             = 0x140490250;
        constexpr uintptr_t AutolockDtor             = 0x1404902F0;
        constexpr uintptr_t EngineMutex              = 0x141870A10;  // returns tu_mutex&
    }

    // ========================================================================
    // MATRIX / CXFORM — Spatial transforms & color transforms
    // ========================================================================
    namespace Matrix {
        constexpr uintptr_t Ctor                     = 0x14191DAA0;
        constexpr uintptr_t SetIdentity              = 0x14191DAD0;
        constexpr uintptr_t Concatenate              = 0x14191DAF0;
        constexpr uintptr_t ConcatenateTranslation   = 0x14191DC50;
        constexpr uintptr_t ConcatenateScale         = 0x14191DCC0;
        constexpr uintptr_t SetLerp                  = 0x14191DD40;
        constexpr uintptr_t SetScaleRotation         = 0x14191DE60;
        constexpr uintptr_t Read                     = 0x14191DF50;
        constexpr uintptr_t TransformPoint           = 0x14191E1A0;
        constexpr uintptr_t TransformRect            = 0x14191E1F0;
        constexpr uintptr_t TransformVector          = 0x14191E3A0;
        constexpr uintptr_t TransformByInversePoint  = 0x14191E3F0;
        constexpr uintptr_t TransformByInverseRect   = 0x14191E4A0;
        constexpr uintptr_t SetInverse               = 0x14191E510;
        constexpr uintptr_t DoesFlip                 = 0x14191E6A0;
        constexpr uintptr_t GetDeterminant           = 0x14191E6D0;
        constexpr uintptr_t GetMaxScale              = 0x14191E6F0;
        constexpr uintptr_t GetXScale                = 0x14191E740;
        constexpr uintptr_t GetYScale                = 0x14191E7C0;
        constexpr uintptr_t GetRotation              = 0x14191E800;
        constexpr uintptr_t Print                    = 0x14191E110;
    }
    namespace CxForm {
        constexpr uintptr_t Ctor                     = 0x14191EE60;
        constexpr uintptr_t Concatenate              = 0x14191EE90;
        constexpr uintptr_t Clamp                    = 0x14191F470;
        constexpr uintptr_t TransformRGBA            = 0x14191EFA0;
        constexpr uintptr_t ReadRGB                  = 0x14191F060;
        constexpr uintptr_t ReadRGBA                 = 0x14191F240;
        constexpr uintptr_t Print                    = 0x14191F5D0;
    }

    // ========================================================================
    // TIMER / DATETIME
    // ========================================================================
    namespace Timer {
        constexpr uintptr_t Init                     = 0x14195C7E0;
        constexpr uintptr_t GetTicks                 = 0x14195C800;
        constexpr uintptr_t Sleep                    = 0x14195C860;
        constexpr uintptr_t GetProfileTicks          = 0x14195C870;
        constexpr uintptr_t ProfileTicksToSeconds    = 0x14195C890;
    }
    namespace DateTime {
        constexpr uintptr_t Ctor                     = 0x14195C920;
        constexpr uintptr_t Get                      = 0x14195C990;
        constexpr uintptr_t Set                      = 0x14195CA40;
        constexpr uintptr_t GetTime                  = 0x14195C950;
        constexpr uintptr_t SetTime                  = 0x14195C970;
    }

    // ========================================================================
    // RANDOM
    // ========================================================================
    namespace Random {
        constexpr uintptr_t Seed                     = 0x14192F830;
        constexpr uintptr_t Next                     = 0x14192F7D0;
        constexpr uintptr_t GetUnitFloat             = 0x14192F850;
    }

    // ========================================================================
    // SOUND
    // ========================================================================
    namespace Sound {
        constexpr uintptr_t SetHandler               = 0x141935500;
        constexpr uintptr_t GetHandler               = 0x141935510;
    }

    // ========================================================================
    // LOGGING & CALLBACKS
    // ========================================================================
    namespace Logging {
        constexpr uintptr_t LogMsg                   = 0x14191A500;
        constexpr uintptr_t LogError                 = 0x14191A580;
        constexpr uintptr_t RegisterLogCallback      = 0x14191A4F0;
        constexpr uintptr_t RegisterFileOpener       = 0x141911E00;
        constexpr uintptr_t RegisterFsCommand        = 0x141911E30;
    }

    // ========================================================================
    // DYNAMIC LIBRARY
    // ========================================================================
    namespace DynLib {
        constexpr uintptr_t LoadLibCtor              = 0x14195CAF0;
        constexpr uintptr_t LoadLibDtor              = 0x14195CC10;
        constexpr uintptr_t GetFunction              = 0x14195CC30;
    }

    // ========================================================================
    // FONT / GLYPH
    // ========================================================================
    namespace Font {
        constexpr uintptr_t CreateFreetype           = 0x141939E30;
        constexpr uintptr_t SetGlyphProvider         = 0x141911E60;
        constexpr uintptr_t GetGlyphProvider         = 0x141911E50;
        constexpr uintptr_t SetDefaultFont           = 0x1419047D0;
        constexpr uintptr_t AddFontMapping           = 0x1419043D0;
    }

    // ========================================================================
    // GLOBAL SETTINGS / CONFIG
    // ========================================================================
    namespace Config {
        constexpr uintptr_t SetVerboseAction         = 0x1419042C0;
        constexpr uintptr_t SetVerboseParse          = 0x1419042D0;
        constexpr uintptr_t GetVerboseAction         = 0x1419042B0;
        constexpr uintptr_t GetVerboseParse          = 0x141904290;
        constexpr uintptr_t GetVerboseDebug          = 0x1419042A0;
        constexpr uintptr_t SetAntialiasProps         = 0x1419042E0;
        constexpr uintptr_t GetAntialiasShapes       = 0x141904330;
        constexpr uintptr_t GetAntialiasLines        = 0x141904340;
        constexpr uintptr_t GetAntialiasAmount       = 0x141904350;
        constexpr uintptr_t SetRenderWireframe       = 0x141904390;
        constexpr uintptr_t GetRenderWireframe       = 0x141904380;
        constexpr uintptr_t SetDrawInteriors         = 0x141904370;
        constexpr uintptr_t GetDrawInteriors         = 0x141904360;
        constexpr uintptr_t SetCurveMaxPixelError    = 0x141949B10;
        constexpr uintptr_t GetCurveMaxPixelError    = 0x141949B40;
        constexpr uintptr_t SetUseCacheFiles         = 0x141911E40;
        constexpr uintptr_t GetPolyCount             = 0x1419043C0;
        constexpr uintptr_t GetFrameCount            = 0x1419043A0;
        constexpr uintptr_t IncrementFrameCount      = 0x1419043B0;
    }

    // ========================================================================
    // STRING UTILITIES
    // ========================================================================
    namespace TuString {
        constexpr uintptr_t OperatorAssignStr        = 0x140490330;
        constexpr uintptr_t OperatorAssignCStr       = 0x1404903B0;
        constexpr uintptr_t OperatorEqualStr         = 0x140490420;
        constexpr uintptr_t OperatorEqualCStr        = 0x1404904A0;
        constexpr uintptr_t OperatorNotEqualStr      = 0x1404904D0;
        constexpr uintptr_t OperatorNotEqualCStr     = 0x1404904E0;
        constexpr uintptr_t OperatorPlus             = 0x1404907A0;
        constexpr uintptr_t OperatorPlusCStr         = 0x1404907F0;
        constexpr uintptr_t OperatorAppendStr        = 0x1404909E0;
        constexpr uintptr_t OperatorAppendChar       = 0x140490A90;
        constexpr uintptr_t OperatorAppendCStr       = 0x140490B00;
        constexpr uintptr_t CStr                     = 0x140490790;
        constexpr uintptr_t Length                   = 0x140490F20;
        constexpr uintptr_t Clear                    = 0x140490DC0;
        constexpr uintptr_t Erase                    = 0x140490DE0;
        constexpr uintptr_t Insert                   = 0x140490E90;
        constexpr uintptr_t Resize                   = 0x14186CC10;
        constexpr uintptr_t Stricmp                  = 0x14186CEE0;
        constexpr uintptr_t Utf8CharAt               = 0x14186CEF0;
        constexpr uintptr_t Utf8Length               = 0x140491090;
        constexpr uintptr_t Utf8CharCount            = 0x14186D400;
        constexpr uintptr_t Utf8Substring            = 0x14186D460;
        constexpr uintptr_t Utf8ToUpper              = 0x14186CF40;
        constexpr uintptr_t Utf8ToLower              = 0x14186D1A0;
        constexpr uintptr_t AppendWideChar_G         = 0x14186CA50;
        constexpr uintptr_t AppendWideChar_I         = 0x14186CB30;
        constexpr uintptr_t EncodeUtf8FromWchar_G    = 0x14186CE10;
        constexpr uintptr_t EncodeUtf8FromWchar_I    = 0x14186CD50;
    }

    // ========================================================================
    // MEMBUF — Memory buffer operations
    // ========================================================================
    namespace Membuf {
        constexpr uintptr_t CtorDefault              = 0x141975050;
        constexpr uintptr_t CtorFromData             = 0x141975070;
        constexpr uintptr_t CtorCopy                 = 0x1419750F0;
        constexpr uintptr_t CtorFromString           = 0x141975170;
        constexpr uintptr_t Dtor                     = 0x141975200;
        constexpr uintptr_t Resize                   = 0x141975290;
        constexpr uintptr_t AppendData               = 0x141975300;
        constexpr uintptr_t AppendByte               = 0x1419753C0;
        constexpr uintptr_t AppendMembuf             = 0x141975360;
        constexpr uintptr_t AppendString             = 0x141975400;
        constexpr uintptr_t Data                     = 0x140490DD0;
        constexpr uintptr_t Size                     = 0x140491020;
    }

    // ========================================================================
    // VM STACK — ActionScript virtual machine stack
    // ========================================================================
    namespace VMStack {
        constexpr uintptr_t Drop                     = 0x14190AAB0;
    }

    // ========================================================================
    // URL HANDLING
    // ========================================================================
    namespace URL {
        constexpr uintptr_t GetFullURL               = 0x14190A8B0;
    }

    // ========================================================================
    // STRING TO NUMBER CONVERSION
    // ========================================================================
    namespace Conversion {
        constexpr uintptr_t StringToInt              = 0x14186D640;
        constexpr uintptr_t StringToDouble           = 0x14186D6A0;
    }

} // namespace W101


// ============================================================================
// PROTECTION ANALYSIS
// ============================================================================
//
// VERDICT: MINIMAL PROTECTION — This binary is wide open.
//
// 1. NO ANTI-CHEAT ENGINE
//    - No EasyAntiCheat, BattlEye, Vanguard, or nProtect exports/imports detected
//    - No kernel-level driver references in exports
//    - No integrity check functions visible in the export table
//
// 2. NOP MUTEX STUBS
//    - tu_mutex::lock, tu_mutex::unlock, tu_condition::signal,
//      tu_condition::wait, tu_thread::kill, tu_thread::wait
//      ALL point to 0x140242C00 — a single `ret` instruction
//    - The gameswf layer has ZERO actual thread synchronization
//    - This means you can hook freely without worrying about deadlocks
//
// 3. NO CODE SIGNING VERIFICATION
//    - No Authenticode verification functions in exports
//    - No self-integrity CRC/hash checking visible
//
// 4. NO ANTI-DEBUG
//    - No IsDebuggerPresent, CheckRemoteDebuggerPresent, NtQueryInformationProcess
//      visible in the gameswf export layer
//    - Standard .pdata exception handling (no SEH-based anti-debug tricks in exports)
//
// 5. FREELY EXPORTED INTERNALS
//    - 343+ gameswf functions exported by ordinal
//    - Full access to: player, root, sprite_instance, as_object, as_value,
//      as_environment, rendering, input, file I/O, scripting engine
//    - Virtual function tables accessible through exported class hierarchies
//
// 6. POTENTIAL SERVER-SIDE CHECKS
//    - While the CLIENT has minimal protection, the game server likely validates:
//      * Position/movement packets
//      * Spell casting sequences
//      * Quest state transitions
//      * Currency/inventory changes
//    - Any modifications that affect gameplay state will need to consider
//      what the server accepts vs rejects
//
// ============================================================================


// ============================================================================
// TRAMPOLINE / HOOK STRATEGY
// ============================================================================
//
// ┌─────────────────────────────────────────────────────────────────────┐
// │ METHOD 1: D3D9 VTABLE HOOK (Rendering Overlay)                    │
// ├─────────────────────────────────────────────────────────────────────┤
// │                                                                     │
// │ create_render_handler_d3d(IDirect3DDevice9*) @ 0x1419364F0         │
// │ accepts the raw D3D9 device pointer.                                │
// │                                                                     │
// │ APPROACH:                                                           │
// │ 1. Hook create_render_handler_d3d to capture IDirect3DDevice9*      │
// │ 2. Read the vtable: *(void***)pDevice                               │
// │ 3. Swap vtable entries:                                             │
// │    - EndScene   (index 42) — draw overlays after scene renders      │
// │    - Present    (index 17) — final frame output                     │
// │    - Reset      (index 16) — handle device lost/recreation          │
// │    - DrawIndexedPrimitive (index 82) — selective render control     │
// │ 4. OR hook reset_render_handler_d3d @ 0x141936530 to catch resets  │
// │                                                                     │
// │ USE: ESP overlay, wallhack, custom UI rendering                     │
// └─────────────────────────────────────────────────────────────────────┘
//
// ┌─────────────────────────────────────────────────────────────────────┐
// │ METHOD 2: FRAME TICK TRAMPOLINE (Per-Frame Logic)                  │
// ├─────────────────────────────────────────────────────────────────────┤
// │                                                                     │
// │ root::advance(float delta) @ 0x141871C00                            │
// │ Called EVERY FRAME with the time delta.                              │
// │                                                                     │
// │ TRAMPOLINE SETUP:                                                   │
// │ 1. Save first 14+ bytes of original function                        │
// │ 2. Write JMP to your hook function                                  │
// │ 3. In hook: execute your per-frame code, then JMP to trampoline     │
// │    that runs saved bytes + JMP back to original+14                   │
// │                                                                     │
// │ void __fastcall hk_Advance(root* thisptr, float dt) {               │
// │     // your per-frame logic here                                    │
// │     // read game state, update overlays, etc.                       │
// │     return oAdvance(thisptr, dt);                                   │
// │ }                                                                   │
// │                                                                     │
// │ USE: game state polling, entity tracking, speedhack (modify dt)     │
// └─────────────────────────────────────────────────────────────────────┘
//
// ┌─────────────────────────────────────────────────────────────────────┐
// │ METHOD 3: INPUT INTERCEPT (Mouse + Keyboard)                      │
// ├─────────────────────────────────────────────────────────────────────┤
// │                                                                     │
// │ root::notify_mouse_state(int x, int y, int buttons, int wheel)     │
// │   @ 0x141871450                                                     │
// │                                                                     │
// │ player::notify_key_event(unsigned short key, bool down)             │
// │   @ 0x141912B20                                                     │
// │                                                                     │
// │ APPROACH:                                                           │
// │ - Trampoline both functions                                         │
// │ - Intercept input before the game processes it                      │
// │ - Inject synthetic input (auto-click, keybinds, macros)             │
// │ - Block input when overlay is active                                │
// │                                                                     │
// │ USE: custom keybinds, aim assist, input automation                  │
// └─────────────────────────────────────────────────────────────────────┘
//
// ┌─────────────────────────────────────────────────────────────────────┐
// │ METHOD 4: ACTIONSCRIPT PROPERTY INTERCEPT                          │
// ├─────────────────────────────────────────────────────────────────────┤
// │                                                                     │
// │ as_object::get_member @ 0x14190E7B0  (VIRTUAL, index varies)       │
// │ as_object::set_member @ 0x14190E550  (VIRTUAL, index varies)       │
// │                                                                     │
// │ These are virtual functions — hook via vtable swap:                  │
// │ 1. Find the as_object vtable in .rdata (0x1429538C0-0x14327E000)   │
// │ 2. Locate get_member/set_member entries                             │
// │ 3. Replace with your hook function pointers                         │
// │                                                                     │
// │ This intercepts ALL property reads/writes on game objects.           │
// │                                                                     │
// │ USE: read health, mana, position, quest state, inventory            │
// │      modify values before they reach the UI                         │
// └─────────────────────────────────────────────────────────────────────┘
//
// ┌─────────────────────────────────────────────────────────────────────┐
// │ METHOD 5: SCRIPT METHOD CALL INTERCEPT                             │
// ├─────────────────────────────────────────────────────────────────────┤
// │                                                                     │
// │ call_method (by value)    @ 0x141920420                             │
// │ call_method (by function) @ 0x1419204F0                             │
// │ call_method (by string)   @ 0x14191FF90                             │
// │ call_method_parsed        @ 0x14191F8C0                             │
// │                                                                     │
// │ Trampoline any of these to intercept ALL ActionScript method calls. │
// │ The string variant gives you the method name as a readable string.  │
// │                                                                     │
// │ USE: log all game function calls, intercept specific game commands, │
// │      inject custom ActionScript behavior                            │
// └─────────────────────────────────────────────────────────────────────┘
//
// ┌─────────────────────────────────────────────────────────────────────┐
// │ METHOD 6: LOGGING HOOK (Passive Monitoring)                        │
// ├─────────────────────────────────────────────────────────────────────┤
// │                                                                     │
// │ register_log_callback @ 0x14191A4F0                                 │
// │   void (*callback)(bool is_error, const char* msg)                  │
// │                                                                     │
// │ NO TRAMPOLINE NEEDED — just call this exported function with your   │
// │ callback. The game will forward all log_msg/log_error calls to you. │
// │                                                                     │
// │ USE: passive monitoring, reverse engineering game state changes      │
// └─────────────────────────────────────────────────────────────────────┘
//
// ┌─────────────────────────────────────────────────────────────────────┐
// │ METHOD 7: FILE I/O INTERCEPT                                       │
// ├─────────────────────────────────────────────────────────────────────┤
// │                                                                     │
// │ register_file_opener_callback @ 0x141911E00                         │
// │   tu_file* (*callback)(const char* path)                            │
// │                                                                     │
// │ Redirect file loading — serve modified SWF/assets at runtime.       │
// │                                                                     │
// │ USE: asset replacement, modified UI, custom spell effects           │
// └─────────────────────────────────────────────────────────────────────┘
//
// ┌─────────────────────────────────────────────────────────────────────┐
// │ METHOD 8: FSCOMMAND INTERCEPT                                      │
// ├─────────────────────────────────────────────────────────────────────┤
// │                                                                     │
// │ register_fscommand_callback @ 0x141911E30                           │
// │   void (*callback)(character*, const char* cmd, const char* args)   │
// │                                                                     │
// │ FSCommand is how Flash communicates with the host application.      │
// │ W101 uses this for game commands passing between SWF UI and the     │
// │ native C++ engine.                                                  │
// │                                                                     │
// │ USE: intercept game commands, inject custom commands                │
// └─────────────────────────────────────────────────────────────────────┘
//
// ┌─────────────────────────────────────────────────────────────────────┐
// │ METHOD 9: IAT HOOK (Import Address Table)                          │
// ├─────────────────────────────────────────────────────────────────────┤
// │                                                                     │
// │ .idata @ 0x142952000 — 0x1429538C0 (primary)                       │
// │ .idata @ 0x14331EDD8 — 0x14331EE90 (secondary, rw)                │
// │                                                                     │
// │ Patch import table entries for Windows API functions:                │
// │ - send/recv/WSASend/WSARecv for packet interception                 │
// │ - CreateFileW for asset loading                                     │
// │ - Direct3DCreate9 for early device capture                          │
// │                                                                     │
// │ USE: network packet sniffing/modification, API redirection          │
// └─────────────────────────────────────────────────────────────────────┘
//
// ============================================================================
// RECOMMENDED INJECTION METHOD
// ============================================================================
//
// DLL INJECTION via:
// 1. LoadLibrary injection (CreateRemoteThread + LoadLibraryA)
// 2. Manual mapping (for stealth — no LoadLibrary call in module list)
// 3. SetWindowsHookEx (if targeting specific window)
//
// On DllMain attach:
//   - Resolve module base: GetModuleHandle("WizardGraphicalClient.exe")
//   - Calculate runtime offsets: export_addr - 0x140000000 + actual_base
//   - Install MinHook/Detours trampolines on target functions
//   - Register callbacks via exported registration functions
//
// Since there's NO anti-cheat, standard LoadLibrary injection works fine.
// No need for manual mapping or evasion techniques.
//
// ============================================================================

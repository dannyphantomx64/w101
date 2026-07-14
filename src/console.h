#pragma once
#include <Windows.h>
#include <cstdio>
#include <string>

namespace W101Hook {

    class Console {
    private:
        static inline FILE* conOut = nullptr;
        static inline FILE* conIn  = nullptr;
        static inline bool  active = false;

    public:
        static bool Open(const char* title = "W101 Framework") {
            if (active) return true;

            AllocConsole();
            SetConsoleTitleA(title);

            freopen_s(&conOut, "CONOUT$", "w", stdout);
            freopen_s(&conIn,  "CONIN$",  "r", stdin);

            HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
            DWORD mode = 0;
            GetConsoleMode(hOut, &mode);
            SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

            active = true;
            return true;
        }

        static void Close() {
            if (!active) return;
            if (conOut) fclose(conOut);
            if (conIn)  fclose(conIn);
            FreeConsole();
            active = false;
        }

        static void Print(const char* fmt, ...) {
            if (!active) return;
            va_list args;
            va_start(args, fmt);
            vprintf(fmt, args);
            va_end(args);
        }

        static void Info(const char* fmt, ...) {
            if (!active) return;
            printf("\033[36m[INFO]\033[0m ");
            va_list args;
            va_start(args, fmt);
            vprintf(fmt, args);
            va_end(args);
            printf("\n");
        }

        static void Success(const char* fmt, ...) {
            if (!active) return;
            printf("\033[32m[OK]\033[0m ");
            va_list args;
            va_start(args, fmt);
            vprintf(fmt, args);
            va_end(args);
            printf("\n");
        }

        static void Warn(const char* fmt, ...) {
            if (!active) return;
            printf("\033[33m[WARN]\033[0m ");
            va_list args;
            va_start(args, fmt);
            vprintf(fmt, args);
            va_end(args);
            printf("\n");
        }

        static void Error(const char* fmt, ...) {
            if (!active) return;
            printf("\033[31m[ERR]\033[0m ");
            va_list args;
            va_start(args, fmt);
            vprintf(fmt, args);
            va_end(args);
            printf("\n");
        }

        static void Header() {
            if (!active) return;
            printf("\033[35m");
            printf("================================================\n");
            printf("  W101 Hook Framework\n");
            printf("  WizardGraphicalClient.exe | x64 | D3D9\n");
            printf("================================================\n");
            printf("\033[0m\n");
        }

        static void Separator() {
            if (!active) return;
            printf("\033[90m------------------------------------------------\033[0m\n");
        }

        static bool IsActive() { return active; }
    };

} // namespace W101Hook

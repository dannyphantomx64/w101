#include <Windows.h>
#include <TlHelp32.h>
#include <stdio.h>
#include <string.h>

static DWORD FindProcess(const char* name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32 pe = {};
    pe.dwSize = sizeof(pe);

    DWORD pid = 0;
    if (Process32First(snap, &pe)) {
        do {
            if (_stricmp(pe.szExeFile, name) == 0) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32Next(snap, &pe));
    }

    CloseHandle(snap);
    return pid;
}

static bool InjectDLL(DWORD pid, const char* dllPath) {
    HANDLE hProc = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
        PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
        FALSE, pid);

    if (!hProc) {
        printf("[!] OpenProcess failed (error %lu)\n", GetLastError());
        printf("[!] Try running as Administrator\n");
        return false;
    }

    size_t pathLen = strlen(dllPath) + 1;

    LPVOID remoteMem = VirtualAllocEx(hProc, NULL, pathLen,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (!remoteMem) {
        printf("[!] VirtualAllocEx failed (error %lu)\n", GetLastError());
        CloseHandle(hProc);
        return false;
    }

    if (!WriteProcessMemory(hProc, remoteMem, dllPath, pathLen, NULL)) {
        printf("[!] WriteProcessMemory failed (error %lu)\n", GetLastError());
        VirtualFreeEx(hProc, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return false;
    }

    HMODULE hKernel = GetModuleHandleA("kernel32.dll");
    if (!hKernel) {
        printf("[!] Failed to get kernel32 handle\n");
        VirtualFreeEx(hProc, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return false;
    }

    FARPROC pLoadLib = GetProcAddress(hKernel, "LoadLibraryA");
    if (!pLoadLib) {
        printf("[!] Failed to get LoadLibraryA address\n");
        VirtualFreeEx(hProc, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return false;
    }

    HANDLE hThread = CreateRemoteThread(hProc, NULL, 0,
        (LPTHREAD_START_ROUTINE)pLoadLib, remoteMem, 0, NULL);

    if (!hThread) {
        printf("[!] CreateRemoteThread failed (error %lu)\n", GetLastError());
        printf("[!] This can happen if the game has anti-cheat protection\n");
        VirtualFreeEx(hProc, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return false;
    }

    printf("[+] Remote thread created, waiting for DLL load...\n");
    WaitForSingleObject(hThread, 10000);

    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);

    CloseHandle(hThread);
    VirtualFreeEx(hProc, remoteMem, 0, MEM_RELEASE);
    CloseHandle(hProc);

    if (exitCode == 0) {
        printf("[!] LoadLibrary returned NULL — DLL may have failed to load\n");
        printf("[!] Check that the DLL path is correct and dependencies are met\n");
        return false;
    }

    printf("[+] DLL loaded at 0x%08lX in target process\n", exitCode);
    return true;
}

static void GetFullDLLPath(const char* relative, char* out, size_t outSize) {
    if (relative[0] && (relative[1] == ':' || (relative[0] == '\\' && relative[1] == '\\'))) {
        strncpy(out, relative, outSize - 1);
        out[outSize - 1] = '\0';
        return;
    }
    GetCurrentDirectoryA((DWORD)outSize, out);
    size_t len = strlen(out);
    if (len > 0 && out[len - 1] != '\\') {
        out[len] = '\\';
        out[len + 1] = '\0';
    }
    strncat(out, relative, outSize - strlen(out) - 1);
}

int main(int argc, char* argv[]) {
    printf("================================================\n");
    printf(" W101 Suite Injector\n");
    printf("================================================\n\n");

    const char* targetProc = "WizardGraphicalClient.exe";
    const char* dllName    = "w101suite.dll";

    if (argc > 1) dllName = argv[1];
    if (argc > 2) targetProc = argv[2];

    char fullPath[MAX_PATH] = {};
    GetFullDLLPath(dllName, fullPath, MAX_PATH);

    DWORD attrib = GetFileAttributesA(fullPath);
    if (attrib == INVALID_FILE_ATTRIBUTES) {
        printf("[!] DLL not found: %s\n", fullPath);
        printf("[!] Place w101suite.dll next to this injector, or pass the path as arg1\n");
        printf("\n    Usage: injector.exe [dll_path] [process_name]\n");
        return 1;
    }

    printf("[*] DLL:     %s\n", fullPath);
    printf("[*] Target:  %s\n", targetProc);
    printf("\n");

    printf("[*] Searching for %s...\n", targetProc);

    DWORD pid = FindProcess(targetProc);

    if (!pid) {
        printf("[*] Process not running. Waiting for launch...\n");
        printf("[*] Start the game now. Press Ctrl+C to cancel.\n\n");

        int dots = 0;
        while (!pid) {
            Sleep(1000);
            pid = FindProcess(targetProc);
            dots++;
            if (dots % 5 == 0) {
                printf("[*] Still waiting... (%d seconds)\n", dots);
            }
        }
        printf("\n");
    }

    printf("[+] Found %s (PID: %lu)\n", targetProc, pid);

    printf("[*] Waiting 3 seconds for process initialization...\n");
    Sleep(3000);

    printf("[*] Injecting...\n");
    if (InjectDLL(pid, fullPath)) {
        printf("\n[+] Injection successful\n");
        printf("[+] Check the game for the overlay (press INSERT to toggle)\n");
    } else {
        printf("\n[!] Injection failed\n");
        printf("[!] Make sure you're running as Administrator\n");
        return 1;
    }

    printf("\nPress Enter to exit...\n");
    getchar();
    return 0;
}

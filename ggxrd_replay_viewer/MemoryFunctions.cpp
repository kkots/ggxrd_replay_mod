#include "MemoryFunctions.h"
#include "WinError.h"

// Finds module (a loaded dll or exe itself) in the given process by name.
// If module is not found, the returned module will have its modBaseAddr equal to 0.
// Parameters:
//  procId - process ID (PID)
//  name - the name of the module, including the .exe or .dll at the end
//  is32Bit - specify true if the target process is 32-bit
MODULEENTRY32 findModule(DWORD procId, LPCTSTR name, bool is32Bit) {
    HANDLE hSnapshot = NULL;
    MODULEENTRY32 mod32{ 0 };
    mod32.dwSize = sizeof(MODULEENTRY32);
    while (true) {
        // If you're a 64-bit process trying to get modules from a 32-bit process,
        // use TH32CS_SNAPMODULE32.
        // If you're a 64-bit process trying to get modules from a 64-bit process,
        // use TH32CS_SNAPMODULE.
        hSnapshot = CreateToolhelp32Snapshot(is32Bit ? (TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32) : TH32CS_SNAPMODULE, procId);
        if (hSnapshot == INVALID_HANDLE_VALUE || !hSnapshot) {
            WinError err;
            if (err.code == ERROR_BAD_LENGTH) {
                continue;
            }
            else {
                OutputDebugStringA("Error in CreateToolhelp32Snapshot: ");
                OutputDebugString(err.getMessage());
                OutputDebugStringA("\n");
                return MODULEENTRY32{ 0 };
            }
        }
        else {
            break;
        }
    }
    if (!Module32First(hSnapshot, &mod32)) {
        WinError winErr;
        OutputDebugStringA("Error in Module32First: ");
        OutputDebugString(winErr.getMessage());
        OutputDebugStringA("\n");
        CloseHandle(hSnapshot);
        return MODULEENTRY32{ 0 };
    }
    while (true) {
        if (CompareString(LOCALE_INVARIANT, NORM_IGNORECASE, mod32.szModule, -1, name, -1) == CSTR_EQUAL) {
            CloseHandle(hSnapshot);
            return mod32;
        }
        BOOL resNext = Module32Next(hSnapshot, &mod32);
        if (!resNext) {
            WinError err;
            if (err.code != ERROR_NO_MORE_FILES) {
                OutputDebugStringA("Error in Module32Next: ");
                OutputDebugString(err.getMessage());
                OutputDebugStringA("\n");
                CloseHandle(hSnapshot);
                return MODULEENTRY32{ 0 };
            }
            break;
        }
    }
    CloseHandle(hSnapshot);
    return MODULEENTRY32{ 0 };
}

bool readWholeModule(HANDLE processHandle, LPCVOID modBaseAddr, DWORD modBaseSize, std::vector<char>& imageBytes) {
    imageBytes.resize(modBaseSize);
    if (!ReadProcessMemory(processHandle, (LPCVOID)modBaseAddr, (LPVOID)&imageBytes.front(), modBaseSize, NULL)) {
        WinError winErr;
        OutputDebugStringA("Failed to read module image: ");
        OutputDebugString(winErr.getMessage());
        OutputDebugStringA("\n");
        imageBytes.clear();
        return false;
    }
    return true;
}

char* sigscan(std::vector<char>& wholeModule, const char* sig, const char* mask) {
    return sigscan(&wholeModule.front(), &wholeModule.back(), sig, mask);
}

char* sigscan(char* start, char* end, const char* sig, const char* mask) {
    char* const last_scan = end - strlen(mask) + 1;
    for (char* addr = start; addr < last_scan; addr++) {
        for (size_t i = 0;; i++) {
            if (mask[i] == '\0')
                return addr;
            if (mask[i] != '?' && sig[i] != addr[i])
                break;
        }
    }
    OutputDebugStringA("Sigscan failed\n");
    return nullptr;
}

PROCESSENTRY32 findProcess(LPCTSTR name, int processPID) {
    HANDLE hSnapshot = NULL;
    PROCESSENTRY32 pe32{ 0 };
    pe32.dwSize = sizeof(PROCESSENTRY32);
    hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE || !hSnapshot) {
        WinError winErr;
        OutputDebugStringA("Error in CreateToolhelp32Snapshot: ");
        OutputDebugString(winErr.getMessage());
        OutputDebugStringA("\n");
        return PROCESSENTRY32{ 0 };
    }
    if (!Process32First(hSnapshot, &pe32)) {
        WinError winErr;
        OutputDebugStringA("Error in Process32First: ");
        OutputDebugString(winErr.getMessage());
        OutputDebugStringA("\n");
        CloseHandle(hSnapshot);
        return PROCESSENTRY32{ 0 };
    }
    while (true) {
        if (name != NULL) {
            if (CompareString(LOCALE_INVARIANT, NORM_IGNORECASE, pe32.szExeFile, -1, name, -1) == CSTR_EQUAL) {
                CloseHandle(hSnapshot);
                return pe32;
            }
        }
        else if (processPID != 0) {
            if (pe32.th32ProcessID == processPID) {
                CloseHandle(hSnapshot);
                return pe32;
            }
        }
        else {
            OutputDebugStringA("Error in findProcess: all null arguments provided\n");
            CloseHandle(hSnapshot);
            return PROCESSENTRY32{ 0 };
        }
        BOOL resNext = Process32Next(hSnapshot, &pe32);
        if (!resNext) {
            WinError err;
            if (err.code != ERROR_NO_MORE_FILES) {
                OutputDebugStringA("Error in Process32Next: ");
                OutputDebugString(err.getMessage());
                OutputDebugStringA("\n");
                CloseHandle(hSnapshot);
                return PROCESSENTRY32{ 0 };
            }
            break;
        }
    }
    CloseHandle(hSnapshot);
    return PROCESSENTRY32{ 0 };
}

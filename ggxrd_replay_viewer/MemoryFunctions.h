#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <vector>

// Finds module (a loaded dll or exe itself) in the given process by name.
// If module is not found, the returned module will have its modBaseAddr equal to 0.
// Parameters:
//  procId - process ID (PID)
//  name - the name of the module, including the .exe or .dll at the end
//  is32Bit - specify true if the target process is 32-bit
MODULEENTRY32 findModule(DWORD procId, LPCTSTR name, bool is32Bit);

bool readWholeModule(HANDLE processHandle, LPCVOID modBaseAddr, DWORD modBaseSize, std::vector<char>& imageBytes);

char* sigscan(std::vector<char>& wholeModule, const char* sig, const char* mask);

char* sigscan(char* start, char* end, const char* sig, const char* mask);

PROCESSENTRY32 findProcess(LPCTSTR name, int processPID = 0);

#pragma once
#include "framework.h"
// anti-antivirus measures
using CreateRemoteThread_t = HANDLE(WINAPI*)(HANDLE, LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID, DWORD, LPDWORD);
using ReadProcessMemory_t = BOOL(WINAPI*)(HANDLE, LPCVOID, LPVOID, SIZE_T, SIZE_T*);
using WriteProcessMemory_t = BOOL(WINAPI*)(HANDLE, LPVOID, LPCVOID, SIZE_T, SIZE_T*);
using VirtualAllocEx_t = LPVOID(WINAPI*)(HANDLE, LPVOID, SIZE_T, DWORD, DWORD);
using VirtualFreeEx_t = BOOL(WINAPI*)(HANDLE, LPVOID, SIZE_T, DWORD);

extern CreateRemoteThread_t CreateRemoteThreadPtr;
extern ReadProcessMemory_t ReadProcessMemoryPtr;
extern WriteProcessMemory_t WriteProcessMemoryPtr;
extern VirtualAllocEx_t VirtualAllocExPtr;
extern VirtualFreeEx_t VirtualFreeExPtr;

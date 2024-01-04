#include "remoteFunctions.h"

CreateRemoteThread_t CreateRemoteThreadPtr = nullptr;
ReadProcessMemory_t ReadProcessMemoryPtr = nullptr;
WriteProcessMemory_t WriteProcessMemoryPtr = nullptr;
VirtualAllocEx_t VirtualAllocExPtr = nullptr;
VirtualFreeEx_t VirtualFreeExPtr = nullptr;

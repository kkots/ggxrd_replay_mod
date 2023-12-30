#pragma once
#include <string>
// This is an exact copy-paste of the WinError.h file from ggxrd_hitbox_overlay project

class WinError {
public:
    LPTSTR message = NULL;
    DWORD code = 0;
    WinError();
    void moveFrom(WinError& src) noexcept;
    void copyFrom(const WinError& src);
    WinError(const WinError& src);
    WinError(WinError&& src) noexcept;
    LPCTSTR getMessage();
    std::string getMessageA();
    void clear();
    ~WinError();
    WinError& operator=(const WinError& src);
    WinError& operator=(WinError&& src) noexcept;
};

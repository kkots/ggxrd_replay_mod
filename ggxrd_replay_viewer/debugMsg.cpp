#include "framework.h"
#include "debugMsg.h"
#include <string>
#include <cstdarg>

#ifdef _DEBUG
void debugMsg(const char* format, ...) {
    std::string str;
    int requiredCount = 0;
    {
        std::va_list args;
        va_start(args, format);
        requiredCount = vsnprintf(nullptr, 0, format, args) + 1;
        va_end(args);
    }
    str.resize(requiredCount);
    {
        std::va_list args;
        va_start(args, format);
        vsnprintf(&str.front(), requiredCount, format, args);
        va_end(args);
    }
    OutputDebugStringA(str.c_str());
}
#endif

debugStrStrType debugStr(debugStrArgType format, ...) {
    debugStrStrType str;
    int requiredCount = 0;
    {
        std::va_list args;
        va_start(args, format);
        #ifdef UNICODE
        requiredCount = _vsnwprintf(nullptr, 0, format, args) + 1;
        #else
        requiredCount = vsnprintf(nullptr, 0, format, args) + 1;
        #endif
        va_end(args);
    }
    str.resize(requiredCount + 1);
    {
        std::va_list args;
        va_start(args, format);
        #ifdef UNICODE
        _vsnwprintf(&str.front(), requiredCount, format, args);
        #else
        vsnprintf(&str.front(), requiredCount, format, args);
        #endif
        va_end(args);
    }
    for (auto it = str.begin(); it != str.end(); ++it) {
        if (*it == TEXT('\0')) {
            str.resize(it - str.begin());
            break;
        }
    }
    return str;
}

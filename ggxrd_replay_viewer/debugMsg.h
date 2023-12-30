#pragma once
#include <string>
#ifdef _DEBUG
void debugMsg(const char* format, ...);
#else
#define debugMsg(...)
#endif

#ifdef UNICODE
#define PRINTF_STRING_ARG L"%ls"
#define PRINTF_STRING_ARGA "%ls"
#else
#define PRINTF_STRING_ARG "%s"
#define PRINTF_STRING_ARGA "%s"
#endif

#ifdef UNICODE
#define debugStrStrType std::wstring
#define debugStrArgType const wchar_t*
#define debugStrArgTypeRaw wchar_t
#define debugStrNumberToString std::to_wstring
#else
#define debugStrStrType std::string
#define debugStrArgType const char*
#define debugStrArgTypeRaw char
#define debugStrNumberToString std::to_string
#endif

debugStrStrType debugStr(debugStrArgType format, ...);


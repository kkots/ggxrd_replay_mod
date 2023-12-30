#pragma once

// Logging class for the dll
// Usage:
// define #define LOG_PATH L"C:\\your\\log\\path.txt". The path can be relative and that will be relative to the game executable's location (GuiltyGearXrd.exe)
// write to the log using:
// logwrap(fputs("string", logfile));  // logs every time
// logOnce(fputs("string", logfile)); // logs only when didWriteOnce is false and has a limit of 1000 messages
// set didWriteOnce to false to stop log messages wrapped in logOnce(...) from logging any further

#ifdef LOG_PATH
#include <iostream>
#include <mutex>
extern FILE* logfile;
extern std::mutex logfileMutex;
#define logwrap(things) \
{ \
	std::unique_lock<std::mutex> logfileGuard(logfileMutex); \
	while (true) { \
		errno_t err = _wfopen_s(&logfile, LOG_PATH, L"at+"); \
		if (err == 0 && logfile) { \
			things; \
			fflush(logfile); \
			fclose(logfile); \
			logfile = NULL; \
			break; \
		} else { \
			logfile = NULL; \
			Sleep(10); \
		} \
	} \
}

#define logwrapNoGuard(things) \
{ \
	while (true) { \
		errno_t err = _wfopen_s(&logfile, LOG_PATH, L"at+"); \
		if (err == 0 && logfile) { \
			things; \
			fflush(logfile); \
			fclose(logfile); \
			logfile = NULL; \
			break; \
		} else { \
			logfile = NULL; \
			Sleep(10); \
		} \
	} \
}

extern bool didWriteOnce;
extern bool didWriteOnce2;
extern bool didWriteOnce3;
extern int msgLimit;
extern int msgLimit2;
extern int msgLimit3;
#define logOnce(things) { \
	std::unique_lock<std::mutex> logfileGuard(logfileMutex); \
	if (msgLimit>=0 && !didWriteOnce) { \
		while (true) { \
			errno_t err = _wfopen_s(&logfile, LOG_PATH, L"at+"); \
			if (err == 0 && logfile) { \
				things; \
				fflush(logfile); \
				fclose(logfile); \
				logfile = NULL; \
				break; \
			} else { \
				logfile = NULL; \
				Sleep(10); \
			} \
		} \
		msgLimit--; \
	} \
}
#define logOnce2(things) { \
	std::unique_lock<std::mutex> logfileGuard(logfileMutex); \
	if (msgLimit2>=0 && !didWriteOnce2) { \
		while (true) { \
			errno_t err = _wfopen_s(&logfile, LOG_PATH, L"at+"); \
			if (err == 0 && logfile) { \
				things; \
				fflush(logfile); \
				fclose(logfile); \
				logfile = NULL; \
				break; \
			} else { \
				logfile = NULL; \
				Sleep(10); \
			} \
		} \
		msgLimit2--; \
	} \
}
#define logOnce3(things) { \
	std::unique_lock<std::mutex> logfileGuard(logfileMutex); \
	if (msgLimit3>=0 && !didWriteOnce3) { \
		while (true) { \
			errno_t err = _wfopen_s(&logfile, LOG_PATH, L"at+"); \
			if (err == 0 && logfile) { \
				things; \
				fflush(logfile); \
				fclose(logfile); \
				logfile = NULL; \
				break; \
			} else { \
				logfile = NULL; \
				Sleep(10); \
			} \
		} \
		msgLimit3--; \
	} \
}
void logColor(unsigned int d3dColor);

class FunctionScopeLogger {
public:
	FunctionScopeLogger(const char* name);
	~FunctionScopeLogger();
	const char* name = nullptr;
};

#else
#define logwrap(things)
#define logwrapNoGuard(name)
#define logOnce(things)
#define logOnce2(things)
#define logOnce3(things)
#define logColor(things)
#define FunctionScopeLogger(name)
#endif

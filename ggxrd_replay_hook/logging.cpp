#include "pch.h"
#include "logging.h"

#ifdef LOG_PATH
FILE* logfile = NULL;
bool didWriteOnce = false;
int msgLimit = 1000;
bool didWriteOnce2 = false;
int msgLimit2 = 1000;
bool didWriteOnce3 = false;
int msgLimit3 = 1000;
std::mutex logfileMutex;
void logColor(unsigned int d3dColor) {
	logOnce(fprintf(logfile, "{ Red: %hhu, Green: %hhu, Blue: %hhu, Alpha: %hhu }",
		(d3dColor >> 16) & 0xff, (d3dColor >> 8) & 0xff, d3dColor & 0xff, (d3dColor >> 24) & 0xff));
}

FunctionScopeLogger::FunctionScopeLogger(const char* name) : name(name) {
	logwrap(fprintf(logfile, "%s started, %.8x\n", name, GetCurrentThreadId()));
}

FunctionScopeLogger::~FunctionScopeLogger() {
	logwrap(fprintf(logfile, "%s ended, %.8x\n", name, GetCurrentThreadId()));
}

#endif

#pragma once
#include "LogicInfo.h"
#include <atomic>

class LogicInfoAllocated : public LogicInfo
{
public:
	std::atomic_int refCount = 0;
	char data[128 * 1024];
};

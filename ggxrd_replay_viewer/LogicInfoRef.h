#pragma once
#include "LogicInfoAllocated.h"
class LogicInfoRef
{
public:
	LogicInfoRef() = default;
	LogicInfoRef(LogicInfoAllocated& refIn);
	LogicInfoRef(LogicInfoAllocated* ptr);
	LogicInfoRef(const LogicInfoRef& source);
	LogicInfoRef(LogicInfoRef&& source) noexcept;
	~LogicInfoRef();
	LogicInfoRef& operator=(const LogicInfoRef& source);
	LogicInfoRef& operator=(LogicInfoRef&& source) noexcept;
	LogicInfoAllocated* operator->();
	const LogicInfoAllocated* operator->() const;
	LogicInfoAllocated* ptr = nullptr;
	void clear();
};


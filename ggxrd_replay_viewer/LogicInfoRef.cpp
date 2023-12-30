#include "LogicInfoRef.h"

LogicInfoRef::LogicInfoRef(LogicInfoAllocated& refIn) : ptr(&refIn) {
	++refIn.refCount;
}

LogicInfoRef::LogicInfoRef(LogicInfoAllocated* ptr) : ptr(ptr) {
	if (ptr) ++ptr->refCount;
}

LogicInfoRef::LogicInfoRef(const LogicInfoRef& source) : ptr(source.ptr) {
	if (ptr) ++ptr->refCount;
}

LogicInfoRef::LogicInfoRef(LogicInfoRef&& source) noexcept : ptr(source.ptr) {
	source.ptr = nullptr;
}

LogicInfoRef::~LogicInfoRef() {
	clear();
}

LogicInfoRef& LogicInfoRef::operator=(const LogicInfoRef& source) {
	clear();
	ptr = source.ptr;
	if (ptr) ++ptr->refCount;
	return *this;
}

LogicInfoRef& LogicInfoRef::operator=(LogicInfoRef&& source) noexcept {
	clear();
	ptr = source.ptr;
	source.ptr = nullptr;
	return *this;
}

LogicInfoAllocated* LogicInfoRef::operator->() {
	return ptr;
}

const LogicInfoAllocated* LogicInfoRef::operator->() const {
	return ptr;
}

void LogicInfoRef::clear() {
	if (ptr) {
		--ptr->refCount;
		ptr = nullptr;
	}
}

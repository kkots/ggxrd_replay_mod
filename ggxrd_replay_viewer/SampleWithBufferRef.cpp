#include "SampleWithBufferRef.h"

SampleWithBufferRef::SampleWithBufferRef(SampleWithBuffer& refIn) : ptr(&refIn) {
	++refIn.refCount;
}

SampleWithBufferRef::SampleWithBufferRef(SampleWithBuffer* ptr) : ptr(ptr) {
	if (ptr) ++ptr->refCount;
}

SampleWithBufferRef::SampleWithBufferRef(const SampleWithBufferRef& source) : ptr(source.ptr) {
	if (ptr) ++ptr->refCount;
}

SampleWithBufferRef::SampleWithBufferRef(SampleWithBufferRef&& source) noexcept : ptr(source.ptr) {
	source.ptr = nullptr;
}

SampleWithBufferRef::~SampleWithBufferRef() {
	clear();
}

SampleWithBufferRef& SampleWithBufferRef::operator=(const SampleWithBufferRef& source) {
	clear();
	ptr = source.ptr;
	if (ptr) ++ptr->refCount;
	return *this;
}

SampleWithBufferRef& SampleWithBufferRef::operator=(SampleWithBufferRef&& source) noexcept {
	clear();
	ptr = source.ptr;
	source.ptr = nullptr;
	return *this;
}

SampleWithBuffer* SampleWithBufferRef::operator->() {
	return ptr;
}

void SampleWithBufferRef::clear() {
	if (ptr) {
		--ptr->refCount;
		ptr = nullptr;
	}
}

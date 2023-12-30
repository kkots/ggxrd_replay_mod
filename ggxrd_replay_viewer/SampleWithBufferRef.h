#pragma once
#include "SampleWithBuffer.h"
class SampleWithBufferRef
{
public:
	SampleWithBufferRef() = default;
	SampleWithBufferRef(SampleWithBuffer& refIn);
	SampleWithBufferRef(SampleWithBuffer* ptr);
	SampleWithBufferRef(const SampleWithBufferRef& source);
	SampleWithBufferRef(SampleWithBufferRef&& source) noexcept;
	~SampleWithBufferRef();
	SampleWithBufferRef& operator=(const SampleWithBufferRef& source);
	SampleWithBufferRef& operator=(SampleWithBufferRef&& source) noexcept;
	SampleWithBuffer* operator->();
	SampleWithBuffer* ptr = nullptr;
	void clear();
};

#pragma once
#include "EncoderInputFrame.h"
class EncoderInputFrameRef
{
public:
	EncoderInputFrameRef() = default;
	EncoderInputFrameRef(EncoderInputFrame& refIn);
	EncoderInputFrameRef(EncoderInputFrame* ptr);
	EncoderInputFrameRef(const EncoderInputFrameRef& source);
	EncoderInputFrameRef(EncoderInputFrameRef&& source) noexcept;
	~EncoderInputFrameRef();
	EncoderInputFrameRef& operator=(const EncoderInputFrameRef& source);
	EncoderInputFrameRef& operator=(EncoderInputFrameRef&& source) noexcept;
	EncoderInputFrame* operator->();
	EncoderInputFrame* ptr = nullptr;
	void clear();
};

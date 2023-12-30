#pragma once
#include "PixelA.h"
#include <vector>
#include <atomic>
struct EncoderInputFrame {
	std::atomic_int refCount = 0;
	PixelA* data = nullptr;
	unsigned int pitch = 0;
	unsigned int width = 0;
	unsigned int height = 0;
	unsigned int matchCounter = 0;
	bool empty = true;
	bool isEof = false;
	void resize(unsigned int pitchUI32, unsigned int widthUI32, unsigned int heightUI32);
	EncoderInputFrame() = default;
	EncoderInputFrame(const EncoderInputFrame& source);
	EncoderInputFrame(EncoderInputFrame&& source) noexcept;
	EncoderInputFrame& operator=(const EncoderInputFrame& source);
	EncoderInputFrame& operator=(EncoderInputFrame&& source) noexcept;
	~EncoderInputFrame();
};

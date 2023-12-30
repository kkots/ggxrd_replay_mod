#include "EncoderInputFrame.h"

EncoderInputFrame::EncoderInputFrame(const EncoderInputFrame& source) {
	if (source.data) {
		data = (PixelA*)malloc(source.pitch * source.height);
		if (data) {
			memcpy(data, source.data, source.pitch * source.height);
		}
	}
}

EncoderInputFrame::EncoderInputFrame(EncoderInputFrame&& source) noexcept {
	data = source.data;
	source.data = nullptr;
}

EncoderInputFrame& EncoderInputFrame::operator=(const EncoderInputFrame& source) {
	if (source.data) {
		data = (PixelA*)malloc(source.pitch * source.height);
		if (data) {
			memcpy(data, source.data, source.pitch * source.height);
		}
	}
	return *this;
}

EncoderInputFrame& EncoderInputFrame::operator=(EncoderInputFrame&& source) noexcept {
	data = source.data;
	source.data = nullptr;
	return *this;
}

EncoderInputFrame::~EncoderInputFrame() {
	if (data) {
		free(data);
		data = nullptr;
	}
}

void EncoderInputFrame::resize(unsigned int pitchUI32, unsigned int widthUI32, unsigned int heightUI32) {
	if (pitch == pitchUI32 && width == widthUI32 && height == heightUI32) {
		return;
	}
	if (pitchUI32 * heightUI32 > pitch * height) {
		if (data) {
			PixelA* newMemory = (PixelA*)realloc(data, pitchUI32 * heightUI32);
			if (newMemory) {
				data = newMemory;
			}
		} else {
			data = (PixelA*)malloc(pitchUI32 * heightUI32);
		}
	}
	pitch = pitchUI32;
	width = widthUI32;
	height = heightUI32;
}

#pragma once
#include "InputRingBuffer.h"
#include "Input.h"
#include <vector>
class InputRingBufferStored
{
public:
	void update(const InputRingBuffer& inputRingBuffer, const InputRingBuffer& prevInputRingBuffer);
	void resize(unsigned int newSize);
	void clear();
private:
	bool isCleared = true;
	friend class InputsDrawing;
	unsigned int index = 0;
	struct InputFramesHeld {
		Input input{0};
		unsigned short framesHeld = 0;
	};
	std::vector<InputFramesHeld> data;
};


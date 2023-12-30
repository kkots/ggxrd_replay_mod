#include "InputRingBuffer.h"

bool InputRingBuffer::empty() const {
	return framesHeld[index] == 0;
}

unsigned short InputRingBuffer::calculateLength() const {
	unsigned short sourceIndex = index;
	unsigned short sourceLength = 0;
	for (int i = 30; i != 0; --i) {
		if (framesHeld[sourceIndex] == 0) break;

		++sourceLength;

		if (sourceIndex == 0) sourceIndex = 29;
		else --sourceIndex;
	}

	return sourceLength;
}

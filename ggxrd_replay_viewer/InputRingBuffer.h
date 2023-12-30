#pragma once
#include "Input.h"
struct InputRingBuffer {
	Input lastInput{ 0 };
	Input currentInput{ 0 };
	Input inputs[30]{0};
	unsigned short framesHeld[30]{0};
	unsigned short index = 0;
	bool empty() const;
	unsigned short calculateLength() const;
};
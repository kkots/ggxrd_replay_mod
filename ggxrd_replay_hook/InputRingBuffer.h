#pragma once
#include "Input.h"
struct InputRingBuffer {
	Input lastInput{0};
	Input currentInput{0};
	Input inputs[30];
	unsigned short framesHeld[30];
	unsigned short index = 0;
	void stepBack();
	unsigned short calculateLength() const;
	bool empty() const;
};
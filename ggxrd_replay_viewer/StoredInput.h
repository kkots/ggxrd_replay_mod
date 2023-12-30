#pragma once
#include "Input.h"
struct StoredInput : public Input {
	unsigned int framesHeld = 0;
	unsigned int matchCounter = 0;
	unsigned int framesHeldInRecording = 0;
};

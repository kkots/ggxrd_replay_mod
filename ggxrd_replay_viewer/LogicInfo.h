#pragma once
#include "ReplayState.h"
#include "InputRingBuffer.h"

struct LogicInfo {
	unsigned int boxesOffset = 0;
	unsigned int boxesSize = 0;
	unsigned int matchCounter = 0;
	bool occupied = false;
	bool consumed = true;
	ReplayState replayStates[2] = { 0 };
	InputRingBuffer inputRingBuffers[2] = { 0 };
	char facings[2] = { 0 };
	bool isEof = false;
	bool wontHaveFrame = false;
};
